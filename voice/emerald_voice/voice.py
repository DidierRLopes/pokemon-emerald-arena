"""Push-to-talk: hold Right Option, speak, release. The microphone stream
stays open while the agent runs (macOS shows the orange microphone dot) so
the start of a word is never cut off; only audio from while the key is held,
plus a short lead-in, is transcribed, on this Mac, with Whisper.

Hands-free (--hands-free): the microphone is listened to all the time; a
voice detector cuts speech into utterances (a pause of about a third of a
second ends one), and each is transcribed and acted on. With two players,
each utterance goes to whoever's voice it matches. Push-to-talk keeps working
alongside."""
import ctypes
import ctypes.util
import queue
import subprocess
import threading
import time
from collections import deque

import numpy as np

RATE = 16000
RIGHT_OPTION = 0x40            # NX_DEVICERALTKEYMASK: the right-hand Option key only
LEFT_OPTION = 0x20             # NX_DEVICELALTKEYMASK
_quartz = ctypes.cdll.LoadLibrary(ctypes.util.find_library('ApplicationServices'))
_quartz.CGEventSourceFlagsState.restype = ctypes.c_uint64
_quartz.CGEventSourceFlagsState.argtypes = [ctypes.c_int32]
_quartz.CGEventSourceKeyState.restype = ctypes.c_bool
_quartz.CGEventSourceKeyState.argtypes = [ctypes.c_int32, ctypes.c_uint16]


def right_option_down():
    return bool(_quartz.CGEventSourceFlagsState(0) & RIGHT_OPTION)   # combined session state


def left_option_down():
    return bool(_quartz.CGEventSourceFlagsState(0) & LEFT_OPTION)


# Player 2 on the same keyboard, as GBA keys: I J K L move, U attacks (A),
# O dodges (B), Y and P change move (L, R). mGBA leaves these keys unbound.
# H switches Pokémon (SELECT).
P2_KEYS = {34: 64, 40: 128, 38: 32, 37: 16, 32: 1, 31: 2, 16: 512, 35: 256, 4: 4}


def p2_keys():
    mask = 0
    for code, bit in P2_KEYS.items():
        if _quartz.CGEventSourceKeyState(0, code):
            mask |= bit
    return mask


class SpeakerId:
    """Tells the two players apart by voice: each enrolls with two phrases,
    and every command goes to the closer voice profile (Resemblyzer, on this
    Mac, about 10 ms per command)."""

    PROFILE = __import__('pathlib').Path.home() / '.config/emerald-voice/voices.npz'

    def __init__(self, active=lambda: True, want_enroll=False):
        # active(): whether a 1P vs 1P battle is on; otherwise every command is Player 1's.
        self.active, self.want_enroll = active, want_enroll
        from resemblyzer import VoiceEncoder, preprocess_wav
        self.encoder, self.pre = VoiceEncoder('cpu', verbose=False), preprocess_wav
        self.embed(np.zeros(RATE, np.float32) + 1e-3)            # warm up
        self.profiles = None
        self.learned = []          # voices picked up during play when none are saved
        self.expect = []           # versus screen: whose voice the next utterances are
        if self.PROFILE.exists():
            d = np.load(self.PROFILE)
            self.profiles = [d['p1'], d['p2']]

    def embed(self, audio):
        e = self.encoder.embed_utterance(self.pre(audio, source_sr=RATE))
        return e / (np.linalg.norm(e) + 1e-9)

    def enroll(self, clips):
        """clips: [[P1 audio, ...], [P2 audio, ...]]"""
        self.profiles = []
        for player in clips:
            m = np.mean([self.embed(a) for a in player], axis=0)
            self.profiles.append(m / np.linalg.norm(m))
        self.PROFILE.parent.mkdir(parents=True, exist_ok=True)
        np.savez(self.PROFILE, p1=self.profiles[0], p2=self.profiles[1])

    def who(self, audio):
        e = self.embed(audio)
        sims = [float(np.dot(e, p)) for p in self.profiles]
        player = 0 if sims[0] >= sims[1] else 1
        # A clear match nudges that profile toward today's voice (microphone,
        # distance, a cold), so recognition improves while playing.
        if abs(sims[0] - sims[1]) > 0.08 and max(sims) > 0.6:
            p = 0.92 * self.profiles[player] + 0.08 * e
            self.profiles[player] = p / np.linalg.norm(p)
        return player, sims

    SAME = 0.72    # cosine similarity above which two clips are taken as one voice

    def learn(self, audio):
        """No saved voices: the first voice heard in a 1P vs 1P battle is
        Player 1, the first clearly different one Player 2; after that it is
        the usual nearest match. Returns (player, note)."""
        e = self.embed(audio)
        if not self.learned:
            self.learned.append(e)
            return 0, 'first voice heard: Player 1'
        sims = [float(np.dot(e, p)) for p in self.learned]
        if len(self.learned) == 1:
            if sims[0] >= self.SAME:
                p = 0.9 * self.learned[0] + 0.1 * e
                self.learned[0] = p / np.linalg.norm(p)
                return 0, f'voice {sims[0]:.2f}'
            self.learned.append(e)
            return 1, f'new voice ({sims[0]:.2f} to Player 1): Player 2'
        player = 0 if sims[0] >= sims[1] else 1
        if abs(sims[0] - sims[1]) > 0.08:
            p = 0.9 * self.learned[player] + 0.1 * e
            self.learned[player] = p / np.linalg.norm(p)
        return player, f'voice {sims[0]:.2f} vs {sims[1]:.2f}'


