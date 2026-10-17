'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const crypto = require('node:crypto');
const web = path.join(__dirname, '../web');
const load = name => fs.existsSync(path.join(web, name)) ? require(path.join(web, name)) : null;
const config = load('config.js');
const Service = load('installer-service.js');
const Mock = load('installer-mock.js');
const game = () => ({ game_title_id: 'OWNED_GAME_42', name: 'Sample game', capability_file: { supported_mod_types: ['textures', 'scripts'] } });
const archive = () => ({ archive_path: 'mock:saved-archive', name: 'Readable menus', mod_type: 'textures', changes: [{ action: 'add', description: 'Add readable menus' }], warnings: ['This mod changes the menus.'], decisions: [{ id: 'keep_settings', question: 'Keep your existing settings?', default: true }] });
const fixture = options => {
  assert.ok(Mock?.create, 'The installer mock factory is available');
  return Mock.create({ games: [game()], archives: [archive()], ...options });
};
const refused = (operation, code) => assert.rejects(operation, error => error.code === code && typeof error.message === 'string' && error.message.length > 0);
const plan = adapter => adapter.plan('OWNED_GAME_42', 'mock:saved-archive');
const apply = (adapter, value, answers = { keep_settings: true }) => adapter.apply(value.plan_id, value.plan_sha256, answers);

