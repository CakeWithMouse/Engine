import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parent
repo = root.parents[2]
results = []
for role in ['calm-city', 'curious-city', 'tired-bomber', 'ai-trash']:
    archives = list((root / role).glob('*r6*source.zip'))
    assert len(archives) == 1, (role, archives)
    archive = archives[0]
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.testzip() is None, archive
        names = bundle.namelist()
        forbidden = [n for n in names if set(Path(n).parts) & {'SDK', 'Intermediate', 'Binaries'}
                     or Path(n).suffix.lower() in {'.dll', '.exe', '.lib', '.pdb', '.obj'}
                     and not n.lower().endswith('.obj')]
        # OBJ model assets are source resources, not compiled .obj files.
        forbidden += [n for n in names if n.lower().endswith('.obj') and 'Content/' not in n]
        assert not forbidden, (archive, forbidden)
        assert any(n.startswith('Source/') for n in names), archive
    for name in ['diary.md', 'feedback.md', 'evidence.json', 'REBUILD.md']:
        assert (root / role / name).is_file(), (role, name)
    results.append(dict(role=role, archive=archive.relative_to(root).as_posix(), entries=len(names),
                        bytes=archive.stat().st_size, sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                        excludedSdkBinariesIntermediate=True))
files = ['Engine/build/x64/Release/Engine.lib', 'Game/build/x64/Release/Runtime/SDK/Lib/Engine.lib',
         'Game/build/x64/Release/Runtime/EngineRuntime.exe', 'Game/build/x64/Release/Runtime/EngineCommandlets.exe',
         'Game/build/x64/Release/Runtime/Editor.dll', 'Engine/Version.json']
fingerprints = {name:hashlib.sha256((repo / name).read_bytes()).hexdigest() for name in files}
assert fingerprints[files[0]] == fingerprints[files[1]], 'SDK library differs from built Engine'
result = dict(archives=results, fingerprints=fingerprints,
              baseCommit='d74aebbc0f450a7cc791255423a37d5df89cfb54',
              runtimeVersion='2026.11.07.6 with local r6 fixes; not a published release')
(root / 'artifact-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(results, indent=2))
