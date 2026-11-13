"""Collect diagnostic JSON, logs and compact visual evidence; preserve scratch BMPs."""
from pathlib import Path
import shutil

repo = Path(__file__).resolve().parents[3]
source = repo / 'Engine/build/playtests/20261107-point'
report = Path(__file__).resolve().parent
for name in ('fixes/baseline', 'fixes/final', 'fixes/packaged', 'visual', 'logs'):
    (report / name).mkdir(parents=True, exist_ok=True)
for item in source.glob('baseline-*.json'):
    shutil.copy2(item, report / 'fixes/baseline' / item.name)
for item in (source / 'final-motion').glob('*.json'):
    shutil.copy2(item, report / 'fixes/final' / item.name)
for item in (source / 'packaged-motion').glob('*.json'):
    shutil.copy2(item, report / 'fixes/packaged' / item.name)
for item in source.glob('*.log'):
    shutil.copy2(item, report / 'logs' / item.name)
