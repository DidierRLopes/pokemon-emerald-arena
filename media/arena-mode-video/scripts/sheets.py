"""Contact sheets per shot from a render: python3 scripts/sheets.py out/draft.mp4"""
import subprocess, sys, tempfile
from pathlib import Path
from PIL import Image, ImageDraw
src = sys.argv[1]
PRE = 0.3
SHOTS = {'0-thumb': (-0.3, 0.0), '1-cap': (0, 4), '2-board': (4, 10), '3-picks': (10, 14), '4-gameday': (14, 16),
         '5-coaches': (16, 22), '6-replays': (22, 28), '7-finale': (28, 33)}
out = Path('out/sheets'); out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    for name, (a, b) in SHOTS.items():
        step = 0.1 if b - a <= 0.5 else 0.25
        times = [a + i * step for i in range(int((b - a) / step))]
        ims = []
        for i, t in enumerate(times):
            f = f'{tmp}/{name}-{i}.png'
            subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-ss', f'{t + PRE + 0.001:.3f}', '-i', src, '-frames:v', '1', '-vf', 'scale=480:270', f], check=True)
            ims.append((t, Image.open(f).convert('RGB')))
        cols = 6
        rows = (len(ims) + cols - 1) // cols
        sheet = Image.new('RGB', (cols * 480, rows * 290), 'white')
        d = ImageDraw.Draw(sheet)
        for i, (t, im) in enumerate(ims):
            x, y = (i % cols) * 480, (i // cols) * 290
            sheet.paste(im, (x, y + 20)); d.text((x + 4, y + 4), f'{name} t={t:.2f}s', fill='black')
        sheet.save(out / f'{name}.png')
print('sheets in', out)