class Segmenter:
    """Cuts a live stream into utterances: WebRTC's voice detector on 30 ms
    frames, gated by loudness over the room's noise floor so fans, clicks and
    quiet game music don't count. Speech starts after 4 voiced frames out of
    6, and ends after `hang` ms without voice; 0.3 s before the start is kept
    so the first sound isn't lost."""

    FRAME = 480            # 30 ms at 16 kHz

    def __init__(self, hang=330, max_seconds=5.0):
        import webrtcvad
        self.vad = webrtcvad.Vad(3)
        self.hang, self.max_frames = hang // 30, int(max_seconds * 1000 / 30)
        self.buf = np.zeros(0, np.float32)
        self.pre = deque(maxlen=10)
        self.recent = deque(maxlen=6)
        self.frames, self.voiced, self.silent = [], 0, 0
        self.noise = 0.004
        self.active = False

    def _is_speech(self, frame):
        rms = float(np.sqrt(np.mean(frame ** 2)) + 1e-9)
        pcm = (np.clip(frame, -1, 1) * 32767).astype(np.int16).tobytes()
        speech = self.vad.is_speech(pcm, RATE) and rms > max(0.006, self.noise * 2.5)
        if not speech and not self.active:
            self.noise = 0.97 * self.noise + 0.03 * rms        # track the room
        return speech

    def feed(self, block):
        """Returns the finished utterances (usually none) for this block."""
        out = []
        self.buf = np.concatenate([self.buf, block])
        while len(self.buf) >= self.FRAME:
            frame, self.buf = self.buf[:self.FRAME], self.buf[self.FRAME:]
            speech = self._is_speech(frame)
            if not self.active:
                self.pre.append(frame)
                self.recent.append(speech)
                if sum(self.recent) >= 4:
                    self.active, self.frames, self.voiced, self.silent = True, list(self.pre), sum(self.recent), 0
                    self.recent.clear()
                continue
            self.frames.append(frame)
            self.voiced += speech
            self.silent = 0 if speech else self.silent + 1
            if self.silent >= self.hang or len(self.frames) >= self.max_frames:
                if self.voiced >= 8:                            # at least ~0.25 s of voice
                    out.append(np.concatenate(self.frames))
                self.active, self.frames = False, []
                self.pre.clear()
        return out


# Whisper's usual inventions for noise, breathing or music.
HALLUCINATIONS = {'you', 'thank you', 'thanks for watching', 'thank you for watching', 'bye', 'okay', 'so',
                  'the end', 'music', 'applause', 'silence', 'hmm', 'uh', 'um'}


