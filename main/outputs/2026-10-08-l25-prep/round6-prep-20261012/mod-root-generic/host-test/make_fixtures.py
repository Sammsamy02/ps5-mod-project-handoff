#!/usr/bin/env python3
"""make_fixtures.py - writes the title-derivation fixture trees (Mac-only).

Each case is a directory fixtures/<case>/root/ that mirrors the console paths
the patched entry reads:
    root/data/plugins/ploader.ini
    root/mnt/sandbox/<TITLE>_<NNN>/app0/sce_sys/param.json
Two cases reuse REAL files read-only as templates:
    outputs/2026-10-08-l25-prep/evidence/ploader-ini-backup-20261008.ini
    .superpowers/native-observations/townfall-ready-recon-2026-10-01/param.json
"ABCD12345" / "ZZZZ00001" are made-up title ids (any game; no real title).
Prints how many cases it wrote and exits 1 when that count is zero.
Writes only under this script's own fixtures/ directory.
"""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'fixtures')
WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
REAL_INI = WHAT + '/outputs/2026-10-08-l25-prep/evidence/ploader-ini-backup-20261008.ini'
REAL_PARAM = WHAT + '/.superpowers/native-observations/townfall-ready-recon-2026-10-01/param.json'
MOD = '/data/plugins/modbridge_supplier_composition.prx'


def param(title, extra=''):
    return ('{\n  "contentVersion": "01.000.000",\n  "localizedParameters": {"defaultLanguage": "en-US",'
            ' "en-US": {"titleName": "Some Game"}},\n  "titleId": "%s"%s\n}\n' % (title, extra))


CASES = {}


def case(name, ini=None, sandboxes=None, links=None, raw_ini=None):
    CASES[name] = (ini, sandboxes or {}, links or {}, raw_ini)


# --- must AGREE ---
case('agree_real_townfall_files', ini='@REAL_INI', sandboxes={'PPSA33286_000': '@REAL_PARAM'})
case('agree_other_title_two_sections',
     ini='[PPSA33286]\n%s = 2\n[ABCD12345]\n%s = 2\n' % (MOD, MOD),
     sandboxes={'PPSA33286_000': None, 'ABCD12345_000': param('ABCD12345')})
case('agree_instance_007_plain_name_crlf',
     ini='; comment\r\n# other comment\r\n\r\n  [ZZZZ00001]  \r\n'
         '   modbridge_supplier_composition.prx   =   1 ; early\r\n'
         '/data/plugins/other.prx = 2\r\n',
     sandboxes={'ZZZZ00001_007': param('ZZZZ00001')})
case('agree_bom_param_json', ini='[ABCD12345]\n%s=2\n' % MOD,
     sandboxes={'ABCD12345_000': b'\xef\xbb\xbf' + param('ABCD12345').encode()})
case('agree_duplicate_section_same_title',
     ini='[ABCD12345]\n%s = 2\n[ABCD12345]\n%s = 1\n' % (MOD, MOD),
     sandboxes={'ABCD12345_000': param('ABCD12345')})
case('agree_nested_titleid_ignored',
     ini='[ABCD12345]\n%s = 2\n' % MOD,
     sandboxes={'ABCD12345_000': '{"x": {"titleId": "XXXX00000"}, "titleId": "ABCD12345"}'})
# --- must FAIL CLOSED: source 1 (ploader.ini) ---
case('fail_missing_ini', ini=None, sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_mode_zero', ini='[PPSA33286]\n%s = 0\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_mode_three', ini='[PPSA33286]\n%s = 3\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_other_module_only', ini='[PPSA33286]\n/data/plugins/other.prx = 2\n',
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_invalid_section_name', ini='[PPSA3328]\n%s = 2\n[ppsa33286]\n%s = 2\n' % (MOD, MOD),
     sandboxes={'PPSA3328_000': param('PPSA3328'), 'PPSA33286_000': param('PPSA33286')})
case('fail_ini_line_before_section', ini='%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_dotdot_path', ini='[PPSA33286]\n/data/plugins/../plugins/modbridge_supplier_composition.prx = 2\n',
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_section_with_comment_is_not_a_section',
     ini='[ABCD12345]\n[PPSA33286] ; note\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_nul_byte', raw_ini=b'[PPSA33286]\n' + MOD.encode() + b' = 2\n\x00\n',
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_oversized', raw_ini=b'[PPSA33286]\n' + MOD.encode() + b' = 2\n' + b';' * (64 * 1024),
     sandboxes={'PPSA33286_000': param('PPSA33286')})
