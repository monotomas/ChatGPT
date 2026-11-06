from pathlib import Path
import difflib, hashlib, json, shutil, subprocess

root = Path('/tmp/flysnapper-v4-uncertainty-profile-combo')
source = Path('/workspace/ChatGPT/docs/validation/guitar-reliability-2026-11-06/fine-frequency-research/preserved/uncertainty-f0/Source')
prior = Path('/workspace/ChatGPT/docs/validation/guitar-reliability-2026-11-06/profile-kernel-performance')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def mapping(path):
    return {p.relative_to(path).as_posix(): sha(p) for p in sorted(path.rglob('*')) if p.is_file()}
root.mkdir(exist_ok=False)
shutil.copytree(source, root / 'baseline-Source')
shutil.copytree(source, root / 'fast-Source')
shutil.copy2(source.parent / 'source-inventory.json', root / 'input-source-inventory.json')
script = (prior / 'make_fast.py').read_text()
(root / 'make_fast.py').write_text(script)
subprocess.run(['python3',str(root / 'make_fast.py')], check=True)
base = (root / 'baseline-Source/Pitch/TimeDomainPursuit.cpp').read_text()
fast = (root / 'fast-Source/Pitch/TimeDomainPursuit.cpp').read_text()
original_profile = (prior / 'frozen-source/baseline/Source/Pitch/TimeDomainPursuit.cpp').read_text()
start = '            for(const auto& profile:data->profiles){'
end = '\n        }\n        if(iteration==0)'
assert base[base.index(start):base.index(end,base.index(start))] == original_profile[original_profile.index(start):original_profile.index(end,original_profile.index(start))]
old, new = mapping(root / 'baseline-Source'), mapping(root / 'fast-Source')
assert old.keys() == new.keys()
assert [p for p in old if old[p] != new[p]] == ['Pitch/TimeDomainPursuit.cpp']
# The exact source used by the previous profile audit; no driver/input changes.
for name in ('payload-cold.cpp','profile-thread-probe.cpp','build_and_measure.py'):
    shutil.copy2(prior / name, root / name)
# Reuse only the audited instrumentation transformation; frozen driver above
# replaces the historical script's absolute-path driver generation.
instrument = (prior / 'instrument.py').read_text().split("driver=Path('/tmp/flysnapper-v4-poly-f0/profile_cpu_probe_cold.cpp').read_text()", 1)[0]
(root / 'instrument.py').write_text(instrument)
subprocess.run(['python3',str(root / 'instrument.py')], check=True)
(root / 'input-binding.json').write_text(json.dumps({
    'frequency_source_origin': str(source), 'frequency_source_full_map': mapping(source),
    'frequency_source_inventory_sha256': sha(source.parent / 'source-inventory.json'),
    'profile_template_origin': str(prior),
    'profile_template_script_sha256': sha(prior / 'make_fast.py'),
    'profile_template_patch_sha256': sha(prior / 'profile-fast.patch'),
    'profile_loop_original_region_identical': True,
    'baseline_Source': old, 'fast_Source': new,
    'gram_lifecycle': 'Fresh projected 8D self-Gram per candidate is read after original retuneObserved; only immutable profile norms cached across frames, for all256 valid masks.',
    'unchanged_frequency_methods': {
       marker: base[base.index(marker):base.index(stop,base.index(marker))] == fast[fast.index(marker):fast.index(stop,fast.index(marker))]
       for marker,stop in [('    bool rebuildModel(', '    void retuneObserved('),('    void retuneObserved(', '\n};')]
    },
}, indent=2) + '\n')
assert all(json.loads((root / 'input-binding.json').read_text())['unchanged_frequency_methods'].values())
print(root)
print('combined_sha256',sha(root / 'fast-Source/Pitch/TimeDomainPursuit.cpp'))
print('patch_sha256',sha(root / 'profile-fast.patch'))
