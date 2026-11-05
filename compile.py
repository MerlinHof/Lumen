#!/usr/bin/env python3
"""Build LUMEN for Kindle (default) or the native host preview."""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "tools"))
from build import build

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", nargs="?", choices=("kindle", "host"), default="kindle")
    build(parser.parse_args().target)
