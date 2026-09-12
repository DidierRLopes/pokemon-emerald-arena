#!/usr/bin/env python3
"""Verify the two-second intro preserves every approved battle frame.

Requires ffmpeg and ffprobe on PATH. No ROM, save or snapshot is needed.
"""
import hashlib
import json
from pathlib import Path
import subprocess

MEDIA = Path(__file__).resolve().parents[1]


def frames(path, skip):
    result = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-i', str(path), '-map', '0:v:0',
        '-vf', f'trim=start_frame={skip},setpts=PTS-STARTPTS',
        '-fps_mode', 'passthrough', '-pix_fmt', 'rgba', '-f', 'framemd5', '-'])
    return [line.split(b',')[-1].strip().decode() for line in result.splitlines()
            if line and not line.startswith(b'#')]


def main():
    evidence = []
    for extension, intro_frames, battle_frames in [('mp4', 120, 900), ('gif', 40, 300)]:
        original = MEDIA / f'emerald-arena-15s.{extension}'
        edited = MEDIA / f'emerald-arena-17s.{extension}'
        before, after = frames(original, 0), frames(edited, intro_frames)
        assert len(before) == battle_frames and before == after, extension
        info = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-show_entries', 'format=duration,size',
            '-of', 'json', str(edited)]))['format']
        assert float(info['duration']) == 17.0, info
        if extension == 'gif':
            assert int(info['size']) < 5_000_000, 'GIF exceeds mobile upload budget'
        evidence.append(dict(format=extension, duration=17, identical_battle_frames=battle_frames,
            size=int(info['size']), sha256=hashlib.sha256(edited.read_bytes()).hexdigest()))
    print(json.dumps(dict(ok=True, files=evidence), indent=2))


if __name__ == '__main__':
    main()
