"""TypeSafe's Jev over its HTTP API: one Choice question per request, on a
kept-open connection."""
import json
import os
import subprocess
import time

import httpx

URL = os.environ.get('EMERALD_VOICE_JEV_URL', 'https://api.typesafe.ai/v1/systemone')   # override for tests only


def find_key():
    """TYPESAFE_API_KEY, else the macOS Keychain item 'emerald-voice-typesafe'."""
    key = os.environ.get('TYPESAFE_API_KEY', '').strip()
    if key:
        return key
    try:
        out = subprocess.run(['security', 'find-generic-password', '-s', 'emerald-voice-typesafe', '-w'],
                             capture_output=True, text=True, timeout=5)
        return out.stdout.strip() or None
    except (OSError, subprocess.SubprocessError):
        return None


class JevError(Exception):
    pass


class Jev:
    def __init__(self, key, model='jev-latest', timeout=3.0):
        self.model = model
        self.client = httpx.Client(timeout=timeout, headers={'Authorization': f'Bearer {key}'})

    def choice(self, state, instructions, criteria):
        """Returns (choice, confidence, probabilities, milliseconds)."""
        body = {'state': state if isinstance(state, (str, dict, list)) else json.dumps(state), 'model': self.model,
                'questions': {'q': {'type': 'choice', 'instructions': instructions, 'criteria': criteria}}}
        for attempt in range(3):
            t = time.perf_counter()
            try:
                r = self.client.post(URL, json=body)
            except httpx.HTTPError as e:
                raise JevError(f'network error: {e}') from e
            ms = (time.perf_counter() - t) * 1000
            if r.status_code in (429, 529) and attempt < 2:
                time.sleep(0.2 * (2 ** attempt))
                continue
            if r.status_code == 401:
                raise JevError('the TypeSafe API key was rejected (401)')
            if r.status_code != 200:
                raise JevError(f'HTTP {r.status_code}: {r.text[:200]}')
            a = r.json()['answers']['q']
            return a['choice'], a.get('confidence', 0.0), a.get('probabilities', {}), ms
        raise JevError('rate limited')
