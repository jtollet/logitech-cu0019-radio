#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Summarize raw values by condition and frequency candidate, never in dBm."""

import argparse
from collections import Counter, defaultdict
import json
import statistics
import sys


def summarize(records):
    groups = defaultdict(list)
    statuses = Counter()
    for record in records:
        condition = record['condition']
        radio = record['radio']
        status = radio['status']
        statuses[(condition, status)] += 1
        if status != 'ok':
            continue
        raw = radio['rssi_raw']
        frequency = radio['frequency_mhz_candidate']
        if (type(raw) is not int or not 0 <= raw <= 255 or
                type(frequency) is not int or not 2400 <= frequency <= 2484):
            raise ValueError('Invalid raw RSSI/frequency candidate')
        groups[(condition, frequency)].append(raw)
    return {
        'groups': [
            {'condition': condition, 'frequency_mhz_candidate': frequency,
             'samples': len(values), 'rssi_raw_min': min(values),
             'rssi_raw_median': statistics.median(values),
             'rssi_raw_max': max(values)}
            for (condition, frequency), values in sorted(groups.items())
        ],
        'statuses': [
            {'condition': condition, 'status': status, 'samples': count}
            for (condition, status), count in sorted(statuses.items())
        ],
        'interpretation': (
            'Raw values only. PLL and RSSI are sequential reads; this grouping '
            'does not establish simultaneous channel identity, calibrated '
            'noise power, mouse signal strength, SNR or packet loss.'
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='*', help='JSONL captures; stdin if absent')
    args = parser.parse_args()
    records = []
    if args.files:
        for filename in args.files:
            with open(filename, encoding='utf-8') as capture:
                records.extend(json.loads(line) for line in capture if line.strip())
    else:
        records = [json.loads(line) for line in sys.stdin if line.strip()]
    print(json.dumps(summarize(records), indent=2, sort_keys=True))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print('Summary failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