def chime(name):
    subprocess.Popen(['afplay', '-v', '0.35', f'/System/Library/Sounds/{name}.aiff'],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


class Transcriber:
    def __init__(self, model):
        import mlx_whisper
        self.mlx_whisper, self.model = mlx_whisper, model
        self.transcribe(np.zeros(RATE // 2, np.float32), '')   # load and warm up

    def transcribe(self, audio, vocabulary):
        prompt = 'Pokémon battle commands: get closer, go away, dodge left, dodge right, go up, stop. ' + vocabulary
        r = self.mlx_whisper.transcribe(audio, path_or_hf_repo=self.model, initial_prompt=prompt, language='en',
                                        temperature=0.0, condition_on_previous_text=False, no_speech_threshold=0.6)
        return r['text'].strip()


class PushToTalk:
    """on_text(text, player): player is 0 unless two players are set up, in
    which case Right Option lets the voice decide and Left Option is Player 2."""

    def __init__(self, transcriber, on_text, vocabulary=lambda: '', log=print, speakers=None, hands_free=False):
        import sounddevice
        self.transcriber, self.on_text, self.vocabulary, self.log = transcriber, on_text, vocabulary, log
        self.speakers, self.forced = speakers, None
        self.hands_free = hands_free
        self.segmenter = Segmenter() if hands_free else None
        self.blocks = queue.Queue()
        self.utterances = queue.Queue()
        self.enrolling = False
        self.on_enrolled = None
        self.lead = deque(maxlen=int(RATE * 0.3 / 512) + 1)   # about 0.3 s before the key press
        self.chunks, self.recording = [], False
        self.stream = sounddevice.InputStream(samplerate=RATE, channels=1, dtype='float32', blocksize=512,
                                              callback=self._audio)

    def _audio(self, data, frames, time_info, status):
        block = data[:, 0].copy()
        if self.recording:
            self.chunks.append(block)
        else:
            self.lead.append(block)
            if self.hands_free:
                self.blocks.put(block)        # the voice detector runs on its own thread

    def _listen(self):
        """Hands-free: utterances from the voice detector, one at a time."""
        while True:
            block = self.blocks.get()
            if self.recording:
                self.segmenter = Segmenter()  # push-to-talk has the floor
                continue
            for audio in self.segmenter.feed(block):
                self.utterances.put(audio)

    def next_utterance(self):
        """Hands-free enrollment: the next thing said, no key needed."""
        while True:
            audio = self.utterances.get()
            if len(audio) > RATE * 1.0:
                return audio
            self.log('  (too short; say the whole sentence)')

    def record(self):
        """One push-to-talk clip: (audio, key) with key 'right' or 'left'."""
        while True:
            key = 'right' if right_option_down() else 'left' if (self.speakers and left_option_down()) else None
            if key:
                break
            time.sleep(0.015)
        self.chunks = list(self.lead)
        self.recording = True
        chime('Tink')
        while right_option_down() or (key == 'left' and left_option_down()):
            time.sleep(0.015)
        self.recording = False
        return (np.concatenate(self.chunks) if self.chunks else np.zeros(0, np.float32)), key

    def enroll(self):
        phrases = ['I choose you, let us win this battle together.', 'Flamethrower, dodge left, get closer, go away.']
        clips = [[], []]
        self.enrolling = True
        while not self.utterances.empty():
            self.utterances.get()
        for player in (0, 1):
            for phrase in phrases:
                if self.hands_free:
                    self.log(f'Player {player + 1}, say: "{phrase}"')
                    clips[player].append(self.next_utterance())
                    chime('Pop')
                    continue
                self.log(f'Player {player + 1}: hold Right Option and say: "{phrase}"')
                while True:
                    audio, _ = self.record()
                    if len(audio) > RATE * 1.0:
                        clips[player].append(audio)
                        break
                    self.log('  (too short; hold the key for the whole sentence)')
        self.speakers.enroll(clips)
        self.enrolling = False
        self.log('Voices saved: each command now goes to whoever said it.'
                 + ('' if self.hands_free else ' Left Option is always Player 2.'))

    def run(self):
        self.stream.start()
        if self.hands_free:
            threading.Thread(target=self._listen, daemon=True).start()
        if self.speakers and self.speakers.profiles is None and self.speakers.want_enroll:
            self.enroll()
        if self.hands_free:
            threading.Thread(target=self._hands_free, daemon=True).start()
        while True:
            audio, key = self.record()
            if len(audio) > RATE * 0.35:
                threading.Thread(target=self._finish, args=(audio, key), daemon=True).start()

    def _hands_free(self):
        # One transcription at a time, in order; a backlog is cut to the
        # latest command (an old order is worse than none in a fight).
        while True:
            audio = self.utterances.get()
            while not self.utterances.empty():
                audio = self.utterances.get()
            self._finish(audio, 'voice')

    def _finish(self, audio, key):
        if self.speakers and self.speakers.expect and key == 'voice' and len(audio) > RATE * 0.4:
            # Versus screen, 1P vs 1P: this utterance sets a player's voice.
            player = self.speakers.expect.pop(0)
            e = self.speakers.embed(audio)
            while len(self.speakers.learned) <= player:
                self.speakers.learned.append(e)
            self.speakers.learned[player] = e
            self.log(f'Voice set for Player {player + 1}.')
            chime('Pop')
            if self.on_enrolled:
                self.on_enrolled(player)
            return
        t = time.perf_counter()
        text = self.transcriber.transcribe(audio, self.vocabulary())
        ms = (time.perf_counter() - t) * 1000
        if not text or not any(c.isalpha() for c in text):
            if key != 'voice':
                self.log(f'(nothing heard, {ms:.0f} ms)')
            return
        if key == 'voice' and ''.join(c for c in text.lower() if c.isalpha() or c == ' ').strip() in HALLUCINATIONS:
            return
        player, who = 0, ''
        if self.speakers:
            if not self.speakers.active():
                player, who = 0, ''
            elif key == 'left':
                player, who = 1, ' P2 (Left Option)'
            elif self.speakers.profiles is None and key == 'voice':
                player, note = self.speakers.learn(audio)
                who = f' P{player + 1} ({note})'
            elif self.speakers.profiles is None:
                player, who = 0, ' P1 (Right Option)'
            else:
                player, sims = self.speakers.who(audio)
                who = f' P{player + 1} (voice {sims[0]:.2f} vs {sims[1]:.2f})'
        self.log(f'🎙{who} "{text}" (heard in {ms:.0f} ms)')
        self.on_text(text, player)
