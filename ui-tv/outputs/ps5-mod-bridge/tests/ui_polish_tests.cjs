'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

// Real browser assets; all provider/API replies are owned fixtures. Navigation
// uses only standard Gamepad d-pad/X/O. DOM reads below observe, never focus/click.
const root = path.resolve(__dirname, '..');
const evidence = path.join(root, 'docs/ui/step-03/evidence');
const preferenceKey = 'modbridge.catalog.preferences.v1';
const assets = ['index.html', 'style.css', 'app.js', 'controller-keyboard.js', 'config.js', 'installer-service.js', 'installer-mock.js', 'installer-ui.js'];
const games = [{ id: 1, domain_name: 'orchard', name: 'Orchard Adventure' }, { id: 2, domain_name: 'blueworld', name: 'Blue World' }, { id: 3, domain_name: 'quietworld', name: 'Quiet World' }];
const mods = [{ mod_id: 101, name: 'Warm lights', author: 'Community author', summary: 'Warmer rooms', version: '1.0' }, { mod_id: 102, name: 'Readable menus', author: 'Community author', summary: 'Larger menu text', version: '2.0' }];
const files = [{ file_id: 201, name: 'Main download', file_name: 'menus.zip', size_kb: 120, version: '2.0', description: 'A community mod.' }];
const observed = value => ({ value, status: 'observed', source: 'owned_polish_fixture' });
const titles = { schema_version: 1, status: 'observed', complete: true, snapshot_id: 'a'.repeat(32), source_scope: 'owned_inventory', reason: null, issues: [], items: [{ title_key: 'd'.repeat(32), title_id: observed('PPSA98765'), display_name: observed('Installed adventure'), build_version: observed('1.0'), build_fingerprint: null }] };
const directions = { up: 12, down: 13, left: 14, right: 15, cross: 0, circle: 1 };
const results = [];

