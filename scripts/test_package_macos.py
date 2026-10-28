"""Bundle closure failures must be detected before publishing; no macOS tools needed."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import package_macos as bundle


class BundleClosureTests(unittest.TestCase):
    def test_rpath_resolves_relative_to_loader_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix='bundle space ') as directory:
            root = Path(directory)
            library = root / 'lib Metal.dylib'
            library.touch()
            self.assertEqual(bundle.resolve_dependency('@rpath/lib Metal.dylib',
                             root / 'loader', root / 'app', ['@loader_path']), library.resolve())

    def test_unresolved_dependency_is_fatal(self):
        with self.assertRaisesRegex(RuntimeError, 'Unresolved'):
            bundle.resolve_dependency('@rpath/missing.dylib', Path('/loader'), Path('/app'), [])

    def check_audit(self, dependency, architecture='arm64', paths=()):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(bundle, 'output', return_value=architecture), \
                 patch.object(bundle, 'rpaths', return_value=list(paths)), \
                 patch.object(bundle, 'dependencies', return_value=[dependency]):
                bundle.audit_libraries(root / 'app', root / 'Frameworks')

    def test_homebrew_dependency_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'External dependency'):
            self.check_audit('/opt/homebrew/lib/libSDL3.dylib')

    def test_missing_bundled_library_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Bundled dependency missing'):
            self.check_audit('@executable_path/../Frameworks/missing.dylib')

    def test_intel_only_binary_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Missing arm64'):
            self.check_audit('/usr/lib/libSystem.B.dylib', architecture='x86_64')

    def test_external_search_path_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Unexpected rpath'):
            self.check_audit('/usr/lib/libSystem.B.dylib', paths=['/opt/homebrew/lib'])

    def test_system_framework_needs_no_copy(self):
        self.check_audit('/System/Library/Frameworks/Metal.framework/Versions/A/Metal')


if __name__ == '__main__':
    unittest.main()
