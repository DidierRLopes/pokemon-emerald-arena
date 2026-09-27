"""Talk to your Pokémon: python -m emerald_voice [--text] [--verbose]"""
import argparse
import sys
import threading
import time
from pathlib import Path

from .brain import Brain
from .bridge import LuaBridge
from .executor import Executor
from .gamedata import GameData, load_symbols
from .jev import Jev, find_key

REPO = Path(__file__).resolve().parents[2]


def control_server(port, command):
    """Typed commands from another terminal, one per line (localhost only)."""
    import socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('127.0.0.1', port))
    except OSError:
        return
    server.listen(4)
    while True:
        conn, _ = server.accept()
        with conn:
            for line in conn.makefile('r', encoding='utf-8', errors='replace'):
                if line.strip():
                    command(line.strip())


def main():
    ap = argparse.ArgumentParser(description='Voice control for Emerald Arena in desktop mGBA.')
    ap.add_argument('--symbols', default=str(REPO / 'Emerald-Arena-voice.sym'),
                    help='symbol list of the ROM being played (arm-none-eabi-nm output, or its .map)')
    ap.add_argument('--source', default=str(REPO / 'workspace'), help='the pokeemerald workspace the ROM was built from')
    ap.add_argument('--port', type=int, default=7355)
    ap.add_argument('--text', action='store_true', help='type commands instead of speaking them')
    ap.add_argument('--no-jev', action='store_true', help='use the built-in rules only')
    ap.add_argument('--stt-model', default='mlx-community/whisper-large-v3-turbo',
                    help='Whisper model; mlx-community/whisper-small.en-mlx is about three times faster')
    ap.add_argument('--verbose', action='store_true', help='log every control decision')
    ap.add_argument('--reset-state', default=str(Path.home() / '.config/mgba/states/voice-charizard.ss1'),
                    help='save state that "reset the fight" loads')
    ap.add_argument('--control-port', type=int, default=7357,
                    help='local port that accepts typed commands, e.g. echo reset | nc localhost 7357')
    args = ap.parse_args()

    bridge = None

    def log(msg):
        print(msg, flush=True)
        if bridge and not msg.startswith('    ['):
            bridge.show(msg.strip())
    data = GameData(args.source)
    symbols = load_symbols(args.symbols)
    executor = Executor()
    bridge = LuaBridge(data, symbols, executor.on_frame, port=args.port, log=log)
    jev = None
    if not args.no_jev:
        key = find_key()
        if key:
            jev = Jev(key)
        else:
            log('No TypeSafe API key found (TYPESAFE_API_KEY or Keychain item "emerald-voice-typesafe"); using the built-in rules.')
    brain = Brain(bridge, executor, data, jev=jev, log=log, verbose=args.verbose, reset_state=args.reset_state)
    bridge.start()
    threading.Thread(target=control_server, args=(args.control_port, brain.command), daemon=True).start()
    log(f'Waiting for the game on port {args.port}: in mGBA, Tools > Scripting > File > Load script > voice/bridge.lua')

    def vocabulary():
        s = bridge.state
        if not s:
            return ''
        names = [data.moves.get(m, '') for m in s.me.moves if m] + [m.name for m in s.party]
        return 'Moves and Pokémon: ' + ', '.join(n for n in names if n) + '.'

    if args.text:
        log('Type a command and press Enter (Ctrl-C to quit).')
        for line in sys.stdin:
            if line.strip():
                brain.command(line.strip())
        return
    from .voice import PushToTalk, Transcriber
    log(f'Loading speech recognition ({args.stt_model})…')
    ptt = PushToTalk(Transcriber(args.stt_model), brain.command, vocabulary, log=log)
    log('Ready. Hold Right Option, say a command, release.')
    ptt.run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
