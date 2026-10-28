"""Embed the native MLX integrity manifest, independently of runtime resources."""
import json
from pathlib import Path
import sys

manifest = json.loads(Path(sys.argv[1]).read_text())
for name in manifest['files']:
    if Path(name).name != name or name in ('.', '..') or '\\' in name:
        raise ValueError('Manifest paths must be bare file names')
text = json.dumps(manifest, separators=(',', ':'))
assert ')manifest"' not in text
out = Path(sys.argv[2])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text('#pragma once\ninline constexpr char kMlxCppManifest[] = R"manifest(' + text + ')manifest";\n')
