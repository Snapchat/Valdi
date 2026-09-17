#!/usr/bin/env python3
"""Build/install/launch smoke check for the Ledger ClientSQL Android demo."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_APK = ROOT / "bazel-bin/apps/ledger_sql_demo/ledger_sql_demo_android.apk"
DEFAULT_SCREENSHOT = Path("/tmp/ledger-sql-android-smoke.png")
PACKAGE = "com.snap.valdi.ledger_sql_demo"
ACTIVITY = f"{PACKAGE}/.StartActivity"
ANDROID_BUILD_COMMAND = [
    "valdi",
    "build",
    "android",
    "--application",
    "//apps/ledger_sql_demo:ledger_sql_demo_android",
]
BOUNDS_RE = re.compile(r"\[(\d+),(\d+)\]\[(\d+),(\d+)\]")


def run(args: Sequence[str], *, cwd: Path = ROOT, capture: bool = False) -> subprocess.CompletedProcess:
    print("$ " + " ".join(str(arg) for arg in args), flush=True)
    return subprocess.run(
        [str(arg) for arg in args],
        cwd=cwd,
        capture_output=capture,
        check=True,
    )


def default_adb_path() -> str:
    if os.environ.get("ADB"):
        return os.environ["ADB"]
    if shutil.which("adb"):
        return "adb"

    candidates = []
    for variable in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        if os.environ.get(variable):
            candidates.append(Path(os.environ[variable]) / "platform-tools/adb")
    candidates.append(Path.home() / "Library/Android/sdk/platform-tools/adb")

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return "adb"


def adb(args: Sequence[str], *, device: str | None, capture: bool = False) -> subprocess.CompletedProcess:
    adb_path = default_adb_path()
    command = [adb_path]
    if device:
        command.extend(["-s", device])
    command.extend(args)
    return run(command, capture=capture)


def wait_for_boot(device: str | None, timeout_seconds: float) -> None:
    adb(["wait-for-device"], device=device)
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        result = adb(["shell", "getprop", "sys.boot_completed"], device=device, capture=True)
        if result.stdout.decode(errors="replace").strip() == "1":
            return
        time.sleep(1)
    raise TimeoutError("Android device did not finish booting before the smoke timeout")


def dump_ui(device: str | None) -> str:
    adb(["shell", "uiautomator", "dump", "/sdcard/ledger_sql_demo_window.xml"], device=device)
    result = adb(["exec-out", "cat", "/sdcard/ledger_sql_demo_window.xml"], device=device, capture=True)
    return result.stdout.decode(errors="replace")


def node_text(node: ET.Element) -> str:
    return node.attrib.get("text", "")


def parse_ui_xml(xml_text: str) -> ET.Element | None:
    try:
        return ET.fromstring(xml_text)
    except ET.ParseError:
        return None


def find_bounds(xml_text: str, text: str) -> tuple[int, int, int, int] | None:
    root = parse_ui_xml(xml_text)
    if root is None:
        return None
    for node in root.iter("node"):
        candidate = node_text(node)
        if candidate == text or text in candidate:
            bounds = node.attrib.get("bounds", "")
            match = BOUNDS_RE.fullmatch(bounds)
            if match:
                return (
                    int(match.group(1)),
                    int(match.group(2)),
                    int(match.group(3)),
                    int(match.group(4)),
                )
    return None


def contains_text(xml_text: str, text: str) -> bool:
    root = parse_ui_xml(xml_text)
    if root is None:
        return False
    return any(text in node_text(node) for node in root.iter("node"))


def tap_bounds(device: str | None, bounds: tuple[int, int, int, int]) -> None:
    left, top, right, bottom = bounds
    adb(["shell", "input", "tap", str((left + right) // 2), str((top + bottom) // 2)], device=device)


def tap_text(device: str | None, text: str) -> bool:
    bounds = find_bounds(dump_ui(device), text)
    if bounds is None:
        return False
    tap_bounds(device, bounds)
    return True


def wait_for_text(device: str | None, text: str, timeout_seconds: float) -> None:
    deadline = time.monotonic() + timeout_seconds
    last_xml = ""
    while time.monotonic() < deadline:
        last_xml = dump_ui(device)
        if contains_text(last_xml, text):
            return
        time.sleep(1)
    raise AssertionError(f"Did not find Android UI text {text!r}. Last dump:\n{last_xml[:2000]}")


def screenshot(device: str | None, path: Path) -> None:
    result = adb(["exec-out", "screencap", "-p"], device=device, capture=True)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(result.stdout)
    print(f"Wrote screenshot: {path}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="Build the Android APK before installing it.")
    parser.add_argument("--apk", type=Path, default=DEFAULT_APK, help="APK to install.")
    parser.add_argument("--device", help="adb device serial. Defaults to adb's selected device.")
    parser.add_argument("--timeout", type=float, default=60, help="Seconds to wait for boot and UI text.")
    parser.add_argument("--reset-seed", action="store_true", help="Tap Reset seed data before the stress batch.")
    parser.add_argument("--screenshot", type=Path, default=DEFAULT_SCREENSHOT, help="Screenshot output path.")
    args = parser.parse_args()

    if args.build:
        run(ANDROID_BUILD_COMMAND)

    if not args.apk.exists():
        raise FileNotFoundError(f"APK not found: {args.apk}. Pass --build or build it first.")

    wait_for_boot(args.device, args.timeout)
    adb(["install", "-r", str(args.apk)], device=args.device)
    adb(["shell", "am", "start", "-n", ACTIVITY], device=args.device)

    wait_for_text(args.device, "Ledger ClientSQL Demo", args.timeout)
    wait_for_text(args.device, "Ledger actions ready.", args.timeout)
    if args.reset_seed:
        if not tap_text(args.device, "Reset seed data"):
            raise AssertionError("Could not find Reset seed data button in Android UI dump")
        wait_for_text(args.device, "Reset and reseeded ledger tables.", args.timeout)

    if not tap_text(args.device, "Run stress batch"):
        raise AssertionError("Could not find Run stress batch button in Android UI dump")

    wait_for_text(args.device, "Committed four transfers in one transaction", args.timeout)
    screenshot(args.device, args.screenshot)
    print("Ledger ClientSQL Android smoke passed.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        if error.stdout:
            sys.stdout.buffer.write(error.stdout)
        if error.stderr:
            sys.stderr.buffer.write(error.stderr)
        raise
