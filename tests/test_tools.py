# SPDX-License-Identifier: GPL-2.0-only
import importlib.util
from pathlib import Path
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1] / 'tools'


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


capture = load_tool('capture_radio')
summary = load_tool('summarize_radio')


class CaptureTests(unittest.TestCase):
    def test_raw_values_only(self):
        result = capture.normalize_radio(
            'status=ok\nrssi_raw=40\nfrequency_mhz_candidate=2455\n'
            'rssi_calibration_verified=false\n')
        self.assertEqual(result, {
            'status': 'ok', 'rssi_raw': 40, 'frequency_mhz_candidate': 2455})
        self.assertNotIn('rssi_dbm', result)

    def test_failed_sample_does_not_publish_old_values(self):
        self.assertEqual(capture.normalize_radio(
            'status=usb_not_active\nrssi_raw=40\nfrequency_mhz_candidate=2455\n'),
            {'status': 'usb_not_active'})

    def test_malformed_or_duplicate_fields(self):
        for content in ('status', '=ok', 'status=ok\nstatus=read_failed'):
            with self.subTest(content=content), self.assertRaises(ValueError):
                capture.parse_fields(content)

    def test_missing_and_out_of_range_values(self):
        for raw, frequency in ((-1, 2455), (256, 2455), (40, 2399), (40, 2485)):
            with self.subTest(raw=raw, frequency=frequency):
                with self.assertRaises(ValueError):
                    capture.normalize_radio(
                        f'status=ok\nrssi_raw={raw}\nfrequency_mhz_candidate={frequency}')
        with self.assertRaises(ValueError):
            capture.normalize_radio('rssi_raw=40')
        with self.assertRaises(KeyError):
            capture.normalize_radio('status=ok')

    def test_no_hidraw_open_without_opt_in(self):
        with mock.patch.object(capture.os, 'open') as opening:
            with capture.hold_awake(Path('/unused'), False):
                pass
            opening.assert_not_called()

    def test_count_and_interval_limits_precede_device_access(self):
        for options in (('--count', '0'), ('--count', '3601'),
                        ('--interval', '0.5'), ('--interval', '61')):
            with self.subTest(options=options):
                with mock.patch.object(capture, 'discover_receiver') as discovering:
                    with mock.patch('sys.stderr'), self.assertRaises(SystemExit):
                        capture.main(['--condition', 'test', *options])
                    discovering.assert_not_called()


class SummaryTests(unittest.TestCase):
    def test_groups_by_condition_and_frequency_and_keeps_invalid_count(self):
        records = [
            {'condition': 'near', 'radio': {
                'status': 'ok', 'rssi_raw': raw, 'frequency_mhz_candidate': frequency}}
            for raw, frequency in ((32, 2428), (40, 2428), (16, 2455))
        ]
        records += [{'condition': 'far', 'radio': {
            'status': 'ok', 'rssi_raw': 24, 'frequency_mhz_candidate': 2428}},
            {'condition': 'near', 'radio': {'status': 'usb_not_active'}}]
        result = summary.summarize(records)
        self.assertEqual(len(result['groups']), 3)
        near = next(row for row in result['groups']
                    if row['condition'] == 'near' and row['frequency_mhz_candidate'] == 2428)
        self.assertEqual(near['samples'], 2)
        self.assertEqual(near['rssi_raw_median'], 36)
        self.assertIn({'condition': 'near', 'status': 'usb_not_active', 'samples': 1},
                      result['statuses'])

    def test_invalid_values_rejected(self):
        for raw in (True, -1, 256, '40'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                summary.summarize([{'condition': 'test', 'radio': {
                    'status': 'ok', 'rssi_raw': raw, 'frequency_mhz_candidate': 2428}}])

    def test_empty_capture(self):
        result = summary.summarize([])
        self.assertEqual(result['groups'], [])
        self.assertEqual(result['statuses'], [])


if __name__ == '__main__':
    unittest.main()
