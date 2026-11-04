#!/usr/bin/env python3
"""Retain complete family/mode results from the first real-noise capture.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
from collections import defaultdict,Counter
import csv,hashlib,json
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    cases={c['name']:c for c in json.loads((HERE/'fixtures.json').read_text())['cases']};results={}
    for label,folder in [('baseline_c5b','baseline-c5b/expression'),('mono_checkpoint_v19_mainpoly17','candidate-mono-v19')]:
        root=HERE/folder;rows=json.loads((root/'scoring/cases.json').read_text());summary=json.loads((root/'scoring/summary.json').read_text());mode_results={};events=defaultdict(list)
        for e in csv.DictReader((root/'events.csv').open()):events[e['case']].append(e)
        for mode in ('mono','poly'):
            selected=[r for r in rows if r['mode']==mode];families={}
            for family in sorted({r['family'] for r in selected}):
                family_rows=[r for r in selected if r['family']==family]
                families[family]=dict(runs=len(family_rows),passed=sum(r['passed'] for r in family_rows),**{k:sum(r[k] for r in family_rows) for k in ('expected','matched','missed','extra_note_ons','octave_errors','wrong_before_correct','early_offs','late_offs','hung','overflow')})
            offsets=Counter()
            for r in selected:
                if r['family']!='real-note-plus-recorded-noise':continue
                anchor=cases[r['case']]['reference_notes'][0]['anchor'];matched=False
                for event in events[r['case']]:
                    if event['type']!='N':continue
                    if not matched and int(event['note'])==anchor:matched=True;continue
                    offsets[int(event['note'])-anchor]+=1
            mode_results[mode]=dict(runs=len(selected),passed=sum(r['passed'] for r in selected),families=families,extra_mixture_onsets_by_semitone_offset_from_reference=dict(sorted(offsets.items())),latency_checkpoint=summary['latency_checkpoints'][mode])
        results[label]=dict(capture_sha256={f:sha(root/f) for f in ('capture.csv','events.csv')},scoring_sha256={f:sha(root/'scoring'/f) for f in ('cases.json','summary.json')},modes=mode_results)
    report=dict(scope='First independently selected recorded-noise edition, now disclosed seen regression data. Intended suppression labels and all original failures unchanged.',fixture_seal=(HERE/'SEALED_SHA256.txt').read_text().strip(),script_sha256=sha(Path(__file__)),interpretation='Baseline lacks poly bend capability and therefore fails its explicit capability checks; compare direct metrics as well as complete pass counts. Same-anchor extra On events can be unwanted rearticulations.',results=results,musical_readiness_eligible=False)
    (HERE/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