case('fail_ini_too_many_titles',
     ini=''.join('[ABCD%05d]\n%s = 2\n' % (i, MOD) for i in range(17)),
     sandboxes={'ABCD00000_000': param('ABCD00000')})
# --- must FAIL CLOSED: source 2 (sandbox param.json) ---
case('fail_no_sandbox_at_all', ini='[PPSA33286]\n%s = 2\n' % MOD, sandboxes={})
case('fail_missing_param_json', ini='[PPSA33286]\n%s = 2\n' % MOD, sandboxes={'PPSA33286_000': None})
case('fail_only_unconfigured_title_running', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'ABCD12345_000': param('ABCD12345')})
case('fail_disagree_param_title', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA99999')})
case('fail_disagree_second_instance', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286'), 'PPSA33286_001': param('XXXX00000')})
case('fail_ambiguous_two_live', ini='[PPSA33286]\n%s = 2\n[ABCD12345]\n%s = 2\n' % (MOD, MOD),
     sandboxes={'PPSA33286_000': param('PPSA33286'), 'ABCD12345_000': param('ABCD12345')})
case('fail_param_truncated', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286')[:-3]})
case('fail_param_duplicate_titleid', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"titleId": "PPSA33286", "titleId": "PPSA33286"}'})
case('fail_param_nested_titleid_only', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"app": {"titleId": "PPSA33286"}}'})
case('fail_param_trailing_garbage', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': param('PPSA33286') + 'x'})
case('fail_param_titleid_escaped', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"titleId": "PPSA3328\\u0036"}'})
case('fail_param_titleid_number', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"titleId": 33286}'})
case('fail_param_escaped_root_key', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"title\\u0049d": "PPSA33286", "titleId": "PPSA33286"}'})
case('fail_param_invalid_utf8', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': b'{"name": "\xc3\x28", "titleId": "PPSA33286"}'})
case('fail_param_too_deep', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"titleId": "PPSA33286", "d": ' + '[' * 17 + ']' * 17 + '}'})
case('fail_param_root_is_array', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '[{"titleId": "PPSA33286"}]'})
case('fail_param_oversized', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': '{"titleId": "PPSA33286", "pad": "' + 'a' * (64 * 1024) + '"}'})
case('fail_param_symlink_not_followed', ini='[PPSA33286]\n%s = 2\n' % MOD,
     sandboxes={'PPSA33286_000': None},
     links={'PPSA33286_000': '../../../../../real-param.json'})


def write_bytes(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if isinstance(data, str):
        data = data.encode()
    with open(path, 'wb') as f:
        f.write(data)


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    n = 0
    for name, (ini, sandboxes, links, raw_ini) in CASES.items():
        root = os.path.join(OUT, name, 'root')
        os.makedirs(root)
        ini_path = os.path.join(root, 'data/plugins/ploader.ini')
        if raw_ini is not None:
            write_bytes(ini_path, raw_ini)
        elif ini == '@REAL_INI':
            os.makedirs(os.path.dirname(ini_path), exist_ok=True)
            shutil.copyfile(REAL_INI, ini_path)
        elif ini is not None:
            write_bytes(ini_path, ini)
        for inst, content in sandboxes.items():
            d = os.path.join(root, 'mnt/sandbox', inst)
            os.makedirs(d, exist_ok=True)
            if content is None:
                continue
            p = os.path.join(d, 'app0/sce_sys/param.json')
            if content == '@REAL_PARAM':
                os.makedirs(os.path.dirname(p), exist_ok=True)
                shutil.copyfile(REAL_PARAM, p)
            else:
                write_bytes(p, content)
        for inst, target in links.items():
            write_bytes(os.path.join(root, 'real-param.json'), param('PPSA33286'))
            p = os.path.join(root, 'mnt/sandbox', inst, 'app0/sce_sys/param.json')
            os.makedirs(os.path.dirname(p), exist_ok=True)
            os.symlink(target, p)
        n += 1
    with open(os.path.join(OUT, 'CASES.json'), 'w') as f:
        json.dump(sorted(CASES), f, indent=1)
    print(f'MAKE-FIXTURES: wrote {n} case(s) under {OUT}')
    if n == 0:
        print('MAKE-FIXTURES: FAIL - zero cases')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
