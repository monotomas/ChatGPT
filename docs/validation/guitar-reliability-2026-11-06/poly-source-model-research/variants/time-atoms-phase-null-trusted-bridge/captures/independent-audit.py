from pathlib import Path
import collections
import csv
import hashlib
import itertools
import json
import statistics

variant = Path(__file__).resolve().parent
base = variant.parent
baseline = base / 'time-atoms-phase-null-guard'
capture = variant / 'chord-repicks-corrected-v2'
run_result = json.loads((capture / 'run-result.json').read_text())
compile_result = json.loads((capture / 'compile-result.json').read_text())
assert run_result['exit_code'] == compile_result['exit_code'] == 0

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

keys = list(itertools.product(range(4), ('0', '0.9', '2.1'), ('0.5', '1'), ('0', '1')))
datasets = {}
for name, folder in [('null_guard', baseline), ('trusted_bridge', variant)]:
    path = folder / 'chord-repicks-corrected-v2/events.csv'
    events = list(csv.DictReader(path.open()))
    groups = collections.defaultdict(list)
    for event in events:
        key = (int(event['wave']), event['phase'], event['new_gain'], event['dynamics'])
        assert key in keys and event['type'] in ('N', 'F')
        groups[key].append(event)
    assert len(groups) == 48
    totals = {kind: collections.Counter() for kind in range(4)}
    matches, extras = {}, []
    for key in keys:
        kind, phase, gain, dynamics = key
        notes = [45, 52, 57, 60, 64] if kind == 2 else [40, 45, 50, 55, 59, 64]
        ons = [event for event in groups[key] if event['type'] == 'N']
        used = set()
        totals[kind]['cases'] += 1
        for gesture in range(2 if kind in (1, 2) else 1):
            label = 'initial' if gesture == 0 else 'second'
            for index, note in enumerate(notes):
                onset = round((.08 + .32 * gesture + index * .007) * 48000)
                end = round((.4 + index * .007) * 48000) if gesture == 0 and kind in (1, 2) else onset + 28800
                choices = [(i, event) for i, event in enumerate(ons)
                           if i not in used and int(event['note']) == note and onset <= int(event['sample']) < end]
                totals[kind][label + '_expected'] += 1
                matched = None
                if choices:
                    i, event = choices[0]
                    used.add(i)
                    sample = int(event['sample'])
                    matched = {'sample': sample, 'expected_sample': onset, 'latency_ms': (sample - onset) / 48}
                    totals[kind][label + '_matched'] += 1
                else:
                    totals[kind][label + '_missed'] += 1
                matches[(key, gesture, note)] = matched
        case_extras = [event for i, event in enumerate(ons) if i not in used]
        extras.extend(case_extras)
        totals[kind]['extra_ons'] += len(case_extras)
        expected_case = len(notes) * (2 if kind in (1, 2) else 1)
        totals[kind]['onset_complete_cases'] += int(len(used) == expected_case and not case_extras)
    datasets[name] = {'path': str(path), 'sha256': sha(path), 'event_rows': len(events),
                      'totals': totals, 'matches': matches, 'extras': extras}

before, after = datasets['null_guard']['matches'], datasets['trusted_bridge']['matches']
gained, lost, timing_changed, timing_rows = [], [], [], []
common = collections.Counter()
for key, gesture, note in before:
    old, new = before[(key, gesture, note)], after[(key, gesture, note)]
    descriptor = {'kind': key[0], 'phase': key[1], 'new_gain': key[2], 'dynamics': key[3],
                  'gesture': gesture, 'note': note, 'null_guard': old, 'trusted_bridge': new}
    if old is None and new is not None:
        gained.append(descriptor)
    elif old is not None and new is None:
        lost.append(descriptor)
    elif old is not None:
        common['initial' if gesture == 0 else 'second'] += 1
        if old['sample'] != new['sample']:
            timing_changed.append(descriptor)
    expected = round((.08 + .32 * gesture + ([45, 52, 57, 60, 64] if key[0] == 2 else [40, 45, 50, 55, 59, 64]).index(note) * .007) * 48000)
    timing_rows.append({'kind': key[0], 'phase': key[1], 'new_gain': key[2], 'dynamics': key[3],
                        'gesture': gesture, 'note': note, 'expected_sample': expected,
                        'null_guard_sample': old['sample'] if old else '',
                        'trusted_bridge_sample': new['sample'] if new else '',
                        'null_guard_latency_ms': old['latency_ms'] if old else '',
                        'trusted_bridge_latency_ms': new['latency_ms'] if new else ''})

