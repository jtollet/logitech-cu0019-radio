# SPDX-License-Identifier: GPL-2.0-only
"""Verify that the browsable snapshots are the exact kernel-patch sources."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOTS = (
    'drivers/hid/hid-logitech-cu0019-debug.c',
    'drivers/hid/hid-logitech-cu0019.h',
    'drivers/hid/hid-logitech-cu0019-test.c',
    'Documentation/hid/hid-logitech-cu0019.rst',
)


def added_files(patch):
    """Extract wholly new files from these git-format-patch review drafts."""
    result = {}
    new_file = False
    name = None
    for line in patch.splitlines():
        if line.startswith('diff --git '):
            new_file = False
            name = None
        elif line == '--- /dev/null':
            new_file = True
        elif line.startswith('+++ b/') and new_file:
            name = line[len('+++ b/'):]
            if name in result:
                raise ValueError('Duplicate added path: ' + name)
            result[name] = []
        elif name is not None and line.startswith('+'):
            result[name].append(line[1:])
    return {name: '\n'.join(lines) + '\n'
            for name, lines in result.items()}


class PackagingTests(unittest.TestCase):
    def test_snapshots_equal_patch_sources(self):
        expected = {}
        for patch in sorted((ROOT / 'patches').glob('000[12]-*.patch')):
            additions = added_files(patch.read_text(encoding='utf-8'))
            self.assertFalse(expected.keys() & additions.keys())
            expected.update(additions)
        self.assertEqual(set(expected), set(SNAPSHOTS))
        for name in SNAPSHOTS:
            with self.subTest(path=name):
                self.assertEqual((ROOT / name).read_text(encoding='utf-8'),
                                 expected[name])


if __name__ == '__main__':
    unittest.main()
