#!/usr/bin/env python3
"""Run LUMEN with a browser-based Kindle display and touch panel."""

import argparse
import fcntl
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import shutil
import struct
import subprocess
import sys
import threading
from urllib.parse import parse_qs, urlsplit
import webbrowser
import zipfile
import zlib

import build
import emulator_vm

ROOT = build.ROOT
WIDTH, HEIGHT = 600, 800
FRAME_MAGIC = b"LUMENFB\x01"
WAVEFORMS = ("DU", "GL16", "GC16", "GC16 full")
BOOK = """# The Quiet Hours

This is a sample book for the LUMEN Kindle emulator.

There is a kind of morning that asks very little of us. A window, a cup of
something warm, a page waiting to be turned.

Tap the sides of the screen or swipe to turn a page. Tap the top of the
reader to show its controls. Hold on a word to open text selection.
"""


def run_native(books, data, log):
    try:
        build.dependencies("host")
    except SystemExit:
        subprocess.run([sys.executable, str(ROOT / "tools/setup.py"), "host"], check=True)
    binary = build.build("host")
    read_fd, write_fd = os.pipe()
    try:
        process = subprocess.Popen(
            [str(binary), "--library", str(books), "--data", str(data)],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=log, stderr=log,
            pass_fds=(write_fd,), start_new_session=True,
            env=dict(os.environ, LUMEN_EMULATOR_DISPLAY_FD=str(write_fd)))
    except BaseException:
        os.close(read_fd)
        raise
    finally:
        os.close(write_fd)
    return process, os.fdopen(read_fd, "rb")


def prepare_profile(profile):
    books, data = profile / "books", profile / "data"
    books.mkdir(exist_ok=True)
    data.mkdir(exist_ok=True)
    for name in ("catalogs", "certs", "fonts", "home"):
        target = profile / name
        if not target.exists() and not target.is_symlink():
            target.symlink_to(ROOT / "lumen/assets" / name, target_is_directory=True)
    if not (profile / ".initialized").exists():
        # Keep demo seeding independent of the screenshot/Pillow tooling.
        with zipfile.ZipFile(books / "The Quiet Hours.epub", "w") as book:
            book.writestr("mimetype", "application/epub+zip")
            book.writestr("META-INF/container.xml", """<container version="1.0"
                xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>
                <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
                </rootfiles></container>""")
            book.writestr("book.opf", """<package xmlns="http://www.idpf.org/2007/opf"
                version="2.0" unique-identifier="id"><metadata
                xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">lumen-emulator</dc:identifier>
                <dc:title>The Quiet Hours</dc:title><dc:creator>LUMEN Studio</dc:creator>
                <dc:language>en</dc:language></metadata><manifest>
                <item id="page" href="book.xhtml" media-type="application/xhtml+xml"/>
                </manifest><spine><itemref idref="page"/></spine></package>""")
            paragraphs = "".join(f"<p>{text}</p>" for text in BOOK.split("\n\n")[1:]) * 8
            book.writestr("book.xhtml", '<html xmlns="http://www.w3.org/1999/xhtml">'
                          "<head><title>The Quiet Hours</title></head><body>"
                          "<h1>The Quiet Hours</h1>" + paragraphs + "</body></html>")
        (profile / ".initialized").touch()
    return books, data


class Panel:
    def __init__(self, width=WIDTH, height=HEIGHT, ppi=167):
        if not (240 <= width <= height <= 3000 and height >= 320):
            raise ValueError("Expected a portrait panel of at least 240 × 320 and at most 3000 × 3000")
        if not 80 <= ppi <= 600:
            raise ValueError("Expected a density of 80–600 PPI")
        self.width, self.height, self.ppi = width, height, ppi
        if width * HEIGHT > height * WIDTH:
            content_w, content_h = height * WIDTH // HEIGHT, height
        else:
            content_w, content_h = width, width * HEIGHT // WIDTH
        left, top = (width - content_w) // 2, (height - content_h) // 2
        self.viewport = (left, top, content_w, content_h)
        self.columns = [x * WIDTH // content_w for x in range(content_w)]
        self.rows = [y * HEIGHT // content_h for y in range(content_h)]

    def expand(self, pixels):
        if (self.width, self.height) == (WIDTH, HEIGHT):
            return pixels
        left, top, width, height = self.viewport
        padding = bytes([255]) * left
        trailing = bytes([255]) * (self.width - left - width)
        scaled = {}
        for y in set(self.rows):
            line = pixels[y*WIDTH:(y+1)*WIDTH]
            scaled[y] = padding + bytes(map(line.__getitem__, self.columns)) + trailing
        margin = bytes([255]) * self.width
        return margin * top + b"".join(scaled[y] for y in self.rows) + margin * (self.height - top - height)

    def encode(self, pixels):
        return png(self.expand(pixels), self.width, self.height)


def png(pixels, width=WIDTH, height=HEIGHT):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))
    scanlines = b"".join(b"\0" + pixels[y:y + width] for y in range(0, len(pixels), width))
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(
        b"IDAT", zlib.compress(scanlines, 3)) + chunk(b"IEND", b"")


