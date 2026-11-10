#!/usr/bin/env python3
"""Preserve every command, return code and log; expected gaps are explicit."""
import argparse,json,pathlib,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
results=[]
def run(name,cmd,expected=0):
    r=subprocess.run(cmd,cwd=repo,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
    (out/(name+'.log')).write_text(r.stdout)
    results.append({'name':name,'command':cmd,'cwd':str(repo),'returncode':r.returncode,'expected':expected,'matched':r.returncode==expected,'log':str(out/(name+'.log'))})
    (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    if r.returncode!=expected:raise RuntimeError(name+' unexpected return code')
    if expected == 8:
        gaps = [line[5:] for line in r.stdout.splitlines() if line.startswith('FAIL ')]
        wanted = ['embedded NUL truncation', 'every-byte payload starts with NUL', 'short read returns immediately', 'short write returns partial count', 'negative recv resized before error check']
        if gaps != wanted or 'RESULT 22 passed 5 failed' not in r.stdout:
            raise RuntimeError(name+' did not reproduce exactly the five known gaps')
for mode in ['Debug','Release']:
    build=out/mode.lower()
    run(mode+'-configure',['cmake','-S',str(repo/'characterization/contract-baseline-v1'),'-B',str(build),'-DCMAKE_BUILD_TYPE='+mode])
    run(mode+'-build',['cmake','--build',str(build),'-j2'])
    run(mode+'-characterization',['ctest','--test-dir',str(build),'-R','^legacy_characterization$','--output-on-failure'])
    run(mode+'-contract-gaps',['ctest','--test-dir',str(build),'-R','^proposed_binary_contract$','--output-on-failure'],8)
# CTest timeouts apply to every run; require 100 consecutive repetitions.
run('repeat-100',['ctest','--test-dir',str(out/'debug'),'-R','^legacy_characterization$','--repeat','until-fail:100','--output-on-failure'])
print(json.dumps({'commands':len(results),'unexpected_results':sum(not x['matched'] for x in results),'characterization_groups_per_execution':27,'contract_gap_groups':5,'repeat_executions':100}))
