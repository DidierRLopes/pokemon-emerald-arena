"""Push-to-talk: hold Right Option, speak, release. The microphone stream
stays open while the agent runs (macOS shows the orange microphone dot) so
the start of a word is never cut off; only audio from while the key is held,
plus a short lead-in, is transcribed, on this Mac, with Whisper."""
import ctypes
import ctypes.util
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
P2_KEYS = {34: 64, 40: 128, 38: 32, 37: 16, 32: 1, 31: 2, 16: 512, 35: 256}


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
        return (0 if sims[0] >= sims[1] else 1), sims


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

    def __init__(self, transcriber, on_text, vocabulary=lambda: '', log=print, speakers=None):
        import sounddevice
        self.transcriber, self.on_text, self.vocabulary, self.log = transcriber, on_text, vocabulary, log
        self.speakers, self.forced = speakers, None
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
        for player in (0, 1):
            for phrase in phrases:
                self.log(f'Player {player + 1}: hold Right Option and say: "{phrase}"')
                while True:
                    audio, _ = self.record()
                    if len(audio) > RATE * 1.0:
                        clips[player].append(audio)
                        break
                    self.log('  (too short; hold the key for the whole sentence)')
        self.speakers.enroll(clips)
        self.log('Voices saved. Right Option: whoever speaks; Left Option: always Player 2.')

    def run(self):
        self.stream.start()
        if self.speakers and self.speakers.profiles is None and self.speakers.want_enroll:
            self.enroll()
        while True:
            audio, key = self.record()
            if len(audio) > RATE * 0.35:
                threading.Thread(target=self._finish, args=(audio, key), daemon=True).start()

    def _finish(self, audio, key):
        t = time.perf_counter()
        text = self.transcriber.transcribe(audio, self.vocabulary())
        ms = (time.perf_counter() - t) * 1000
        if not text or not any(c.isalpha() for c in text):
            self.log(f'(nothing heard, {ms:.0f} ms)')
            return
        player, who = 0, ''
        if self.speakers:
            if not self.speakers.active():
                player, who = 0, ''
            elif key == 'left':
                player, who = 1, ' P2 (Left Option)'
            elif self.speakers.profiles is None:
                player, who = 0, ' P1 (Right Option)'
            else:
                player, sims = self.speakers.who(audio)
                who = f' P{player + 1} (voice {sims[0]:.2f} vs {sims[1]:.2f})'
        self.log(f'🎙{who} "{text}" (heard in {ms:.0f} ms)')
        self.on_text(text, player)
