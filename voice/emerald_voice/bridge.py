"""Connection to the game. LuaBridge serves the mGBA bridge script over TCP;
each frame it decodes the watched memory into a GameState, asks the executor
for the keys to hold, and sends them back when they change."""
import socket
import threading
import time

from .state import WATCH, parse


class LuaBridge:
    def __init__(self, data, symbols, on_frame, port=7355, log=print):
        self.data, self.symbols, self.on_frame, self.port, self.log = data, symbols, on_frame, port, log
        missing = [s for _, s, _ in WATCH if s not in symbols]
        if missing:
            raise SystemExit('Symbols missing from the map file: ' + ', '.join(missing))
        self.watch = 'W ' + ','.join(f'{symbols[s]:08x}:{n}' for _, s, n in WATCH) + '\n'
        self.conn = None
        self.state = None
        self.connected = threading.Event()
        self._lock = threading.Lock()
        self._keys = None

    def start(self):
        threading.Thread(target=self._serve, daemon=True).start()

    def write_byte(self, symbol, value):
        with self._lock:
            if self.conn:
                self.conn.sendall(f'B {self.symbols[symbol]:08x} {value}\n'.encode())

    def show(self, text):
        """A line in mGBA's scripting console, next to the game."""
        with self._lock:
            if self.conn:
                self.conn.sendall(('L ' + text.replace('\n', ' ') + '\n').encode())

    def _serve(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('127.0.0.1', self.port))
        server.listen(1)
        while True:
            conn, _ = server.accept()
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with self._lock:
                self.conn, self._keys = conn, None
            conn.sendall(self.watch.encode())
            self.connected.set()
            self.log('Game connected.')
            try:
                self._read(conn)
            except OSError:
                pass
            with self._lock:
                self.conn = None
            self.connected.clear()
            self.log('Game disconnected; waiting for it to reconnect.')

    def _read(self, conn):
        buffer = b''
        sizes = [n for _, _, n in WATCH]
        names = [n for n, _, _ in WATCH]
        while True:
            data = conn.recv(65536)
            if not data:
                return
            buffer += data
            lines = buffer.split(b'\n')
            buffer = lines.pop()
            frames = [l for l in lines if l.startswith(b'F ')]
            if not frames:
                continue
            parts = frames[-1].split()   # only the newest frame matters
            if len(parts) != 2 + len(sizes):
                continue
            chunks = {n: bytes.fromhex(p.decode()) for n, p in zip(names, parts[2:])}
            self.state = parse(int(parts[1]), chunks, self.data, self.symbols)
            keys = self.on_frame(self.state)
            if keys != self._keys:
                with self._lock:
                    conn.sendall(f'K {keys}\n'.encode())
                self._keys = keys


class HeadlessBridge:
    """The same interface over an in-process mGBA core, for tests."""

    def __init__(self, core, data, symbols, on_frame):
        self.core, self.data, self.symbols, self.on_frame = core, data, symbols, on_frame
        self.state = None
        self.connected = threading.Event()
        self.connected.set()
        self.keys = 0
        self.frame = 0

    def write_byte(self, symbol, value):
        self.core.memory.u8[self.symbols[symbol]] = value

    def show(self, text):
        pass

    def step(self, extra_keys=0):
        mem = self.core.memory
        chunks = {n: bytes(mem.u8[self.symbols[s] + i] for i in range(size)) for n, s, size in WATCH}
        self.state = parse(self.frame, chunks, self.data, self.symbols)
        self.keys = self.on_frame(self.state)
        keys = self.keys | extra_keys
        self.core.set_keys(*[b for b in range(10) if keys & (1 << b)])
        self.core.run_frame()
        self.frame += 1
