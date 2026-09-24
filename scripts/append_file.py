#!/usr/bin/env python3
"""Appends one binary file to another, e.g. caps.mpq to the SNP dll."""

import sys
from pathlib import Path


def main():
    if len(sys.argv) != 4:
        sys.exit(f"usage: {Path(sys.argv[0]).name} <source> <appendage> <output>")
    source, appendage, output = (Path(argument) for argument in sys.argv[1:])
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "wb") as out:
        out.write(source.read_bytes())
        out.write(appendage.read_bytes())


if __name__ == "__main__":
    main()