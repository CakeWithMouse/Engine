import argparse
import json
import math
import pathlib
import wave


def read_mono(path):
    with wave.open(str(path), "rb") as f:
        channels, width, rate, frames = f.getnchannels(), f.getsampwidth(), f.getframerate(), f.getnframes()
        data = f.readframes(frames)
    if width != 2:
        raise ValueError(f"{path}: expected PCM16, got sample width {width}")
    count = len(data) // 2
    raw = [int.from_bytes(data[i * 2:i * 2 + 2], "little", signed=True) / 32768.0 for i in range(count)]
    if channels != 1:
        raw = [sum(raw[i:i + channels]) / channels for i in range(0, len(raw), channels)]
    return rate, raw


def tone_amplitude(data, rate, frequency):
    n = len(data)
    omega = 2.0 * math.pi * frequency / rate
    coeff = 2.0 * math.cos(omega)
    s1 = s2 = 0.0
    for x in data:
        s0 = x + coeff * s1 - s2
        s2, s1 = s1, s0
    power = max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)
    return 2.0 * math.sqrt(power) / n if n else 0.0


def corr(a, b):
    ma, mb = sum(a) / len(a), sum(b) / len(b)
    da, db = [x - ma for x in a], [x - mb for x in b]
    denom = math.sqrt(sum(x * x for x in da) * sum(x * x for x in db))
    return sum(x * y for x, y in zip(da, db)) / denom if denom else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--capture", required=True)
    ap.add_argument("--result", required=True)
    ap.add_argument("--frequency", type=float, default=733.0)
    ap.add_argument("--chip-ms", type=float, default=125.0)
    ap.add_argument("--offset-step-ms", type=float, default=25.0)
    args = ap.parse_args()
    sr, source = read_mono(args.source)
    cr, capture = read_mono(args.capture)
    if sr != cr:
        raise ValueError(f"sample rates differ: source={sr} capture={cr}")
    chip = round(sr * args.chip_ms / 1000.0)
    source_count = len(source) // chip
    reference = [tone_amplitude(source[i * chip:(i + 1) * chip], sr, args.frequency) for i in range(source_count)]
    if source_count < 8:
        raise ValueError("source needs at least eight complete chips")
    step = max(1, round(sr * args.offset_step_ms / 1000.0))
    best = None
    windows = []
    for start in range(0, max(1, len(capture) - source_count * chip + 1), step):
        observed = [tone_amplitude(capture[start + i * chip:start + (i + 1) * chip], sr, args.frequency) for i in range(source_count)]
        score = corr(reference, observed)
        if best is None or score > best["correlation"]:
            gain = sum(observed) / max(1e-12, sum(reference))
            best = {"startSeconds": start / sr, "correlation": score, "linearGain": gain,
                    "referenceChipAmplitudes": reference, "observedChipAmplitudes": observed}
        windows.append({"startSeconds": start / sr, "correlation": score})
    repeats = []
    if best:
        first = round(best["startSeconds"] * sr)
        for cycle in range(1, 4):
            start = first + cycle * source_count * chip
            if start + source_count * chip > len(capture):
                break
            observed = [tone_amplitude(capture[start + i * chip:start + (i + 1) * chip], sr, args.frequency) for i in range(source_count)]
            repeats.append({"cycle": cycle, "startSeconds": start / sr,
                            "correlation": corr(reference, observed),
                            "linearGain": sum(observed) / max(1e-12, sum(reference))})
    # Report fixed-time tone amplitudes for pre/post stop evaluation, separately from the best-match search.
    fixed = []
    for start in range(0, len(capture) - chip + 1, chip):
        fixed.append({"startSeconds": start / sr, "toneAmplitude": tone_amplitude(capture[start:start + chip], sr, args.frequency)})
    result = {"source": str(pathlib.Path(args.source)), "capture": str(pathlib.Path(args.capture)),
              "sampleRate": sr, "frequencyHz": args.frequency, "chipSeconds": chip / sr,
              "captureDurationSeconds": len(capture) / sr, "bestMatch": best, "repeatMatches": repeats,
              "searchStepSeconds": step / sr, "fixedWindows": fixed}
    pathlib.Path(args.result).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"bestMatch": best, "fixedWindowCount": len(fixed)}, indent=2))


if __name__ == "__main__":
    main()
