"""Create a UTF-8 ZIP and verify CRC plus every staged SHA-256 entry."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

package, output = map(Path, sys.argv[1:3])
if output.exists():
    raise SystemExit("Output already exists; preserve it or choose a new path")
with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for source in sorted(package.rglob("*")):
        if source.is_file():
            archive.write(source, (Path(package.name) / source.relative_to(package)).as_posix())
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None, "ZIP CRC failure"
    prefix = package.name + "/"
    lines = archive.read(prefix + "SHA256SUMS.txt").decode("utf-8-sig").splitlines()
    for line in lines:
        expected, relative = line.split("  ", 1)
        actual = hashlib.sha256(archive.read(prefix + relative.replace("\\", "/"))).hexdigest()
        assert actual == expected, relative
print(json.dumps({"archive": str(output), "bytes": output.stat().st_size,
                  "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                  "crc": "passed", "manifestFilesVerified": len(lines)}, indent=2))
