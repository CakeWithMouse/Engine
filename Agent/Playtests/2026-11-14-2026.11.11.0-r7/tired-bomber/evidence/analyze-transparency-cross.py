import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent
request = json.loads((ROOT / "transparency-cross.request.json").read_text(encoding="utf-8-sig"))
result = json.loads((ROOT / "transparency-cross.result.json").read_text(encoding="utf-8-sig"))

def dot(a, b):
    return sum(x * y for x, y in zip(a, b))

def sub(a, b):
    return [x - y for x, y in zip(a, b)]

def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def unit(a):
    length = math.sqrt(dot(a, a))
    return [x / length for x in a]

def read_bmp(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, signed_height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    height = abs(signed_height)
    stride = ((width * bits + 31) // 32) * 4
    def pixel(x, y):
        row = height - 1 - y if signed_height > 0 else y
        i = offset + row * stride + x * (bits // 8)
        return [data[i+2], data[i+1], data[i]]
    return width, height, pixel

summary = []
for frame, rendered in zip(request["frames"], result["frames"]):
    width, height, pixel = read_bmp(rendered["image"])
    eye = frame["camera"]
    forward = unit(sub(frame["target"], eye))
    right = unit(cross([0, 1, 0], forward))
    up = cross(forward, right)
    samples = 0
    wrong = 0
    max_error = 0
    for y in range(36, height - 36, 6):
        for x in range(42, width - 42, 6):
            sx = (2 * (x + .5) / width - 1) * width / height * math.tan(math.pi / 6)
            sy = (1 - 2 * (y + .5) / height) * math.tan(math.pi / 6)
            ray = [forward[k] + sx * right[k] + sy * up[k] for k in range(3)]
            hits = []
            # The diagnostic cross consists of two real planes at yaw -45/+45;
            # the shifted UV selects the material's red/blue branch.
            for yaw, color in ((-45, [255, 0, 0]), (45, [0, 0, 255])):
                angle = math.radians(yaw)
                tangent = [math.cos(angle), 0, -math.sin(angle)]
                normal = [math.sin(angle), 0, math.cos(angle)]
                denominator = dot(ray, normal)
                if abs(denominator) < 1e-8:
                    continue
                distance = dot([0-eye[k] for k in range(3)], normal) / denominator
                point = [eye[k] + distance * ray[k] for k in range(3)]
                if distance <= 0:
                    continue
                local = point
                u = dot(local, tangent)
                v = local[1]
                if abs(u) < 3 and abs(v) < 2:
                    hits.append((distance, color))
            if not hits or (len(hits) == 2 and abs(hits[0][0] - hits[1][0]) < .003):
                continue
            expected = [0, 0, 51]
            for _, color in sorted(hits, reverse=True):
                expected = [round(color[k] * .5 + expected[k] * .5) for k in range(3)]
            measured = pixel(x, y)
            error = max(abs(a-b) for a, b in zip(expected, measured))
            wrong += error > 8
            max_error = max(max_error, error)
            samples += 1
    summary.append({"image": Path(rendered["image"]).name, "size": [width, height],
                    "drawCalls": rendered["drawCalls"], "oracleSamples": samples,
                    "wrongPixelsOver8": wrong, "maxError": max_error})
(ROOT / "transparency-cross-analysis.json").write_text(json.dumps(summary, indent=2))
print(json.dumps(summary))
