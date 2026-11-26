"""Boot the shipped ARM application and its packages on QEMU's Linux virt board."""

import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import time
from tempfile import TemporaryDirectory
from zipfile import ZipFile

import build
from package import runtime_mapping

ROOT = build.ROOT


def write_entry(archive, name, mode, ino, size=0):
    name = name.encode("utf-8") + b"\0"
    fields = (ino, mode, 0, 0, 1, 0, size, 0, 0, 0, 0, len(name), 0)
    archive.write(b"070701" + "".join(f"{n:08x}" for n in fields).encode("ascii"))
    archive.write(name)
    archive.write(b"\0" * (-(110 + len(name)) % 4))


def write_guest_image(profile, books, rebuild=False):
    mapping = runtime_mapping()
    files = {dest: ROOT / src for dest, src in mapping.items() if
             dest.startswith(("lumen/app-packages/", "lumen/fonts/", "lumen/certs/",
                              "lumen/dictionary/", "lumen/photos/", "lumen/home/")) or
             dest in {"lumen/bin/lumen", "lumen/system/selftest.epub"}}
    binary=files["lumen/bin/lumen"]
    version=(ROOT / "VERSION").read_text().strip()
    marker=f"LUMEN {version} / MuPDF ".encode("ascii")
    if rebuild or not all(path.is_file() for path in files.values()) or marker not in binary.read_bytes():
        build.build("kindle")

    files.update({f"lumen/books/{book.name}": book for book in books.iterdir()
                  if book.is_file() and not book.is_symlink() and not book.name.startswith(".")})
    # Seed the guest with the actual release executables. This avoids installing
    # and rescanning every app in the ARM guest on each offline emulator boot.
    app_archives=[name for name in files if name.startswith("lumen/app-packages/")]
    with TemporaryDirectory(dir=profile) as staging:
        for name in app_archives:
            package=files.pop(name)
            app_id=package.stem
            with ZipFile(package) as source:
                content=source.read("manifest.json")
                manifest=json.loads(content)
                if manifest["id"]!=app_id:
                    raise ValueError(f"App identity mismatch in {package}")
                members={"manifest.json": content}
                for entry in manifest["files"]:
                    path=PurePosixPath(entry["path"])
                    if path.is_absolute() or ".." in path.parts or not path.parts:
                        raise ValueError(f"Invalid app member in {package}")
                    member=source.read("payload/"+str(path))
                    if len(member)!=entry["size"] or hashlib.sha256(member).hexdigest()!=entry["sha256"]:
                        raise ValueError(f"Verification failed for {package}/{path}")
                    members[str(path)]=member
                for leaf,member in members.items():
                    destination=Path(staging)/app_id/leaf
                    destination.parent.mkdir(parents=True,exist_ok=True)
                    destination.write_bytes(member)
                    files[f"lumen/apps/{app_id}/{leaf}"]=destination
        return write_initramfs(profile,files)


def write_initramfs(profile,files):
    directories = {".", "dev", "proc", "sys", "tmp", "lumen/books", "lumen/data"}
    for name in files:
        directories.update(str(parent) for parent in PurePosixPath(name).parents)
    image = profile / "guest.cpio.gz"
    with gzip.open(image, "wb", compresslevel=1) as archive:
        ino = 1
        for name in sorted(directories, key=lambda value: (value.count("/"), value)):
            write_entry(archive, name, stat.S_IFDIR | (0o1777 if name == "tmp" else 0o755), ino)
            ino += 1
        for name, path in sorted(files.items()):
            mode = 0o755 if name == "lumen/bin/lumen" or (name.startswith("lumen/apps/") and name.endswith("/app")) else 0o644
            with path.open("rb") as data:
                size = os.fstat(data.fileno()).st_size
                write_entry(archive, name, stat.S_IFREG | mode, ino, size)
                shutil.copyfileobj(data, archive)
                archive.write(b"\0" * (-size % 4))
            ino += 1
        write_entry(archive, "TRAILER!!!", 0, ino)
    return image


def launch(qemu, kernel, profile, books, log, rebuild=False, bluetooth=False):
    image = write_guest_image(profile, books, rebuild)
    return subprocess.Popen([
        qemu, "-machine", "virt", "-cpu", "cortex-a15", "-accel", "tcg",
        "-m", "512M", "-display", "none", "-monitor", "none", "-serial", "stdio",
        "-net", "none", "-no-reboot", "-kernel", kernel, "-initrd", image,
        "-append", ("console=ttyAMA0 loglevel=4 panic=1 rdinit=/lumen/bin/lumen "
                    f"LUMEN_EMULATOR_INIT=1 LUMEN_EMULATOR_EPOCH={int(time.time())}"
                    + (" LUMEN_EMULATOR_BLUETOOTH=1" if bluetooth else "")),
    ], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log,
        start_new_session=True)
