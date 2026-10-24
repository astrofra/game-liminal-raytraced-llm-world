"""Unit tests for the camera geometry audit in scene_benchmark_quality."""

from __future__ import annotations

import unittest

try:
    from .scene_benchmark_quality import audit_camera
except ImportError:  # Direct execution: python scripts/test_scene_benchmark_quality.py
    from scene_benchmark_quality import audit_camera


def camera(
    eye: str = "(0.0,1.75,-1.0)",
    target: str = "(0.0,1.75,1.0)",
    up: str = "(0.0,1.0,0.0)",
    fov: str = "46.0",
) -> str:
    return f"camera eye{eye} target{target} up{up} fov({fov})"


class CameraAuditTests(unittest.TestCase):
    def assert_anomalies(self, result: list[str]) -> list[str]:
        self.assertIsInstance(result, list)
        self.assertGreater(len(result), 0, "expected at least one diagnostic")
        for message in result:
            self.assertIsInstance(message, str)
            self.assertTrue(message.strip())
        return result

    def assert_no_anomalies(self, result: list[str]) -> None:
        self.assertEqual(result, [])

    def test_normal_camera_is_accepted(self) -> None:
        scene = 'room "camera audit"\n' + camera()
        self.assert_no_anomalies(audit_camera(scene))

    def test_leading_whitespace_and_tabs_are_accepted(self) -> None:
        scene = "\t  " + camera()
        self.assert_no_anomalies(audit_camera(scene))

    def test_no_camera_is_reported(self) -> None:
        scene = 'room "empty"\nplane "floor" pos(0,0,0) normal(0,1,0) size(8,18) gray(0.1)'
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("camera" in message.lower() for message in messages))

    def test_empty_scene_has_no_camera(self) -> None:
        self.assert_anomalies(audit_camera(""))

    def test_multiple_cameras_are_reported(self) -> None:
        scene = camera() + "\n" + camera(fov="50.0")
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("multiple" in message.lower() for message in messages))

    def test_commented_camera_lines_are_ignored(self) -> None:
        scene = "\n".join(
            [
                "# camera eye(0.0,0.0,0.0) target(0.0,1.0,0.0) up(0.0,0.0,0.0) fov(10.0)",
                "   # camera eye(0.0,0.0,0.0) target(0.0,1.0,0.0) up(0.0,0.0,0.0) fov(10.0)",
                camera(),
            ]
        )
        self.assert_no_anomalies(audit_camera(scene))

    def test_commented_only_camera_is_absent(self) -> None:
        scene = "# camera eye(0.0,1.75,0.0) target(0.0,1.0,0.0) up(0.0,1.0,0.0) fov(46.0)"
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("no camera" in message.lower() for message in messages))

    def test_inline_comment_is_ignored(self) -> None:
        self.assert_no_anomalies(audit_camera(camera() + "  # trailing note"))

    def test_camera_word_inside_room_name_is_not_a_camera(self) -> None:
        scene = 'room "camera test"\ncameraman eye(0,0,0)\n' + camera()
        self.assert_no_anomalies(audit_camera(scene))

    def test_degenerate_parallel_example_is_reported(self) -> None:
        scene = "camera eye(0,1.75,0) target(0,1.18,0) up(0,1,0) fov(46)"
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("parallel" in message.lower() for message in messages))

    def test_eye_equal_to_target_is_reported(self) -> None:
        scene = camera(eye="(0.5,1.75,2.0)", target="(0.5,1.75,2.0)")
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("coincide" in message.lower() for message in messages))

    def test_null_up_vector_is_reported(self) -> None:
        scene = camera(up="(0.0,0.0,0.0)")
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("up vector is null" in message.lower() for message in messages))

    def test_nearly_parallel_axes_are_rejected(self) -> None:
        scene = camera(
            eye="(0.0,1.75,-5.0)",
            target="(0.0,1.75,5.0)",
            up="(1e-5,0.0,1.0)",
        )
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("parallel" in message.lower() for message in messages))

    def test_parallel_threshold_boundary_is_rejected(self) -> None:
        scene = camera(
            eye="(0.0,1.75,-5.0)",
            target="(0.0,1.75,5.0)",
            up="(1e-4,0.0,1.0)",
        )
        self.assert_anomalies(audit_camera(scene))

    def test_just_above_parallel_threshold_is_accepted(self) -> None:
        scene = camera(
            eye="(0.0,1.75,-5.0)",
            target="(0.0,1.75,5.0)",
            up="(1e-3,0.0,1.0)",
        )
        self.assert_no_anomalies(audit_camera(scene))

    def test_vector_lengths_do_not_matter(self) -> None:
        cases = [
            camera(eye="(0.0,1.75,-0.5)", target="(0.0,1.75,0.5)"),
            camera(eye="(0.0,1.75,-5000.0)", target="(0.0,1.75,5000.0)", up="(0.0,25.0,0.0)"),
            camera(up="(0.0,1e-6,0.0)"),
            camera(eye="(0.0,1.75,-1.0)", target="(0.0,1.75,1.0)", up="(0.0,1e-6,0.0)"),
        ]
        for scene in cases:
            with self.subTest(scene=scene):
                self.assert_no_anomalies(audit_camera(scene))

    def test_missing_optional_up_and_fov_use_engine_defaults(self) -> None:
        scene = "camera eye(0.0,1.75,-1.0) target(0.0,1.75,1.0)"
        self.assert_no_anomalies(audit_camera(scene))

    def test_implicit_up_still_rejects_vertical_view(self) -> None:
        self.assert_anomalies(audit_camera("camera eye(0,2,0) target(0,0,0)"))

    def test_missing_eye_is_reported(self) -> None:
        scene = "camera target(0.0,1.75,1.0) up(0.0,1.0,0.0) fov(46.0)"
        messages = self.assert_anomalies(audit_camera(scene))
        self.assertTrue(any("missing" in message.lower() and "eye" in message.lower() for message in messages))

    def test_scientific_notation_is_accepted(self) -> None:
        scene = (
            "camera eye(0e0,1.75e+0,-7.4E0) "
            "target(4.0e-1,1.3e0,4.0e0) "
            "up(0.0,1e0,0.0) fov(5.0e1)"
        )
        self.assert_no_anomalies(audit_camera(scene))

    def test_whitespace_inside_properties_is_accepted(self) -> None:
        scene = "camera eye ( 0.0 , 1.75 , -1.0 ) target ( 0.0 , 1.75 , 1.0 ) up ( 0.0 , 1.0 , 0.0 ) fov ( 46 )"
        self.assert_no_anomalies(audit_camera(scene))

    def test_invalid_fov_values_are_reported(self) -> None:
        for fov in ("0", "0.0", "-46", "180", "180.0", "181", "1e3"):
            with self.subTest(fov=fov):
                self.assert_anomalies(audit_camera(camera(fov=fov)))

    def test_fov_near_bounds_is_accepted(self) -> None:
        for fov in ("0.001", "179.999"):
            with self.subTest(fov=fov):
                self.assert_no_anomalies(audit_camera(camera(fov=fov)))

    def test_non_finite_values_are_reported(self) -> None:
        scenes = [
            camera(eye="(nan,1.75,-1.0)"),
            camera(target="(inf,1.75,1.0)"),
            camera(up="(0.0,1e999,0.0)"),
            camera(eye="(-Infinity,0.0,0.0)"),
            camera(fov="inf"),
            camera(fov="1e999"),
        ]
        for scene in scenes:
            with self.subTest(scene=scene):
                messages = self.assert_anomalies(audit_camera(scene))
                self.assertTrue(any("finite" in message.lower() for message in messages))

    def test_malformed_properties_are_reported(self) -> None:
        scenes = [
            "camera eye(0,1,2 target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(0,1) target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(0,1,2,3) target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(0,1,abc) target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(0 1 2) target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(1_000,1,2) target(0,1,0) up(0,1,0) fov(46)",
            "camera eye(0,1,2) target(0,1,0) up(0,1,0) fov()",
            "camera eye(0,1,2) target(0,1,0) up(0,1,0) fov(46,50)",
            "camera eye(0,1,2) target(0,1,0) up(0,1,0) fov(46) wobble(1)",
            "camera eye(0,1,2) eye(0,1,3) target(0,1,0) up(0,1,0) fov(46)",
        ]
        for scene in scenes:
            with self.subTest(scene=scene):
                self.assert_anomalies(audit_camera(scene))

    def test_empty_camera_directive_is_reported(self) -> None:
        self.assert_anomalies(audit_camera("camera"))

    def test_unexpected_input_never_raises(self) -> None:
        for scene in (None, 42, object(), "camera ((((", "\x00\x01", "camera eye("):
            with self.subTest(scene=repr(scene)):
                self.assert_anomalies(audit_camera(scene))

    def test_huge_values_return_a_diagnostic(self) -> None:
        scene = camera(
            eye="(1e308,1e308,1e308)",
            target="(-1e308,-1e308,-1e308)",
            up="(0.0,1.0,0.0)",
        )
        self.assert_anomalies(audit_camera(scene))


if __name__ == "__main__":
    unittest.main()
