// Record actual mGBA video + native stereo audio using only timed GBA inputs.
// No RAM writes, cheats, interpolation or synthetic gameplay. MIT.
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>
#include <SDL2/SDL.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mColor pixels[240 * 160];
static int16_t samples[8192];
static void quiet(struct mLogger *l, int c, enum mLogLevel level, const char *f, va_list a)
{ (void)l; (void)c; (void)level; (void)f; (void)a; }
static void require(int ok, const char *message)
{ if (!ok) { fprintf(stderr, "%s\n", message); exit(1); } }

int main(int argc, char **argv)
{
    if (argc != 7 && argc != 8) {
        fprintf(stderr, "capture ROM STATE INPUT.txt VIDEO.rgb0 AUDIO.s16le FINAL.state [--live]\n");
        return 2;
    }
    require(sizeof(mColor) == 4, "This capture requires a 32-bit mGBA build");
    struct mLogger logger = {.log = quiet};
    mLogSetDefaultLogger(&logger);
    struct mCore *core = mCoreFind(argv[1]);
    require(core && core->init(core), "Cannot initialize mGBA");
    mCoreInitConfig(core, NULL);
    mCoreConfigSetDefaultIntValue(&core->config, "useBios", 0);
    mCoreConfigSetDefaultIntValue(&core->config, "skipBios", 1);
    mCoreConfigSetDefaultIntValue(&core->config, "volume", 256);
    mCoreLoadConfig(core);
    core->setVideoBuffer(core, pixels, 240);
    core->setAudioBufferSize(core, 4096);
    require(mCoreLoadFile(core, argv[1]), "Cannot load ROM");
    core->reset(core);
    struct VFile *vf = VFileOpen(argv[2], O_RDONLY);
    require(vf && mCoreLoadStateNamed(core, vf, SAVESTATE_ALL), "Cannot load matching state");
    vf->close(vf);
    mCoreLoadConfig(core); // Lab's muted frontend is not used for this recording.
    struct mAudioBuffer *audio = core->getAudioBuffer(core);
    require(audio && audio->channels == 2, "Expected native stereo audio");
    mAudioBufferClear(audio);
    unsigned sampleRate = core->audioSampleRate(core);
    int frequency = core->frequency(core), frameCycles = core->frameCycles(core);
    FILE *input = fopen(argv[3], "r"), *video = fopen(argv[4], "wx"), *pcm = fopen(argv[5], "wx");
    require(input && video && pcm, "Cannot open inputs or output already exists");
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_AudioDeviceID speaker = 0;
    if (argc == 8) {
        require(!strcmp(argv[7], "--live"), "Unknown option");
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, SDL_GetError());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        window = SDL_CreateWindow("Emerald Arena — LIVE RECORDING", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 960, 640, SDL_WINDOW_SHOWN);
        require(window != NULL, SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        require(renderer != NULL, SDL_GetError());
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 240, 160);
        require(texture != NULL, SDL_GetError());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        SDL_RaiseWindow(window);
        SDL_AudioSpec wanted = {.freq = (int)sampleRate, .format = AUDIO_S16SYS, .channels = 2, .samples = 1024};
        speaker = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (speaker) SDL_PauseAudioDevice(speaker, 0);
    }
    unsigned totalFrames = 0;
    size_t totalSamples = 0;
    char line[256];
    Uint64 start = SDL_GetPerformanceCounter(), timerHz = SDL_GetPerformanceFrequency();
    while (fgets(line, sizeof(line), input)) {
        unsigned frames, keys;
        if (line[0] == '#' || line[0] == '\n') continue;
        require(sscanf(line, "%u %u", &frames, &keys) == 2, "Expected: frame_count key_mask");
        require(frames > 0 && frames <= 1800 && totalFrames + frames <= 3600 && keys < 1024, "Input out of bounds");
        core->setKeys(core, keys);
        for (unsigned i = 0; i < frames; i++) {
            core->runFrame(core);
            require(fwrite(pixels, sizeof(mColor), 240 * 160, video) == 240 * 160, "Video write failed");
            size_t available = mAudioBufferAvailable(audio);
            require(available < mAudioBufferCapacity(audio), "Native audio buffer overflow");
            size_t n = mAudioBufferRead(audio, samples, 4096);
            require(fwrite(samples, 2 * sizeof(int16_t), n, pcm) == n, "Audio write failed");
            totalSamples += n;
            ++totalFrames;
            if (window) {
                if (speaker) SDL_QueueAudio(speaker, samples, n * 2 * sizeof(int16_t));
                SDL_UpdateTexture(texture, NULL, pixels, 240 * sizeof(mColor));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);
                SDL_Event e;
                while (SDL_PollEvent(&e)) require(e.type != SDL_QUIT, "Recording cancelled");
                double target = (double)totalFrames * frameCycles / frequency;
                double elapsed = (double)(SDL_GetPerformanceCounter() - start) / timerHz;
                if (target > elapsed) SDL_Delay((Uint32)((target - elapsed) * 1000));
            }
        }
    }
    require(!ferror(input) && totalFrames, "Empty or unreadable input script");
    fclose(input);
    require(fclose(video) == 0 && fclose(pcm) == 0, "Output flush failed");
    vf = VFileOpen(argv[6], O_WRONLY | O_CREAT | O_EXCL);
    require(vf && mCoreSaveStateNamed(core, vf, SAVESTATE_ALL), "Cannot save final state");
    vf->close(vf);
    printf("{\"frames\":%u,\"frequency\":%d,\"frame_cycles\":%d,\"sample_rate\":%u,\"stereo_samples\":%zu,\"duration\":%.6f}\n",
           totalFrames, frequency, frameCycles, sampleRate, totalSamples, (double)totalFrames * frameCycles / frequency);
    if (speaker) SDL_CloseAudioDevice(speaker);
    if (window) { SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); }
    core->deinit(core);
    return 0;
}
