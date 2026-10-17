'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const crypto = require('node:crypto');
const { chromium } = require('playwright');

// Owned localhost fixtures and the real UI. Only d-pad, X and O drive navigation
// and text entry; observation never edits, clicks or focuses production elements.
const webRoot = path.join(__dirname, '..', 'web');
const evidenceRoot = process.env.UI_EVIDENCE_ROOT ? path.resolve(process.env.UI_EVIDENCE_ROOT) : path.join(__dirname, '..', 'docs', 'ui', 'step-01b');
const directions = { up: 12, down: 13, left: 14, right: 15 };
const sizes = [{ width: 1920, height: 1080 }, { width: 3840, height: 2160 }];

async function pad(page, name) {
  const index = directions[name] ?? ({ cross: 0, circle: 1 })[name];
  assert.notEqual(index, undefined);
  await page.evaluate(index => { window.keyboardPad.buttons[index].pressed = true; window.keyboardPad.buttons[index].value = 1; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  await page.evaluate(index => { window.keyboardPad.buttons[index].pressed = false; window.keyboardPad.buttons[index].value = 0; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}

async function inspectFocus(page, target) {
  return page.evaluate(target => {
    const candidates = [...document.querySelectorAll(target.selector || '#controller-keyboard button')].filter(el => el.getClientRects().length && !el.disabled && !el.closest('[hidden]'));
    const selected = target.label === undefined ? candidates[0] : candidates.find(el => (el.getAttribute('aria-label') || el.textContent).trim() === target.label);
    const active = document.activeElement;
    const controls = [...document.querySelectorAll('button,input,a[href],[tabindex]')];
    const rect = el => { const r = el?.getBoundingClientRect(); return r ? { x: r.x + r.width / 2, y: r.y + r.height / 2 } : { x: 0, y: 0 }; };
    return { exists: !!selected, matched: active === selected, key: active.id || `${active.tagName}:${controls.indexOf(active)}`, active: rect(active), target: rect(selected) };
  }, target);
}

async function reach(page, target) {
  const graph = new Map(), get = key => { if (!graph.has(key)) graph.set(key, {}); return graph.get(key); };
  for (let move = 0; move < 320; move++) {
    const before = await inspectFocus(page, target);
    assert(before.exists, `Visible controller target missing: ${JSON.stringify(target)}`);
    if (before.matched) return;
    const dx = before.target.x - before.active.x, dy = before.target.y - before.active.y;
    const preferred = Math.abs(dy) > Math.abs(dx) ? [dy >= 0 ? 'down' : 'up', dx >= 0 ? 'right' : 'left'] : [dx >= 0 ? 'right' : 'left', dy >= 0 ? 'down' : 'up'];
    let direction = [...new Set([...preferred, 'down', 'right', 'up', 'left'])].find(value => !Object.hasOwn(get(before.key), value));
    if (!direction) {
      const queue = [{ key: before.key, first: null }], seen = new Set([before.key]);
      while (queue.length && !direction) {
        const node = queue.shift();
        if (Object.keys(get(node.key)).length < 4) { direction = node.first; break; }
        for (const [edge, next] of Object.entries(get(node.key))) if (!seen.has(next)) { seen.add(next); queue.push({ key: next, first: node.first || edge }); }
      }
    }
    assert(direction, `Controller cannot reach ${JSON.stringify(target)}`);
    await pad(page, direction); get(before.key)[direction] = (await inspectFocus(page, target)).key;
  }
  assert.fail(`Controller did not reach ${JSON.stringify(target)} within its bounded navigation graph`);
}

async function key(page, label) {
  await reach(page, { label }); await pad(page, 'cross');
}

async function typeText(page, value) {
  for (const character of value) {
    if (character === ' ') { await key(page, 'Space'); continue; }
    const target = { label: character };
    if (!(await inspectFocus(page, target)).exists && /^[a-z]$/i.test(character)) await key(page, 'Shift');
    await reach(page, target); await pad(page, 'cross');
  }
}

async function openKeyboard(page, selector) {
  await reach(page, { selector }); await pad(page, 'cross');
  assert(await page.locator('#controller-keyboard').isVisible(), 'Controller X did not open the on-screen keyboard');
}

async function screenshot(page, viewport, name) {
  if (viewport.width === 1920) await page.screenshot({ path: path.join(evidenceRoot, `${name}-1080.png`) });
}

async function main() {
  fs.mkdirSync(evidenceRoot, { recursive: true });
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, 'http://localhost'), name = url.pathname === '/' ? 'index.html' : url.pathname.slice(1);
    if (!/^[a-z0-9-]+\.(?:html|css|js)$/.test(name) || !fs.existsSync(path.join(webRoot, name))) return res.writeHead(404).end();
    res.writeHead(200, { 'Content-Type': name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript' });
    res.end(fs.readFileSync(path.join(webRoot, name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const options = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) options.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) options.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(options), contexts = [];
  const results = [];
  async function fixture(viewport) {
    const context = await browser.newContext({ viewport }); contexts.push(context);
    const api = { errors: [], outbound: [], gets: [] };
    await context.addInitScript(() => {
      window.keyboardPad = { id: 'Owned keyboard controller', index: 0, mapping: 'standard', connected: true, axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0, touched: false })) };
      Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.keyboardPad] });
      window.keyboardInputEvents = [];
      for (const type of ['input', 'change']) document.addEventListener(type, event => window.keyboardInputEvents.push({ type, id: event.target.id, value: event.target.value }));
    });
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, route => {
      const url = new URL(route.request().url()); api.gets.push(url.pathname);
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (url.pathname === '/api/capabilities') return json({ mode: 'desktop_preparation', csrf_token: 'owned-keyboard-csrf', capabilities: { nexus_catalog: true, account_status: true, detected_titles: false, jobs: false, installed_records: false, download_analyze: false, deploy: false } });
      if (url.pathname === '/api/account') return json({ status: 'connected', live_service: 'verified', account: { display_name: 'TV player' } });
      if (url.pathname === '/api/detected-titles') return json({ schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'unavailable', reason: 'platform_provider_unavailable', issues: [], items: [] });
      if (url.pathname === '/api/games') return json({ items: [{ domain_name: 'one', name: 'Game One' }, { domain_name: 'two', name: 'Game Two' }] });
      if (url.pathname === '/api/games/one/mods/42') return json({ item: { mod_id: 42, game_domain: 'one', name: 'A directly found mod', author: 'Community author', summary: 'Owned lookup result', version: '1' } });
      if (url.pathname === '/api/games/one/mods/42/files') return json({ items: [{ file_id: 7, name: 'Main download', file_name: 'main.zip', version: '1', size_kb: 1 }] });
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
      await test(`${viewport.width}: X opens controller text entry from the search box`, async () => {
        const { page, api } = await fixture(viewport);
        await reach(page, { selector: '#catalog-search' }); await pad(page, 'cross');
        assert(await page.locator('#controller-keyboard').isVisible(), 'Controller X did not open the on-screen keyboard');
        const ui = await page.locator('#controller-keyboard').evaluate(dialog => {
          const r = dialog.getBoundingClientRect();
          return { width: innerWidth, height: innerHeight, r: { x: r.x, y: r.y, right: r.right, bottom: r.bottom }, keys: [...dialog.querySelectorAll('button')].map(el => ({ height: el.getBoundingClientRect().height, font: parseFloat(getComputedStyle(el).fontSize) })), focusInDialog: dialog.contains(document.activeElement) };
        });
        const scale = viewport.width / 1920;
        assert(ui.r.x >= 0 && ui.r.y >= 0 && ui.r.right <= ui.width && ui.r.bottom <= ui.height, 'Controller keyboard is clipped outside the TV viewport');
        assert(ui.keys.length >= 36 && ui.keys.every(item => item.height >= 56 * scale && item.font >= 24 * scale), 'Keyboard keys are too small to use from a TV');
        assert(ui.focusInDialog, 'Controller keyboard opened without usable focus');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: Shift, Space, Backspace and Clear edit a draft, Done filters games`, async () => {
        const { page, api } = await fixture(viewport);
        await openKeyboard(page, '#catalog-search');
        await typeText(page, 'discard me'); await key(page, 'Clear');
        await typeText(page, 'Game Twox'); await key(page, 'Backspace');
        assert.equal(await page.locator('#catalog-search').inputValue(), '', 'Draft edits changed the underlying search before Done');
        assert.equal(await page.evaluate(() => window.keyboardInputEvents.filter(item => item.id === 'catalog-search').length), 0);
        await screenshot(page, viewport, 'controller-keyboard');
        await key(page, 'Done');
        assert.equal(await page.locator('#catalog-search').inputValue(), 'Game Two');
        assert.equal(await page.locator('[data-card^="game:"]').count(), 1);
        assert.equal(await page.locator('[data-card="game:two"]').count(), 1);
        assert.equal(await page.evaluate(() => document.activeElement.id), 'catalog-search');
        assert.deepEqual(await page.evaluate(() => window.keyboardInputEvents.filter(item => item.id === 'catalog-search').map(item => item.type)), ['input', 'change']);
        await screenshot(page, viewport, 'controller-filter-result');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: O and Cancel discard edits and preserve the original filter`, async () => {
        const { page, api } = await fixture(viewport);
        await openKeyboard(page, '#catalog-search'); await typeText(page, 'one'); await key(page, 'Done');
        for (const cancel of ['circle', 'Cancel']) {
          await openKeyboard(page, '#catalog-search'); await key(page, 'Clear'); await typeText(page, 'two');
          if (cancel === 'circle') await pad(page, 'circle'); else await key(page, cancel);
          assert.equal(await page.locator('#controller-keyboard').isVisible(), false);
          assert.equal(await page.locator('#catalog-search').inputValue(), 'one');
          assert.equal(await page.locator('[data-card="game:one"]').count(), 1);
          assert.equal(await page.locator('[data-card="game:two"]').count(), 0);
          assert.equal(await page.evaluate(() => document.activeElement.id), 'catalog-search');
        }
        assert.deepEqual(await page.evaluate(() => window.keyboardInputEvents.filter(item => item.id === 'catalog-search').map(item => item.type)), ['input', 'change']);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: controller entry can find a Nexus mod by domain and number`, async () => {
        const { page, api } = await fixture(viewport);
        await openKeyboard(page, '#lookup-domain'); await typeText(page, 'one'); await key(page, 'Done');
        await openKeyboard(page, '#lookup-mod-id'); await typeText(page, '42'); await key(page, 'Done');
        await screenshot(page, viewport, 'controller-lookup-entry');
        await reach(page, { selector: '#lookup-submit' }); await pad(page, 'cross');
        await page.locator('[data-file-card="7"]').waitFor();
        assert(api.gets.includes('/api/games/one/mods/42'));
        assert(api.gets.includes('/api/games/one/mods/42/files'));
        assert.match(await page.locator('#games-view h1').innerText(), /A directly found mod/);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: controller text entry respects the input length limit`, async () => {
        const { page, api } = await fixture(viewport);
        await openKeyboard(page, '#lookup-mod-id');
        await typeText(page, '1'.repeat(22)); await key(page, 'Done');
        assert.equal(await page.locator('#lookup-mod-id').inputValue(), '1'.repeat(19));
        assert.equal(await page.evaluate(() => document.activeElement.id), 'lookup-mod-id');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
    }
  } finally { await browser.close(); await new Promise(resolve => server.close(resolve)); }
  const passed = results.filter(item => item.passed).length, failed = results.length - passed;
  fs.writeFileSync(path.join(evidenceRoot, 'RESULTS.json'), JSON.stringify({ passed, failed, navigation: 'standard Gamepad API only; d-pad and cross/circle', assets: Object.fromEntries(['app.js', 'index.html', 'style.css', 'controller-keyboard.js'].map(name => [name, fs.existsSync(path.join(webRoot, name)) ? crypto.createHash('sha256').update(fs.readFileSync(path.join(webRoot, name))).digest('hex') : null])), results }, null, 2) + '\n');
  console.log(JSON.stringify({ passed, failed })); if (failed) process.exitCode = 1;
}
main().catch(error => { console.error(error); process.exitCode = 1; });
