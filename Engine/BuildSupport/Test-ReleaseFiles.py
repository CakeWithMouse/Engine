"""Check offline HTML links and native DLL closure without developer PATH fallbacks."""
import argparse
import json
import os
import re
import struct
import subprocess
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote


def check(package, dumpbin):
    missing_links, missing_dlls, wrong_arch = [], [], []
    pages, links, native_files = 0, 0, 0

    class Links(HTMLParser):
        def handle_starttag(self, tag, attributes):
            nonlocal links
            for key, url in attributes:
                if key not in ('href', 'src') or not url or re.match(r'^(?:https?:|data:|mailto:|#|blob:)', url):
                    continue
                target = unquote(url.split('#')[0].split('?')[0])
                links += 1
                if target and not (self.path.parent / target).exists():
                    missing_links.append([str(self.path.relative_to(package)), target])

    for path in (package / 'Engine/docs').rglob('*.html'):
        parser = Links()
        parser.path = path
        parser.feed(path.read_text(encoding='utf-8-sig'))
        pages += 1
    for path in package.rglob('*'):
        if path.suffix.lower() not in ('.dll', '.exe') or '.shadow_cache' in path.parts or 'Redist' in path.parts:
            continue
        relative = path.relative_to(package)
        with path.open('rb') as stream:
            stream.seek(0x3c)
            stream.seek(struct.unpack('<I', stream.read(4))[0] + 4)
            if struct.unpack('<H', stream.read(2))[0] != 0x8664:
                wrong_arch.append(str(relative))
        app = (package / 'Runtime' if relative.parts[0] in ('Runtime', 'Tutorials') else
               package / relative.parts[0] if len(relative.parts) > 1 else package)
        result = subprocess.run([str(dumpbin), '/NOLOGO', '/DEPENDENTS', str(path)], capture_output=True)
        if result.returncode:
            raise RuntimeError(f'dumpbin failed: {path}')
        for dependency in re.findall(rb'^\s+([A-Za-z0-9_.-]+\.dll)\s*$', result.stdout, re.M | re.I):
            name = dependency.decode('ascii')
            if name.lower().startswith(('api-ms-', 'ext-ms-')):
                continue
            if not any((folder / name).exists() for folder in
                       (path.parent, app, Path(os.environ['SystemRoot']) / 'System32')):
                missing_dlls.append([str(relative), name])
        native_files += 1
    return {'htmlPages': pages, 'localLinks': links, 'missingLinks': missing_links,
            'peFilesChecked': native_files, 'missingDlls': missing_dlls, 'wrongArchitecture': wrong_arch}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--dumpbin', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = check(args.package.resolve(), args.dumpbin.resolve())
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    raise SystemExit(bool(result['missingLinks'] or result['missingDlls'] or result['wrongArchitecture']))