async function pad(page, input) {
  await page.evaluate(index => { window.polishPad.buttons[index].pressed = true; }, directions[input]);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  await page.evaluate(index => { window.polishPad.buttons[index].pressed = false; }, directions[input]);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function focusState(page, selector) {
  return page.evaluate(selector => {
    const controls = [...document.querySelectorAll('button,a[href],input,[tabindex]')];
    const active = document.activeElement;
    const target = [...document.querySelectorAll(selector)].find(el => el.getClientRects().length && !el.disabled && !el.closest('[hidden]'));
    const key = el => !el || el === document.body ? 'body' : el.id || `${el.tagName}:${controls.indexOf(el)}:${el.dataset.card || el.dataset.key || el.textContent.trim().slice(0, 40)}`;
    const center = el => { const rect = el?.getBoundingClientRect(); return rect ? { x: rect.x + rect.width / 2, y: rect.y + rect.height / 2 } : null; };
    return { key: key(active), matched: !!active?.matches(selector), targetExists: !!target, active: center(active), target: center(target) };
  }, selector);
}
async function reach(page, selector) {
  const graph = new Map();
  const edges = key => { if (!graph.has(key)) graph.set(key, {}); return graph.get(key); };
  for (let move = 0; move < 220; move++) {
    const current = await focusState(page, selector);
    assert(current.targetExists, `No enabled visible action: ${selector}`);
    if (current.matched) return;
    const dx = current.target.x - (current.active?.x || 0), dy = current.target.y - (current.active?.y || 0);
    const preferred = Math.abs(dy) > Math.abs(dx) ? [dy >= 0 ? 'down' : 'up', dx >= 0 ? 'right' : 'left'] : [dx >= 0 ? 'right' : 'left', dy >= 0 ? 'down' : 'up'];
    let direction = [...new Set([...preferred, 'down', 'right', 'up', 'left'])].find(value => !Object.hasOwn(edges(current.key), value));
    if (!direction) {
      const queue = [{ key: current.key, first: null }], visited = new Set([current.key]);
      while (queue.length && !direction) {
        const node = queue.shift();
        if (Object.keys(edges(node.key)).length < 4) { direction = node.first; break; }
        for (const [edge, next] of Object.entries(edges(node.key))) if (!visited.has(next)) { visited.add(next); queue.push({ key: next, first: node.first || edge }); }
      }
    }
    assert(direction, `Controller focus graph exhausted before ${selector}`);
    await pad(page, direction); edges(current.key)[direction] = (await focusState(page, selector)).key;
  }
  assert.fail(`Controller did not reach ${selector}`);
}
async function activate(page, selector) { await reach(page, selector); await pad(page, 'cross'); }
async function enterText(page, selector, text) {
  await activate(page, selector);
  await page.waitForFunction(() => document.querySelector('#controller-keyboard')?.open);
  await activate(page, '#controller-keyboard [data-key="Clear"]');
  for (const char of text) await activate(page, `#controller-keyboard [data-key="${char}"]`);
  await activate(page, '#controller-keyboard [data-key="Done"]');
  assert.equal(await page.locator(selector).inputValue(), text);
}
const filter = (page, text) => enterText(page, '#catalog-search', text);

async function main() {
  fs.mkdirSync(evidence, { recursive: true });
  const server = http.createServer((request, response) => {
    const name = request.url === '/' ? 'index.html' : request.url.slice(1);
    if (!assets.includes(name)) return response.writeHead(404).end();
    response.writeHead(200, { 'Content-Type': name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript' });
    response.end(fs.readFileSync(path.join(root, 'web', name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const launch = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(launch);
  const contexts = [];
  async function fixture(viewport, options = {}) {
    const context = await browser.newContext({ viewport }); contexts.push(context);
    const api = { calls: [], posts: [], errors: [], external: [], gates: {}, delay: null, gamesError: null, modsError: null, filesError: null, jobsError: null, gamesItems: games, modsItems: mods, filesItems: files, connected: true, authURL: 'https://users.nexusmods.com/oauth/authorize?owned=1', diagnostic: false, ...options };
    await context.addInitScript(options => {
      window.polishPad = { id: 'Owned polish controller', index: 0, mapping: 'standard', connected: true, buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })), axes: [0, 0], timestamp: 0 };
      Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.polishPad] });
      if (options.blockStorage) for (const method of ['getItem', 'setItem']) Object.defineProperty(Storage.prototype, method, { configurable: true, value: () => { throw new Error('Owned blocked storage'); } });
      else if (options.badPreferences) { try { localStorage.setItem('modbridge.catalog.preferences.v1', '{malformed'); } catch (_) {} }
    }, { blockStorage: options.blockStorage, badPreferences: options.badPreferences });
    context.on('page', page => page.on('pageerror', error => api.errors.push(error.message)));
    await context.route('**/*', route => {
      const url = new URL(route.request().url());
      if (url.origin === origin) return route.continue();
      api.external.push(url.href);
      if (url.origin === 'https://users.nexusmods.com') return route.fulfill({ contentType: 'text/html', body: '<title>Owned official login destination fixture</title>' });
      return route.abort();
    });
    await context.route(`${origin}/api/**`, async route => {
      const request = route.request(), url = new URL(request.url()); api.calls.push([request.method(), url.pathname]);
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (request.method() === 'POST') api.posts.push([url.pathname, request.postDataJSON()]);
      const hold = async kind => { if (api.delay === kind) await new Promise(resolve => { api.gates[kind] = resolve; }); };
      if (url.pathname === '/api/capabilities') return json({ mode: 'desktop_preparation', csrf_token: 'owned-csrf-not-a-user-secret', capabilities: { nexus_catalog: true, account_status: true, nexus_auth_start: true, detected_titles: true, jobs: true, installed_records: true, download_analyze: true, retained_downloads: api.diagnostic, mod_service_records: api.serviceRecords || false, deploy: false } });
      if (url.pathname === '/api/account') return json(api.account || (api.connected ? { status: 'connected', live_service: 'verified', account: { display_name: 'TV player', is_premium: false } } : { status: 'disconnected' }));
      if (url.pathname === '/api/auth/start') return json({ authorization_url: api.authURL });
      if (url.pathname === '/api/games') { await hold('games'); return api.gamesError ? json({ error: api.gamesError }, 503) : json({ items: api.gamesItems }); }
      if (/^\/api\/games\/[a-z_]+\/mods$/.test(url.pathname)) { await hold('mods'); return api.modsError ? json({ error: api.modsError }, 503) : json({ items: api.modsItems }); }
      if (/^\/api\/games\/[a-z_]+\/mods\/\d+\/files$/.test(url.pathname)) { await hold('files'); return api.filesError ? json({ error: api.filesError }, 503) : json({ items: api.filesItems }); }
      const directMod = /^\/api\/games\/([a-z_]+)\/mods\/(\d+)$/.exec(url.pathname);
      if (directMod) return json({ item: { ...mods.find(item => String(item.mod_id) === directMod[2]), game_domain: directMod[1] } });
      if (url.pathname === '/api/detected-titles') return json(api.diagnostic ? titles : { schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'none', reason: 'platform_provider_unavailable', issues: [], items: [] });
      if (url.pathname === '/api/jobs') return api.jobsError ? json({ error: api.jobsError }, 503) : json({ items: api.diagnostic ? [{ id: 'e'.repeat(32), game: 'orchard', mod_id: 102, file_id: 201, label: 'Readable menus', status: 'analyzed', sha256: 'b'.repeat(64), bytes_downloaded: 120 }] : api.jobs || [] });
      if (url.pathname === '/api/downloads') return json({ items: [{ download_id: 'dl-' + 'b'.repeat(64), state: 'downloaded', retained: true, game: 'orchard', mod_id: 102, file_id: 201, sha256: 'b'.repeat(64), bytes: 120 }] });
      if (url.pathname === '/api/installed') return json({ items: api.diagnostic ? [{ name: 'Installed menus', transaction_confirmed: true, sha256: 'c'.repeat(64) }] : [] });
      if (url.pathname === '/api/mod-service') return json({ ok: true, result: { contract: 'modforge-service/1' } });
      if (url.pathname === '/api/mod-service/records') return json({ ok: true, result: { mods: [{ download_id: 'owned-record', title_id: 'PPSA98765', state: 'prepared', source: { name: 'Saved glow' }, blockers: [{ code: 'missing_backend', message: '/private/owned/mods 0xdeadbeef' }] }], games: [{ title_id: 'PPSA98765', installed: { mods: ['owned-record'], generation: 1, load_list_sha256: 'c'.repeat(64) }, runtime: { state: 'mods-started', detail: '/private/owned/runtime 0xdeadbeef' } }] } });
      return json({ error: { code: 'not_found', message: 'Owned fixture route unavailable.' } }, 404);
    });
    const page = await context.newPage(); page.setDefaultTimeout(1300);
    await page.goto(origin);
    if (!api.delay && !api.gamesError && api.gamesItems.some(item => item.domain_name === 'orchard')) await page.locator('[data-card="game:orchard"]').waitFor();
    return { page, api, context };
  }
  async function test(name, action) {
    if (process.env.UI_TEST_FILTER && !new RegExp(process.env.UI_TEST_FILTER).test(name)) return;
    const before = contexts.length;
    try { await action(); results.push({ name, passed: true }); console.log(`PASS ${name}`); }
    catch (error) { results.push({ name, passed: false, error: error.message }); console.error(`FAIL ${name}: ${error.message}`); }
    finally { for (const context of contexts.slice(before)) await context.close(); }
  }
  const capture = async (page, viewport, name) => { if (viewport.width === 1920) await page.screenshot({ path: path.join(evidence, `${name}-1080.png`) }); };
  async function enterGame(page) { await activate(page, '[data-card="game:orchard"]'); await page.locator('[data-card="mod:102"]').waitFor(); }
  try {
    for (const viewport of [{ width: 1920, height: 1080 }, { width: 3840, height: 2160 }]) {
      await test(`${viewport.width}: pending games mods files show loading instead of empty`, async () => {
        const { page, api } = await fixture(viewport, { delay: 'games' });
        await page.locator('[data-catalog-loading="games"]').waitFor();
        assert.doesNotMatch(await page.locator('#games-content').innerText(), /No games|No mods|No files/i); await capture(page, viewport, 'loading-games');
        api.delay = 'mods'; api.gates.games(); await page.locator('[data-card="game:orchard"]').waitFor(); await activate(page, '[data-card="game:orchard"]');
        await page.locator('[data-catalog-loading="mods"]').waitFor(); assert.doesNotMatch(await page.locator('#games-content').innerText(), /No recent mods|No mods/i); await capture(page, viewport, 'loading-mods');
        api.delay = 'files'; api.gates.mods(); await page.locator('[data-card="mod:102"]').waitFor(); await activate(page, '[data-card="mod:102"]');
        await page.locator('[data-catalog-loading="files"]').waitFor(); assert.doesNotMatch(await page.locator('#games-content').innerText(), /No files/i); await capture(page, viewport, 'loading-files');
        api.delay = null; api.gates.files(); await page.locator('[data-file-card="201"]').waitFor(); assert.deepEqual(api.errors, []);
        await reach(page, '.mod-overview [data-more-info]'); await capture(page, viewport, 'mod-overview');
        await reach(page, '[data-file-details="201"]'); await capture(page, viewport, 'mod-files');
        await activate(page, '[data-basket-add="201"]'); await reach(page, '[data-basket-info]'); await capture(page, viewport, 'selected-downloads');
        await activate(page, '#basket-clear'); assert.deepEqual(api.posts, []);
      });
      await test(`${viewport.width}: plain connection error has controller More info and read-only retry`, async () => {
        const { page, api } = await fixture(viewport, { gamesError: { code: 'network', message: '/private/account 0xdeadbeef fixture-secret-token' } });
        await page.locator('#status-banner:not([hidden])').waitFor();
        const visible = await page.locator('body').innerText(); assert.match(visible, /connect|internet/i); assert.match(visible, /try again|reconnect/i); assert.doesNotMatch(visible, /\/private|0xdeadbeef|fixture-secret/);
        await activate(page, '[data-error-action="more"]'); await page.waitForFunction(() => document.querySelector('#details-dialog').open);
        assert.doesNotMatch(await page.locator('#details-dialog').innerText(), /fixture-secret/); await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement?.dataset.errorAction), 'more'); await capture(page, viewport, 'connection-error');
        api.gamesError = null; await activate(page, '[data-error-action="retry"]'); await page.locator('[data-card="game:orchard"]').waitFor();
        assert.equal(api.calls.filter(([method, route]) => method === 'GET' && route === '/api/games').length, 2); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
      });
      await test(`${viewport.width}: controller preferences survive refresh and a new page without private state`, async () => {
        const { page, api, context } = await fixture(viewport);
        await filter(page, 'orch'); await activate(page, '#catalog-sort');
        await enterGame(page); await filter(page, 'read'); await activate(page, '#catalog-sort'); await activate(page, '#catalog-sort');
        const stored = await page.evaluate(key => JSON.parse(localStorage.getItem(key)), preferenceKey);
        assert.ok(stored, 'Public catalog preferences were not saved');
        assert.equal(stored.last_game.domain, 'orchard'); assert.deepEqual(stored.filters, { games: 'orch', mods: 'read' }); assert.deepEqual(stored.sort, { games: 1, mods: 2 });
        assert.doesNotMatch(JSON.stringify(stored), /csrf|token|authorization|target_ref|snapshot|nxm|archive_path/i);
        await page.reload(); await page.waitForFunction(() => !!document.querySelector('[data-card="mod:102"]') || document.querySelector('#continue-game')?.getClientRects().length > 0);
        if (await page.locator('#continue-game').isVisible()) await activate(page, '#continue-game');
        await page.locator('[data-card="mod:102"]').waitFor(); assert.equal(await page.locator('#catalog-search').inputValue(), 'read'); assert.match(await page.locator('#catalog-sort').innerText(), /Z.*A/);
        const second = await context.newPage(); second.setDefaultTimeout(1500); await second.goto(origin);
        await second.waitForFunction(() => !!document.querySelector('[data-card="mod:102"]') || document.querySelector('#continue-game')?.getClientRects().length > 0);
        if (await second.locator('#continue-game').isVisible()) { await reach(second, '#continue-game'); await capture(second, viewport, 'continue-game'); await pad(second, 'cross'); }
        await second.locator('[data-card="mod:102"]').waitFor(); assert.equal(await second.locator('#catalog-search').inputValue(), 'read');
        await pad(second, 'circle'); await second.locator('[data-card="game:orchard"]').waitFor(); assert.equal(await second.locator('#catalog-search').inputValue(), 'orch');
        await capture(second, viewport, 'remembered-filters'); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
      });
      await test(`${viewport.width}: normal screens hide diagnostic bytes while More info retains evidence`, async () => {
        const { page, api } = await fixture(viewport, { diagnostic: true });
        const hidden = /[a-f0-9]{64}|PPSA98765|\/private\/|0xdeadbeef/;
        assert.doesNotMatch(await page.locator('body').innerText(), hidden);
        await reach(page, '[data-title-key]'); await capture(page, viewport, 'console-game');
        await activate(page, '#tab-activity'); await page.locator('[data-job-id]').waitFor(); assert.doesNotMatch(await page.locator('body').innerText(), hidden);
        await activate(page, '#activity-content button'); await page.waitForFunction(() => document.querySelector('#details-dialog').open); assert.match(await page.locator('#details-dialog').innerText(), /b{64}/); await pad(page, 'circle'); await capture(page, viewport, 'activity-plain');
        await activate(page, '#tab-installed'); await page.getByText('Installed menus', { exact: true }).waitFor(); assert.doesNotMatch(await page.locator('body').innerText(), hidden);
        await activate(page, '#installed-content button'); await page.waitForFunction(() => document.querySelector('#details-dialog').open); assert.match(await page.locator('#details-dialog').innerText(), /c{64}/); await pad(page, 'circle'); await capture(page, viewport, 'installed-plain'); assert.deepEqual(api.errors, []);
      });
    }
    await test('1080: malformed or blocked preference storage leaves controller browsing usable', async () => {
      for (const options of [{ badPreferences: true }, { blockStorage: true }]) {
        const { page, api } = await fixture({ width: 1920, height: 1080 }, options); await activate(page, '#catalog-sort'); await enterGame(page);
        assert.equal(await page.locator('[data-card="mod:102"]').count(), 1); assert.deepEqual(api.errors, []);
      }
    });
    await test('1080: historical service diagnostics remain behind More info', async () => {
      const { page } = await fixture({ width: 1920, height: 1080 }, { diagnostic: true, serviceRecords: true }); await activate(page, '#tab-installed'); await page.getByText('Saved glow', { exact: true }).waitFor();
      assert.doesNotMatch(await page.locator('body').innerText(), /PPSA98765|\/private\/|0xdeadbeef|missing_backend|mods-started|[a-f0-9]{64}/);
      await activate(page, '#installed-content button'); await page.waitForFunction(() => document.querySelector('#details-dialog').open); assert.match(await page.locator('#details-dialog').innerText(), /PPSA98765/); await pad(page, 'circle');
    });
    await test('1080: own-account sign-in accepts only the configured official origin', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { connected: false, authURL: 'https://users.nexusmods.com.evil.example/oauth/authorize' });
      await reach(page, '#sign-in'); await capture(page, { width: 1920 }, 'nexus-sign-in');
      await activate(page, '#sign-in'); await page.locator('#status-banner:not([hidden])').waitFor(); assert.deepEqual(api.external, []);
      api.authURL = 'https://users.nexusmods.com/oauth/authorize?owned=1'; await reach(page, '#sign-in');
      const navigation = page.waitForURL('https://users.nexusmods.com/oauth/authorize?owned=1');
      await page.evaluate(() => { window.polishPad.buttons[0].pressed = true; });
      await navigation; // The old document is gone; its gamepad release is irrelevant.
      assert.deepEqual(api.external, ['https://users.nexusmods.com/oauth/authorize?owned=1']); assert.equal(api.posts.filter(([route]) => route === '/api/auth/start').length, 2);
    });
    await test('1080: free download explains the real Nexus handoff without fake callback or full search', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { jobs: [{ id: 'e'.repeat(32), status: 'needs_nexus_authorization', game: 'orchard', mod_id: 102, file_id: 201, label: 'Readable menus' }] });
      await activate(page, '#tab-activity'); const copy = await page.locator('#activity-content').innerText(); assert.match(copy, /approval|approve|Nexus.*download/i); assert.match(copy, /return.*not available|callback.*not available|not.*available.*return/i);
      assert.equal(await page.locator('.source-link').getAttribute('href'), 'https://www.nexusmods.com/orchard/mods/102?tab=files&file_id=201');
      await activate(page, '#tab-games'); await filter(page, 'orch'); assert.equal(api.calls.some(([_method, route]) => /search|images/.test(route)), false); assert.deepEqual(api.posts, []);
    });
    await test('1080: expired Nexus account explains reconnect without losing browsing', async () => {
      const { page } = await fixture({ width: 1920, height: 1080 }, { account: { status: 'error', credentials_present: true, live_service: 'unavailable', error: { code: 'reconnect_required', message: 'fixture-secret-token' } } });
      assert.match(await page.locator('#account').innerText(), /expired|sign in again|reconnect/i); assert.doesNotMatch(await page.locator('body').innerText(), /fixture-secret/); await reach(page, '#sign-in');
    });
    await test('1080: Nexus expiration during browsing exposes Sign in and keeps selected downloads', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 });
      await enterGame(page); await activate(page, '[data-card="mod:102"]'); await page.locator('[data-file-card="201"]').waitFor();
      await activate(page, '[data-basket-add="201"]');
      await pad(page, 'circle'); await page.locator('[data-card="mod:102"]').waitFor();
      await pad(page, 'circle'); await page.locator('[data-card="game:orchard"]').waitFor();
      api.modsError = { code: 'reconnect_required', message: '/private/account fixture-secret-token' };
      api.account = { status: 'error', credentials_present: true, live_service: 'unavailable', error: { code: 'reconnect_required' } };
      await activate(page, '[data-card="game:orchard"]'); await page.locator('[data-catalog-retry]').waitFor();
      assert.equal(await page.locator('#sign-in').count(), 1, 'reconnect_required left a stale connected header without Sign in');
      assert.equal(await page.locator('#disconnect-account').count(), 0);
      assert.match(await page.locator('#status-banner').innerText(), /expired.*sign in again/i);
      assert.equal(await page.locator('[data-basket-key="orchard:102:201"]').count(), 1);
      assert.equal(await page.locator('#basket-submit').isDisabled(), true);
      assert.doesNotMatch(await page.locator('body').innerText(), /\/private|fixture-secret/);
      await reach(page, '#sign-in'); await capture(page, { width: 1920 }, 'expired-during-browsing');
      assert.deepEqual(api.posts, []);
      const navigation = page.waitForURL(api.authURL);
      await page.evaluate(() => { window.polishPad.buttons[0].pressed = true; });
      await navigation;
      assert.deepEqual(api.posts, [['/api/auth/start', {}]]); assert.deepEqual(api.errors, []);
    });
    await test('1080: repeated background account expiry preserves controller focus on Sign in', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 });
      assert.equal(await page.locator('#disconnect-account').count(), 1);
      api.jobsError = { code: 'reconnect_required', message: '/private/account fixture-secret-token' };
      await page.locator('#sign-in').waitFor({ timeout: 6000 });
      await reach(page, '#sign-in');
      const previousSignIn = await page.locator('#sign-in').elementHandle();
      const previousPolls = api.calls.filter(([method, route]) => method === 'GET' && route === '/api/jobs').length;
      for (let count = 0; count < 120 && api.calls.filter(([method, route]) => method === 'GET' && route === '/api/jobs').length === previousPolls; count++) await page.waitForTimeout(50);
      assert(api.calls.filter(([method, route]) => method === 'GET' && route === '/api/jobs').length > previousPolls, 'The next actual jobs poll did not run');
      await page.waitForFunction(element => !element.isConnected, previousSignIn);
      await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
      assert.equal(await page.evaluate(() => document.activeElement?.id), 'sign-in', 'Background expiry replaced Sign in and moved controller focus');
      assert.doesNotMatch(await page.locator('body').innerText(), /\/private|fixture-secret/);
      await capture(page, { width: 1920 }, 'expired-poll-focus'); assert.deepEqual(api.posts, []);
      const navigation = page.waitForURL(api.authURL);
      await page.evaluate(() => { window.polishPad.buttons[0].pressed = true; });
      await navigation;
      assert.deepEqual(api.posts, [['/api/auth/start', {}]]); assert.deepEqual(api.errors, []);
    });
    await test('1080: catalog keeps its own retry when a background downloads check fails', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { gamesError: { code: 'network', message: 'Owned games failure' } });
      await page.locator('#status-banner:not([hidden])').waitFor(); api.jobsError = { code: 'timeout', message: 'Owned jobs failure' };
      const previous = api.calls.filter(([_method, route]) => route === '/api/jobs').length;
      for (let count = 0; count < 70 && api.calls.filter(([_method, route]) => route === '/api/jobs').length === previous; count++) await page.waitForTimeout(50);
      assert(api.calls.filter(([_method, route]) => route === '/api/jobs').length > previous, 'Background check did not run');
      await page.locator('[data-catalog-retry]').waitFor(); api.gamesError = null;
      await activate(page, '[data-catalog-retry]'); await page.locator('[data-card="game:orchard"]').waitFor();
      assert.equal(api.calls.filter(([method, route]) => method === 'GET' && route === '/api/games').length, 2); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('1080: quoted paths UUIDs and opaque names hide without censoring Unicode or standalone slashes', async () => {
      const names = ['"/private/owned/game"', '83d2d1ab-3697-40aa-b779-565901b5ea22', 'ZyxWvU1234567890_ABCDEFGHIJKLMNOP'];
      const unicode = '星 Café 🎮 / \\ "0"';
      const { page } = await fixture({ width: 1920, height: 1080 }, { gamesItems: [...games.map((item, index) => ({ ...item, name: names[index] })), { id: 4, domain_name: 'unicodeworld', name: unicode }] });
      const visible = await page.locator('body').innerText();
      for (const name of names) assert(!visible.includes(name), `Technical name leaked: ${name}`);
      assert(visible.includes(unicode), 'Readable Unicode title with standalone slashes was censored');
      await activate(page, '[data-show-details="game:orchard"]'); await page.waitForFunction(() => document.querySelector('#details-dialog').open);
      assert((await page.locator('#details-dialog').innerText()).includes(names[0]), 'Original technical name is unavailable in More info'); await pad(page, 'circle');
    });
    await test('1080: successful empty games response does not misdiagnose a verified account', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { gamesItems: [] });
      await page.waitForFunction(() => !document.querySelector('[data-catalog-loading="games"]') && document.querySelector('#account').classList.contains('connected'));
      const visible = await page.locator('#games-content').innerText(); assert.match(visible, /no games/i); assert.doesNotMatch(visible, /sign in/i);
      await capture(page, { width: 1920 }, 'empty-games');
      assert.equal(await page.locator('#status-banner:not([hidden])').count(), 0); assert.deepEqual(api.posts, []);
    });
    await test('1080: technical names remain hidden in breadcrumbs after game and mod selection', async () => {
      const gameName = '"/private/owned/game"', modName = '83d2d1ab-3697-40aa-b779-565901b5ea22';
      const { page } = await fixture({ width: 1920, height: 1080 }, { gamesItems: [{ ...games[0], name: gameName }], modsItems: [{ ...mods[1], name: modName }] });
      await activate(page, '[data-card="game:orchard"]'); await page.locator('[data-card="mod:102"]').waitFor();
      assert(!(await page.locator('body').innerText()).includes(gameName), 'Breadcrumb exposed the original technical game name');
      await activate(page, '[data-card="mod:102"]'); await page.locator('[data-file-card="201"]').waitFor();
      const visible = await page.locator('body').innerText(); assert(!visible.includes(gameName)); assert(!visible.includes(modName), 'Breadcrumb exposed the original technical mod name');
    });
    await test('1080: mods and files distinguish successful empty responses from recoverable failures', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { modsItems: [] });
      await activate(page, '[data-card="game:orchard"]'); await page.waitForFunction(() => !document.querySelector('[data-catalog-loading="mods"]'));
      assert.match(await page.locator('#games-content').innerText(), /no recent mods|no mods/i); assert.equal(await page.locator('#status-banner:not([hidden])').count(), 0);
      await capture(page, { width: 1920 }, 'empty-mods');
      await pad(page, 'circle'); api.modsItems = mods; api.modsError = { code: 'network', message: 'Owned failure' };
      await activate(page, '[data-card="game:orchard"]'); await page.locator('[data-error-action="retry"]').waitFor();
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /no recent mods|no mods/i); api.modsError = null;
      await activate(page, '[data-error-action="retry"]'); await page.locator('[data-card="mod:102"]').waitFor(); api.filesItems = [];
      await activate(page, '[data-card="mod:102"]'); await page.waitForFunction(() => !document.querySelector('[data-catalog-loading="files"]'));
      assert.match(await page.locator('#games-content').innerText(), /no files|no downloads/i); assert.equal(await page.locator('#status-banner:not([hidden])').count(), 0);
      await reach(page, '#reload-titles'); await capture(page, { width: 1920 }, 'empty-files');
      await pad(page, 'circle'); api.filesItems = files; api.filesError = { code: 'timeout', message: 'Owned failure' };
      await activate(page, '[data-card="mod:102"]'); await page.locator('[data-error-action="retry"]').waitFor();
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /no files|no downloads/i); api.filesError = null;
      await activate(page, '[data-error-action="retry"]'); await page.locator('[data-file-card="201"]').waitFor(); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('1080: leaving a loading list ignores its late response', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }); api.delay = 'mods';
      await activate(page, '[data-card="game:orchard"]'); await page.locator('[data-catalog-loading="mods"]').waitFor();
      await pad(page, 'circle'); await page.locator('[data-card="game:orchard"]').waitFor(); api.delay = null; api.gates.mods(); await page.waitForTimeout(100);
      assert.equal(await page.locator('[data-card="mod:102"]').count(), 0); assert.equal(await page.locator('#games-content [data-catalog-loading]').count(), 0); assert.deepEqual(api.errors, []);
    });
    await test('1080: initial games completion cannot clear a direct mod lookup files skeleton', async () => {
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { delay: 'games' });
      await page.locator('[data-catalog-loading="games"]').waitFor(); api.delay = 'files';
      await enterText(page, '#lookup-domain', 'o'); await enterText(page, '#lookup-mod-id', '102'); await activate(page, '#lookup-submit');
      await page.locator('[data-catalog-loading="files"]').waitFor(); api.gates.games(); await page.waitForTimeout(100);
      assert.equal(await page.locator('[data-catalog-loading="files"]').count(), 1, 'Games reply cleared the pending files loading state');
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /no files|no downloads/i);
      api.delay = null; api.gates.files(); await page.locator('[data-file-card="201"]').waitFor(); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('1080: known Nexus and host errors explain the next action without raw provider content', async () => {
      const cases = [
        ['credentials', /sign in/i], ['reconnect_required', /expired|sign in again/i], ['configuration', /not available|set up/i],
        ['no_internet', /internet|reconnect/i], ['timeout', /longer|try again/i], ['rate_limit', /wait|pause/i],
        ['authorization_cancelled', /cancelled/i], ['authorization', /sign.in/i], ['download_authorization', /approval|Nexus/i],
        ['content_hidden', /hiding|settings/i], ['truncated', /stopped|finished/i], ['corrupt_download', /damaged/i],
        ['storage', /save|restart/i], ['destination', /save|restart/i], ['storage_unavailable', /save|restart/i],
        ['job_limit', /queue|downloads/i], ['game_running', /close.*game/i], ['console_busy', /busy|wait/i],
        ['unsupported_mod_type', /kind.*mod|support/i], ['busy', /wait|busy|another.*task/i], ['session_required', /open.*app|reopen|restart/i],
        ['translation_unavailable', /not available|not.*support/i], ['stale_target_ref', /choose.*game|game.*changed/i],
        ['invalid_input', /choose|incomplete|invalid|check.*link/i], ['provider', /Nexus/i], ['payload', /Nexus|read.*information/i],
        ['redirect', /location|Nexus|official.*page/i], ['size_limit', /large|limit/i], ['not_found', /available|found|choose/i],
        ['origin_refused', /open.*app|reopen|restart/i], ['intent_conflict', /match|choose.*again/i], ['interrupted', /stopped|finished|interrupted/i], ['operation_failed', /try again|More info/i],
      ];
      const { page, api } = await fixture({ width: 1920, height: 1080 }, { gamesError: { code: cases[0][0], message: '/private/provider 0xdeadbeef fixture-secret' } });
      const failures = [];
      for (let index = 0; index < cases.length; index++) {
        const [code, pattern] = cases[index];
        if (index) { api.gamesError = { code, message: '/private/provider 0xdeadbeef fixture-secret' }; await activate(page, '[data-error-action="retry"]'); }
        await page.waitForFunction(code => document.querySelector('[data-error-action="more"]')?.dataset.moreInfo.includes(code), code);
        const text = await page.locator('#status-banner').innerText();
        if (!pattern.test(text)) failures.push({ code, copy: text });
        assert.doesNotMatch(text, /\/private|0xdeadbeef|fixture-secret/);
        assert.equal(await page.locator('[data-error-action="retry"]').count(), 1);
        if (['no_internet', 'reconnect_required', 'download_authorization', 'rate_limit', 'corrupt_download', 'game_running', 'console_busy', 'unsupported_mod_type', 'session_required'].includes(code)) await capture(page, { width: 1920 }, `error-${code}`);
      }
      assert.deepEqual(failures, [], 'Known errors need a meaningful explanation and recovery');
      assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
  } finally {
    await browser.close(); await new Promise(resolve => server.close(resolve));
    const report = { results, passed: results.filter(item => item.passed).length, failed: results.filter(item => !item.passed).length, navigation: 'Standard Gamepad API d-pad/X/O only; DOM reads observe state', console_calls: 0, external_network_calls: 0 };
    fs.writeFileSync(path.join(evidence, `${process.env.UI_EVIDENCE_NAME || 'test-results'}.json`), JSON.stringify(report, null, 2) + '\n');
    process.exitCode = report.failed ? 1 : 0;
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