test('preview plans never change installed records before apply', async () => {
  const { adapter } = fixture();
  const value = await plan(adapter);
  assert.deepEqual(Object.keys(adapter).sort(), ['apply', 'plan', 'restore', 'status']);
  assert.deepEqual(value.changes, [{ action: 'add', description: 'Add readable menus' }]);
  assert.equal(value.decisions[0].default, true);
  assert.match(value.plan_sha256, /^[a-f0-9]{64}$/);
  assert.deepEqual((await adapter.status('OWNED_GAME_42')).installed_mods, []);
});
test('plan digest changes when planned content changes', async () => {
  const first = fixture(), second = fixture({ archives: [{ ...archive(), changes: [{ action: 'replace', description: 'Replace the menu style' }] }] });
  assert.notEqual((await plan(first.adapter)).plan_sha256, (await plan(second.adapter)).plan_sha256);
});
test('apply and replay return one installation, restore removes it', async () => {
  const { adapter } = fixture(); const value = await plan(adapter);
  const result = await apply(adapter, value);
  assert.equal(result.ok, true); assert.ok(result.install_id);
  assert.deepEqual(await apply(adapter, value), result);
  const status = await adapter.status('OWNED_GAME_42');
  assert.equal(status.installed_mods.length, 1); assert.equal(status.installed_mods[0].name, 'Readable menus');
  assert.equal(status.installed_mods[0].enabled, true);
  assert.equal((await adapter.restore(result.install_id)).ok, true);
  assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
});
test('apply refuses unknown plans and wrong hashes without changing records', async () => {
  const { adapter } = fixture(); const value = await plan(adapter);
  await refused(adapter.apply('unknown', value.plan_sha256, { keep_settings: true }), 'unknown_plan');
  await refused(adapter.apply(value.plan_id, '0'.repeat(64), { keep_settings: true }), 'plan_drift');
  assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
});
test('answers are exact boolean decisions and replay cannot change them', async () => {
  const { adapter } = fixture(); const value = await plan(adapter);
  for (const answers of [{}, { keep_settings: 'yes' }, { keep_settings: true, extra: true }, null, []]) await refused(apply(adapter, value, answers), 'invalid_answers');
  const result = await apply(adapter, value, { keep_settings: false });
  assert.equal(result.ok, true);
  await refused(apply(adapter, value, { keep_settings: true }), 'intent_conflict');
});
test('game starting after plan refuses apply and preserves prior status', async () => {
  const { adapter, controls } = fixture(); const value = await plan(adapter);
  controls.setGameRunning('OWNED_GAME_42', true);
  await refused(apply(adapter, value), 'game_running');
  assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
});
test('changed archive and changed capability invalidate a plan', async () => {
  const { adapter, controls } = fixture(); const value = await plan(adapter);
  controls.registerArchive({ ...archive(), name: 'Changed menus' });
  await refused(apply(adapter, value), 'plan_drift');
  const next = await plan(adapter);
  controls.registerGame({ ...game(), capability_file: null });
  await refused(apply(adapter, next), 'unsupported_type');
});
test('different game IDs and unsupported or missing capabilities do not gain support', async () => {
  const { adapter } = fixture({ games: [game(), { game_title_id: 'OTHER_GAME', name: 'Other game', capability_file: null }] });
  assert.equal((await adapter.status('OTHER_GAME')).capability_file, null);
  await refused(adapter.plan('OTHER_GAME', 'mock:saved-archive'), 'unsupported_type');
  await refused(adapter.plan('UNKNOWN_GAME', 'mock:saved-archive'), 'unavailable');
});
test('malformed call arguments are rejected before mutation', async () => {
  const { adapter } = fixture();
  await refused(adapter.plan('', 'mock:saved-archive'), 'invalid_input');
  await refused(adapter.plan('OWNED_GAME_42', ''), 'invalid_input');
  await refused(adapter.plan('OWNED_GAME_42', 'mock:saved-archive', 'extra'), 'invalid_input');
  await refused(adapter.status('OWNED_GAME_42', 'extra'), 'invalid_input');
  await refused(adapter.restore({ install_id: 'wrong shape' }), 'invalid_input');
  await refused(adapter.restore('unknown'), 'unknown_install');
});
for (const scenario of ['no_internet', 'nexus_expired', 'corrupt_download', 'unsupported_type', 'game_running', 'console_busy']) {
  test(`preview explains ${scenario} without installing`, async () => {
    const { adapter, controls } = fixture(); controls.setScenario(scenario);
    await refused(plan(adapter), scenario);
    controls.setScenario(null);
    assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
  });
}
for (const scenario of ['install_refused', 'plan_drift', 'console_busy', 'no_internet']) {
  test(`apply explains ${scenario} without changing records`, async () => {
    const { adapter, controls } = fixture(); const value = await plan(adapter); controls.setScenario(scenario);
    await refused(apply(adapter, value), scenario); controls.setScenario(null);
    assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
  });
}
test('mock management stays separate from installer methods and preserves order', async () => {
  const { adapter, controls } = fixture(); const a = await apply(adapter, await plan(adapter));
  controls.registerArchive({ ...archive(), archive_path: 'mock:second', name: 'Second style' });
  const b = await apply(adapter, await adapter.plan('OWNED_GAME_42', 'mock:second'));
  controls.setEnabled('OWNED_GAME_42', a.install_id, false);
  controls.reorder('OWNED_GAME_42', [b.install_id, a.install_id]);
  const status = await adapter.status('OWNED_GAME_42');
  assert.deepEqual(status.installed_mods.map(mod => [mod.install_id, mod.enabled, mod.load_order]), [[b.install_id, true, 0], [a.install_id, false, 1]]);
  assert.equal(status.mock, true); assert.equal(adapter.setEnabled, undefined);
  assert.throws(() => controls.reorder('OWNED_GAME_42', [a.install_id]), error => error.code === 'invalid_input');
});
test('status copies prevent callers changing state and optional preview progress survives navigation', async () => {
  const { adapter, controls } = fixture();
  controls.setSetup('OWNED_GAME_42', { state: 'setting_up', message: 'Setting up this game for mods — one time only' });
  controls.setProgress('OWNED_GAME_42', { stage: 'converting_textures', message: 'Converting textures', estimated_minutes: 10, percent: 25, can_leave: true });
  const status = await adapter.status('OWNED_GAME_42'); status.capability_file.supported_mod_types.length = 0;
  const next = await adapter.status('OWNED_GAME_42');
  assert.equal(next.capability_file.supported_mod_types.length, 2); assert.equal(next.setup.state, 'setting_up'); assert.equal(next.progress.estimated_minutes, 10);
});
test('only injected owned mock storage persists and restored state is checked', async () => {
  const values = new Map(); const storage = { getItem: key => values.get(key) || null, setItem: (key, value) => values.set(key, value) };
  const first = fixture({ storage }); const installed = await apply(first.adapter, await plan(first.adapter));
  const restarted = fixture({ storage }); assert.equal((await restarted.adapter.status('OWNED_GAME_42')).installed_mods[0].install_id, installed.install_id);
  values.set('modbridge.installer.mock.v1', '{"version":1,"games":"bad"}');
  assert.throws(() => fixture({ storage }), error => error.code === 'invalid_mock_state');
});
test('failed preview persistence rolls back apply and allows the same plan to retry', async () => {
  let broken = false;
  const storage = { getItem: () => null, setItem: () => { if (broken) throw new Error('Full storage'); } };
  const { adapter } = fixture({ storage }); const value = await plan(adapter); broken = true;
  await refused(apply(adapter, value), 'invalid_mock_state');
  assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 0);
  broken = false; assert.equal((await apply(adapter, value)).ok, true);
  assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 1);
});
test('duplicate persisted identities are refused instead of silently replacing records', () => {
  const data = { version: 1, serial: 0, games: [game(), game()], archives: [archive()] };
  assert.throws(() => fixture({ storage: { getItem: () => JSON.stringify(data), setItem() {} } }), error => error.code === 'invalid_mock_state');
});
test('concurrent apply confirmations remain one installation', async () => {
  const { adapter } = fixture(); const value = await plan(adapter);
  const [first, second] = await Promise.all([apply(adapter, value), apply(adapter, value)]);
  assert.deepEqual(first, second); assert.equal((await adapter.status('OWNED_GAME_42')).installed_mods.length, 1);
});
test('new installations do not reuse an existing seeded installation identity', async () => {
  const { adapter } = fixture({ games: [{ ...game(), installed_mods: [{ install_id: 'preview-install-2', name: 'Previous mod', mod_type: 'scripts', enabled: true }] }] });
  const result = await apply(adapter, await plan(adapter));
  assert.notEqual(result.install_id, 'preview-install-2');
  assert.equal(new Set((await adapter.status('OWNED_GAME_42')).installed_mods.map(mod => mod.install_id)).size, 2);
});
test('service defaults explicitly to the configured mock without network', async () => {
  assert.ok(config && Service?.create, 'Service and centralized config are available');
  let calls = 0; const session = fixture();
  const adapter = Service.create({ config, mock: session.adapter, fetch: async () => { calls++; throw new Error('must not use network'); } });
  await plan(adapter); assert.equal(calls, 0);
});
test('real service sends exact call bodies and no preview-only metadata is required', async () => {
  assert.ok(Service?.create); const calls = [];
  const responses = [{ plan_id: 'real-plan', plan_sha256: 'a'.repeat(64), changes: [], warnings: [], decisions: [] }, { ok: true, install_id: 'real-install', report: { message: 'Installed' } }, { ok: true, report: { message: 'Restored' } }, { installed_mods: [], capability_file: null }];
  const fetch = async (url, options) => { calls.push({ url, options }); return { ok: true, json: async () => responses.shift() }; };
  const adapter = Service.create({ config: { installer: { mode: 'real', paths: { plan: '/plan', apply: '/apply', restore: '/restore', status: '/status' } } }, fetch, csrf: () => 'session-token' });
  await adapter.plan('OWNED_GAME_42', 'trusted:saved'); await adapter.apply('real-plan', 'a'.repeat(64), {}); await adapter.restore('real-install'); await adapter.status('OWNED_GAME_42');
  assert.deepEqual(calls.map(call => [call.url, call.options.method, call.options.body ? JSON.parse(call.options.body) : null]), [
    ['/plan', 'POST', { game_title_id: 'OWNED_GAME_42', archive_path: 'trusted:saved' }],
    ['/apply', 'POST', { plan_id: 'real-plan', plan_sha256: 'a'.repeat(64), answers: {} }],
    ['/restore', 'POST', { install_id: 'real-install' }], ['/status?game_title_id=OWNED_GAME_42', 'GET', null],
  ]);
  assert.equal(calls[0].options.headers['X-CSRF-Token'], 'session-token'); assert.equal(calls[0].options.credentials, 'same-origin');
});
test('failed real transport never falls back to successful preview', async () => {
  assert.ok(Service?.create); let mockCalls = 0;
  const adapter = Service.create({ config: { installer: { mode: 'real', paths: { plan: '/plan', apply: '/apply', restore: '/restore', status: '/status' } } }, fetch: async () => { throw new Error('/private/path 0xdead'); }, mock: { plan: async () => { mockCalls++; return { ok: true }; } } });
  await refused(adapter.plan('OWNED_GAME_42', 'trusted:saved'), 'no_internet'); assert.equal(mockCalls, 0);
});
test('malformed real replies and refusal clear success rather than being accepted', async () => {
  assert.ok(Service?.create); let body = { plan_id: 'p', plan_sha256: 'bad', changes: [], warnings: [], decisions: [] };
  const adapter = Service.create({ config: { installer: { mode: 'real', paths: { plan: '/plan', apply: '/apply', restore: '/restore', status: '/status' } } }, fetch: async () => ({ ok: true, json: async () => body }) });
  await refused(adapter.plan('OWNED_GAME_42', 'trusted:saved'), 'invalid_reply');
  body = { ok: false, report: { code: 'game_running', message: '/private/path 0xbad' } };
  const reply = await adapter.apply('p', 'a'.repeat(64), {}); assert.equal(reply.ok, false); assert.equal(reply.report.code, 'game_running'); assert(!reply.report.message.includes('/private'));
});
test('real replies reject malformed nested items before screen rendering', async () => {
  let body;
  const adapter = Service.create({ config: { installer: { mode: 'real', paths: { plan: '/plan', apply: '/apply', restore: '/restore', status: '/status' } } }, fetch: async () => ({ ok: true, json: async () => body }) });
  const validPlan = { plan_id: 'p', plan_sha256: 'a'.repeat(64), changes: [], warnings: [], decisions: [] };
  for (const fields of [{ changes: [null] }, { changes: [42] }, { changes: [{ description: 42 }] }, { warnings: [null] }, { warnings: [[]] }, { warnings: [{ message: 42 }] }]) {
    body = { ...validPlan, ...fields }; await refused(adapter.plan('OWNED_GAME_42', 'trusted:saved'), 'invalid_reply');
  }
  for (const fields of [{ installed_mods: [null] }, { installed_mods: [{}] }, { installed_mods: [{ install_id: 'i', enabled: 'yes' }] }, { installed_mods: [{ install_id: 'i' }, { install_id: 'i' }] }, { capability_file: [] }, { capability_file: { supported_mod_types: 'textures' } }]) {
    body = { installed_mods: [], capability_file: null, ...fields }; await refused(adapter.status('OWNED_GAME_42'), 'invalid_reply');
  }
});
test('real replies preserve plain changes/warnings and do not invent optional capability schema', async () => {
  let body = { plan_id: 'p', plan_sha256: 'a'.repeat(64), changes: ['Add the selected mod', { action: 'add' }], warnings: ['Close the game first', { message: 'Review this choice' }], decisions: [] };
  const adapter = Service.create({ config: { installer: { mode: 'real', paths: { plan: '/plan', apply: '/apply', restore: '/restore', status: '/status' } } }, fetch: async () => ({ ok: true, json: async () => body }) });
  assert.deepEqual((await adapter.plan('OWNED_GAME_42', 'trusted:saved')).changes, ['Add the selected mod', { action: 'add' }]);
  body = { installed_mods: [{ install_id: 'i' }], capability_file: {} };
  assert.equal((await adapter.status('OWNED_GAME_42')).capability_file, null);
  body = { installed_mods: [{ install_id: 'i' }], capability_file: { version: 1, available_types: ['textures'] } };
  assert.deepEqual((await adapter.status('OWNED_GAME_42')).capability_file, { version: 1, available_types: ['textures'] });
});
test('configured routes cannot normalize to another origin or hide endpoint text', () => {
  let fetches = 0;
  for (const route of ['/\\example.com/installer', '/api/plan\n', '/api/plan\u0000', '//example.com/installer', 'https://example.com/installer', '/api/plan#hidden']) {
    assert.throws(() => Service.create({ config: { installer: { mode: 'real', paths: { plan: route, apply: '/apply', restore: '/restore', status: '/status' } } }, fetch: async () => { fetches++; } }), error => error.code === 'configuration');
  }
  assert.equal(fetches, 0);
});
test('browser globals load without CommonJS and use genuine crypto SHA256', async () => {
  for (const name of ['config.js', 'installer-service.js', 'installer-mock.js']) assert(fs.existsSync(path.join(web, name)), `${name} exists`);
  const context = vm.createContext({ crypto: crypto.webcrypto, TextEncoder, URLSearchParams });
  context.window = context; context.globalThis = context;
  for (const name of ['config.js', 'installer-service.js', 'installer-mock.js']) vm.runInContext(fs.readFileSync(path.join(web, name), 'utf8'), context);
  assert.equal(context.ModBridgeConfig.installer.mode, 'mock');
  const session = context.ModBridgeInstallerMock.create({ games: [game()], archives: [archive()] });
  const value = await session.adapter.plan('OWNED_GAME_42', 'mock:saved-archive'); assert.match(value.plan_sha256, /^[a-f0-9]{64}$/);
});
test('SHA256 fallback passes NIST known vectors without Web Crypto', async () => {
  assert.equal(typeof Mock.sha256, 'function');
  for (const [input, expected] of [
    ['', 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'],
    ['abc', 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'],
    ['abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq', '248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1'],
    ['a'.repeat(1000000), 'cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0'],
  ]) assert.equal(await Mock.sha256(input, {}), expected);
});
test('SHA256 fallback matches Web Crypto and Node across Unicode and block boundaries', async () => {
  assert.equal(typeof Mock.sha256, 'function');
  const inputs = ['café 🕹️ 日本語', '\u0000\ud800\udfff', ...[55, 56, 63, 64, 65, 119, 120, 127, 128, 129].map(length => 'x'.repeat(length))];
  for (const input of inputs) {
    const expected = crypto.createHash('sha256').update(input).digest('hex');
    assert.equal(await Mock.sha256(input, {}), expected);
    assert.equal(await Mock.sha256(input, crypto.webcrypto), expected);
  }
});
test('ordinary HTTP browser with no crypto.subtle can preview and apply the same bound plan', async () => {
  const context = vm.createContext({ TextEncoder }); context.window = context; context.globalThis = context;
  for (const name of ['config.js', 'installer-service.js', 'installer-mock.js']) vm.runInContext(fs.readFileSync(path.join(web, name), 'utf8'), context);
  const session = context.ModBridgeInstallerMock.create({ games: [game()], archives: [archive()] });
  const value = await plan(session.adapter); const secure = await plan(fixture().adapter);
  assert.equal(value.plan_sha256, secure.plan_sha256); assert.equal((await apply(session.adapter, value)).ok, true);
});
test('per-game conflict fixture reaches status without implying real management support', async () => {
  const { adapter } = fixture({ games: [{ ...game(), conflicts: [{ mod_ids: ['preview-one', 'preview-two'] }] }] });
  assert.deepEqual((await adapter.status('OWNED_GAME_42')).conflicts, [{ mod_ids: ['preview-one', 'preview-two'] }]);
});