class Display:
    def __init__(self, durations, panel=None):
        self.panel = panel or Panel()
        self.durations = durations
        self.condition = threading.Condition()
        self.stop = threading.Event()
        self.revision = 0
        self.pending = None
        self.image = None
        self.snapshot = None
        self.waveform = ""

    def read(self, pipe, boot_log):
        with pipe:
            # QEMU's serial port first carries the guest kernel's boot log.
            prefix = bytearray()
            while not self.stop.is_set():
                byte = pipe.read(1)
                if not byte:
                    boot_log.write(prefix)
                    self.close()
                    return
                prefix.extend(byte)
                if prefix.endswith(FRAME_MAGIC):
                    boot_log.write(prefix[:-len(FRAME_MAGIC)])
                    boot_log.flush()
                    break
                if byte == b"\n" or len(prefix) >= 4096:
                    boot_log.write(prefix)
                    boot_log.flush()
                    prefix.clear()
            while not self.stop.is_set():
                mode = pipe.read(1)
                pixels = pipe.read(WIDTH * HEIGHT) if mode else b""
                if len(pixels) != WIDTH * HEIGHT or mode[0] >= len(WAVEFORMS):
                    break
                with self.condition:
                    self.pending = (mode[0], pixels)
                    self.condition.notify_all()
        self.close()

    def publish(self, image, waveform, snapshot=None):
        with self.condition:
            self.image, self.waveform = image, waveform
            if snapshot is not None:
                self.snapshot = snapshot
            self.revision += 1
            self.condition.notify_all()

    def update(self):
        dark = png(bytes(WIDTH * HEIGHT))
        white = png(bytes([255]) * WIDTH * HEIGHT)
        while not self.stop.is_set():
            with self.condition:
                self.condition.wait_for(lambda: self.pending is not None or self.stop.is_set())
                if self.stop.is_set():
                    return
                mode, pixels = self.pending
                self.pending = None
            duration = self.durations[mode] / 1000
            label = WAVEFORMS[mode]
            if duration:
                label += f" / {self.durations[mode]} ms"
            if mode == 3 and duration:
                self.publish(dark, label)
                if self.stop.wait(duration * .23):
                    return
                self.publish(white, label)
                duration *= .77
            if self.stop.wait(duration):
                return
            self.publish(png(pixels), label, snapshot=pixels)

    def close(self):
        self.stop.set()
        with self.condition:
            self.condition.notify_all()


def handler_for(token, display, process, backend="native"):
    prefix = "/" + token + "/"
    panel = display.panel
    config = json.dumps(dict(width=panel.width, height=panel.height,
                             logical=[WIDTH, HEIGHT], ppi=panel.ppi, backend=backend))
    html = (ROOT / "tools/emulator.html").read_bytes().replace(b"__LUMEN_PANEL__", config.encode("ascii"))
    input_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        disable_nagle_algorithm = True

        def log_message(self, *_):
            pass

        def reply(self, code, body=b"", kind="text/plain", headers=None):
            try:
                self.send_response(code)
                self.send_header("Content-Type", kind)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Referrer-Policy", "no-referrer")
                for key, value in (headers or {}).items():
                    self.send_header(key, str(value))
                self.end_headers()
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path == prefix:
                self.reply(200, html, "text/html; charset=utf-8")
            elif url.path == prefix + "snapshot.png":
                with display.condition:
                    snapshot = display.snapshot
                if snapshot is None:
                    self.reply(503)
                else:
                    self.reply(200, panel.encode(snapshot), "image/png")
            elif url.path == prefix + "frame":
                try:
                    after = int(parse_qs(url.query).get("after", ["0"])[0])
                except ValueError:
                    self.reply(400)
                    return
                with display.condition:
                    display.condition.wait_for(
                        lambda: display.revision > after or display.stop.is_set(), timeout=15)
                    frame = display.image if display.revision > after else None
                    headers = {"X-Frame": display.revision, "X-Waveform": display.waveform}
                if frame is not None:
                    self.reply(200, frame, "image/png", headers)
                else:
                    self.reply(503 if display.stop.is_set() else 204)
            else:
                self.reply(404)

        def do_POST(self):
            if self.path != prefix + "input":
                self.reply(404)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= 80:
                    raise ValueError
                values = json.loads(self.rfile.read(length))
                if (not isinstance(values, list) or len(values) != 3 or
                        any(type(value) is not int for value in values)):
                    raise ValueError
                kind, x, y = values
                if not 1 <= kind <= 6 or not 0 <= x < WIDTH or not 0 <= y < HEIGHT:
                    raise ValueError
            except (ValueError, UnicodeDecodeError):
                self.reply(400)
                return
            if display.revision == 0:
                self.reply(503)
                return
            try:
                with input_lock:
                    process.stdin.write(struct.pack("<iii", kind, x, y))
                    process.stdin.flush()
            except (BrokenPipeError, OSError):
                self.reply(503)
                return
            self.reply(204)

    return Handler


