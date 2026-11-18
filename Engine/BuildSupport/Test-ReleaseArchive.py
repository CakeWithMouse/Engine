"""Verify a release ZIP, extract to a new directory, and check every file hash."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(archive, destination):
    archive, destination = archive.resolve(), destination.resolve()
    if destination.exists():
        raise RuntimeError('Use a new extraction directory')
    expected = archive.with_suffix(archive.suffix + '.sha256').read_text().split()[0]
    actual = digest(archive)
    if actual != expected:
        raise RuntimeError('ZIP SHA256 mismatch')
    with zipfile.ZipFile(archive) as source:
        names = source.namelist()
        if len(set(names)) != len(names):
            raise RuntimeError('Duplicate archive entries')
        for name in names:
            target = (destination / name).resolve()
            if not target.is_relative_to(destination) or '\\' in name or ':' in name:
                raise RuntimeError(f'Unsafe archive entry: {name}')
        if source.testzip() is not None:
            raise RuntimeError('ZIP CRC failed')
        source.extractall(destination)
    roots = list(destination.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise RuntimeError('Expected a single package directory')
    package = roots[0]
    sums = package / 'SHA256SUMS.txt'
    checked = set()
    for line in sums.read_text(encoding='utf-8').splitlines():
        expected, name = line.split('  ', 1)
        target = (package / name).resolve()
        if not target.is_relative_to(package) or name in checked:
            raise RuntimeError(f'Invalid hash entry: {name}')
        if not target.is_file() or digest(target) != expected:
            raise RuntimeError(f'File SHA256 mismatch: {name}')
        checked.add(name)
    files = {p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file()}
    if files != checked | {'SHA256SUMS.txt'}:
        raise RuntimeError('Hash list does not cover all packaged files')
    return {'archive': archive.name, 'bytes': archive.stat().st_size,
            'sha256': actual, 'crc': 'passed', 'filesVerified': len(checked),
            'status': 'passed', 'package': str(package)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--destination', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = verify(args.archive, args.destination)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
