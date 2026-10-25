"""Embed the versioned composition guide in native builds and portable releases."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
text = source.read_text(encoding="utf-8")
assert ')GUIDE"' not in text
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text('#pragma once\nstatic const char kSceneCompositionV1[] = R"GUIDE(' + text + ')GUIDE";\n', encoding="utf-8")
