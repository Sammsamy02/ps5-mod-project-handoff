'use strict';
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const [directory, webFile] = process.argv.slice(2);
const code = fs.readFileSync(webFile, 'utf8');
const start = code.indexOf('const titlePageCodec = (() => {');
const endMarker = 'return {normalizeTitlePage, encode};\n  })();';
const end = code.indexOf(endMarker, start);
assert(start >= 0 && end > start, 'Actual frozen pure UI codec must exist');
const context = vm.createContext({Uint8Array});
vm.runInContext(code.slice(start, end + endMarker.length) + '\nthis.codec = titlePageCodec;', context);
const normalize = bytes => context.codec.normalizeTitlePage(new Uint8Array(bytes));
let checks = 0;
const check = (condition, label) => { ++checks; assert(condition, label); };
for (const name of fs.readdirSync(directory).filter(name => /-page-\d{3}\.json$/.test(name)).sort()) {
  const raw = fs.readFileSync(path.join(directory, name));
  const result = normalize(raw);
  check(result.ok && result.selectable === false, name + ': parsed page never selectable');
  check(result.page.items.every(item => item.build_fingerprint === null), name + ': metadata digest never fingerprint');
  check(raw.every(byte => byte < 128) && raw.length <= 8191, name + ': bounded ASCII wire');
  const text = raw.toString('ascii');
  const mutations = [
    '{"items":[],' + text.slice(1),
    ' ' + text,
    text.replace('"kind"', '"\\u006bind"'),
    text.replace('"schema_version":2', '"schema_version":2.0'),
    text.slice(0, -1),
  ];
  for (const mutation of mutations) {
    const rejected = normalize(Buffer.from(mutation));
    check(!rejected.ok && rejected.page === null && rejected.selectable === false,
      name + ': malformed/alternate bytes publish no parsed keys');
  }
  if (result.page.snapshot.issues.length) {
    const badIssue = text.replace(result.page.snapshot.issues[0].code, 'not_a_public_reason');
    const rejected = normalize(Buffer.from(badIssue));
    check(!rejected.ok && rejected.page === null, name + ': closed issue constants');
  }
}
console.log(`C_TO_ACTUAL_JS ${checks} checks 0 failures`);
