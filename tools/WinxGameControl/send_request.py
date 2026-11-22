"""One ordered request to the local, explicitly started WinxGameControl session."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import time


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", required=True, type=Path)
    parser.add_argument("--action", required=True, choices=["Bind", "Snapshot", "Pad", "Key", "Mouse", "CenterSystemCursor", "Neutral", "Stop"])
    parser.add_argument("--target", type=Path)
    parser.add_argument("--buttons", nargs="*", default=[])
    for flag in ("lx", "ly", "rx", "ry"):
        parser.add_argument("--" + flag, type=int, default=0)
    for flag in ("lt", "rt"):
        parser.add_argument("--" + flag, type=int, default=0)
    parser.add_argument("--scan", type=lambda x: int(x, 0), default=0)
    parser.add_argument("--extended", action="store_true")
    parser.add_argument("--dx", type=int, default=0)
    parser.add_argument("--dy", type=int, default=0)
    parser.add_argument("--button", choices=["left", "right"])
    parser.add_argument("--hold-ms", type=int, default=150)
    parser.add_argument("--neutral-ms", type=int, default=200)
    parser.add_argument("--timeout", type=float, default=15)
    args = parser.parse_args()
    session = args.session.resolve(strict=True)
    if not args.session.is_absolute() or not session.is_dir():
        parser.error("Use the absolute directory of the explicitly started session.")
    if not 1 <= args.timeout <= 50 or not 50 <= args.hold_ms <= 2000 or not 100 <= args.neutral_ms <= 1000:
        parser.error("Bound timeout1..50s, hold50..2000ms, neutral100..1000ms.")
    if any(not -32768 <= x <= 32767 for x in (args.lx, args.ly, args.rx, args.ry)) or any(not 0 <= x <= 255 for x in (args.lt, args.rt)):
        parser.error("Stick values are signed16-bit; triggers are unsigned8-bit.")
    if (session / "final.json").exists():
        parser.error("This session has ended; inspect final.json. Do not replay input.")
    ready = read_json(session / "ready.json")
    if ready.get("version") != 1 or ready.get("kind") != "WinxGameControl-ready" or args.action not in ready.get("actions", []):
        parser.error("Unexpected session protocol or unsupported action.")
    if datetime.now(timezone.utc) >= datetime.fromisoformat(ready["deadlineUtc"].replace("Z", "+00:00")):
        parser.error("Session deadline has passed.")
    if args.action == "Bind" and not args.target:
        parser.error("Bind requires the selected target identity file.")
    if args.action == "Pad" and any(b not in ready.get("buttons", []) for b in args.buttons):
        parser.error("Use only button names declared in ready.json.")
    lock = session / ".request-client.lock"
    try:
        lock_stream = lock.open("x", encoding="utf-8")
    except FileExistsError:
        parser.error("Another client or an interrupted request owns the session lock; inspect it before proceeding.")
    try:
        lock_stream.write(str(os.getpid())); lock_stream.flush()
        sequences = sorted(int(p.stem) for p in session.iterdir() if re.fullmatch(r"\d{4}\.json", p.name))
        if sequences != list(range(1, len(sequences) + 1)):
            parser.error("Request history contains a sequence gap.")
        for number in sequences:
            result_path = session / f"{number:04d}-result.json"
            if not result_path.exists():
                parser.error(f"Request{number:04d} has no confirmed result. Inspect it; do not resend.")
            result = read_json(result_path)
            if result.get("version") != 1 or result.get("sequence") != number:
                parser.error("Result history is inconsistent.")
        sequence = len(sequences) + 1
        if sequence >= 10000:
            parser.error("Session request bound reached.")
        body = {"Version": 1, "Sequence": sequence, "Action": args.action,
                "HoldMs": args.hold_ms, "NeutralMs": args.neutral_ms}
        if args.target:
            target = read_json(args.target)
            names = ("Pid", "CreatedFileTimeUtc", "Hwnd", "ImagePath")
            if set(target) != set(names) or any(not isinstance(target[n], int) for n in names[:3]):
                parser.error("Target requires exactly the four Get-Target fields, with integer FILETIME/HWND.")
            body["Target"] = target
        if args.action == "Pad":
            body["Pad"] = {"LeftThumbX": args.lx, "LeftThumbY": args.ly,
                           "RightThumbX": args.rx, "RightThumbY": args.ry,
                           "LeftTrigger": args.lt, "RightTrigger": args.rt, "Buttons": args.buttons}
        elif args.action == "Key":
            if not 1 <= args.scan <= 127:
                parser.error("Use a base DI scan1..127 and --extended when appropriate.")
            body.update(Scan=args.scan, Extended=args.extended)
        elif args.action == "Mouse":
            body.update(Dx=args.dx, Dy=args.dy, Button=args.button)
        pending = session / f".{sequence:04d}.client-writing"
        request_path = session / f"{sequence:04d}.json"
        with pending.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(body, stream, ensure_ascii=False, indent=2)
            stream.flush(); os.fsync(stream.fileno())
        # On the target Windows filesystem rename does not overwrite a raced
        # existing request; the server never sees a partial JSON write.
        if os.name != "nt":
            parser.error("The live control client is Windows-only.")
        os.rename(pending, request_path)
        result_path = session / f"{sequence:04d}-result.json"
        began = time.monotonic()
        while not result_path.exists():
            if (session / "final.json").exists() or time.monotonic() - began >= args.timeout:
                print(json.dumps({"sequence": sequence, "outcome": "unconfirmed", "request": str(request_path),
                                  "finalPresent": (session / "final.json").exists(), "retryInput": False}))
                return 4
            time.sleep(0.05)
        result = read_json(result_path)
        if result.get("version") != 1 or result.get("sequence") != sequence:
            parser.error("The received result does not match this request.")
        print(json.dumps({"sequence": sequence, "successful": result.get("successful"),
                          "result": str(result_path), "error": result.get("error")}, ensure_ascii=False))
        return 0 if result.get("successful") else 2
    finally:
        lock_stream.close()
        lock.unlink()


if __name__ == "__main__":
    raise SystemExit(main())
