'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const crypto = require('node:crypto');
const { chromium } = require('playwright');

// The full production app, owned loopback services and injected standard gamepad.
// Test setup may configure the public mock service; every user action is d-pad/X/O.
const webRoot = path.join(__dirname, '..', 'web');
const evidenceRoot = process.env.UI_EVIDENCE_ROOT ? path.resolve(process.env.UI_EVIDENCE_ROOT) : path.join(__dirname, '..', 'docs', 'ui', 'step-02', 'controller');
const sizes = [{ width: 1920, height: 1080 }, { width: 3840, height: 2160 }];
const directions = { up: 12, down: 13, left: 14, right: 15 };
async function pad(page, name) {
  const index = directions[name] ?? ({ cross: 0, circle: 1 })[name]; assert.notEqual(index, undefined);
  await page.evaluate(index => { window.installerPad.buttons[index].pressed = true; window.installerPad.buttons[index].value = 1; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  await page.evaluate(index => { window.installerPad.buttons[index].pressed = false; window.installerPad.buttons[index].value = 0; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function focusState(page, selector) {
  return page.evaluate(selector => {
    const all = [...document.querySelectorAll('button,input,a[href],[tabindex]')];
    const key = el => el.id || `${el.tagName}:${all.indexOf(el)}:${el.dataset.installerKey || ''}`;
    const active = document.activeElement;
    const target = [...document.querySelectorAll(selector)].find(el => el.getClientRects().length && !el.disabled && !el.closest('[hidden]'));
    const rect = el => { const r = el?.getBoundingClientRect(); return r ? { x: r.x + r.width / 2, y: r.y + r.height / 2 } : { x: 0, y: 0 }; };
    return { exists: !!target, matched: active === target, key: key(active), active: rect(active), target: rect(target) };
  }, selector);
}
async function reach(page, selector) {
  const graph = new Map(), get = key => { if (!graph.has(key)) graph.set(key, {}); return graph.get(key); };
  for (let move = 0; move < 320; move++) {
    const before = await focusState(page, selector); assert(before.exists, `Visible enabled target missing: ${selector}`); if (before.matched) return;
    const dx = before.target.x - before.active.x, dy = before.target.y - before.active.y;
    const preferred = Math.abs(dy) > Math.abs(dx) ? [dy >= 0 ? 'down' : 'up', dx >= 0 ? 'right' : 'left'] : [dx >= 0 ? 'right' : 'left', dy >= 0 ? 'down' : 'up'];
    let direction = [...new Set([...preferred, 'down', 'right', 'up', 'left'])].find(item => !Object.hasOwn(get(before.key), item));
    if (!direction) {
      const queue = [{ key: before.key, first: null }], seen = new Set([before.key]);
      while (queue.length && !direction) {
        const node = queue.shift(); if (Object.keys(get(node.key)).length < 4) { direction = node.first; break; }
        for (const [edge, next] of Object.entries(get(node.key))) if (!seen.has(next)) { seen.add(next); queue.push({ key: next, first: node.first || edge }); }
      }
    }
    if (!direction) {
      fs.writeFileSync(path.join(evidenceRoot, `unreachable-${page.viewportSize().width}-${selector.replace(/[^a-z0-9_-]/gi, '_')}.json`), JSON.stringify({ selector, graph: Object.fromEntries(graph), state: before }, null, 2) + '\n');
      assert.fail(`Controller focus graph cannot reach ${selector}`);
    }
    await pad(page, direction); get(before.key)[direction] = (await focusState(page, selector)).key;
  }
  assert.fail(`Controller navigation exceeded its bound while reaching ${selector}`);
}
async function select(page, selector) { await reach(page, selector); await pad(page, 'cross'); }
async function screenshot(page, viewport, name) {
  if (viewport.width === 1920) await page.screenshot({ path: path.join(evidenceRoot, `${name}-1080.png`) });
}
async function assertUsableFocus(page, viewport) {
  const state = await page.evaluate(() => {
    const el = document.activeElement, r = el.getBoundingClientRect(), css = getComputedStyle(el);
    return { usable: el !== document.body && !el.disabled && el.getClientRects().length > 0 && !el.closest('[hidden]'), rect: { x: r.x, y: r.y, right: r.right, bottom: r.bottom }, outline: css.outlineStyle, outlineWidth: parseFloat(css.outlineWidth), scrollWidth: document.documentElement.scrollWidth };
  });
  assert(state.usable, 'Controller focus was lost or left on an unavailable action');
  assert(state.outline !== 'none' && state.outlineWidth >= 3 * viewport.width / 1920, 'Controller focus has no clear outline');
  assert(state.rect.x >= 0 && state.rect.y >= 0 && state.rect.right <= viewport.width + 1 && state.rect.bottom <= viewport.height + 1, `Focused action is outside the TV viewport: ${JSON.stringify(state.rect)}`);
  assert(state.scrollWidth <= viewport.width + 1, 'Installer page overflows horizontally');
}
async function configurePreview(page, options = {}) {
  await page.evaluate(options => {
    const game = { game_title_id: 'OWNED_CONTROLLER_GAME', name: 'Sample adventure', capability_file: options.missing ? null : { supported_mod_types: ['textures'] }, game_running: options.running === true,
      installed_mods: [{ install_id: 'owned-soft', name: 'Soft lights', mod_type: 'textures', enabled: true, update_available: true }, { install_id: 'owned-warm', name: 'Warm lights', mod_type: 'textures', enabled: true }],
      conflicts: [{ mod_ids: ['owned-soft', 'owned-warm'] }], last_session: [{ install_id: 'owned-soft', worked: true }, { install_id: 'owned-warm', worked: false, reason: 'The game could not use this mod.' }],
      self_test: { state: options.selfTestFailed ? 'failed' : 'passed', message: options.selfTestFailed ? 'Check your mod settings.' : 'The game is ready for supported mods.', diagnostics: options.long ? Array.from({ length: 100 }, (_, i) => `Owned diagnostic line ${i + 1}`) : [] } };
    const archive = { archive_path: 'mock:sample-textures', name: 'Sample textures', mod_type: 'textures', changes: [{ action: 'add', description: 'Add the selected texture mod' }], warnings: ['Another mod changes some of the same files.'], decisions: [{ id: 'keep_settings', question: 'Keep your existing settings?', default: true }] };
    const games = [game];
    if (options.observed) for (let i = 0; i < 2; i++) games.push({ game_title_id: `PPSA9000${i}`, name: `Observed game ${i + 1}`, capability_file: { supported_mod_types: ['textures'] } });
    const session = window.ModBridgeInstallerMock.create({ games, archives: [archive] });
    const fixture = window.installerFixture = { calls: [], controls: session.controls, finish: null, finishStatus: null, holdNextStatus: false, lastInstallId: null };
    if (options.slow) session.controls.setProgress(game.game_title_id, { stage: 'converting', message: 'Converting textures.', estimated_minutes: 10, percent: 30, can_leave: true });
    const adapter = Object.fromEntries(['status', 'plan', 'apply', 'restore'].map(method => [method, async (...args) => {
      fixture.calls.push({ method, args });
      if (method === 'status' && fixture.holdNextStatus) { fixture.holdNextStatus = false; await new Promise(resolve => { fixture.finishStatus = resolve; }); }
      if (method === 'apply' && options.slow) await new Promise(resolve => { fixture.finish = resolve; });
      const result = await session.adapter[method](...args);
      if (method === 'apply' && result.ok) fixture.lastInstallId = result.install_id;
      return result;
    }]));
    const controls = { ...session.controls,
      setEnabled(...args) { fixture.calls.push({ method: 'setEnabled', args }); return session.controls.setEnabled(...args); },
      reorder(...args) { fixture.calls.push({ method: 'reorder', args }); return session.controls.reorder(...args); } };
    const service = window.ModBridgeInstaller.create({ config: window.ModBridgeConfig, mock: adapter });
    if (!window.ModBridgeInstallerUI.init({ service, controls, mock: true, games })) throw new Error('Public installer initialization failed');
  }, options);
  await select(page, '#tab-installed');
  await select(page, '[data-installer-game="OWNED_CONTROLLER_GAME"]');
  await page.locator('[data-installer-action="plan"]').waitFor();
}
const action = name => `[data-installer-action="${name}"]`;
async function main() {
  fs.mkdirSync(evidenceRoot, { recursive: true });
  const server = http.createServer((req, res) => {
    const name = new URL(req.url, 'http://localhost').pathname.slice(1) || 'index.html';
    if (!/^[a-z0-9-]+\.(?:html|css|js)$/.test(name) || !fs.existsSync(path.join(webRoot, name))) return res.writeHead(404).end();
    res.writeHead(200, { 'Content-Type': name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript' }); res.end(fs.readFileSync(path.join(webRoot, name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const options = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) options.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) options.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(options), contexts = [], results = [];
  async function fixture(viewport, extra = {}) {
    const context = await browser.newContext({ viewport }); contexts.push(context); const api = { errors: [], outbound: [], observed: false, generation: 'c'.repeat(32), ...extra };
    await context.addInitScript(options => {
      window.installerPad = { mapping: 'standard', index: 0, connected: true, axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })) };
      Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.installerPad] });
      if (options.savedTarget) sessionStorage.setItem('modbridge.navigation.v1', JSON.stringify({ domain: '', mod: '', file: '', target_ref: { snapshot_id: 'c'.repeat(32), title_key: '1'.repeat(32) } }));
    }, extra);
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, route => {
      const url = new URL(route.request().url()), json = body => route.fulfill({ contentType: 'application/json', body: JSON.stringify(body) });
      if (url.pathname === '/api/capabilities') return json({ mode: 'desktop_preparation', csrf_token: 'owned-installer-controller-csrf', capabilities: { nexus_catalog: true, account_status: true, detected_titles: api.observed, jobs: false, installed_records: false, download_analyze: false, deploy: false } });
      if (url.pathname === '/api/account') return json({ status: 'connected', live_service: 'verified', account: { display_name: 'TV player' } });
      if (url.pathname === '/api/games') return json({ items: [{ domain_name: 'one', name: 'Game One' }] });
      if (url.pathname === '/api/detected-titles' || url.pathname === '/api/detected-titles/refresh') {
        const observed = value => ({ value, status: 'observed', source: 'owned_controller_fixture' });
        return json(api.observed ? { schema_version: 1, status: 'observed', complete: true, snapshot_id: api.generation, source_scope: 'running_processes', reason: null, issues: [], items: [0, 1].map(i => ({ title_key: String(i + 1).repeat(32), title_id: observed(`PPSA9000${i}`), display_name: observed(`Observed game ${i + 1}`), build_version: observed('1'), build_fingerprint: null })) } : { schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'unavailable', reason: 'platform_provider_unavailable', issues: [], items: [] });
      }
      return json({ items: [] });
    });
    const page = await context.newPage(); page.setDefaultTimeout(4000); page.on('pageerror', error => api.errors.push(error.message));
    await page.goto(origin); await page.locator('[data-card="game:one"]').waitFor();
    return { page, api };
  }
  async function test(name, action) {
    if (process.env.UI_TEST_FILTER && !new RegExp(process.env.UI_TEST_FILTER).test(name)) return;
    const before = contexts.length;
    try { await action(); results.push({ name, passed: true }); console.log(`PASS ${name}`); }
    catch (error) { results.push({ name, passed: false, error: error.message }); console.error(`FAIL ${name}: ${error.stack}`); }
    finally { for (const context of contexts.slice(before)) await context.close(); }
  }
  try {
    for (const viewport of sizes) {
      await test(`${viewport.width}: Installed exposes the ready installer in the full app`, async () => {
        const { page, api } = await fixture(viewport);
        await select(page, '#tab-installed');
        assert(await page.locator('#installer-panel').isVisible(), 'Installed has no ready installer panel');
        assert(await page.evaluate(() => typeof window.ModBridgeInstallerUI?.init === 'function'), 'The full app did not load its installer UI module');
        assert.equal(await page.locator('[data-installer-game]').count() > 0, true);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: preview plan, decisions, apply and restore use controller confirmation`, async () => {
        const { page, api } = await fixture(viewport); await configurePreview(page);
        await select(page, action('plan')); await page.locator('[data-installer-state="plan"]').waitFor();
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'apply').length), 0);
        await select(page, '[data-installer-answer="keep_settings"][data-value="true"]');
        await select(page, '[data-installer-answer="keep_settings"][data-value="false"]');
        assert.equal(await page.locator('[data-installer-answer="keep_settings"][data-value="false"]').getAttribute('aria-pressed'), 'true');
        await assertUsableFocus(page, viewport); await screenshot(page, viewport, 'plan');
        await select(page, action('apply')); await page.locator('[data-installer-state="result"]').waitFor();
        assert.match(await page.locator('#installer-panel').innerText(), /Preview complete/);
        assert.deepEqual(await page.evaluate(() => installerFixture.calls.find(item => item.method === 'apply').args[2]), { keep_settings: false });
        await assertUsableFocus(page, viewport); await screenshot(page, viewport, 'result');
        await select(page, action('done')); await page.locator('[data-installer-state="game"]').waitFor();
        assert.equal(await page.locator('.installer-mod').count(), 3);
        const installId = await page.evaluate(() => installerFixture.lastInstallId);
        await select(page, `[data-installer-restore="${installId}"]`);
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'restore').length), 0);
        await page.locator('[data-installer-state="restore"]').waitFor(); await screenshot(page, viewport, 'restore-confirmation');
        await pad(page, 'circle'); await page.locator('[data-installer-state="game"]').waitFor();
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'restore').length), 0);
        await select(page, `[data-installer-restore="${installId}"]`); await select(page, action('restore-confirm'));
        await page.locator('[data-installer-state="result"]').waitFor();
        assert.deepEqual(await page.evaluate(() => installerFixture.calls.find(item => item.method === 'restore').args), [installId]);
        await select(page, action('done')); assert.equal(await page.locator('.installer-mod').count(), 2);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: More info scrolls fully and O cancels a plan without applying`, async () => {
        const { page, api } = await fixture(viewport); await configurePreview(page, { long: true });
        await select(page, action('more')); await page.waitForFunction(() => document.querySelector('#installer-info-dialog').open);
        await reach(page, '[data-installer-details]');
        assert(await page.locator('[data-installer-details]').evaluate(el => el.scrollHeight > el.clientHeight));
        for (let step = 0; step < 80; step++) {
          if (await page.locator('[data-installer-details]').evaluate(el => el.scrollTop + el.clientHeight >= el.scrollHeight - 2)) break;
          assert(await page.evaluate(() => document.activeElement.matches('[data-installer-details]')), 'Focus left More info before the end');
          await pad(page, 'down');
        }
        assert(await page.locator('[data-installer-details]').evaluate(el => el.scrollTop > 0 && el.scrollTop + el.clientHeight >= el.scrollHeight - 2));
        await assertUsableFocus(page, viewport); await screenshot(page, viewport, 'more-info-end'); await pad(page, 'circle');
        assert(await page.evaluate(() => document.activeElement.matches('[data-installer-action="more"]')));
        await select(page, action('plan')); await page.locator('[data-installer-state="plan"]').waitFor(); await pad(page, 'circle');
        await page.locator('[data-installer-state="game"]').waitFor();
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'apply').length), 0);
        await assertUsableFocus(page, viewport); assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: long progress survives leaving through real app tabs`, async () => {
        const { page, api } = await fixture(viewport); await configurePreview(page, { slow: true });
        await select(page, action('plan')); await page.locator('[data-installer-state="plan"]').waitFor(); await select(page, action('apply'));
        await page.waitForFunction(() => typeof installerFixture.finish === 'function');
        await page.locator('[data-installer-state="progress"]').waitFor();
        assert.match(await page.locator('#installer-panel').innerText(), /Converting textures.*About 10 minutes/s);
        await select(page, '#tab-activity'); assert.equal(await page.locator('#installer-panel').isVisible(), false);
        await select(page, '#tab-installed'); await page.locator('[data-installer-state="progress"]').waitFor();
        assert.match(await page.locator('#installer-panel').innerText(), /leave this screen/);
        await reach(page, '[data-installer-state="progress"]');
        await assertUsableFocus(page, viewport); await screenshot(page, viewport, 'progress');
        await page.evaluate(() => installerFixture.finish()); await page.locator('[data-installer-state="result"]').waitFor();
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'apply').length), 1);
        await assertUsableFocus(page, viewport); assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: preview controls show and change order, on/off, session and self-test results`, async () => {
        const { page, api } = await fixture(viewport); await configurePreview(page);
        const copy = await page.locator('#installer-panel').innerText();
        for (const phrase of [/Update available/, /lower in the list wins/, /Last play session: Worked/, /Last play session: Didn't work/, /Game self-test: Passed/, /one time only/]) assert.match(copy, phrase);
        await select(page, '[data-installer-toggle="owned-soft"]');
        assert.equal(await page.locator('[data-installer-toggle="owned-soft"]').innerText(), 'Turn on');
        assert.deepEqual(await page.evaluate(() => installerFixture.calls.find(item => item.method === 'setEnabled').args), ['OWNED_CONTROLLER_GAME', 'owned-soft', false]);
        await select(page, '[data-installer-toggle="owned-soft"]');
        await select(page, '[data-installer-move="owned-soft"][data-direction="down"]');
        assert.deepEqual(await page.locator('.installer-mod h4').allTextContents(), ['1. Warm lights', '2. Soft lights']);
        await assertUsableFocus(page, viewport);
        await select(page, '[data-installer-move="owned-soft"][data-direction="up"]');
        assert.deepEqual(await page.locator('.installer-mod h4').allTextContents(), ['1. Soft lights', '2. Warm lights']);
        await screenshot(page, viewport, 'game-preview');
        await reach(page, action('more')); await screenshot(page, viewport, 'game-controls-conflicts');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: all three lookup fields and observed title controls are reachable by d-pad`, async () => {
        const { page, api } = await fixture(viewport, { observed: true });
        for (const selector of ['#lookup-url', '#lookup-domain', '#lookup-mod-id', '[data-title-key]']) {
          await reach(page, selector); await assertUsableFocus(page, viewport);
        }
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: running games and missing support information block install actions`, async () => {
        for (const options of [{ running: true }, { missing: true }, { selfTestFailed: true }]) {
          const { page, api } = await fixture(viewport); await configurePreview(page, options);
          const copy = await page.locator('#installer-panel').innerText();
          if (options.running || options.missing) {
            assert.equal(await page.locator(action('plan')).isDisabled(), true);
            assert.match(copy, options.running ? /Close the game first/ : /not available yet/);
            assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'plan' || item.method === 'apply').length), 0);
            await reach(page, action('more')); await assertUsableFocus(page, viewport);
          } else assert.match(copy, /Game self-test: Needs attention/);
          assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
        }
      });
      await test(`${viewport.width}: observed target selection and clearing keep the installer game and plan in sync`, async () => {
        const { page, api } = await fixture(viewport, { observed: true }); await configurePreview(page, { observed: true });
        await select(page, '#tab-games'); await select(page, '[data-title-key="' + '1'.repeat(32) + '"]');
        await page.waitForFunction(() => installerFixture.calls.some(item => item.method === 'status' && item.args[0] === 'PPSA90000'));
        await page.evaluate(() => ModBridgeInstallerUI.offerArchive({ game_title_id: 'PPSA90000', archive_path: 'mock:sample-textures', label: 'Sample textures' }));
        await select(page, '#tab-installed'); assert.equal(await page.locator('[data-installer-state="game"] h3').first().innerText(), 'Observed game 1');
        await select(page, action('plan')); await page.locator('[data-installer-state="plan"]').waitFor();
        await select(page, '#tab-games'); await select(page, '#clear-target');
        await select(page, '#tab-installed');
        assert.equal(await page.locator('[data-installer-state="plan"]').count(), 0, 'Clearing the console target kept the old installer plan');
        assert.equal(await page.locator('[data-installer-game]').count() > 0, true, 'Clearing the target kept the old installer game');
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'apply').length), 0);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: a valid restored target selects the installer game and invalidation clears it`, async () => {
        const { page, api } = await fixture(viewport, { observed: true, savedTarget: true });
        await page.waitForFunction(() => document.querySelector('[data-title-key][aria-pressed="true"]'));
        await select(page, '#tab-installed');
        await page.waitForFunction(() => document.querySelector('[data-installer-state="game"] h3')?.textContent === 'Observed game 1');
        await select(page, '#tab-games'); api.generation = 'd'.repeat(32); await select(page, '#reload-titles');
        await page.locator('#reload-titles:not(:disabled)').waitFor();
        assert.equal(await page.locator('[data-title-key][aria-pressed="true"]').count(), 0);
        await select(page, '#tab-installed');
        assert.equal(await page.locator('[data-installer-game]').count() > 0, true, 'A stale target left its old installer game selected');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: target controls lock during status and apply while browsing tabs keep working`, async () => {
        const { page, api } = await fixture(viewport, { observed: true }); await configurePreview(page, { observed: true, slow: true });
        await select(page, '#tab-games'); await page.evaluate(() => { installerFixture.holdNextStatus = true; });
        await select(page, '[data-title-key="' + '2'.repeat(32) + '"]');
        await page.waitForFunction(() => typeof installerFixture.finishStatus === 'function');
        const checkLocked = async () => {
          assert.equal(await page.locator('[data-title-key]:not(:disabled)').count(), 0, 'Target selection remains enabled during installer work');
          assert.equal(await page.locator('#clear-target').isDisabled(), true);
          assert.equal(await page.locator('#reload-titles').isDisabled(), true);
        };
        await checkLocked(); await select(page, '#tab-activity'); assert.equal(await page.locator('#activity-view').isVisible(), true);
        await select(page, '#tab-games'); await reach(page, '#catalog-sort');
        await page.evaluate(() => installerFixture.finishStatus());
        await page.waitForFunction(() => !document.querySelector('#reload-titles').disabled);
        assert.equal(await page.evaluate(() => document.activeElement.id), 'catalog-sort', 'Completing installer status stole intervening controller focus');
        await page.locator('[data-title-key]:not(:disabled)').first().waitFor();
        await page.evaluate(() => ModBridgeInstallerUI.offerArchive({ game_title_id: 'PPSA90001', archive_path: 'mock:sample-textures', label: 'Sample textures' }));
        await select(page, '#tab-installed'); await select(page, action('plan')); await page.locator('[data-installer-state="plan"]').waitFor();
        await select(page, action('apply')); await page.waitForFunction(() => typeof installerFixture.finish === 'function');
        await select(page, '#tab-games'); await checkLocked(); await select(page, '#tab-activity');
        assert.equal(await page.locator('#activity-view').isVisible(), true);
        await page.evaluate(() => installerFixture.finish()); await page.waitForFunction(() => document.querySelector('[data-installer-state="result"]'));
        await select(page, '#tab-games'); await page.locator('[data-title-key]:not(:disabled)').first().waitFor();
        assert.equal(await page.locator('[data-title-key="' + '2'.repeat(32) + '"]').getAttribute('aria-pressed'), 'true');
        assert.equal(await page.evaluate(() => installerFixture.calls.filter(item => item.method === 'apply').length), 1);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      const errors = { no_internet: /No internet connection/, nexus_expired: /Nexus login expired/, corrupt_download: /download is damaged/, unsupported_type: /not supported for this game yet/, install_refused: /install was refused/, game_running: /Close the game first/, console_busy: /console is busy/ };
      for (const [code, message] of Object.entries(errors)) await test(`${viewport.width}: ${code} exposes controller-reachable recovery, details and Back`, async () => {
        const { page, api } = await fixture(viewport); await configurePreview(page);
        const trigger = async () => {
          await page.evaluate(code => installerFixture.controls.setScenario(code), code);
          await select(page, action('plan'));
          if (code === 'install_refused') { await page.locator('[data-installer-state="plan"]').waitFor(); await select(page, action('apply')); }
          await page.locator('[data-installer-state="error"]').waitFor();
        };
        await trigger(); const copy = await page.locator('#installer-panel').innerText();
        assert.match(copy, message); assert.match(copy, /Nothing was changed/); assert.doesNotMatch(copy, /0x[0-9a-f]{6}|mock:|plan_sha256|OWNED_CONTROLLER_GAME/);
        await select(page, action('more')); await page.waitForFunction(() => document.querySelector('#installer-info-dialog').open); await pad(page, 'circle');
        assert(await page.evaluate(() => document.activeElement.matches('[data-installer-action="more"]')));
        await reach(page, action('retry')); await assertUsableFocus(page, viewport); await screenshot(page, viewport, `error-${code}`);
        await select(page, action('cancel')); await page.locator('[data-installer-state="game"]').waitFor();
        await trigger(); await page.evaluate(() => installerFixture.controls.setScenario(null));
        await select(page, action('retry')); await page.locator('[data-installer-state="plan"]').waitFor();
        await pad(page, 'circle'); await page.locator('[data-installer-state="game"]').waitFor(); await assertUsableFocus(page, viewport);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
    }
  } finally { await browser.close(); await new Promise(resolve => server.close(resolve)); }
  const passed = results.filter(item => item.passed).length, failed = results.length - passed;
  fs.writeFileSync(path.join(evidenceRoot, 'RESULTS.json'), JSON.stringify({ passed, failed, navigation: 'standard Gamepad API only; d-pad and cross/circle', assets: Object.fromEntries(['app.js', 'index.html', 'style.css', 'installer-ui.js', 'installer-mock.js', 'installer-service.js', 'config.js'].map(name => [name, crypto.createHash('sha256').update(fs.readFileSync(path.join(webRoot, name))).digest('hex')])), results }, null, 2) + '\n');
  console.log(JSON.stringify({ passed, failed })); if (failed) process.exitCode = 1;
}
main().catch(error => { console.error(error); process.exitCode = 1; });
