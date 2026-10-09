#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Bounded debugfs sampling; no vendor ioctls, input capture or disk workload."""

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import re
import sys
import time

DEBUGFS = Path('/sys/kernel/debug/cu0019')
HID_DEVICE = re.compile(r'0003:046D:C542\.[0-9A-F]{4,}')


def parse_fields(content):
    result = {}
    for line in content.splitlines():
        key, separator, value = line.partition('=')
        if not separator or not key or key in result:
            raise ValueError('Malformed or duplicate debugfs field')
        result[key] = value
    return result


def normalize_radio(content):
    fields = parse_fields(content)
    status = fields.get('status')
    if not status:
        raise ValueError('Missing radio status')
    result = {'status': status}
    if status != 'ok':
        return result
    raw = int(fields['rssi_raw'])
    frequency = int(fields['frequency_mhz_candidate'])
    if not 0 <= raw <= 255 or not 2400 <= frequency <= 2484:
        raise ValueError('Out-of-range radio sample')
    result.update(rssi_raw=raw, frequency_mhz_candidate=frequency)
    return result


def discover_receiver(root=DEBUGFS):
    receivers = [p for p in root.iterdir()
                 if HID_DEVICE.fullmatch(p.name) and (p / 'identity').is_file()]
    if len(receivers) != 1:
        raise RuntimeError('Expected exactly one C-U0019 debugfs receiver')
    identity = parse_fields((receivers[0] / 'identity').read_text())
    if identity.get('usb_id') != '046d:c542':
        raise RuntimeError('Unexpected USB identity')
    if identity.get('bcd_device') != '0x0302':
        raise RuntimeError('This capture is restricted to bcdDevice 0302')
    if identity.get('reads_enabled') != '1':
        raise RuntimeError('Module reads are disabled; reload only after opting in')
    return receivers[0]


@contextmanager
def hold_awake(receiver, enabled):
    if not enabled:
        yield
        return
    sysfs = Path('/sys/bus/hid/devices') / receiver.name
    raw_nodes = list((sysfs / 'hidraw').glob('hidraw*'))
    if len(raw_nodes) != 1:
        raise RuntimeError('Expected exactly one matching hidraw node')
    name = raw_nodes[0].name
    if (Path('/sys/class/hidraw') / name / 'device').resolve() != sysfs.resolve():
        raise RuntimeError('hidraw device mapping changed')
    descriptor = os.open('/dev/' + name,
                         os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
    try:
        # Advisory only: this is not kernel feature-channel arbitration.
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        os.close(descriptor)


def make_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--condition', required=True,
                        help='Controlled condition label, e.g. near_usb3_idle')
    parser.add_argument('--count', type=int, default=180)
    parser.add_argument('--interval', type=float, default=1.1)
    parser.add_argument('--keep-awake', action='store_true',
                        help='Temporarily open matching hidraw; no reads/ioctls')
    return parser


def main(argv=None):
    parser = make_parser()
    args = parser.parse_args(argv)
    if not 1 <= args.count <= 3600 or not 1.0 <= args.interval <= 60.0:
        parser.error('count must be 1..3600 and interval must be 1..60 seconds')
    if os.geteuid() != 0:
        parser.error('Run as root to read the 0400 debugfs files')
    receiver = discover_receiver()
    with hold_awake(receiver, args.keep_awake):
        # Let a previous inactive cached sample expire after a normal HID open.
        if args.keep_awake:
            time.sleep(1.05)
        for index in range(args.count):
            started = time.monotonic()
            content = (receiver / 'radio').read_text()
            record = {
                'condition': args.condition,
                'index': index,
                'hid_device': receiver.name,
                'polled_utc': datetime.now(timezone.utc).isoformat(),
                'poll_started_monotonic_ns': int(started * 1_000_000_000),
                'poll_duration_ms': (time.monotonic() - started) * 1000,
                'radio': normalize_radio(content),
            }
            print(json.dumps(record, sort_keys=True), flush=True)
            if index + 1 < args.count:
                time.sleep(max(0, args.interval - (time.monotonic() - started)))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print('Capture stopped: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
