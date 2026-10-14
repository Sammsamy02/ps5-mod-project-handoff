#!/usr/bin/env python3
"""Exercise the real harvest script with fake curl; no network process is used.

On macOS the script subprocess also runs under deny-network sandbox-exec.
Receipts include fixture inputs, stdout/stderr, curl calls, verdict, and assertions.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
MODS = '/data/modbridge/roots/PPSA33286/ue4ss/Mods/'
STATUS = '/data/modbridge/roots/PPSA33286/runtime-status.txt'
LOG = '/data/plugins/UE4SS.log'
DEADLINE = 'Unreal initialization startup deadline exceeded'
LISTING = 'drwxr-xr-x 1 root wheel 0 Oct 7 12:00 TownfallItemSpawner\n'
SINKS = ['modbridge-copyin-sink.bin', 'modbridge-copyin-p138-s2.bin']

FAKE_CURL = '''#!{python}
import json, os, pathlib, sys
args = sys.argv[1:]
url = args[-1]
prefix = 'ftp://harvest-fixture.invalid:2121'
if not url.startswith(prefix + '/') or '-o' not in args:
    raise SystemExit('fake curl rejected unexpected arguments')
remote = url[len(prefix):].replace('%20', ' ')
out = pathlib.Path(args[args.index('-o') + 1])
fixture = json.loads(pathlib.Path(os.environ['HARVEST_FIXTURE']).read_text())
with open(os.environ['HARVEST_CURL_CALLS'], 'a') as record:
    record.write(json.dumps({{'remote': remote, 'args': args}}) + '\\n')
item = fixture.get(remote, {{'rc': 78}})
if 'data' in item:
    out.write_text(item['data'])
if item.get('rc', 0):
    print('fake curl: fixture error ' + str(item['rc']), file=sys.stderr)
raise SystemExit(item.get('rc', 0))
'''

FAKE_DATE = '''#!{python}
import os, sys
print(os.environ['HARVEST_TEST_ID'] if sys.argv[1:] == ['+%s'] else '2026-10-07 00:00:00 UTC')
'''


def fixtures():
    base = {
        LOG: {'data': '[Seeds][L12] pool admission retry armed\n'},
        STATUS: {'data': 'state\t2\tfailed\t0\n'},
        '/data/modbridge-supplier-diag.jsonl': {'data': ''},
        '/data/shadowmount/debug.log': {'data': ''},
        '/data/ps5upload/activity.json': {'data': '{}\n'},
        '/data/plugins/': {'data': ''},
        MODS: {'data': LISTING},
    }
    yield 'no_admitted_no_refuted', base, 0
    timeout = dict(base)
    timeout[LOG] = {'data': '[Seeds][L12] round 19 exhausted\n' + DEADLINE +
                           "\nStopping mod 'TownfallItemSpawner' for uninstall\n"}
    timeout[STATUS] = {'data': 'state\t2\tfailed\t1\t' + DEADLINE + '\n'}
    yield 'startup_timeout', timeout, 0
    yield 'missing_logs', {}, 4
    failed = {key: {'rc': 28, 'data': 'partial transfer must not be evidence\n'} for key in base}
    yield 'curl_timeout', failed, 4
    normal = dict(base)
    normal[LOG] = {'data': '[Seeds][L12] pool admitted via VirtualQuery coverage\n'
                          '[Seeds] FName pool validated\nR3Marshal armed\n'
                          '[TownfallItemSpawner] Menu created.\n'
                          'pad error=0x80920004\n'}
    normal['/data/modbridge-supplier-diag.jsonl'] = {'data': 'pool_copyin_probe_stop=131072\n'}
    normal['/data/shadowmount/debug.log'] = {'data': 'Big App started\n'}
    normal['/data/plugins/'] = {'data': ''.join(
        ' -rw-r--r-- 1 root wheel 6 Oct 7 12:00 ' + name + '\n' for name in SINKS + [SINKS[1]])}
    for name in SINKS:
        normal['/data/plugins/' + name] = {'data': 'probe\n'}
    yield 'normal_results', normal, 0


def run_case(script, receipts, case, fixture, expected):
    result_dir = receipts / case
    result_dir.mkdir(parents=True, exist_ok=False)
    fixture_file = result_dir / 'fixture.json'
    fixture_file.write_text(json.dumps(fixture, indent=2) + '\n')
    calls_file = result_dir / 'curl-calls.jsonl'
    with tempfile.TemporaryDirectory(prefix='harvest-fixture-') as work:
        work = Path(work)
        binary = work / 'bin'
        binary.mkdir()
        for command in ['awk', 'sort', 'grep', 'sed', 'wc', 'tr', 'rm', 'mkdir', 'tail', 'tee']:
            (binary / command).symlink_to(shutil.which(command))
        for command, source in [('curl', FAKE_CURL), ('date', FAKE_DATE)]:
            target = binary / command
            target.write_text(source.format(python=sys.executable))
            target.chmod(0o755)
        test_id = 'fixture-' + uuid.uuid4().hex
        env = dict(os.environ, PATH=str(binary), FTP='ftp://harvest-fixture.invalid:2121',
                   HARVEST_BASE=str(work / 'output'), HARVEST_FIXTURE=str(fixture_file),
                   HARVEST_CURL_CALLS=str(calls_file), HARVEST_TEST_ID=test_id)
        command = ['/bin/bash', str(script)]
        sandbox = Path('/usr/bin/sandbox-exec')
        if sandbox.exists():
            command = [str(sandbox), '-p', '(version 1) (allow default) (deny network*)'] + command
        proc = subprocess.run(command, env=env, text=True, capture_output=True, timeout=30)
        (result_dir / 'stdout.txt').write_text(proc.stdout)
        (result_dir / 'stderr.txt').write_text(proc.stderr)
        out_line = next((line[4:] for line in proc.stdout.splitlines() if line.startswith('OUT=')), '')
        out = Path(out_line) if out_line else work / 'no-output'
        verdict = (out / 'verdict.txt').read_text() if (out / 'verdict.txt').exists() else ''
        (result_dir / 'verdict.txt').write_text(verdict)
        calls = [json.loads(line) for line in calls_file.read_text().splitlines()] if calls_file.exists() else []
        checks = {
            'exit_code': proc.returncode == expected,
            'reaches_verdict': '=== 3. VERDICT' in proc.stdout,
            'writes_complete_verdict': 'harvest complete:' in verdict,
            'all_fetches_used_fake_curl': len(calls) >= 7,
            'no_runtime_proof_from_file_presence': 'YES — TownfallItemSpawner still present' not in verdict,
            'uninstall_count_is_single_value': 'n/a) occurrence' not in verdict and '0\nn/a' not in verdict,
        }
        if case == 'no_admitted_no_refuted':
            checks.update(zero_match_summary='verdicts (admitted via|REFUTED) : 0 match(es)' in proc.stdout,
                          no_false_failure='NO — runtime-status reports failed=0' in verdict,
                          files_only='FILES PRESENT' in verdict and 'UNKNOWN — current runtime activity' in verdict,
                          zero_uninstalls='0 occurrence(s)' in verdict)
        elif case == 'startup_timeout':
            checks.update(deadline_retained=DEADLINE in verdict,
                          runtime_stopped='STOPPED/FAILED' in verdict,
                          files_still_present='FILES PRESENT' in verdict)
        elif case in ('missing_logs', 'curl_timeout'):
            checks.update(core_unavailable='FETCH_FAILED:' in proc.stdout,
                          unknown_admission='UNKNOWN — UE4SS.log not fetched.' in verdict,
                          failed_partial_discarded=not (out / 'UE4SS.log').exists() and not (out / 'runtime-status.txt').exists(),
                          no_partial_as_evidence='partial transfer must not be evidence' not in proc.stdout)
        else:
            checks.update(admission_count='verdicts (admitted via|REFUTED) : 1 match(es)' in proc.stdout,
                          unique_sink_discovery=all(sum(c['remote'] == '/data/plugins/' + sink for c in calls) == 1 for sink in SINKS),
                          sink_contents=all((out / sink).exists() and (out / sink).read_text() == 'probe\n' for sink in SINKS),
                          zero_uninstalls='0 occurrence(s)' in verdict,
                          no_false_failure='NO — runtime-status reports failed=0' in verdict)
        result = dict(case=case, exit_code=proc.returncode, expected_exit_code=expected,
                      sandbox_network_denied=sandbox.exists(), checks=checks, passed=all(checks.values()))
        (result_dir / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        # Old script hardcodes /tmp/l12-harvest; remove only this UUID fixture output.
        legacy = Path('/tmp/l12-harvest') / ('out-' + test_id)
        if out == legacy and legacy.is_dir():
            shutil.rmtree(legacy)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--script', type=Path, default=ROOT / 'tools/harvest.sh')
    parser.add_argument('--receipts', type=Path, required=True)
    parser.add_argument('--case', choices=[case for case, _, _ in fixtures()])
    args = parser.parse_args()
    receipts = args.receipts.resolve()
    receipts.mkdir(parents=True, exist_ok=True)
    script = args.script.resolve()
    results = [run_case(script, receipts, case, fixture, expected)
               for case, fixture, expected in fixtures() if not args.case or case == args.case]
    receipt = dict(script=str(script), script_sha256=hashlib.sha256(script.read_bytes()).hexdigest(), results=results)
    (receipts / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
    for result in results:
        failures = [key for key, passed in result['checks'].items() if not passed]
        print(('PASS' if result['passed'] else 'FAIL') + ' ' + result['case'] +
              ' exit=' + str(result['exit_code']) + (' failures=' + ','.join(failures) if failures else ''))
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
