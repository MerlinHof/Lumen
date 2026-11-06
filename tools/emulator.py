#!/usr/bin/env python3
"""Run the unmodified LUMEN host UI with virtual Kindle hardware over localhost."""

import argparse
import fcntl
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import struct
import subprocess
import sys
import threading
from urllib.parse import parse_qs, urlsplit
import webbrowser
import zipfile
import zlib

import build

ROOT = build.ROOT
WIDTH, HEIGHT = 600, 800
WAVEFORMS = ("DU", "GL16", "GC16", "GC16 full")
BOOK = """# The Quiet Hours

This is a sample book for the LUMEN Kindle emulator.

There is a kind of morning that asks very little of us. A window, a cup of
something warm, a page waiting to be turned.

Tap the sides of the screen or swipe to turn a page. Tap the top of the
reader to show its controls. Hold on a word to open text selection.
"""


def build_emulator(output):
    try:
        mupdf, _, libraries = build.dependencies("host")
    except SystemExit:
        subprocess.run([sys.executable, str(ROOT / "tools/setup.py"), "host"], check=True)
        mupdf, _, libraries = build.dependencies("host")
    build.build("host")
    adapter = output / "virtual-device.o"
    binary = output / "lumen-emulator"
    subprocess.run(["clang", "-O2", "-Wall", "-Wextra", "-I" + str(ROOT / "lumen"),
                    "-I" + str(mupdf / "include"), "-c",
                    str(ROOT / "tools/emulator-device.c"), "-o", str(adapter)], check=True)
    objects = [build.object_path("host", source) for source in build.sources()
               if source != "system/device.c"]
    subprocess.run(["clang++", *map(str, objects + [adapter] + libraries),
                    "-lm", "-lpthread", "-o", str(binary)], check=True)
    return binary


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


def png(pixels):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload)))
    scanlines = b"".join(b"\0" + pixels[y:y + WIDTH] for y in range(0, len(pixels), WIDTH))
    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 0, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(
        b"IDAT", zlib.compress(scanlines, 3)) + chunk(b"IEND", b"")


class Display:
    def __init__(self, durations):
        self.durations = durations
        self.condition = threading.Condition()
        self.stop = threading.Event()
        self.revision = 0
        self.pending = None
        self.image = None
        self.snapshot = None
        self.waveform = ""

    def read(self, pipe):
        with pipe:
            while not self.stop.is_set():
                mode = pipe.read(1)
                pixels = pipe.read(WIDTH * HEIGHT) if mode else b""
                if len(pixels) != WIDTH * HEIGHT:
                    break
                with self.condition:
                    self.pending = (mode[0], pixels)
                    self.condition.notify_all()
        self.close()

    def publish(self, image, waveform, settled=False):
        with self.condition:
            self.image, self.waveform = image, waveform
            if settled:
                self.snapshot = image
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
            label = f"{WAVEFORMS[mode]} / {self.durations[mode]} ms"
            if mode == 3 and duration:
                self.publish(dark, label)
                if self.stop.wait(duration * .23):
                    return
                self.publish(white, label)
                duration *= .77
            if self.stop.wait(duration):
                return
            self.publish(png(pixels), label, settled=True)

    def close(self):
        self.stop.set()
        with self.condition:
            self.condition.notify_all()


def handler_for(token, display, process):
    prefix = "/" + token + "/"
    html = (ROOT / "tools/emulator.html").read_bytes()
    input_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
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
                    self.reply(200, snapshot, "image/png")
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
            try:
                with input_lock:
                    process.stdin.write(struct.pack("=iii", kind, x, y))
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
    parser.add_argument("--du-ms", type=timing, default=260)
    parser.add_argument("--gl16-ms", type=timing, default=450)
    parser.add_argument("--gc16-ms", type=timing, default=450)
    parser.add_argument("--instant", action="store_true", help="skip simulated display delay")
    args = parser.parse_args()
    profile = ROOT / "build/emulator"
    profile.mkdir(parents=True, exist_ok=True)
    with (profile / "session.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.exit(1, "Der LUMEN-Emulator läuft bereits. Beende den ersten Starter.\n")
        books, data = prepare_profile(profile)
        binary = build_emulator(profile)
        durations = (0, 0, 0, 0) if args.instant else (
            args.du_ms, args.gl16_ms, args.gc16_ms, args.gc16_ms)
        display = Display(durations)
        read_fd, write_fd = os.pipe()
        with (profile / "emulator.log").open("wb") as log:
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
            reader = threading.Thread(target=display.read, args=(os.fdopen(read_fd, "rb"),), daemon=True)
            painter = threading.Thread(target=display.update, daemon=True)
            reader.start()
            painter.start()
            token = secrets.token_urlsafe(18)
            try:
                with ThreadingHTTPServer(("127.0.0.1", args.port),
                                         handler_for(token, display, process)) as server:
                    server.timeout = .25
                    url = f"http://127.0.0.1:{server.server_port}/{token}/"
                    print(url, flush=True)
                    print(f"Bücher: {books}\nBeenden mit Ctrl+C.", flush=True)
                    if not args.no_open:
                        webbrowser.open(url)
                    while process.poll() is None:
                        server.handle_request()
            except KeyboardInterrupt:
                pass
            finally:
                # EOF runs LUMEN's normal exit path and saves its profile.
                if process.stdin:
                    process.stdin.close()
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
            if process.returncode:
                print(f"LUMEN wurde beendet ({process.returncode}). Log: {profile / 'emulator.log'}",
                      file=sys.stderr)
                return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
