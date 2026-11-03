#!/usr/bin/env python3
"""Reconstruct native fixed-note observations from raw MIDI and frozen truth.
SPDX-License-Identifier: AGPL-3.0-or-later
This audit does not execute or tune the detector. Continuous-bend rows remain
legacy fixed-note diagnostics; the mandatory C++ test evaluates real bend mode.
"""
import argparse
from collections import defaultdict, deque
import csv
import hashlib
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def nearest(value):
    return math.floor(value + .5)


def quantile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    return values[math.ceil(fraction * (len(values) - 1))]


def audit(report, fixtures=HERE):
    fixture = json.loads((fixtures / 'fixtures.json').read_text())
    cases = {c['name']: c for c in fixture['cases']}
    records = {r['file']: r for r in fixture['records']}
    rows = list(csv.DictReader((report / 'audio-cases.csv').open()))
    events = defaultdict(list)
    for event in csv.DictReader((report / 'audio-events.csv').open()):
        events[(event['case'], int(event['rate']), int(event['block']))].append(event)
    configs = {(int(r['rate']), int(r['block'])) for r in rows}
    expected_keys = {(name, rate, block) for name in cases for rate, block in configs}
    keys = [(r['case'], int(r['rate']), int(r['block'])) for r in rows]
    if not configs or len(keys) != len(set(keys)) or set(keys) != expected_keys:
        raise ValueError('Incomplete or duplicate native report coverage')
    if set(events) - expected_keys:
        raise ValueError('Unexpected raw MIDI case/configuration')
    differences, direction_counts, reference_rows = [], defaultdict(int), []
    metrics = ('expected', 'matched', 'missed', 'false_note_ons', 'octave_errors',
               'wrong_before_correct', 'early_offs', 'hung')
    for row in rows:
        key = row['case'], int(row['rate']), int(row['block'])
        name, rate, _ = key
        record = records[cases[name]['file']]
        base_rate = record.get('sample_rate', fixture.get('sample_rate'))
        if not isinstance(base_rate, int) or base_rate <= 0:
            raise ValueError('Invalid fixture sample rate')
        truth = [dict(note=t['note'], on=nearest(t['on_sample'] * rate / base_rate),
                      off=nearest(t['off_sample'] * rate / base_rate)) for t in cases[name]['truth']]
        end = nearest(record['frames'] * rate / base_rate)
        held, intervals = defaultdict(deque), []
        previous = -1
        for event in events[key]:
            sample, note = int(event['sample']), int(event['note'])
            if sample < previous or not 0 <= sample < end or not 0 <= note <= 127:
                raise ValueError('Invalid or unordered raw MIDI event')
            previous = sample
            if int(event['on']):
                intervals.append(dict(note=note, on=sample, off=end, used=False))
                held[note].append(len(intervals) - 1)
            elif held[note]:
                intervals[held[note].popleft()]['off'] = sample
        chosen, latencies, releases = [], [], []
        early = 0
        for t in truth:
            selected = None
            useful = min(nearest(.020 * rate), t['off'] - t['on'])
            for interval in intervals:
                overlap = min(t['off'], interval['off']) - max(t['on'], interval['on'])
                if not interval['used'] and interval['note'] == t['note'] and t['on'] - 2 <= interval['on'] <= t['off'] + 2 and overlap >= useful:
                    selected = interval
                    interval['used'] = True
                    break
            chosen.append(selected)
            reference_rows.append(dict(case=name, family=row['family'], mode=row['mode'], rate=rate,
                                       block=key[2], note=t['note'], truth_on_sample=t['on'], truth_off_sample=t['off'],
                                       matched=selected is not None,
                                       detected_on_sample=selected['on'] if selected is not None else '',
                                       detected_off_sample=selected['off'] if selected is not None else '',
                                       first_correct_ms=1000 * (selected['on'] - t['on']) / rate if selected is not None else '',
                                       release_after_source_ms=1000 * (selected['off'] - t['off']) / rate if selected is not None else ''))
            if selected is not None:
                latencies.append(1000 * (selected['on'] - t['on']) / rate)
                if selected['off'] < t['off'] - int(.010 * rate):
                    early += 1
                else:
                    releases.append(1000 * (selected['off'] - t['off']) / rate)
        false = [i for i in intervals if not i['used']]
        octaves = 0
        for interval in false:
            active = [t for t in truth if t['on'] <= interval['on'] <= t['off']]
            diffs = [interval['note'] - t['note'] for t in active]
            if any(abs(value) in (12, 24, 36) for value in diffs):
                octaves += 1
                direction_counts['up' if any(value in (12, 24, 36) for value in diffs) else 'down'] += 1
        wrong = 0
        for t, selected in zip(truth, chosen):
            correct = selected['on'] if selected is not None else t['off']
            for interval in intervals:
                if t['on'] <= interval['on'] < correct and not any(other['note'] == interval['note'] and other['on'] - 2 <= interval['on'] <= other['off'] + 2 for other in truth):
                    wrong += 1
                    break
        calculated = dict(expected=len(truth), matched=sum(i is not None for i in chosen),
                          missed=sum(i is None for i in chosen), false_note_ons=len(false),
                          octave_errors=octaves, wrong_before_correct=wrong, early_offs=early,
                          hung=sum(len(q) for q in held.values()))
        for metric in metrics:
            if calculated[metric] != int(row[metric]):
                differences.append(dict(case=name, rate=rate, field=metric, expected=calculated[metric], reported=row[metric]))
        for metric, values, q in (('latency_median_ms', latencies, .5), ('latency_p95_ms', latencies, .95),
                                  ('latency_p99_ms', latencies, .99), ('latency_max_ms', latencies, 1),
                                  ('release_median_ms', releases, .5), ('release_p95_ms', releases, .95)):
            value = quantile(values, q)
            native = float(row[metric]) if row[metric] else None
            if (value is None) != (native is None) or value is not None and abs(value - native) > 1e-6:
                differences.append(dict(case=name, rate=rate, field=metric, expected=value, reported=native))
    block_differences = []
    for name in cases:
        for rate in {r for r, _ in configs}:
            signatures = []
            for _, block in sorted(config for config in configs if config[0] == rate):
                signatures.append([(e['on'], e['note'], e['velocity'], e['sample']) for e in events[(name, rate, block)]])
            if signatures[1:] != signatures[:-1]:
                block_differences.append(dict(case=name, rate=rate))
    result = dict(audit='Independent Python reconstruction from raw MIDI and sealed guitar annotations',
                  rows=len(rows), configurations=len(configs), case_count=len(cases),
                  fixture_manifest_sha256=digest(fixtures / 'fixtures.json'),
                  report_files_sha256={name: digest(report / name) for name in ('audio-cases.csv', 'audio-events.csv', 'audio-summary.txt')},
                  scoring_differences=differences, block_event_invariance_differences=block_differences,
                  false_octave_direction_counts=dict(direction_counts),
                  source_overflow_and_cpu_metrics_not_reconstructed=True,
                  continuous_bend_rows_are_fixed_note_diagnostics=any(c['family'] == 'guitar-continuous-bend' for c in cases.values()))
    with (report / 'per-reference.csv').open('w', newline='') as output:
        fields = ('case', 'family', 'mode', 'rate', 'block', 'note', 'truth_on_sample', 'truth_off_sample',
                  'matched', 'detected_on_sample', 'detected_off_sample', 'first_correct_ms', 'release_after_source_ms')
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(reference_rows)
    (report / 'scoring-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    if differences or block_differences:
        raise SystemExit(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--fixtures', type=Path, default=HERE)
    args = parser.parse_args()
    audit(args.report, args.fixtures)
