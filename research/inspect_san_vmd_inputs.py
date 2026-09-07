"""Aggregate the actual Bloom SAN curves before choosing converter scope.

Run: python research/inspect_san_vmd_inputs.py [directory_in_workspace] [output.json]
Uses the existing bounded PC SAN inspector. This does not retarget animation.
"""

import collections
import json
from pathlib import Path
import sys

from inspect_pc_san_keys import ROOT, inspect


def scan(directory):
    shapes = collections.Counter()
    names = collections.Counter()
    scaled = collections.Counter()
    intervals = collections.Counter()
    rows, errors, unknown, nonlinear = [], [], [], []
    max_keys = 0
    paths = sorted(directory.glob("*.san"))
    if not paths:
        raise ValueError(f"No SAN files in {directory}")
    for path in paths:
        try:
            header, tracks = inspect(path.resolve())
        except Exception as error:
            errors.append({"file": path.name, "error": str(error)})
            continue
        nonlinear_names = []
        for track in tracks:
            names[track["name"]] += 1
            if any(channel["representation"] != 1
                   for data in track["roles"].values() for channel in data["channels"]):
                nonlinear_names.append(track["name"])
            for role, data in track["roles"].items():
                for channel in data["channels"]:
                    representation = channel["representation"]
                    shapes[f"role{role}/repr{representation}"] += 1
                    max_keys = max(max_keys, channel["count"])
                    times = channel["times"]
                    intervals.update(round(b - a, 6) for a, b in zip(times, times[1:]))
                    # Threshold excludes ordinary float32 noise near identity.
                    if role == 2 and representation == 1:
                        if any(abs(v - 1) > 0.001 for v in channel["values"]):
                            scaled[track["name"]] += 1
        if header["unknown_fields"]:
            unknown.append({"file": path.name, "fields": header["unknown_fields"]})
        if nonlinear_names:
            nonlinear.append({"file": path.name, "names": nonlinear_names})
        rows.append({"file": path.name, "sha256": header["sha256"],
                     "duration": header["duration"], "tracks": len(tracks),
                     "names": [track["name"] for track in tracks]})
    durations = [row["duration"] for row in rows]
    return {
        "directory": str(directory), "files": len(paths), "parsed": len(rows),
        "errors": errors, "unknown_fields": unknown, "nonlinear_clips": nonlinear,
        "curve_shapes": shapes,
        "nonidentity_scale_tracks": scaled, "common_key_intervals": intervals.most_common(12),
        "max_keys_per_curve": max_keys,
        "duration_range": [min(durations), max(durations)] if durations else None,
        "track_counts": collections.Counter(row["tracks"] for row in rows),
        "names": names, "clips": rows,
    }


if __name__ == "__main__":
    directory = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (
        ROOT / "local-data/pc-pristine/Media/Characters/Bloom")
    report = scan(directory)
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(report, indent=2), encoding="utf-8")
    summary = {k: v for k, v in report.items()
               if k not in ("clips", "unknown_fields", "nonlinear_clips", "names")}
    summary["uninterpreted_field_ids"] = sorted({
        field["field"] for row in report["unknown_fields"] for field in row["fields"]})
    summary["clips_with_uninterpreted_fields"] = len(report["unknown_fields"])
    summary["nonlinear_clips"] = [row["file"] for row in report["nonlinear_clips"]]
    print(json.dumps(summary, ensure_ascii=True))
