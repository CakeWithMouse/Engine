import json
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent


def bmp(name):
    path = ROOT / name
    data = path.read_bytes()
    assert data[:2] == b"BM", f"not a BMP: {path}"
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, signed_height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    assert bits == 24 and compression == 0, f"expected uncompressed 24-bit BMP: {path}"
    height = abs(signed_height)
    stride = ((width * 3 + 3) // 4) * 4
    rows = []
    for y in range(height):
        source_y = y if signed_height < 0 else height - 1 - y
        row_offset = pixel_offset + source_y * stride
        rows.append([
            tuple(reversed(data[row_offset + x * 3:row_offset + x * 3 + 3]))
            for x in range(width)
        ])
    return rows


def average(image, box):
    x0, y0, x1, y1 = box
    pixels = [image[y][x] for y in range(y0, y1) for x in range(x0, x1)]
    assert pixels, f"empty sample region: {box}"
    return [round(sum(pixel[channel] for pixel in pixels) / len(pixels), 2) for channel in range(3)]


def difference(a, b, box, threshold):
    x0, y0, x1, y1 = box
    points = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            pa, pb = a[y][x], b[y][x]
            delta = sum(pa) - sum(pb)
            if abs(delta) >= threshold:
                points.append((x, y, delta, pa, pb))
    return {
        "pixels": len(points),
        "bbox": [min(p[0] for p in points), min(p[1] for p in points),
                 max(p[0] for p in points), max(p[1] for p in points)] if points else None,
        "darkened": sum(p[2] < 0 for p in points),
        "brightened": sum(p[2] > 0 for p in points),
        "meanDeltaRGB": [round(sum(p[3][c] - p[4][c] for p in points) / len(points), 2)
                         for c in range(3)] if points else None,
        "coords": [(p[0], p[1]) for p in points],
    }


def centroid(points):
    return [round(sum(p[0] for p in points) / len(points), 2),
            round(sum(p[1] for p in points) / len(points), 2)] if points else None


# Material samples and independent alpha/source-over expectations.
forward = bmp("material-blends-forward.bmp")
source_over = bmp("material-blends-sourceover.bmp")
deferred = bmp("material-blends-deferred.bmp")
material = {
    "expectedOpaqueRGB": [217, 31, 20],
    "opaqueRGB": average(forward, (125, 165, 155, 195)),
    "expectedSourceOverRGB": [119, 41, 131],
    "sourceOverForwardRGB": average(source_over, (125, 165, 155, 195)),
    "sourceOverDeferredRGB": average(deferred, (125, 165, 155, 195)),
    "maskedHoleRGB": average(source_over, (270, 165, 300, 195)),
    "maskedCoveredRGB": average(source_over, (345, 165, 375, 195)),
    "backgroundRGB": average(source_over, (25, 165, 45, 195)),
    "translucentOnlyRGB": average(source_over, (485, 165, 515, 195)),
    "deferredMaskedHoleRGB": average(deferred, (270, 165, 300, 195)),
    "deferredMaskedCoveredRGB": average(deferred, (345, 165, 375, 195)),
}
for key in ("sourceOverForwardRGB", "sourceOverDeferredRGB"):
    assert max(abs(a - b) for a, b in zip(material[key], material["expectedSourceOverRGB"])) <= 2, key
assert max(abs(a - b) for a, b in zip(material["opaqueRGB"], material["expectedOpaqueRGB"])) <= 2
for hole_key in ("maskedHoleRGB", "deferredMaskedHoleRGB"):
    assert max(abs(a - b) for a, b in zip(material[hole_key], material["backgroundRGB"])) <= 2, hole_key
for covered_key in ("maskedCoveredRGB", "deferredMaskedCoveredRGB"):
    r, g, b = material[covered_key]
    assert g > 150 and g > r * 3 and g > b * 3, covered_key

# Fixed primitive/camera/receiver: light shadow enable A/B; moved light gets its own matched A/B.
shadow_frames = [bmp(f"shadows-frame-{index}.bmp") for index in range(5)]
base = difference(shadow_frames[0], shadow_frames[1], (50, 35, 600, 440), 40)
moved = difference(shadow_frames[2], shadow_frames[4], (50, 35, 600, 440), 40)
base_points = set(base.pop("coords"))
moved_points = set(moved.pop("coords"))
return_diff = difference(shadow_frames[3], shadow_frames[0], (0, 0, 640, 480), 1)["pixels"]
assert base["darkened"] > 0 and moved["darkened"] > 0
assert return_diff == 0, "return-to-base primitive shadow frame must be pixel-identical"
shadow = {
    "baseLightShadowOnVsOff": base,
    "movedLightShadowOnVsOff": moved,
    "baseShadowCentroid": centroid(base_points),
    "movedShadowCentroid": centroid(moved_points),
    "maskIntersection": len(base_points & moved_points),
    "maskUnion": len(base_points | moved_points),
    "returnFrameDifferentPixels": return_diff,
}
assert shadow["baseShadowCentroid"] != shadow["movedShadowCentroid"], "light move must move the shadow"

# Imported CesiumMan remains fixed; this request toggles the point light's shadow enable.
imported_on = bmp("imported-shadow-on.bmp")
imported_off = bmp("imported-shadow-off.bmp")
imported_return = bmp("imported-shadow-return.bmp")
imported_delta = difference(imported_on, imported_off, (50, 35, 600, 440), 40)
imported_delta.pop("coords")
imported_return_diff = difference(imported_return, imported_on, (0, 0, 640, 480), 1)["pixels"]
assert imported_delta["darkened"] > 0, "imported-model receiver must darken with point-light shadows enabled"
assert imported_return_diff == 0, "return-to-base imported-model frame must be pixel-identical"

# Imported UV quadrant texture: assert color-channel pattern, not exact lighting RGB values.
uv = bmp("imported-uv-chart-frame.bmp")
quadrants = {
    "upperLeftGreen": average(uv, (677, 197, 710, 230)),
    "upperRightRed": average(uv, (750, 197, 785, 230)),
    "lowerLeftYellow": average(uv, (677, 270, 710, 305)),
    "lowerRightBlue": average(uv, (750, 270, 785, 305)),
}
green, red, yellow, blue = (quadrants[k] for k in quadrants)
assert green[1] > green[0] * 2 and green[1] > green[2] * 2
assert red[0] > red[1] * 2 and red[0] > red[2] * 2
assert yellow[0] > yellow[2] * 3 and yellow[1] > yellow[2] * 3
assert blue[2] > blue[0] * 3 and blue[2] > blue[1] * 3
assert len({tuple(round(c) for c in color) for color in quadrants.values()}) == 4

result = {
    "materialBlends": material,
    "shadows": shadow,
    "importedShadows": {
        "fixedImportedCesiumManPointLightShadowOnVsOff": imported_delta,
        "returnFrameDifferentPixels": imported_return_diff,
        "casterCastShadowsToggle": "not exercised; frames toggle PointLightComponent shadow enable",
    },
    "importedUv": {"frameSize": [len(uv[0]), len(uv)], "quadrants": quadrants},
    "assertions": "all passed",
}
output = ROOT / "material-shadow-uv-pixel-analysis.json"
output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, ensure_ascii=False, indent=2))