def timing(value):
    duration = int(value)
    if not 0 <= duration <= 5000:
        raise argparse.ArgumentTypeError("Refresh duration must be 0–5000 ms")
    return duration


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0, help="localhost port, default automatic")
    parser.add_argument("--no-open", action="store_true", help="print URL without opening a browser")
    parser.add_argument("--du-ms", type=timing, default=0)
    parser.add_argument("--gl16-ms", type=timing, default=0)
    parser.add_argument("--gc16-ms", type=timing, default=0)
    parser.add_argument("--instant", action="store_true", help="skip simulated display delay")
    parser.add_argument("--profile", choices=("2014", "2026"), default="2014",
                        help="panel simulation preset")
    parser.add_argument("--panel", metavar="WIDTHxHEIGHT", help="override simulated portrait panel size")
    parser.add_argument("--ppi", type=int, help="override simulated pixel density")
    parser.add_argument("--backend", choices=("auto", "arm", "native"), default="auto",
                        help="use QEMU ARM Linux when configured, or run the standard host build")
    parser.add_argument("--kernel", type=Path, help="Linux ARMv7 kernel with QEMU virt support")
    parser.add_argument("--qemu", default=os.environ.get("LUMEN_QEMU", "qemu-system-arm"))
    parser.add_argument("--rebuild", action="store_true", help="rebuild the standard Kindle artifacts before booting")
    args = parser.parse_args()
    profile = ROOT / "build/emulator"
    kernel = (args.kernel or profile / "vmlinuz-armv7").expanduser().resolve()
    qemu = shutil.which(args.qemu)
    if not qemu and args.qemu == "qemu-system-arm":
        qemu = shutil.which(str(ROOT / ".local/deps/qemu/bin/qemu-system-arm"))
    arm = args.backend == "arm" or (args.backend == "auto" and
                                   (args.kernel or (qemu and kernel.is_file())))
    if arm and not (qemu and kernel.is_file()):
        parser.error("ARM mode requires qemu-system-arm and an ARMv7 kernel (--kernel FILE).")
    backend = "arm" if arm else "native"
    try:
        dimensions = tuple(map(int,args.panel.lower().split("x"))) if args.panel else (
            (WIDTH,HEIGHT) if args.profile=="2014" else (1080,1440))
        panel = Panel(*dimensions, ppi=args.ppi if args.ppi is not None else (167 if args.profile == "2014" else 300))
    except (ValueError,TypeError) as error:
        parser.error(f"Invalid panel: {error}")
    profile.mkdir(parents=True, exist_ok=True)
    with (profile / "session.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.exit(1, "Der LUMEN-Emulator läuft bereits. Beende den ersten Starter.\n")
        books, data = prepare_profile(profile)
        durations = (0, 0, 0, 0) if args.instant else (
            args.du_ms, args.gl16_ms, args.gc16_ms, args.gc16_ms)
        display = Display(durations, panel)
        with (profile / "emulator.log").open("wb") as log, (profile / "guest-boot.log").open("wb") as boot_log:
            if arm:
                process = emulator_vm.launch(qemu, kernel, profile, books, log, args.rebuild)
                frame_pipe = process.stdout
            else:
                process, frame_pipe = run_native(books, data, log)
            reader = threading.Thread(target=display.read, args=(frame_pipe, boot_log), daemon=True)
            painter = threading.Thread(target=display.update, daemon=True)
            reader.start()
            painter.start()
            token = secrets.token_urlsafe(18)
            stopped = False
            try:
                with ThreadingHTTPServer(("127.0.0.1", args.port),
                                         handler_for(token, display, process, backend)) as server:
                    server.timeout = .25
                    url = f"http://127.0.0.1:{server.server_port}/{token}/"
                    print(url, flush=True)
                    print(f"Modus: {backend}\nBücher: {books}\nBeenden mit Ctrl+C.", flush=True)
                    if not args.no_open:
                        webbrowser.open(url)
                    while process.poll() is None:
                        server.handle_request()
            except KeyboardInterrupt:
                stopped = True
            finally:
                # EOF runs LUMEN's normal exit path and saves its profile.
                if process.stdin:
                    try:
                        if arm and display.revision:
                            process.stdin.write(struct.pack("<iii", 7, 0, 0))
                        process.stdin.close()
                    except BrokenPipeError:
                        pass
                try:
                    process.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                display.close()
                reader.join(timeout=1)
            if process.returncode and not stopped:
                print(f"LUMEN wurde beendet ({process.returncode}). Log: {profile / 'emulator.log'}",
                      file=sys.stderr)
                return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
