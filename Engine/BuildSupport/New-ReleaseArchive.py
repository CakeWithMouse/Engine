"""Stage a clean Windows distribution from a completed CMake Release build.

Requires Python 3.11+. Run the staged package checks before using --archive.
Never overwrites an existing staging directory or archive.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GAMES = ('BasicExamples', 'Pong', 'SunGame', 'Battle', 'Katamari')
REPORTS = ('2026-11-18-0.4.0-release', '2026-11-18-0.4.0-strategy')
CRT = ('concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
       'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll', 'vccorlib140.dll',
       'vcruntime140.dll', 'vcruntime140_1.dll', 'vcruntime140_threads.dll')
SKIP_DIRS = {'.shadow_cache', '__pycache__', 'Intermediate', '.git', 'obj'}
# .obj is also the Wavefront model format used by Katamari; never filter it by suffix.
SKIP_SUFFIXES = {'.pdb', '.exp', '.ilk', '.log', '.tmp', '.pyc'}


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def copy_file(source: Path, target: Path):
    if not source.is_file() or source.is_symlink():
        raise RuntimeError(f'Required regular file missing: {source}')
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_tree(source: Path, target: Path, *, libraries=False):
    if not source.is_dir():
        raise RuntimeError(f'Required directory missing: {source}')
    for item in source.rglob('*'):
        relative = item.relative_to(source)
        if any(part in SKIP_DIRS or part.startswith('.sdk-') for part in relative.parts):
            continue
        if item.is_symlink():
            raise RuntimeError(f'Linked input is not supported: {item}')
        if item.is_file() and item.suffix.lower() not in SKIP_SUFFIXES:
            if item.suffix.lower() == '.lib' and not libraries:
                continue
            if item.name.endswith('.before-build') or item.name == '.project-build.lock':
                continue
            copy_file(item, target / relative)


def write_json(path: Path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def stage(args):
    version = json.loads((REPO / 'Engine/Version.json').read_text(encoding='utf-8'))['version']
    build = args.build.resolve()
    msvc = (REPO / 'Game/build/x64/Release/Runtime').resolve()
    package = args.package.resolve()
    if package.exists():
        raise RuntimeError(f'Use a new staging directory: {package}')
    runtime = package / 'Runtime'
    metadata = json.loads((build / 'Runtime/EngineVersion.json').read_text(encoding='utf-8'))
    if metadata['version'] != version:
        raise RuntimeError('Built runtime version differs from Engine/Version.json')
    package.mkdir(parents=True)
    copy_file(build / 'EngineLauncher.exe', package / 'EngineLauncher.exe')
    copy_tree(build / 'Runtime', runtime, libraries=True)
    # This Win32 launcher has no Engine ABI dependency. CMake does not yet build it.
    copy_file(msvc / 'GameExecutable.exe', runtime / 'GameExecutable.exe')
    copy_file(msvc / 'EngineCommandlets.exe', runtime / 'EngineCommandlets.exe')
    copy_file(msvc / 'Qt6Test.dll', runtime / 'Qt6Test.dll')
    # Commandlets explicitly select Qt's offscreen platform; windeployqt only
    # deploys qwindows by default for the interactive editor.
    copy_file(REPO / 'Editor/ThirdParty/Qt6.7/plugins/platforms/qoffscreen.dll',
              runtime / 'platforms/qoffscreen.dll')
    for name in ('libcrypto-3-x64.dll', 'libssl-3-x64.dll'):
        copy_file(REPO / 'Editor/ThirdParty/Qt6.7/bin' / name, runtime / name)
    for game in GAMES:
        copy_tree(build / game, package / game)
    for directory in [package, runtime, *(package / name for name in GAMES)]:
        for name in CRT:
            copy_file(msvc / name, directory / name)
    compiler = Path(args.d3dcompiler).resolve()
    for directory in [runtime, *(package / name for name in GAMES)]:
        copy_file(compiler, directory / 'D3Dcompiler_47.dll')
    installer = runtime / 'vc_redist.x64.exe'
    if installer.exists():
        copy_file(installer, package / 'Redist/vc_redist.x64.exe')
        installer.unlink()  # only the newly staged copy

    tutorial = package / 'Tutorials/AiTutorial'
    source = REPO / 'Game/build/x64/Release/Tutorials/AiTutorial'
    for name in ('Project.json', 'Engine.lock.json', 'Game.dll', 'README.md'):
        copy_file(source / name, tutorial / name)
    copy_tree(source / 'Content', tutorial / 'Content')
    strategy_source = args.strategy.resolve()
    strategy = package / 'Tutorials/RepairCorridor'
    for name in ('Project.json', 'Engine.lock.json', 'Build.ps1', 'Game.sln', 'Game.vcxproj', 'README.md'):
        copy_file(strategy_source / name, strategy / name)
    for name in ('Source', 'Content'):
        copy_tree(strategy_source / name, strategy / name)
    manifest = json.loads((strategy / 'Project.json').read_text(encoding='utf-8-sig'))
    copy_file(strategy_source / manifest['module'], strategy / manifest['module'])
    for sample in (tutorial, strategy):
        (sample / 'OpenEditor.cmd').write_text(
            '@echo off\r\n"%~dp0..\\..\\Runtime\\EngineRuntime.exe" --project "%~dp0Project.json" --mode editor\r\n',
            encoding='ascii', newline='')

    docs_source = REPO / 'Engine/docs'
    docs = package / 'Engine/docs'
    for item in docs_source.rglob('*'):
        relative = item.relative_to(docs_source)
        if relative.parts[:2] == ('tutorial', 'releases'):
            continue  # historical presentations are not the 0.4.0 manual
        if relative.parts[0] == 'tutorial' and len(relative.parts) == 2 and item.suffix == '.json':
            continue
        if item.is_file():
            copy_file(item, docs / relative)
    copy_tree(REPO / 'Agent/Tasks', package / 'Agent/Tasks')
    reports = list(dict.fromkeys((*REPORTS, *args.report)))
    for name in reports:
        if Path(name).name != name:
            raise RuntimeError(f'Invalid report directory: {name}')
        # Preserve complete evidence, including logs; no SDK or build binaries live here.
        shutil.copytree(REPO / 'Agent/Playtests' / name, package / 'Agent/Playtests' / name)
    copy_file(args.source_archive, package / 'Sources' / args.source_archive.name)
    copy_file(REPO / 'Engine/Version.json', package / 'Engine/Version.json')
    (package / 'Documentation.html').write_text(
        '<!doctype html><html lang="ru"><meta charset="utf-8"><meta http-equiv="refresh" '
        'content="0;url=Engine/docs/index.html"><title>PainEngine documentation</title>'
        '<a href="Engine/docs/index.html">Открыть документацию 0.4.0</a></html>', encoding='utf-8')
    notes = f'RELEASE-{version}{"-" + args.revision if args.revision else ""}.md'
    (package / 'README.txt').write_text(f'''PainEngine {version}{" " + args.revision if args.revision else ""} — Windows x64 Release

Распакуйте архив целиком и запустите EngineLauncher.exe.
Готовые примеры и учебные проекты доступны в лаунчере.
Documentation.html открывает актуальную офлайн-справку; изменения — Engine/docs/{notes}.
Runtime/SDK — CMake SDK. Для C++ нужны совместимые MSVC x64 tools и Windows SDK,
а также CMake 3.24+ и Ninja в PATH. Готовые игры и проекты без C++ не требуют компилятора.
AiTutorial и RepairCorridor находятся в Tutorials; OpenEditor.cmd открывает их редактор.
Исходный архив стратегии — Sources; история проверки — Agent/Playtests.
Физика Jolt и импорт Assimp включены. Лицензии — Licenses; VC++ Redist — Redist.
Blender не включён. Для .blend установите Blender согласно руководству импорта.
Исторические презентации и старые runtime в этот пакет не включены.
''', encoding='utf-8-sig')

    licenses = package / 'Licenses'
    for component in ('assimp', 'jhasse-poly2tri', 'kubazip', 'minizip', 'pugixml', 'zlib', 'joltphysics'):
        copy_file(REPO / 'Engine/vcpkg_installed/x64-windows/share' / component / 'copyright',
                  licenses / f'{component}.txt')
    for component, source in (
        ('glm', 'GLM-master/copying.txt'), ('nlohmann', 'nlohmann/LICENSE.MIT'),
        ('meshoptimizer', 'meshoptimizer/LICENSE.md'), ('stb', 'stb/stb_image.h')):
        copy_file(REPO / 'Engine/ThirdParty' / source, licenses / f'{component}.txt')
    copy_tree(args.qt_licenses.resolve(), licenses / 'Qt')
    qt_header = (REPO / 'Editor/ThirdParty/Qt6.7/include/QtCore/qconfig.h').read_text(encoding='utf-8')
    qt_version = re.search(r'#define\s+QT_VERSION_STR\s+"([^"]+)"', qt_header).group(1)
    (licenses / 'Qt/NOTICE.txt').write_text(f'''Qt {qt_version}: Core, Gui, Widgets, Network, Svg, Test and plugins.
Copyright The Qt Company Ltd. and contributors. Dynamically linked under LGPLv3.
License texts: LGPL-3.0-only.txt and GPL-3.0-only.txt.
Corresponding upstream sources and third-party notices:
https://download.qt.io/archive/qt/6.7/{qt_version}/submodules/
https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v{qt_version}
https://code.qt.io/cgit/qt/qtsvg.git/tree/?h=v{qt_version}
Qt DLLs remain separate and can be replaced with compatible builds.
OpenSSL 3.0.14 DLLs are from the same Qt installation; see OpenSSL-3.0.14-LICENSE.txt.
OpenSSL corresponding source: https://github.com/openssl/openssl/tree/openssl-3.0.14
''', encoding='utf-8')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO, text=True).strip()
    write_json(package / 'RELEASE-MANIFEST.json', {
        'version': version, 'revision': args.revision, 'platform': 'Windows x64', 'configuration': 'Release',
        'packagingRevision': revision, 'buildSystem': 'CMake/Ninja + MSVC',
        'sdk': 'CMake', 'assimp': True, 'jolt': True, 'qt': qt_version,
        'supplementalMsbuildBinaries': ['Runtime/GameExecutable.exe', 'Runtime/EngineCommandlets.exe'],
        'documentation': 'Engine/docs/index.html',
        'runtimeSha256': digest(runtime / 'EngineRuntime.exe'),
        'editorSha256': digest(runtime / 'Editor.dll'),
        'sdkLibrarySha256': digest(runtime / 'SDK/lib/Engine.lib'),
        'd3dCompilerSha256': digest(compiler),
        'sourceArchiveSha256': digest(args.source_archive), 'reports': reports,
    })
    print(package)


def archive(args):
    package = args.package.resolve()
    output = args.archive.resolve()
    if output.exists():
        raise RuntimeError(f'Archive already exists: {output}')
    if not (package / 'VALIDATION.json').is_file():
        raise RuntimeError('Run package validation before creating the archive')
    files = sorted(p for p in package.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt'
                   and not any(x in SKIP_DIRS for x in p.relative_to(package).parts))
    (package / 'SHA256SUMS.txt').write_text(''.join(
        f'{digest(p)}  {p.relative_to(package).as_posix()}\n' for p in files), encoding='utf-8')
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for p in sorted([*files, package / 'SHA256SUMS.txt']):
            z.write(p, Path(package.name) / p.relative_to(package))
    with zipfile.ZipFile(output) as z:
        bad = z.testzip()
        if bad:
            raise RuntimeError(f'Archive CRC failed: {bad}')
    value = digest(output)
    output.with_suffix(output.suffix + '.sha256').write_text(f'{value}  {output.name}\n', encoding='ascii')
    print(json.dumps({'archive': str(output), 'bytes': output.stat().st_size, 'sha256': value}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--build', type=Path, default=REPO / 'build/cmake/editor-Release/bin/Release')
    parser.add_argument('--strategy', type=Path, default=REPO / 'Engine/build/playtests/strategy-040/fresh-unpack-msbuild')
    parser.add_argument('--source-archive', type=Path, default=REPO / 'Agent/Playtests' / REPORTS[1] / 'RepairCorridor-0.4.0-source.zip')
    parser.add_argument('--revision', default='')
    parser.add_argument('--report', action='append', default=[])
    parser.add_argument('--d3dcompiler', type=Path, default=Path('C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/d3dcompiler_47.dll'))
    parser.add_argument('--qt-licenses', type=Path, default=REPO / 'Engine/BuildSupport/ReleaseLicenses')
    parser.add_argument('--archive', type=Path)
    options = parser.parse_args()
    archive(options) if options.archive else stage(options)
