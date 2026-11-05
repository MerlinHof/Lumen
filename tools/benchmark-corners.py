#!/usr/bin/env python3
"""Build and compare square, circular and superellipse UI corners on the host."""
import statistics
import subprocess
import build

root = build.ROOT
out = root / "build/corner-benchmark"
(out / "data").mkdir(parents=True, exist_ok=True)
build.build("host")
mupdf, _, libs = build.dependencies("host")
source = root / "tools/tests/benchmark_corners.c"
obj, executable = out / "benchmark.o", out / "benchmark"
subprocess.run(["clang", "-DLUMEN_HOST", "-O2", "-I" + str(mupdf / "include"),
                "-c", str(source), "-o", str(obj)], check=True)
objects = [build.object_path("host", s) for s in build.sources()
           if s not in ("main.c", "system/ui/ui.c")]
subprocess.run(["clang++", str(obj), *map(str, objects + libs), "-lm", "-lpthread",
                "-o", str(executable)], check=True)
result = subprocess.run([str(executable)], cwd=root, check=True, capture_output=True, text=True)
(out / "samples.txt").write_text(result.stdout)
times = {i: [] for i in range(3)}
for line in result.stdout.splitlines():
    profile, frames, primitives = map(int, line.split())
    times[profile].append((frames / 280 / 1e6, primitives / 10000 / 1e3))
baseline = statistics.median(row[0] for row in times[0])
print("Profile          CPU ms/frame    Change     us/outlined control")
for i, name in enumerate(("Square", "Circle", "Superellipse")):
    frame = statistics.median(row[0] for row in times[i])
    primitive = statistics.median(row[1] for row in times[i])
    print(f"{name:16} {frame:12.3f}  {(frame / baseline - 1):+8.1%} {primitive:19.2f}")
