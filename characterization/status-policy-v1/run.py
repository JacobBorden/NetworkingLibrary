"""Run the proposal table and two mutations; preserve all subprocess evidence."""
import argparse
import datetime
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
a = parser.parse_args()
if not a.output.is_absolute(): parser.error('--output must be absolute')
a.output.mkdir(parents=True, exist_ok=False)
source = Path(__file__).resolve().parent
repo = source.parents[1]
records = []

def execute(label, script, expected_failure):
    cmd = [sys.executable, '-B', str(script)]
    try:
        p = subprocess.run(cmd, cwd=repo, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=10)
        log, code = p.stdout, p.returncode
    except subprocess.TimeoutExpired as e:
        log = str(e); code = 124
    (a.output / (label+'.log')).write_text(log)
    groups = 'Ran 5 tests' in log
    failure_lines = re.findall(r'^FAIL: (.*)$', log, re.MULTILINE)
    expected_lines = {
        'expiry_beats_stop': [
            "test_preflight_table (__main__.BoundaryTable.test_preflight_table) (label='stop dominates expiry')",
            "test_preflight_table (__main__.BoundaryTable.test_preflight_table) (label='partial stop dominates expiry')",
            'test_race_sequences (__main__.BoundaryTable.test_race_sequences)'],
        'completion_ignored': [
            "test_preflight_table (__main__.BoundaryTable.test_preflight_table) (label='empty dominates every gate')",
            "test_preflight_table (__main__.BoundaryTable.test_preflight_table) (label='finished progress dominates stop')"]}
    failures_match = failure_lines == expected_lines.get(label, []) and not re.search(r'^ERROR: ', log, re.MULTILINE)
    valid = groups and failures_match and (code == 1 and 'FAIL:' in log if expected_failure else code == 0 and '\nOK\n' in log)
    records.append({'label':label,'command':cmd,'cwd':str(repo),'exit_code':code,
                    'expected_failure':expected_failure,'groups_executed':5 if groups else 0,
                    'valid':valid,'log':str(a.output/(label+'.log'))})

execute('proposal', source/'test_policy.py', False)
original = (source/'policy.py').read_text()
mutations = {
    'expiry_beats_stop': ('if stopped:', 'if stopped and not expired:'),
    'completion_ignored': ('if transferred == size:', 'if False:'),
}
for label,(old,new) in mutations.items():
    if original.count(old) != 1: raise RuntimeError('mutation anchor is not unique')
    folder = a.output/label
    folder.mkdir()
    shutil.copyfile(source/'test_policy.py',folder/'test_policy.py')
    (folder/'policy.py').write_text(original.replace(old,new))
    execute(label,folder/'test_policy.py',True)
result = {'utc_time':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'commands':records,'proposal_groups':5,'proposal_assertions':46,
          'mutations_rejected':sum(r['valid'] for r in records[1:]),
          'native_socket_tests':0,'cpp_builds':0,
          'input_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()},
          'success':all(r['valid'] for r in records)}
(a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
sys.exit(0 if result['success'] else 1)