timing_rollup = []
for kind in range(4):
    notes = [45, 52, 57, 60, 64] if kind == 2 else [40, 45, 50, 55, 59, 64]
    for gesture in range(2 if kind in (1, 2) else 1):
        for note in notes:
            row = {'kind': kind, 'gesture': gesture, 'note': note, 'expected': 12}
            for name, dataset in datasets.items():
                latencies = [match['latency_ms'] for (key, g, n), match in dataset['matches'].items()
                             if key[0] == kind and g == gesture and n == note and match]
                row[name] = {'matched': len(latencies), 'latency_ms_min_median_max':
                             [min(latencies), statistics.median(latencies), max(latencies)] if latencies else None}
            timing_rollup.append(row)

source_hashes = {p.relative_to(variant).as_posix(): sha(p) for p in sorted((variant / 'Source').rglob('*')) if p.is_file()}
assert source_hashes == json.loads((variant / 'source-inventory.json').read_text())['sha256']
source_differences = [relative for relative, digest in source_hashes.items() if sha(baseline / relative) != digest]
assert source_differences == ['Source/Pitch/TimeDomainPursuit.cpp']
existing = json.loads((base / 'corrected-v2-chord-repick-analysis.json').read_text())['null_guard']['totals']
for kind, count in datasets['null_guard']['totals'].items():
    assert count['initial_matched'] + count['second_matched'] == existing[str(kind)]['matched']
    assert count['extra_ons'] == existing[str(kind)]['extra']

def group_extras(extras):
    groups = collections.defaultdict(list)
    for row in extras:
        groups[(int(row['wave']), int(row['note']), int(row['sample']))].append(
            [row['phase'], row['new_gain'], row['dynamics']])
    return [{'kind': key[0], 'note': key[1], 'sample': key[2], 'time_ms': key[2] / 48,
             'count': len(cases), 'phase_gain_dynamics': cases} for key, cases in sorted(groups.items())]

summary = {
    'scope': 'Independent read-only source/event audit of the same corrected-v2 48 configurations; no compile or Source edit by auditor.',
    'matching': 'Each expected fixed MIDI note requires one fresh On in [authored onset, next same-string repick); final gesture window ends 600 ms after authored onset. Earliest unused eligible On matches once; all other Ons are extras. Sustained MIDI cannot match a repick.',
    'sample_rate': 48000,
    'driver_sha256': sha(base / 'chord_repick_probe_v2.cpp'),
    'patch_sha256': sha(variant / 'trusted-phase-reference.patch'),
    'source_differences_from_null_guard': source_differences,
    'source_inventory_verified': True,
    'compile_result': compile_result,
    'run_result': run_result,
    'patch_review': {
        'actual_dt_from_last_trusted_sample': True,
        'unreliable_angle_overwrites_trusted_reference': False,
        'bridge_bound_analysis_samples': 128,
        'bridge_bound_ms_at_12k': 128 / 12,
        'preserve_requires_previously_valid_and_null_unexplained': True,
        'existing_threshold_constants_changed': False,
        'caveats': ['Reliable NULL-explained return still learns trend and clears departure.',
                    'Warmup is per note; phase differences remain wrapped; long gaps and permanent coherence exclusions remain.']},
    'datasets': {name: {key: value for key, value in data.items() if key not in ('matches', 'extras')}
                 for name, data in datasets.items()},
    'common_matched_on_counts': common,
    'common_matched_on_timing_changes': timing_changed,
    'gained_expected_ons': gained,
    'lost_expected_ons': lost,
    'extra_on_groups': {name: group_extras(data['extras']) for name, data in datasets.items()},
    'per_note_timing_rollup': timing_rollup,
    'conclusion': 'Reject as a musical pass: two G3 repick matches gained, but held/gain damping controls acquire 24 false E2 Ons; total extras rise32 to60. All256 common expected matches retain exact sample timing; second-gesture failures remain114/132.',
    'limits': 'Synthetic capture only. Phase/new_gain are unused in held/gain renderer controls, so those parameter repetitions are not independent waveforms. No performance claim.'}
