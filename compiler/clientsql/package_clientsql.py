#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Sequence


SOURCE_ROOT = Path(__file__).resolve().parent / "src"
FIXED_ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
ENTRYPOINT = "from clientsql.cli import entrypoint\n\nentrypoint()\n"


def write_zip_entry(archive: zipfile.ZipFile, name: str, content: bytes) -> None:
    info = zipfile.ZipInfo(name, date_time=FIXED_ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, content, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def package_clientsql(output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=f".{output.name}.", delete=False) as temporary:
        temporary_path = Path(temporary.name)
        temporary.write(b"#!/usr/bin/env python3\n")

    try:
        with zipfile.ZipFile(temporary_path, mode="a") as archive:
            write_zip_entry(archive, "__main__.py", ENTRYPOINT.encode("utf-8"))
            for source_path in sorted(SOURCE_ROOT.rglob("*.py")):
                archive_path = source_path.relative_to(SOURCE_ROOT).as_posix()
                write_zip_entry(archive, archive_path, source_path.read_bytes())
        os.chmod(temporary_path, 0o755)
        temporary_path.replace(output)
    finally:
        temporary_path.unlink(missing_ok=True)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description="Package the modular ClientSQL generator as one executable zipapp")
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Path for the generated executable zipapp",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify that the supplied executable matches the canonical source",
    )
    args = parser.parse_args(argv)

    if not args.check:
        package_clientsql(args.output)
        return 0

    with tempfile.TemporaryDirectory(prefix="clientsql-package-check-") as temporary_directory:
        candidate = Path(temporary_directory) / "clientsql"
        package_clientsql(candidate)
        if not args.output.is_file() or candidate.read_bytes() != args.output.read_bytes():
            print(
                f"{args.output} is stale; rerun this command without --check",
                file=sys.stderr,
            )
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
