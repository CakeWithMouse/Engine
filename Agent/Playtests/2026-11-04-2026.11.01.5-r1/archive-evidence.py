#!/usr/bin/env python3
"""Prepare small playtest evidence and source-only sideprobe archives.

Intentionally not run during drafting. Outputs go under this script's report root.
"""
from __future__ import annotations

import hashlib
import re
import shutil
import zipfile
from pathlib import Path


MAX_ARTIFACT_BYTES = 100 * 1024 * 1024
EVIDENCE_SUFFIXES = {".log", ".txt", ".png", ".bmp"}
SKIP_DIRS = {".git", ".painengine", "EngineStore", "SDK", "Intermediate", "Runtime", "python-deps"}
SENSITIVE_NAME = re.compile(r"(?:secret|credential|password|token|private[-_]?key|\.env)", re.I)
SENSITIVE_TEXT = re.compile(
    r"(?:BEGIN\s+[^\n]*PRIVATE\s+KEY|\bAKIA[0-9A-Z]{16}\b|"
    r"\bBearer\s+[A-Za-z0-9._~+/-]{20,}|(?:password|api[_ -]?key|secret)\s*[:=]\s*\S+)",
    re.I,
)

SIDE_PROBES = {
    "calm-city-final": "Engine/build/playtests/20261104-r1/calm-city/CalmCityProject",
    "curious-city-final": "Engine/build/playtests/20261104-r1/curious-city/Project",
    "tired-bomber-final": "Engine/build/playtests/20261104-r1/tired-bomber/Tired Bomber",
    "ai-trash-final": "Engine/build/playtests/20261104-r1/ai-trash/TrashGame",
    "ai-trash-AIProbe": "Engine/build/playtests/20261104-r1/ai-trash/AIProbe",
    "ai-trash-DirectCpp": "Engine/build/playtests/20261104-r1/ai-trash/DirectCpp",
    "calm-city-CalmCityProbe": "Engine/build/playtests/20261104-r1/calm-city/CalmCityProbe",
    "curious-city-DirectCpp": "Engine/build/playtests/20261104-r1/curious-city/DirectCpp",
    "tired-bomber-FeatureProbeCopy": "Engine/build/playtests/20261104-r1/tired-bomber/FeatureProbeCopy",
}


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def skipped_path(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)


def evidence_files(build_root: Path):
    for path in sorted(build_root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in EVIDENCE_SUFFIXES:
            continue
        relative = path.relative_to(build_root)
        if skipped_path(relative) or SENSITIVE_NAME.search(path.name):
            continue
        size = path.stat().st_size
        if size > MAX_ARTIFACT_BYTES:
            print(f"SKIP oversize evidence ({size} bytes): {relative}")
            continue
        if path.suffix.lower() in {".log", ".txt"}:
            sample = path.read_bytes()
            if SENSITIVE_TEXT.search(sample.decode("utf-8", errors="replace")):
                print(f"SKIP sensitive-looking text: {relative}")
                continue
        yield path, relative


def copy_evidence(build_root: Path, evidence_out: Path) -> int:
    copied = 0
    for source, relative in evidence_files(build_root):
        destination = evidence_out / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            if destination.stat().st_size == source.stat().st_size and digest(destination) == digest(source):
                continue
            raise FileExistsError(f"Refusing to overwrite different evidence: {destination}")
        shutil.copy2(source, destination)
        copied += 1
    return copied


def source_entries(project_root: Path):
    required = [project_root / "Project.json", project_root / "Engine.lock.json"]
    required.extend([project_root / "Content", project_root / "Source"])
    if not all(path.exists() for path in required):
        return None
    entries = [project_root / "Project.json", project_root / "Engine.lock.json"]
    entries += [project_root/name for name in ["Game.vcxproj", "Game.sln", "Build.ps1", "ProjectTypes.tsv", "Project.code.json"] if (project_root/name).exists()]
    for folder in (project_root / "Content", project_root / "Source"):
        for path in sorted(folder.rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(project_root)
            if skipped_path(relative):
                continue
            if path.stat().st_size > MAX_ARTIFACT_BYTES:
                raise ValueError(f"Source artifact exceeds 100 MiB: {path}")
            entries.append(path)
    return entries


def write_sideprobe_archives(repo_root: Path, report_root: Path) -> int:
    output = report_root / "reports" / "sideprobes"
    output.mkdir(parents=True, exist_ok=True)
    count = 0
    for label, relative_root in SIDE_PROBES.items():
        project_root = repo_root / relative_root
        entries = source_entries(project_root)
        if entries is None:
            print(f"SKIP incomplete sideprobe: {relative_root}")
            continue
        archive = output / f"{label}.zip"
        if archive.exists():
            raise FileExistsError(f"Refusing to overwrite archive: {archive}")
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for path in entries:
                bundle.write(path, path.relative_to(project_root).as_posix())
        if archive.stat().st_size > MAX_ARTIFACT_BYTES:
            archive.unlink()
            raise ValueError(f"Sideprobe archive exceeds 100 MiB: {archive}")
        # Defensive verification: source bundles may not contain build/runtime payloads.
        violation = None
        with zipfile.ZipFile(archive) as bundle:
            for name in bundle.namelist():
                parts = Path(name).parts
                if any(part in {"Binaries", "SDK", "Intermediate", "Runtime", "EngineStore", ".painengine"} for part in parts):
                    violation = f"Forbidden build/runtime entry in archive {archive}: {name}"
                    break
                info = bundle.getinfo(name)
                if info.file_size > MAX_ARTIFACT_BYTES:
                    violation = f"Oversize entry in archive {archive}: {name}"
                    break
        if violation:
            archive.unlink()
            raise ValueError(violation)
        count += 1
    return count


def main() -> None:
    report_root = Path(__file__).resolve().parent
    repo_root = report_root.parents[2]
    build_root = repo_root / "Engine" / "build" / "playtests" / "20261104-r1"
    if not build_root.is_dir():
        raise FileNotFoundError(build_root)
    evidence_out = report_root / "reports" / "evidence"
    evidence_out.mkdir(parents=True, exist_ok=True)
    copied = copy_evidence(build_root, evidence_out)
    archives = write_sideprobe_archives(repo_root, report_root)
    print(f"Copied evidence files: {copied}")
    print(f"Sideprobe source archives: {archives}")


if __name__ == "__main__":
    main()