(variant / 'independent-on-audit.json').write_text(json.dumps(summary, indent=2) + '\n')
with (variant / 'independent-per-note-timing.csv').open('w') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(timing_rows[0]))
    writer.writeheader()
    writer.writerows(timing_rows)
with (variant / 'independent-extra-ons.csv').open('w') as handle:
    columns = ['variant', 'wave', 'phase', 'new_gain', 'dynamics', 'sample', 'type', 'note']
    writer = csv.DictWriter(handle, fieldnames=columns)
    writer.writeheader()
    for name, data in datasets.items():
        writer.writerows(dict(variant=name, **event) for event in data['extras'])

report = '''# Independent trusted-reference bridge audit

The bridge recovers two expected G3 repicks, but introduces damping-related false events. It is not a musical pass.

The completed bridge capture records compile exit0 and run exit0,636 event rows across all48 configurations. NULLguard has576 rows across the same48 configurations. The driver, source inventory, patch, and both event CSVs are hashed in `independent-on-audit.json`. Only TimeDomainPursuit.cpp differs from NULLguard. This audit launched no compiler and edited no Source.

| Kind | Initial matches NULLguard→bridge | Second matches NULLguard→bridge | Extra Ons NULLguard→bridge |
|---|---:|---:|---:|
|0 held open6|72/72→72/72|—|0→12|
|1 replucked open6|72/72→72/72|10/72→12/72|6→10|
|2 replucked five-note chord|24/60→24/60|6/60→6/60|26→26|
|3 gain-ramp open6|72/72→72/72|—|0→12|

Across all408 authored On references, matched Ons rise256→258; extras rise32→60. The240 common initial matches and16 common second matches retain exactly the same output sample. No expected match is lost and no common expected On moves. The24 previously complete held/gain cases now all contain an extra On; no case is onset-complete after the bridge.

The only gained references are kind1,phase0,newGain1,dynamics0 and1,note55. Both emit atsample22399 (466.645833ms),45.645833ms after the authored G3 repick atsample20208 (421ms). Other kind1 second matches are unchanged: E2 six/12 (31.979–79.979ms delay),D3 four/12 (52.646–55.313ms); A2,B3,E4 remain zero/12. Kind2 second matches remain E3 six/12 (67.646–72.979ms); A2,A3,C4,E4 remain zero/12. Exact timings and misses for every note/configuration are in `independent-per-note-timing.csv`.

All12 held-control configurations and all12 gain-control configurations acquire a false E2 On atsample35071 (730.645833ms),50.645833ms after E2's authored damping at680ms. These false Ons occur after damping, not during the gain ramp. Kind1 additionally gains four late false E2 Ons forphase0 atsample50047 (1042.645833ms),after damping at1000ms. Its two existing phase.9/gain.5 late E2 extras move from50431 to50047 (8ms earlier); four existing wrong-note52 extras remain. Kind2 extras are unchanged. `independent-extra-ons.csv` retains every unmatched On for both variants.

The patch implements its stated mechanics: per-partial dt is clock−trustedSample; unreliable angles cannot replace trusted phase/omega/power. A valid reference survives only while NULL-unexplained and within128 analysis samples (10.667ms at12k). Return at exactly128 can compare; later return reinitializes. NULL-explained unreliable observations clear the reference. Existing held-set, nonmonotonic-clock, and excessive observation-gap resets remain. No threshold constant changed. Reliable NULL-explained returns can still learn trend and clear departure, as before; permanent coherence exclusions and longer gaps remain.

Matching independently reproduces the existing NULLguard totals: one fresh exact-note On is required per authored gesture; first-gesture windows end at the same-string repick, final windows end600ms after onset. A connected held note cannot satisfy the second gesture. Phase/newGain parameters are unused for kinds0/3, so their repeated configurations are not independent waveforms. These findings establish a localized recovery with control regressions, not broad polyphonic reliability or performance.
'''
(variant / 'independent-on-audit.md').write_text(report)
print(json.dumps({'report': str(variant / 'independent-on-audit.md'),
                  'gained_expected_ons': len(gained), 'lost_expected_ons': len(lost),
                  'common_matched_ons': dict(common), 'common_timing_changes': len(timing_changed),
                  'per_note_reference_rows': len(timing_rows),
                  'extra_ons': {name: len(data['extras']) for name, data in datasets.items()}}, indent=2))
