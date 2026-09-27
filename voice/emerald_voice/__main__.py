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
                line = line.strip()
                if line.lower().startswith('p2 '):
                    command(line[3:], 1)
                elif line:
                    command(line)


def main():
    ap = argparse.ArgumentParser(description='Voice control for Emerald Arena in desktop mGBA.')
    ap.add_argument('--symbols', default=str(REPO / 'Emerald-Arena-voice.sym'),
                    help='symbol list of the ROM being played (arm-none-eabi-nm output, or its .map)')
    ap.add_argument('--source', default=str(REPO / 'workspace'), help='the pokeemerald workspace the ROM was built from')
    ap.add_argument('--port', type=int, default=7355)
    ap.add_argument('--text', action='store_true', help='type commands instead of speaking them')
    ap.add_argument('--no-jev', action='store_true', help='use the built-in rules only')
    ap.add_argument('--stt-model', default='mlx-community/whisper-small.en-mlx',
                    help='Whisper model (default: the fast small English one, ~80 ms); '
                         'mlx-community/whisper-large-v3-turbo is more robust, ~250 ms')
    ap.add_argument('--two-players', action='store_true',
                    help='Arena mode 1P vs 1P: Player 2 on I J K L / U O / Y P, voices told apart')
    ap.add_argument('--enroll', action='store_true', help='record both players\' voices again')
    ap.add_argument('--hands-free', action='store_true',
                    help='listen all the time: just speak, no key (push-to-talk still works)')
    ap.add_argument('--verbose', action='store_true', help='log every control decision')
    ap.add_argument('--reset-state', default=str(Path.home() / '.config/mgba/states/voice-charizard.ss1'),
                    help='save state that "reset the fight" loads')
    ap.add_argument('--control-port', type=int, default=7357,
                    help='local port that accepts typed commands, e.g. echo reset | nc localhost 7357')
    args = ap.parse_args()

    bridge = None

    log_path = Path.home() / '.config/emerald-voice/agent.log'
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_file = open(log_path, 'a', buffering=1)
    log_file.write(f'--- agent started {time.strftime("%Y-%m-%d %H:%M:%S")}\n')

    def log(msg):
        print(msg, flush=True)
        log_file.write(f'{time.strftime("%H:%M:%S")} {msg}\n')
        if bridge and not msg.startswith('    ['):
            bridge.show(msg.strip())
    data = GameData(args.source)
    symbols = load_symbols(args.symbols)
    executor, executor2 = Executor(), Executor()
    p2_state = {'keys': None}

    def on_frame(state):
        keys = executor.on_frame(state)
        if args.two_players and state.pvp:
            from .state import view
            from .voice import p2_keys
            mask = executor2.on_frame(view(state, 1)) | p2_keys()
            if mask != p2_state['keys']:
                bridge.write_u16('gArenaP2Keys', mask)
                p2_state['keys'] = mask
        return keys

    bridge = LuaBridge(data, symbols, on_frame, port=args.port, log=log)
    jev = None
    if not args.no_jev:
        key = find_key()
        if key:
            jev = Jev(key)
        else:
            log('No TypeSafe API key found (TYPESAFE_API_KEY or Keychain item "emerald-voice-typesafe"); using the built-in rules.')
    brain = Brain(bridge, executor, data, jev=jev, log=log, verbose=args.verbose, reset_state=args.reset_state)
    brain2 = Brain(bridge, executor2, data, jev=jev, log=log, verbose=args.verbose, side=1)

    def command(text, player=0):
        (brain2 if player else brain).command(text)
    bridge.start()
    threading.Thread(target=control_server, args=(args.control_port, command), daemon=True).start()
    log(f'Waiting for the game on port {args.port}: in mGBA, Tools > Scripting > File > Load script > voice/bridge.lua')

    def vocabulary():
        s = bridge.state
        if not s:
            return ''
        if s.draft_screen:
            return 'Pokémon draft: pick Charizard and Gengar, remove Pikachu, show Dragonite, done, fight.'
        names = [data.moves.get(m, '') for m in s.me.moves if m] + [m.name for m in s.party]
        return 'Moves and Pokémon: ' + ', '.join(n for n in names if n) + '.'

    if args.text:
        log('Type a command and press Enter (Ctrl-C to quit).')
        for line in sys.stdin:
            line = line.strip()
            if line.lower().startswith('p2 '):
                command(line[3:], 1)
            elif line:
                command(line)
        return
    from .voice import PushToTalk, SpeakerId, Transcriber
    log(f'Loading speech recognition ({args.stt_model})…')
    speakers = None
    if args.two_players:
        log('Loading speaker recognition…')
        speakers = SpeakerId(active=lambda: bool(bridge.state and bridge.state.pvp), want_enroll=args.enroll)
        if args.enroll:
            speakers.profiles = None
        elif speakers.profiles is None and args.hands_free:
            log('No saved voices: in 1P vs 1P the first voice heard is Player 1 and the next different one '
                'Player 2 (run with --enroll to record both voices first).')
        elif speakers.profiles is None:
            log('No saved voices: in 1P vs 1P, Right Option is Player 1 and Left Option is Player 2 '
                '(run with --enroll to let voices decide).')
    ptt = PushToTalk(Transcriber(args.stt_model), command, vocabulary, log=log, speakers=speakers,
                     hands_free=args.hands_free)
    if speakers and args.hands_free and speakers.profiles is None:
        # 1P vs 1P: the versus screen asks each player for one line, so each
        # voice is known before the fight ("first voice heard" otherwise).
        def on_enrolled(player):
            bridge.write_byte('gArenaVoiceEnroll', 2 if player == 0 else 3)

        def versus_watch():
            on_versus = False
            while True:
                time.sleep(0.1)
                s = bridge.state
                now = bool(s and s.draft_screen == 3 and s.raw.get('dmode', b'\0')[0] == 2)
                if now and not on_versus:
                    speakers.learned, speakers.expect = [], [0, 1]
                    bridge.write_byte('gArenaVoiceEnroll', 1)
                    log('Versus screen: Player 1, then Player 2, say anything to set your voice.')
                elif not now and on_versus:
                    speakers.expect = []
                on_versus = now
        ptt.on_enrolled = on_enrolled
        threading.Thread(target=versus_watch, daemon=True).start()
    log(('Ready and listening: just say a command (Right Option push-to-talk works too).' if args.hands_free
         else 'Ready. Hold Right Option, say a command, release.') + (
        ' Player 2: I J K L move, U attack, O dodge, Y/P change move.' if args.two_players else ''))
    ptt.run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
