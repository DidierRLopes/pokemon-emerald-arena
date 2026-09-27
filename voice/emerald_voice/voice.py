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
_quartz = ctypes.cdll.LoadLibrary(ctypes.util.find_library('ApplicationServices'))
_quartz.CGEventSourceFlagsState.restype = ctypes.c_uint64
_quartz.CGEventSourceFlagsState.argtypes = [ctypes.c_int32]


def right_option_down():
    return bool(_quartz.CGEventSourceFlagsState(0) & RIGHT_OPTION)   # combined session state


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
    def __init__(self, transcriber, on_text, vocabulary=lambda: '', log=print):
        import sounddevice
        self.transcriber, self.on_text, self.vocabulary, self.log = transcriber, on_text, vocabulary, log
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

    def run(self):
        self.stream.start()
        was_down = False
        while True:
            down = right_option_down()
            if down and not was_down:
                self.chunks = list(self.lead)
                self.recording = True
                chime('Tink')
            elif was_down and not down:
                self.recording = False
                audio = np.concatenate(self.chunks) if self.chunks else np.zeros(0, np.float32)
                if len(audio) > RATE * 0.35:
                    threading.Thread(target=self._finish, args=(audio,), daemon=True).start()
            was_down = down
            time.sleep(0.015)

    def _finish(self, audio):
        t = time.perf_counter()
        text = self.transcriber.transcribe(audio, self.vocabulary())
        ms = (time.perf_counter() - t) * 1000
        if not text or not any(c.isalpha() for c in text):
            self.log(f'(nothing heard, {ms:.0f} ms)')
            return
        self.log(f'🎙 "{text}" (heard in {ms:.0f} ms)')
        self.on_text(text)
