"""Camera geometry audit for Scene Format V1 scenes.

Only the ``camera`` directive is inspected. A clean result means that the
camera declares a usable orientation; it says nothing about lighting,
framing, content or overall image quality.
"""

from __future__ import annotations

import math
import re

_PARALLEL_EPSILON = 1e-4
_FOV_MIN_EXCLUSIVE = 0.0
_FOV_MAX_EXCLUSIVE = 180.0

_CAMERA_RE = re.compile(r"^\s*camera\b(.*)$", re.IGNORECASE)
_PROPERTY_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(([^()]*)\)")
_NUMBER_RE = re.compile(
    r"^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$"
)
_NON_FINITE_RE = re.compile(r"^[+-]?(?:nan|inf(?:inity)?)$", re.IGNORECASE)

_VECTOR_PROPERTIES = ("eye", "target", "up")
_SCALAR_PROPERTIES = ("fov",)
_ALLOWED_PROPERTIES = frozenset(_VECTOR_PROPERTIES + _SCALAR_PROPERTIES)


def _shorten(text: str, limit: int = 32) -> str:
    compact = " ".join(text.split())
    if len(compact) <= limit:
        return compact
    return compact[: limit - 3] + "..."


def _camera_remainders(scene_text: str) -> list[str]:
    remainders = []
    for line in scene_text.splitlines():
        code = line.split("#", 1)[0]
        match = _CAMERA_RE.match(code)
        if match:
            remainders.append(match.group(1))
    return remainders


def _parse_property(
    name: str, raw: str, expected_count: int
) -> tuple[list[float] | None, str | None]:
    parts = [part.strip() for part in raw.split(",")]
    if len(parts) != expected_count or any(not part for part in parts):
        plural = "" if expected_count == 1 else "s"
        return None, (
            f"malformed camera property '{name}': "
            f"expected {expected_count} number{plural}"
        )

    values = []
    for part in parts:
        if _NON_FINITE_RE.match(part):
            return None, f"camera property '{name}' contains a non-finite value"
        if not _NUMBER_RE.match(part):
            return None, (
                f"malformed camera property '{name}': "
                f"'{_shorten(part, 24)}' is not a number"
            )
        value = float(part)
        if not math.isfinite(value):
            return None, f"camera property '{name}' contains a non-finite value"
        values.append(value)
    return values, None


def _parse_directive(remainder: str) -> tuple[dict[str, str], list[str]]:
    properties: dict[str, str] = {}
    issues: list[str] = []
    cursor = 0

    for match in _PROPERTY_RE.finditer(remainder):
        gap = remainder[cursor : match.start()]
        if gap.strip():
            issues.append(
                "malformed camera directive near "
                f"'{_shorten(gap.strip())}'"
            )
        name = match.group(1)
        raw = match.group(2)
        if name in properties:
            issues.append(f"duplicate camera property '{name}'")
        else:
            properties[name] = raw
        cursor = match.end()

    tail = remainder[cursor:]
    if tail.strip():
        issues.append(
            f"malformed camera directive near '{_shorten(tail.strip())}'"
        )

    for name in properties:
        if name not in _ALLOWED_PROPERTIES:
            issues.append(f"unknown camera property '{name}'")

    return properties, issues


def _audit_camera(scene_text: str) -> list[str]:
    if not isinstance(scene_text, str):
        return ["scene text must be a string"]

    remainders = _camera_remainders(scene_text)
    if not remainders:
        return ["no camera directive found"]
    if len(remainders) > 1:
        return [
            f"multiple camera directives found ({len(remainders)}); "
            "exactly one is required"
        ]

    remainder = remainders[0]
    if not remainder.strip():
        return ["camera directive has no properties"]

    properties, issues = _parse_directive(remainder)
    if issues:
        return issues

    # Scene Format V1 keeps Camera's defaults when these properties are omitted.
    properties.setdefault("up", "0,1,0")
    properties.setdefault("fov", "40")
    values: dict[str, list[float]] = {}
    for name in _VECTOR_PROPERTIES:
        if name not in properties:
            issues.append(f"missing camera property '{name}'")
            continue
        parsed, error = _parse_property(name, properties[name], 3)
        if error is not None:
            issues.append(error)
        else:
            values[name] = parsed

    fov: float | None = None
    if "fov" not in properties:
        issues.append("missing camera property 'fov'")
    else:
        parsed, error = _parse_property("fov", properties["fov"], 1)
        if error is not None:
            issues.append(error)
        else:
            fov = parsed[0]

    if issues:
        return issues

    if not (_FOV_MIN_EXCLUSIVE < fov < _FOV_MAX_EXCLUSIVE):
        issues.append(
            "camera fov must be strictly between 0 and 180 degrees "
            f"(got {format(fov, '.12g')})"
        )

    eye = values["eye"]
    target = values["target"]
    up = values["up"]
    direction = tuple(target[i] - eye[i] for i in range(3))

    if not all(math.isfinite(component) for component in direction):
        issues.append("camera eye-to-target vector is out of numeric range")
        return issues

    view_length = math.hypot(*direction)
    up_length = math.hypot(*up)

    if not math.isfinite(view_length):
        issues.append("camera eye-to-target vector is out of numeric range")
    elif view_length == 0.0:
        issues.append("camera eye and target coincide (null view direction)")

    if not math.isfinite(up_length):
        issues.append("camera up vector is out of numeric range")
    elif up_length == 0.0:
        issues.append("camera up vector is null")

    if (
        math.isfinite(view_length)
        and math.isfinite(up_length)
        and view_length > 0.0
        and up_length > 0.0
    ):
        view_x = direction[0] / view_length
        view_y = direction[1] / view_length
        view_z = direction[2] / view_length
        up_x = up[0] / up_length
        up_y = up[1] / up_length
        up_z = up[2] / up_length

        cross_x = view_y * up_z - view_z * up_y
        cross_y = view_z * up_x - view_x * up_z
        cross_z = view_x * up_y - view_y * up_x
        sine = math.hypot(cross_x, cross_y, cross_z)

        if not math.isfinite(sine):
            issues.append("camera orientation could not be evaluated")
        elif sine <= _PARALLEL_EPSILON:
            issues.append(
                "camera view direction is parallel to its up vector "
                f"(normalized cross product {format(sine, '.3g')} <= 1e-4)"
            )

    return issues


def audit_camera(scene_text: str) -> list[str]:
    """Return camera geometry anomalies found in a Scene Format V1 scene.

    An empty list means the scene declares exactly one camera with finite,
    non-degenerate orientation values. Any returned string is a short English
    diagnostic. Unexpected input never raises: it yields a diagnostic instead.
    """

    try:
        return _audit_camera(scene_text)
    except Exception as error:  # Defensive: an audit must never raise.
        return [f"camera audit could not evaluate the scene ({type(error).__name__})"]


__all__ = ["audit_camera"]
