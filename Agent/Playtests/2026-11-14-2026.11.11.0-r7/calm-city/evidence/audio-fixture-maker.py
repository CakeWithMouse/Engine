import json
import math
import pathlib
import struct
import sys
import wave

out = pathlib.Path(sys.argv[1])
rate = 48000
frequency = 733
chip_seconds = 0.125
chip_samples = round(rate * chip_seconds)
code = "11010010111000101101001100010110"
samples = []
phase = 0.0
for bit in code:
    amp = 0.72 if bit == "1" else 0.18
    for i in range(chip_samples):
        edge = min(1.0, i / 240.0, (chip_samples - 1 - i) / 240.0)
        value = math.sin(phase) * amp * max(0.0, edge)
        phase = (phase + 2.0 * math.pi * frequency / rate) % (2.0 * math.pi)
        samples.append(max(-32768, min(32767, round(value * 32767))))
out.parent.mkdir(parents=True, exist_ok=True)
with wave.open(str(out), "wb") as f:
    f.setnchannels(1)
    f.setsampwidth(2)
    f.setframerate(rate)
    f.writeframes(b"".join(struct.pack("<h", x) for x in samples))
out.with_suffix(".json").write_text(json.dumps({
    "sampleRate": rate, "channels": 1, "bitsPerSample": 16,
    "frequencyHz": frequency, "chipSeconds": chip_seconds,
    "amplitudeCode": code, "durationSeconds": len(samples) / rate
}, indent=2) + "\n", encoding="utf-8")
print(f"wrote {out} samples={len(samples)} seconds={len(samples)/rate:.3f}")
