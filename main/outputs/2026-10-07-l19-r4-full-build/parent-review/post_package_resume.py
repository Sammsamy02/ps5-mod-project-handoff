"""Resume only original post-package gates on the preserved, compiled R4 pair."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "execution"))
from admission import Admission, OWN, pin, read, require, sha
from composition import write
from hooks import install
from full_pipeline import load_odr_generation


def select_composed_object(rundir, receipt_path, receipt_digest, expected_basename):
    rundir = Path(rundir).resolve()
    receipt_path = Path(receipt_path).resolve()
    require(receipt_path == rundir / 'receipt.json', 'composition receipt path mismatch')
    receipt = read(pin({'path': str(receipt_path), 'sha256': receipt_digest}, 'composition receipt'))
    require(isinstance(receipt.get('runtime_object'), str)
            and isinstance(receipt.get('runtime_sha256'), str), 'runtime object metadata missing')
    obj = Path(receipt['runtime_object']).resolve()
    require(obj == rundir / expected_basename and obj.suffix == '.o', 'runtime object path mismatch')
    return pin({'path': str(obj), 'sha256': receipt['runtime_sha256']}, 'recorded composed object')


def built_pins():
    paths = []
    for number in (1, 2):
        run = OWN / f'runs/run-l19-{number}'
        for p in run.iterdir():
            if (p.suffix in ('.o', '.d') or p.name.startswith('compile-')
                    or p.name in ('link.command.json', 'receipt.json', 'input-hashes.json', 'runtime.map')):
                paths.append(p)
        artifact = read(OWN / f'prx-candidate-l19-{number}/artifacts-receipt.json')['artifacts']['final_prx']
        paths.append(pin(artifact, 'preserved compiled artifact'))
    return {str(p): sha(p) for p in paths}


def authorize_post_package(admission_path, admission_digest, go_path, go_digest):
    admission_path = Path(admission_path).resolve()
    go_path = Path(go_path).resolve()
    post = read(pin({'path': str(admission_path), 'sha256': admission_digest}, 'standalone post-package admission'))
    go = read(pin({'path': str(go_path), 'sha256': go_digest}, 'standalone post-package GO'))
    require(post.get('scope') == 'post-package-only' and post.get('full_build_enabled') is False
            and post.get('no_compile_link_package') is True, 'post-package-only admission required')
    require(go.get('r4_post_package_go') is True and go.get('r4_full_build_go') is False
            and go.get('scope') == 'post-package-only', 'post-package-only GO required')
    require(go.get('admission_sha256') == admission_digest, 'post-package GO admission mismatch')
    compiled = read(pin(post['compilation_admission'], 'unchanged actual compilation admission'))
    pin(post['compilation_go'], 'unchanged actual compilation GO')
    expected = dict(compiled['execution_source_pins'])
    expected[str(Path(__file__).resolve())] = sha(Path(__file__))
    require(post.get('execution_source_pins') == expected
            and go.get('execution_source_pins') == expected, 'post-package source catalog mismatch')
    for path, digest in expected.items():
        pin({'path': path, 'sha256': digest}, 'exact original-five plus standalone wrapper')
    for key in ('source_manifest', 'profile_json', 'generated_header'):
        require(post.get(key) == compiled[key], 'post-package ' + key + ' identity mismatch')
        pin(post[key], 'unchanged post-package source/profile identity')
    return post


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--post-admission', required=True)
    parser.add_argument('--post-admission-sha256', required=True)
    parser.add_argument('--post-go', required=True)
    parser.add_argument('--post-go-sha256', required=True)
    parser.add_argument('--selector-review', required=True)
    parser.add_argument('--selector-review-sha256', required=True)
    args = parser.parse_args()
    review_pin = {'path': str(Path(args.selector_review).resolve()), 'sha256': args.selector_review_sha256}
    review = read(pin(review_pin, 'independent selector review'))
    require(review.get('pass') is True and review.get('findings') == [], 'selector review must pass')
    require(Path(review['resume_wrapper']['path']).resolve() == Path(__file__).resolve(), 'reviewed wrapper path mismatch')
    pin(review['resume_wrapper'], 'independently reviewed resume wrapper')
    post = authorize_post_package(args.post_admission, args.post_admission_sha256,
                                  args.post_go, args.post_go_sha256)
    a = Admission.load(post['compilation_admission']['path'], post['compilation_admission']['sha256'],
                       post['compilation_go']['path'], post['compilation_go']['sha256'])
    for number in (1, 2):
        recorded = read(OWN / f'runs/run-l19-{number}/receipt.json')
        require(recorded['r4_execution_admission'] == post['compilation_admission']
                and recorded['r4_parent_go'] == post['compilation_go'], 'actual compiled pair provenance mismatch')
    before = built_pins()
    install(a)
    import launch19_one_pass as launcher
    launcher.EDITION = read(a.pins['edition'])
    original_run = launcher.run
    selection = {}

    def run(name, argv, tail=4):
        require(name in ('odr-emitter-check', 'finalize-manifest'), 'resume refuses compile/package stage')
        values = list(argv)
        if name == 'odr-emitter-check':
            checker = pin(a.data['odr_generation_checker'], 'unchanged explicit generation ODR checker')
            require(Path(values[1]) == OWN / 'check_odr_emitters_l19.py', 'original ODR invocation drift')
            rundir = Path(values[2]).resolve()
            generation = rundir / 'R4-ODR-GENERATION.json'
            load_odr_generation(generation, sha(generation), rundir)
            data = read(generation)
            obj = select_composed_object(rundir, data['current_composition']['path'],
                                         data['current_composition']['sha256'], Path(a.base['runtime_object']).name)
            selection.update({'generation': {'path': str(generation), 'sha256': sha(generation)},
                              'composition_receipt': data['current_composition'],
                              'explicit_object': {'path': str(obj), 'sha256': sha(obj)},
                              'argv_object_option': ['--object', str(obj)]})
            values[1] = str(checker)
            values += ['--generation-receipt', str(generation), '--generation-receipt-sha256', sha(generation),
                       '--object', str(obj)]
        return original_run(name, values, tail)

    launcher.run = run
    rd2 = launcher.post_build('prx-candidate-l19-2')
    runtime_hashes = [launcher.determinism(OWN / 'runs/run-l19-1'), launcher.determinism(rd2)]
    require(runtime_hashes[0] == runtime_hashes[1], 'actual runtime twin mismatch')
    artifacts = [read(OWN / f'prx-candidate-l19-{n}/artifacts-receipt.json')['artifacts']['final_prx'] for n in (1, 2)]
    for artifact in artifacts:
        pin(artifact, 'actual final twin artifact')
    require(artifacts[0]['sha256'] == artifacts[1]['sha256'], 'actual PRX twin mismatch')
    require(before == built_pins(), 'post-package resume changed compiled/package outputs')
    require(not (OWN / 'R4-EXECUTION-RECEIPT.json').exists(), 'new completed execution receipt required')
    write(OWN / 'R4-EXECUTION-RECEIPT.json', {
        'pass': True, 'inherited_full_pipeline_complete': True,
        'admission': {'path': str(a.path), 'sha256': a.digest},
        'parent_go': {'path': str(a.go_path), 'sha256': a.go_digest},
        'selected_units': 369, 'composition_inputs': 386, 'affected_units': 147,
        'pass1_reused': 144, 'pass1_fresh': 3, 'pass2_fresh': 147,
        'actual_artifacts': artifacts, 'composition_sha256': runtime_hashes,
        'post_package_resume': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))},
        'selector_review': review_pin, 'explicit_composed_selection': selection,
        'post_package_admission': {'path': str(Path(args.post_admission).resolve()), 'sha256': args.post_admission_sha256},
        'post_package_go': {'path': str(Path(args.post_go).resolve()), 'sha256': args.post_go_sha256},
        'compiled_package_files_unchanged': before,
        'scope_note': 'Actual full pair attempt6 stopped at ambiguous size heuristic; only post-package gates resumed, no repeated compile/link/package',
        'console_contact': False, 'upload': 'HOLD'})
    print('R4 ACTUAL PAIR COMPLETE: original post-package gates passed; no compile/link/package repeated', flush=True)


if __name__ == '__main__':
    main()
