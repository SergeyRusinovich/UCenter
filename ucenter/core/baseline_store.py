"""Хранение измерений: JSON (между запусками) и экспорт в CSV."""
import csv
import json
import os
from dataclasses import asdict, fields

from ucenter.core.baseline import Baseline

CSV_COLUMNS = (
    ("Saved", lambda m: m.saved_at),
    ("A lat", lambda m: f"{m.a_lat:.8f}"),
    ("A lon", lambda m: f"{m.a_lon:.8f}"),
    ("B lat", lambda m: f"{m.b_lat:.8f}"),
    ("B lon", lambda m: f"{m.b_lon:.8f}"),
    ("Azimuth A->B (deg)", lambda m: "" if m.azimuth is None else f"{m.azimuth:.4f}"),
    ("Back azimuth B->A (deg)", lambda m: "" if m.back_azimuth is None else f"{m.back_azimuth:.4f}"),
    ("Deviation from true N (deg)", lambda m: "" if m.deviation is None else f"{m.deviation:+.4f}"),
    ("Distance (m)", lambda m: f"{m.distance_m:.3f}"),
    ("dNorth (m)", lambda m: f"{m.d_north:.3f}"),
    ("dEast (m)", lambda m: f"{m.d_east:.3f}"),
)


def write_csv(path: str, items: list[Baseline]):
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(name for name, _ in CSV_COLUMNS)
        for m in items:
            w.writerow(fmt(m) for _, fmt in CSV_COLUMNS)


def save_json(path: str, items: list[Baseline]):
    """Сохраняет атомарно: сбой записи не портит прежний файл."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump([asdict(m) for m in items], fh, ensure_ascii=False, indent=1)
    os.replace(tmp, path)


def load_json(path: str) -> list[Baseline]:
    """Пустой список, если файла нет; ValueError/TypeError — если он повреждён."""
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except FileNotFoundError:
        return []
    names = {f.name for f in fields(Baseline)}
    return [Baseline(**{k: v for k, v in d.items() if k in names}) for d in data]
