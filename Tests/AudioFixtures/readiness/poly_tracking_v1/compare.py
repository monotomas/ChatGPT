#!/usr/bin/env python3
"""Compare frozen17/19 identity-preserving whole-curve error without alignment.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
from collections import defaultdict
import csv,hashlib,json,os
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    variants=('baseline17','candidate19','candidate19-full512-diagnostic');summaries={v:json.loads((HERE/v/'scoring/summary.json').read_text()) for v in variants};rows=[]
    for case in json.loads((HERE/'fixtures.json').read_text())['cases']:
        result=dict(case=case['name'],variants={})
        for variant,summary in summaries.items():
            selected=next((r for r in summary['results'] if r['case']==case['name']),None)
            if selected:result['variants'][variant]=dict(passed=selected['passed'],error_cents=selected['error_cents'],worst_voice_p95=max(v['error_cents']['p95'] for v in selected['per_voice'].values()),missing=selected['missing_source_frames'],phase_unobservable=selected['phase_unobservable_source_frames'])
        rows.append(result)
    report=dict(scope='Seen frozen-model comparison; original oracle synthetic first-capture results retained',original_fixture_seal=(HERE/'SEALED_SHA256.txt').read_text().strip(),support_sha256={f:sha(HERE/f) for f in ('compare.py','make_full512_diagnostic.py','test_scoring.py')},measurement_manifests_sha256={v:sha(HERE/v/'manifest.json') for v in variants},passed_cases={v:s['passed_cases'] for v,s in summaries.items()},cases=rows,musical_readiness_eligible=False)
    (HERE/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    os.environ.setdefault('MPLCONFIGDIR','/tmp/flysnapper-readiness-matplotlib')
    try:import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
    except ImportError:return
    fig,axes=plt.subplots(4,2,figsize=(12,12),sharex=True);fig.suptitle('Oracle-seeded six-source tracking: error against original IDs at actual time\nSynthetic research; every true voice provided before tracking, no acquisition proof',fontsize=12)
    for axis,row in zip(axes.flat,rows):
        for variant,color,label in zip(variants,('#31688e','#de604d','#2e9b55'),('17 original','19 original','19 full512 diagnostic')):
            points=defaultdict(list)
            with (HERE/variant/'scoring/errors.csv').open() as f:
                for r in csv.DictReader(f):
                    if r['case']==row['case'] and r['error_cents']:points[int(r['sample'])].append(float(r['error_cents']))
            if not points:continue
            axis.plot([n/12000 for n in sorted(points)],[max(points[n]) for n in sorted(points)],color=color,lw=.8,label=label,alpha=.90)
        axis.axhline(75,color='#777777',lw=.7,ls='--');axis.axvline(.50,color='#b8b8b8',lw=.7,ls=':');axis.set_title(row['case'].replace('_',' '),fontsize=10);axis.set_ylabel('Worst original-ID error (cents)',fontsize=8);axis.set_ylim(0,140);axis.grid(alpha=.15)
    for a in axes[-1]:a.set_xlabel('Source time at causal observation end (seconds)')
    from matplotlib.lines import Line2D
    fig.legend(handles=[Line2D([],[],color=color,lw=1) for color in ('#31688e','#de604d','#2e9b55')],labels=['17 original','19 original','19 full512 post hoc diagnostic'],loc='upper center',bbox_to_anchor=(.5,.94),ncol=3,fontsize=9)
    fig.tight_layout(rect=(0,0,1,.92));fig.savefig(HERE/'comparison.png',dpi=150);fig.savefig(HERE/'comparison.svg');plt.close(fig)
if __name__=='__main__':main()
