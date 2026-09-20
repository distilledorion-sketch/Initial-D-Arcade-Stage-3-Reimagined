#!/usr/bin/env python3
"""Compare tick-aligned driving traces without inferring original-game fidelity.

CSV schema: tick,speed,yaw,pos_x,pos_y,pos_z. Defaults: m/s, radians, metres.
Python standard library only. See docs/FIDELITY_ACCEPTANCE.md for the contract.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Sequence

FIELDS = ("tick", "speed", "yaw", "pos_x", "pos_y", "pos_z")


class TraceError(ValueError):
    """A trace cannot be compared without discarding or guessing information."""


def column_mapping(values: Sequence[str]) -> dict[str, str]:
    mapping = {name: name for name in FIELDS}
    seen = set()
    for value in values:
        key, separator, column = value.partition("=")
        if not separator or key not in FIELDS or not column or key in seen:
            raise TraceError(f"Invalid or duplicate mapping {value!r}; use canonical=column")
        mapping[key] = column
        seen.add(key)
    if len(set(mapping.values())) != len(FIELDS):
        raise TraceError("Each canonical field must map to a different CSV column")
    return mapping


def read_trace(
    path: Path,
    mapping: dict[str, str],
    speed_unit: str = "mps",
    yaw_unit: str = "rad",
    position_scale: float = 1.0,
    tick_offset: int = 0,
) -> dict[int, dict[str, float]]:
    if not math.isfinite(position_scale) or position_scale <= 0:
        raise TraceError("Position scale must be positive and finite")
    trace = {}
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            header = reader.fieldnames or []
            if len(header) != len(set(header)):
                raise TraceError(f"{path}: duplicate CSV column names")
            missing = sorted(set(mapping.values()) - set(header))
            if missing:
                raise TraceError(f"{path}: missing columns: {', '.join(missing)}")
            previous = None
            for line_number, row in enumerate(reader, start=2):
                try:
                    # Parse integer ticks directly: floats silently lose large integers.
                    tick = int(row[mapping["tick"]]) + tick_offset
                    values = {name: float(row[mapping[name]]) for name in FIELDS[1:]}
                except (ValueError, TypeError, KeyError) as error:
                    raise TraceError(f"{path}:{line_number}: invalid or missing numeric value") from error
                if previous is not None and tick <= previous:
                    raise TraceError(f"{path}:{line_number}: ticks must be unique and increasing")
                if not all(math.isfinite(value) for value in values.values()):
                    raise TraceError(f"{path}:{line_number}: non-finite numeric value")
                values["speed"] /= 3.6 if speed_unit == "kmh" else 1.0
                values["yaw"] *= math.pi / 180.0 if yaw_unit == "deg" else 1.0
                for axis in ("pos_x", "pos_y", "pos_z"):
                    values[axis] *= position_scale
                if not all(math.isfinite(value) for value in values.values()):
                    raise TraceError(f"{path}:{line_number}: conversion produced non-finite value")
                trace[tick] = values
                previous = tick
    except (OSError, UnicodeError, csv.Error) as error:
        raise TraceError(f"Cannot read {path}: {error}") from error
    if not trace:
        raise TraceError(f"{path}: trace has no data rows")
    return trace


def summarize(errors: Sequence[float]) -> dict[str, float]:
    absolute = sorted(abs(value) for value in errors)
    # hypot avoids overflow from squaring otherwise finite errors.
    return {
        "mean_signed": math.fsum(value / len(errors) for value in errors),
        "mean_abs": math.fsum(value / len(errors) for value in absolute),
        "rmse": math.hypot(*errors) / math.sqrt(len(errors)),
        "p95_abs": absolute[max(0, math.ceil(len(errors) * 0.95) - 1)],
        "max_abs": absolute[-1],
    }


def compare(
    reference: dict[int, dict[str, float]],
    candidate: dict[int, dict[str, float]],
    allow_partial: bool = False,
) -> dict:
    ref_ticks, cand_ticks = set(reference), set(candidate)
    common = sorted(ref_ticks & cand_ticks)
    if not common:
        raise TraceError("Traces have no shared ticks; align race epochs explicitly")
    if ref_ticks != cand_ticks and not allow_partial:
        raise TraceError(
            f"Tick sets differ: {len(ref_ticks - cand_ticks)} missing candidate ticks, "
            f"{len(cand_ticks - ref_ticks)} extra candidate ticks; "
            "fix capture coverage or explicitly use --allow-partial"
        )
    errors = {name: [] for name in FIELDS[1:]}
    distances = []
    for tick in common:
        for name in errors:
            delta = candidate[tick][name] - reference[tick][name]
            if not math.isfinite(delta):
                raise TraceError(f"Numeric difference overflow at tick {tick}, {name}")
            if name == "yaw":
                delta = math.remainder(delta, math.tau)
            errors[name].append(delta)
        distances.append(math.hypot(*(errors[axis][-1] for axis in ("pos_x", "pos_y", "pos_z"))))
    metrics = {name: summarize(values) for name, values in errors.items()}
    metrics["position_distance"] = summarize(distances)
    if not all(math.isfinite(value) for metric in metrics.values() for value in metric.values()):
        raise TraceError("Metric overflow; inspect trace units and coordinate ranges")
    return {
        "coverage": {
            "reference_rows": len(reference),
            "candidate_rows": len(candidate),
            "compared_rows": len(common),
            "reference_fraction": len(common) / len(reference),
            "candidate_fraction": len(common) / len(candidate),
            "missing_candidate_ticks": sorted(ref_ticks - cand_ticks),
            "extra_candidate_ticks": sorted(cand_ticks - ref_ticks),
            "first_tick": common[0],
            "last_tick": common[-1],
            "complete": ref_ticks == cand_ticks,
        },
        "metrics": metrics,
        "endpoint": {
            "tick": common[-1],
            "speed_error_mps": errors["speed"][-1],
            "yaw_error_rad": errors["yaw"][-1],
            "position_distance_m": distances[-1],
        },
    }


def nonnegative_finite(value: str) -> float:
    result = float(value)
    if not math.isfinite(result) or result < 0:
        raise argparse.ArgumentTypeError("must be a finite number >= 0")
    return result


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("reference", type=Path)
    result.add_argument("candidate", type=Path)
    result.add_argument("--output", type=Path, help="Write the full JSON report")
    for side in ("reference", "candidate"):
        result.add_argument(f"--{side}-map", action="append", default=[], metavar="FIELD=COLUMN")
        result.add_argument(f"--{side}-speed-unit", choices=("mps", "kmh"), default="mps")
        result.add_argument(f"--{side}-yaw-unit", choices=("rad", "deg"), default="rad")
        result.add_argument(f"--{side}-position-scale", type=float, default=1.0,
                            help="Multiply all position axes to convert to metres")
    result.add_argument("--candidate-tick-offset", type=int, default=0,
                        help="Explicit integer offset added to candidate ticks")
    result.add_argument("--allow-partial", action="store_true",
                        help="Compare shared ticks and disclose incomplete coverage")
    result.add_argument("--reference-kind", choices=("unknown", "synthetic", "original-capture", "recomp-capture"),
                        default="unknown", help="Operator-declared provenance; not independently verified")
    result.add_argument("--max-speed-error", type=nonnegative_finite, help="Maximum absolute speed error in m/s")
    result.add_argument("--max-yaw-error", type=nonnegative_finite, help="Maximum wrapped yaw error in radians")
    result.add_argument("--max-position-error", type=nonnegative_finite, help="Maximum 3D separation in metres")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        traces = {}
        sources = {}
        for side in ("reference", "candidate"):
            path = getattr(args, side)
            mapping = column_mapping(getattr(args, f"{side}_map"))
            speed_unit = getattr(args, f"{side}_speed_unit")
            yaw_unit = getattr(args, f"{side}_yaw_unit")
            position_scale = getattr(args, f"{side}_position_scale")
            tick_offset = args.candidate_tick_offset if side == "candidate" else 0
            traces[side] = read_trace(path, mapping, speed_unit, yaw_unit, position_scale, tick_offset)
            sources[side] = {
                "path": str(path.resolve()),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "column_map": mapping,
                "input_speed_unit": speed_unit,
                "input_yaw_unit": yaw_unit,
                "position_scale_to_metres": position_scale,
                "tick_offset": tick_offset,
            }
        report = compare(traces["reference"], traces["candidate"], args.allow_partial)
        gates = []
        for option, metric, unit in (
            ("max_speed_error", "speed", "m/s"),
            ("max_yaw_error", "yaw", "rad"),
            ("max_position_error", "position_distance", "m"),
        ):
            threshold = getattr(args, option)
            if threshold is not None:
                actual = report["metrics"][metric]["max_abs"]
                gates.append({"metric": metric, "maximum": actual, "threshold": threshold,
                              "unit": unit, "passed": actual <= threshold})
        status = "unscored" if not gates else "passed" if all(gate["passed"] for gate in gates) else "failed"
        report.update({
            "schema_version": 1,
            "status": status,
            "declared_reference_kind": args.reference_kind,
            "provenance_verified": False,
            "original_game_fidelity_established": False,
            "notice": "This report compares supplied samples only. Threshold success does not establish original-game fidelity.",
            "units": {"speed": "m/s", "yaw": "rad (shortest signed angle)", "position": "m"},
            "sources": sources,
            "gates": gates,
        })
        output = json.dumps(report, indent=2, allow_nan=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output, encoding="utf-8")
        print(output, end="")
        return 1 if status == "failed" else 0
    except (TraceError, OSError, ValueError) as error:
        print(f"Telemetry comparison error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
