"""Synthetic controlled tests of the comparator; no original game capture."""

import contextlib
import importlib.util
import io
import json
import math
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "compare_telemetry.py"
SPEC = importlib.util.spec_from_file_location("compare_telemetry", SCRIPT)
telemetry = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(telemetry)


class CompareTelemetryTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)

    def write(self, name, rows, header="tick,speed,yaw,pos_x,pos_y,pos_z"):
        path = self.root / name
        path.write_text(header + "\n" + "\n".join(rows) + "\n", encoding="utf-8")
        return path

    def read(self, path, **kwargs):
        return telemetry.read_trace(path, telemetry.column_mapping([]), **kwargs)

    def run_cli(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = telemetry.main([str(arg) for arg in args])
        return result, stdout.getvalue(), stderr.getvalue()

    def test_identical_samples_are_unscored_and_do_not_establish_fidelity(self):
        path = self.write("a.csv", ["0,10,0,1,2,3", "1,20,1,3,4,5"])
        code, output, _ = self.run_cli(path, path, "--reference-kind", "synthetic")
        report = json.loads(output)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "unscored")
        self.assertEqual(report["metrics"]["position_distance"]["max_abs"], 0)
        self.assertFalse(report["original_game_fidelity_established"])

    def test_known_position_speed_errors_and_wrapped_yaw(self):
        a = self.read(self.write("a.csv", [f"0,10,{math.pi - 0.05},0,0,0"]))
        b = self.read(self.write("b.csv", [f"0,12,{-math.pi + 0.05},3,4,0"]))
        report = telemetry.compare(a, b)
        self.assertAlmostEqual(report["metrics"]["yaw"]["max_abs"], 0.1)
        self.assertEqual(report["metrics"]["position_distance"]["rmse"], 5)
        self.assertEqual(report["metrics"]["speed"]["mean_signed"], 2)

    def test_explicit_column_units_and_epoch_mapping(self):
        a = self.write("a.csv", [f"0,10,{math.pi},1,2,3"])
        b = self.write("b.csv", ["100,36,180,100,200,300"], "frame,kmh,heading,x,y,z")
        code, output, _ = self.run_cli(a, b, "--candidate-map", "tick=frame",
            "--candidate-map", "speed=kmh", "--candidate-map", "yaw=heading",
            "--candidate-map", "pos_x=x", "--candidate-map", "pos_y=y", "--candidate-map", "pos_z=z",
            "--candidate-speed-unit", "kmh", "--candidate-yaw-unit", "deg",
            "--candidate-position-scale", "0.01", "--candidate-tick-offset", "-100",
            "--max-speed-error", "0", "--max-yaw-error", "0", "--max-position-error", "0")
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(output)["status"], "passed")

    def test_threshold_failure_has_nonzero_exit(self):
        a = self.write("a.csv", ["0,10,0,0,0,0"])
        b = self.write("b.csv", ["0,12,0,0,0,0"])
        code, output, _ = self.run_cli(a, b, "--max-speed-error", "1.9")
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(output)["status"], "failed")

    def test_tick_coverage_defaults_to_error_and_partial_is_disclosed(self):
        a = self.read(self.write("a.csv", ["0,10,0,0,0,0", "1,10,0,0,0,0"]))
        b = self.read(self.write("b.csv", ["1,10,0,0,0,0"]))
        with self.assertRaises(telemetry.TraceError):
            telemetry.compare(a, b)
        coverage = telemetry.compare(a, b, allow_partial=True)["coverage"]
        self.assertEqual(coverage["reference_fraction"], 0.5)
        self.assertEqual(coverage["missing_candidate_ticks"], [0])
        self.assertFalse(coverage["complete"])

    def test_no_overlap_is_always_an_error(self):
        a = self.read(self.write("a.csv", ["0,10,0,0,0,0"]))
        b = self.read(self.write("b.csv", ["1,10,0,0,0,0"]))
        with self.assertRaises(telemetry.TraceError):
            telemetry.compare(a, b, allow_partial=True)

    def test_duplicate_nonmonotonic_and_fractional_ticks_are_rejected(self):
        for ticks in ([0, 0], [1, 0], ["0.5", 1]):
            with self.subTest(ticks=ticks), self.assertRaises(telemetry.TraceError):
                self.read(self.write("bad.csv", [f"{tick},0,0,0,0,0" for tick in ticks]))

    def test_missing_empty_and_nonfinite_values_are_rejected(self):
        for rows in ([], ["0,nan,0,0,0,0"], ["0,0,inf,0,0,0"], ["0,0,0,0,0"]):
            with self.subTest(rows=rows), self.assertRaises(telemetry.TraceError):
                self.read(self.write("bad.csv", rows))
        with self.assertRaises(telemetry.TraceError):
            self.read(self.write("header.csv", ["0,1"], "tick,speed"))

    def test_duplicate_mapped_columns_rejected(self):
        for values in (["speed=tick"], ["speed=v", "speed=w"], ["bogus=v"]):
            with self.subTest(values=values), self.assertRaises(telemetry.TraceError):
                telemetry.column_mapping(values)

    def test_large_integer_ticks_are_not_rounded(self):
        first = 2 ** 53
        trace = self.read(self.write("a.csv", [f"{first},0,0,0,0,0", f"{first + 1},0,0,0,0,0"]))
        self.assertEqual(list(trace), [first, first + 1])

    def test_report_written_and_bad_input_exit_code(self):
        path = self.write("a.csv", ["0,0,0,0,0,0"])
        destination = self.root / "reports" / "comparison.json"
        code, output, _ = self.run_cli(path, path, "--output", destination)
        self.assertEqual(code, 0)
        self.assertEqual(destination.read_text(encoding="utf-8"), output)
        code, _, error = self.run_cli(path, self.root / "missing.csv")
        self.assertEqual(code, 2)
        self.assertIn("Cannot read", error)


if __name__ == "__main__":
    unittest.main()
