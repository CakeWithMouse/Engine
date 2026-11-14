import argparse
import json
import pathlib
import re
import sys

from AnalyzeAudioCode import read_mono, tone_amplitude


def fields(path):
    text = pathlib.Path(path).read_text(encoding="utf-8")
    values = {}
    for line in text.splitlines():
        for key, value in re.findall(r"([A-Za-z][A-Za-z0-9_]*)=([0-9.]+)", line):
            values[key] = float(value) if "." in value else int(value)
    return values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", required=True)
    ap.add_argument("--capture-summary", required=True)
    ap.add_argument("--game-log", required=True)
    ap.add_argument("--result", required=True)
    ap.add_argument("--frequency", type=float, default=733.0)
    ap.add_argument("--chip-ms", type=float, default=125.0)
    ap.add_argument("--grace-ms", type=float, default=500.0)
    args = ap.parse_args()
    summary = json.loads(pathlib.Path(args.capture_summary).read_text(encoding="utf-8-sig"))
    game = fields(args.game_log)
    required = ("playCallQpc100ns", "stopCallQpc100ns")
    if any(key not in game for key in required) or "captureStartQpc100ns" not in summary:
        raise ValueError("QPC timestamps are missing from capture or game log")
    start = (game["playCallQpc100ns"] - summary["captureStartQpc100ns"]) / 10_000_000.0
    stop = (game["stopCallQpc100ns"] - summary["captureStartQpc100ns"]) / 10_000_000.0
    rate, samples = read_mono(args.capture)
    chip = round(rate * args.chip_ms / 1000.0)
    rows = []
    for first in range(0, len(samples) - chip + 1, chip):
        t = first / rate
        rows.append({"startSeconds": t, "toneAmplitude": tone_amplitude(samples[first:first + chip], rate, args.frequency)})
    pre = [r["toneAmplitude"] for r in rows if start + 0.5 <= r["startSeconds"] < stop - 0.15]
    post = [r["toneAmplitude"] for r in rows if r["startSeconds"] >= stop + args.grace_ms / 1000.0]
    median = lambda values: sorted(values)[len(values) // 2] if values else None
    result = {
        "captureStartQpc100ns": summary["captureStartQpc100ns"],
        "playCallQpc100ns": game["playCallQpc100ns"],
        "stopCallQpc100ns": game["stopCallQpc100ns"],
        "playCallRelativeSeconds": start,
        "stopCallRelativeSeconds": stop,
        "graceSeconds": args.grace_ms / 1000.0,
        "preStopMedianToneAmplitude": median(pre),
        "preStopWindowCount": len(pre),
        "postGraceMedianToneAmplitude": median(post),
        "postGraceMaxToneAmplitude": max(post) if post else None,
        "postGraceWindowCount": len(post),
        "windows": rows,
        "measurement": "733 Hz Goertzel amplitude over fixed 125 ms windows; QPC ties game calls to loopback timeline.",
    }
    pathlib.Path(args.result).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: result[k] for k in (
        "playCallRelativeSeconds", "stopCallRelativeSeconds", "preStopMedianToneAmplitude",
        "postGraceMedianToneAmplitude", "postGraceMaxToneAmplitude", "preStopWindowCount", "postGraceWindowCount")}, indent=2))


if __name__ == "__main__":
    main()
