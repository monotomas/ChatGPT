from pathlib import Path
import subprocess,os,json,csv,collections,importlib.util,copy,sys
base=Path('/tmp/flysnapper-v4-real-repeat-audit');root=Path('/workspace/ChatGPT');sys.path.insert(0,str(root/'Tools'));import guitar_evaluation_score as score
policy=json.loads(score.POLICY.read_text()); result={}
for name in sys.argv[1:] or ['baseline','plain-phase','variance2','variance3']:
 variant={}; result[name]=variant
 for mode in ['bend','plain']:
  entry={};variant[mode]=entry
  for suite,fixtures in [('controlled',root/'Tests/AudioFixtures/readiness'),('noise',root/'Tests/AudioFixtures/readiness/recorded_noise'),('hammers',root/'Tests/AudioFixtures/readiness/recorded_hammers')]:
   folder=base/name/(suite+'-'+mode);folder.mkdir(exist_ok=True)
   env=os.environ.copy()
   if mode=='plain':env['PLAIN_MODE']='1'
   with (folder/'run.txt').open('w') as log:r=subprocess.run([str(base/name/'Capture'),str(fixtures),str(folder),'quick','default'],env=env,stdout=log,stderr=subprocess.STDOUT)
   assert r.returncode==0,(name,mode,suite)
   manifest=json.loads((fixtures/'fixtures.json').read_text()); cases={c['name']:c for c in manifest['cases']};curves=score.original.read_curves(fixtures)
   metrics=list(csv.DictReader((folder/'capture.csv').open()));events=collections.defaultdict(list)
   for e in csv.DictReader((folder/'events.csv').open()):events[e['case']].append(e)
   rows=[]
   for m in metrics:
    c=copy.deepcopy(cases[m['case']])
    if mode=='plain':
     if c['family']=='continuous-bend':continue
     c['bend_enabled']=False;c['pitch_curve_ground_truth']=False
    rows.append(score.evaluate(c,m,events[c['name']],curves,policy))
   summary=score.summarize(rows,[]);entry[suite]=summary
   (folder/'cases.json').write_text(json.dumps(rows,indent=2)+'\n');(folder/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
   print(name,mode,suite,summary['passed_runs'],'/',summary['runs'],summary['counts'],flush=True)
(base/('experiment-summary-'+('-'.join(sys.argv[1:]) or 'initial')+'.json')).write_text(json.dumps(result,indent=2)+'\n')
