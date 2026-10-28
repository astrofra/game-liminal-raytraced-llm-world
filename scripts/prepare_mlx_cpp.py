"""Extract a verified vocabulary-only GGUF for native MLX; stdlib, build time only."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import struct
from download_gemma import validate_gguf
from model_profiles import resolve_model_path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = Path(__file__).with_name('models_mlx_cpp.json')


def extract_metadata(source, destination):
    """Keep GGUF v3 metadata verbatim; remove tensor descriptors and tensor data."""
    def read_exact(stream, size):
        data = stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated GGUF metadata')
        return data

    def string(stream):
        size, = struct.unpack('<Q', read_exact(stream, 8))
        if size > 64 * 1024 * 1024:
            raise ValueError('Oversized GGUF string')
        read_exact(stream, size)

    def value(stream, kind, nested=False):
        sizes = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
        if kind in sizes:
            read_exact(stream, sizes[kind])
        elif kind == 8:
            string(stream)
        elif kind == 9 and not nested:
            subtype, count = struct.unpack('<IQ', read_exact(stream, 12))
            if count > 1_000_000:
                raise ValueError('Oversized GGUF array')
            for _ in range(count):
                value(stream, subtype, nested=True)
        else:
            raise ValueError(f'Unsupported GGUF value type {kind}')

    with Path(source).open('rb') as stream:
        magic, version, _, count = struct.unpack('<4sIQQ', read_exact(stream, 24))
        if magic != b'GGUF' or version != 3 or count > 100_000:
            raise ValueError('Expected a bounded little-endian GGUF v3 header')
        for _ in range(count):
            string(stream)
            kind, = struct.unpack('<I', read_exact(stream, 4))
            value(stream, kind)
        end = stream.tell()
        stream.seek(0)
        data = bytearray(read_exact(stream, end))
    data[8:16] = struct.pack('<Q', 0)
    # The pinned source uses the default 32-byte alignment; the output hash is authoritative.
    data.extend(b'\0' * (-len(data) % 32))
    Path(destination).write_bytes(data)


def verified(path, spec):
    if not path.is_file() or path.stat().st_size != spec['size_bytes']:
        return False
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while chunk := stream.read(4 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest() == spec['sha256']


def main():
    manifest = json.loads(MANIFEST.read_text())
    target = ROOT / manifest['relative_path'] / 'tokenizer.gguf'
    spec = manifest['files']['tokenizer.gguf']
    if verified(target, spec):
        print('Native vocabulary verified:', target)
        return
    source = resolve_model_path()
    validate_gguf(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_suffix('.gguf.partial')
    try:
        extract_metadata(source, partial)
        if not verified(partial, spec):
            raise ValueError('Extracted vocabulary differs from pinned manifest')
        partial.replace(target)
    finally:
        partial.unlink(missing_ok=True)
    print('Native vocabulary ready:', target)


if __name__ == '__main__':
    main()
