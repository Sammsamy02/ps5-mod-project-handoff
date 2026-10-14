'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const crypto = require('node:crypto');
const { chromium } = require('playwright');

// Real browser assets; every service response is owned and every external URL is blocked.
// Navigation uses the standard Gamepad API only. DOM access below observes state;
// it never clicks, focuses, edits fields, or dispatches keyboard events.
const webRoot = path.resolve(process.env.UI_WEB_ROOT || path.join(__dirname, '..', 'web'));
const evidenceRoot = process.env.UI_EVIDENCE_ROOT ? path.resolve(process.env.UI_EVIDENCE_ROOT) : path.join(__dirname, '..', 'docs', 'ui', 'step-01', 'owned-controller-tests');
const directions = { up: 12, down: 13, left: 14, right: 15 };
const sizes = [{ width: 1920, height: 1080 }, { width: 3840, height: 2160 }];
const games = Array.from({ length: 12 }, (_, i) => ({ id: i + 1, domain_name: `world_${i}`, name: `Game ${String(i + 1).padStart(2, '0')}` }));
const mods = Array.from({ length: 12 }, (_, i) => ({ mod_id: 100 + i, name: `Mod ${String(i + 1).padStart(2, '0')}`, author: 'Community author', summary: 'A community-created test mod.', version: '1.0' }));
const files = [{ file_id: 501, name: 'Main download', file_name: 'main.zip', version: '1.0', size_kb: 120, description: Array.from({ length: 70 }, (_, i) => `Description paragraph ${i + 1}. Read this information before choosing this download.`).join('\n\n') }, { file_id: 502, name: 'Optional download', file_name: 'optional.zip', version: '1.0', size_kb: 40, description: 'An optional owned test file.' }];

async function pad(page, name, holdMs = 0) {
  const index = directions[name] ?? ({ cross: 0, circle: 1 })[name];
  assert.notEqual(index, undefined, `Unknown controller input ${name}`);
  await page.evaluate(index => { window.ownedPad.buttons[index].pressed = true; window.ownedPad.buttons[index].value = 1; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  if (holdMs) await page.waitForTimeout(holdMs);
  await page.evaluate(index => { window.ownedPad.buttons[index].pressed = false; window.ownedPad.buttons[index].value = 0; }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}

async function focusState(page, selector) {
  return page.evaluate(selector => {
    const all = [...document.querySelectorAll('button,a[href],input,[tabindex]')];
    const key = el => !el || el === document.body ? 'body' : el.id || `${el.tagName}:${all.indexOf(el)}:${el.dataset.card || el.dataset.basketAdd || el.dataset.basketRemove || el.dataset.fileAction || el.dataset.fileDetails || el.textContent.trim().slice(0, 50)}`;
    const active = document.activeElement;
    const target = [...document.querySelectorAll(selector)].find(el => el.getClientRects().length && !el.disabled && !el.closest('[hidden]'));
    const rectangle = el => { const r = el?.getBoundingClientRect(); return r ? { x: r.x + r.width / 2, y: r.y + r.height / 2 } : null; };
    return { key: key(active), matched: !!active?.matches(selector), active: rectangle(active), target: rectangle(target), targetExists: !!target };
  }, selector);
}

async function activeIsUsable(page) {
  return page.evaluate(() => {
    const el = document.activeElement;
    return el !== document.body && !el.disabled && el.getClientRects().length > 0 && !el.closest('[hidden]');
  });
}

async function enterFiles(page) {
  await reach(page, '[data-card="game:world_0"]'); await pad(page, 'cross');
  await page.locator('[data-card="mod:100"]').waitFor();
  await reach(page, '[data-card="mod:100"]'); await pad(page, 'cross');
  await page.locator('[data-file-card="501"]').waitFor();
}

async function screenshot(page, viewport, name) {
  if (viewport.width === 1920) await page.screenshot({ path: path.join(evidenceRoot, `${name}-1080.png`) });
}

// Discover the directional focus graph through real gamepad presses. Geometry only
// prioritizes exploration; a visited-edge walk can also cope with wrapped layouts.
async function reach(page, selector, limit = 240) {
  const graph = new Map();
  const get = key => { if (!graph.has(key)) graph.set(key, {}); return graph.get(key); };
  for (let move = 0; move < limit; move++) {
    const before = await focusState(page, selector);
    assert(before.targetExists, `No enabled visible target for ${selector}`);
    if (before.matched) return;
    const dx = before.target.x - (before.active?.x || 0), dy = before.target.y - (before.active?.y || 0);
    const preferred = Math.abs(dy) > Math.abs(dx) ? [dy >= 0 ? 'down' : 'up', dx >= 0 ? 'right' : 'left'] : [dx >= 0 ? 'right' : 'left', dy >= 0 ? 'down' : 'up'];
    const order = [...new Set([...preferred, 'down', 'right', 'up', 'left'])];
    let direction = order.find(value => !Object.hasOwn(get(before.key), value));
    if (!direction) {
      const queue = [{ key: before.key, first: null }], seen = new Set([before.key]);
      while (queue.length && !direction) {
        const node = queue.shift();
        if (Object.keys(get(node.key)).length < 4) { direction = node.first; break; }
        for (const [edge, next] of Object.entries(get(node.key))) if (!seen.has(next)) { seen.add(next); queue.push({ key: next, first: node.first || edge }); }
      }
    }
    if (!direction) {
      fs.writeFileSync(path.join(evidenceRoot, `navigation-failure-${page.viewportSize().width}-${selector.replace(/[^a-z0-9_-]/gi, '_')}.json`), JSON.stringify({ selector, moves: move, state: before, graph: Object.fromEntries(graph) }, null, 2) + '\n');
      assert.fail(`Controller cannot reach ${selector}; focus graph exhausted at ${before.key}`);
    }
    await pad(page, direction);
    get(before.key)[direction] = (await focusState(page, selector)).key;
  }
  fs.writeFileSync(path.join(evidenceRoot, `navigation-failure-${page.viewportSize().width}-${selector.replace(/[^a-z0-9_-]/gi, '_')}.json`), JSON.stringify({ selector, moves: limit, state: await focusState(page, selector), graph: Object.fromEntries(graph) }, null, 2) + '\n');
  assert.fail(`Controller did not reach ${selector} after ${limit} presses`);
}

async function main() {
  fs.mkdirSync(evidenceRoot, { recursive: true });
  const server = http.createServer((req, res) => {
    const pathname = new URL(req.url, 'http://localhost').pathname;
    const name = pathname === '/' ? 'index.html' : pathname.slice(1);
    if (!['index.html','app.js','style.css','controller-keyboard.js','config.js','installer-service.js','installer-mock.js','installer-ui.js'].includes(name)) return res.writeHead(404).end();
    res.writeHead(200, { 'Content-Type': name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript' });
    res.end(fs.readFileSync(path.join(webRoot, name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const options = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) options.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) options.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(options);
  const contexts = [];
  let passed = 0, failed = 0;
  const results = [];
  async function fixture(viewport, extra = {}) {
    const context = await browser.newContext({ viewport }); contexts.push(context);
    const api = { outbound: [], errors: [], posts: [], signedIn: true, failCapabilities: false, titles: false, jobBytes: 0, ...extra };
    await context.addInitScript(() => {
      window.ownedPad = { id: 'Owned controller fixture', index: 0, mapping: 'standard', connected: true, buttons: Array.from({ length: 17 }, () => ({ pressed: false, touched: false, value: 0 })), axes: [0, 0, 0, 0], timestamp: 0 };
      Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.ownedPad] });
    });
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, route => {
      const req = route.request(), url = new URL(req.url());
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (req.method() === 'POST') api.posts.push({ path: url.pathname, body: req.postDataJSON() });
      if (url.pathname === '/api/capabilities') return api.failCapabilities ? json({ error: { code: 'unavailable', message: 'The connection is unavailable.' } }, 503) : json({ mode: 'desktop_preparation', csrf_token: 'owned-controller-csrf', capabilities: { nexus_catalog: true, account_status: true, nexus_auth_start: true, detected_titles: api.titles, jobs: true, installed_records: true, download_analyze: true, retained_downloads: false, mod_service_records: false, deploy: false } });
      if (url.pathname === '/api/account') return json(api.signedIn ? { status: 'connected', live_service: 'verified', account: { display_name: 'TV player' } } : { status: 'disconnected' });
      if (url.pathname === '/api/auth/disconnect') { api.signedIn = false; return json({ status: 'disconnected' }); }
      if (url.pathname === '/api/auth/start') return json({ error: { code: 'offline', message: 'No internet connection. Try again when you are connected.' } }, 503);
      if (url.pathname === '/api/games') return json({ items: games });
      if (/^\/api\/games\/world_\d+\/mods$/.test(url.pathname)) return json({ items: mods });
      if (/^\/api\/games\/world_\d+\/mods\/\d+\/files$/.test(url.pathname)) return json({ items: files });
      if (url.pathname === '/api/jobs' && req.method() === 'GET') return json({ items: [{ id: 'a'.repeat(32), game: 'world_0', mod_id: 100, file_id: 501, label: 'Main download', status: 'needs_nexus_authorization', bytes_downloaded: api.jobBytes }] });
      if (url.pathname === '/api/jobs' && req.method() === 'POST') return json({ id: 'b'.repeat(32), status: 'queued' }, 202);
      if (url.pathname === '/api/installed') return json({ items: [] });
      if (url.pathname === '/api/detected-titles' || url.pathname === '/api/detected-titles/refresh') {
        const observed = value => ({ value, status: 'observed', source: 'owned_controller_fixture' });
        return json(api.titles ? { schema_version: 1, status: 'observed', complete: true, snapshot_id: 'c'.repeat(32), source_scope: 'owned_inventory', reason: null, issues: [], items: [0, 1].map(i => ({ title_key: String(i + 1).repeat(32), title_id: observed(`PPSA9000${i}`), display_name: observed(`Installed game ${i + 1}`), build_version: observed('1.0'), build_fingerprint: null })) } : { items: [], status: 'unavailable' });
      }
      return json({ error: { code: 'not_found', message: 'Unavailable in this owned fixture.' } }, 404);
    });
    const page = await context.newPage(); page.setDefaultTimeout(4000);
    page.on('pageerror', error => api.errors.push(error.message));
    await page.goto(origin);
    if (!api.failCapabilities) await page.locator('[data-card="game:world_0"]').waitFor();
    else await page.locator('#status-banner:not([hidden])').waitFor();
    return { page, context, api };
  }
  async function test(name, action) {
    if (process.env.UI_TEST_FILTER && !new RegExp(process.env.UI_TEST_FILTER).test(name)) return;
    const before = contexts.length;
    try { await action(); passed++; results.push({ name, passed: true }); console.log(`PASS ${name}`); }
    catch (error) { failed++; results.push({ name, passed: false, error: error.message }); console.error(`FAIL ${name}: ${error.stack}`); }
    finally { for (const context of contexts.slice(before)) await context.close(); }
  }
  try {
    for (const viewport of sizes) {
      await test(`${viewport.width}: entering a game retains usable controller focus`, async () => {
        const { page, api } = await fixture(viewport);
        await reach(page, '[data-card="game:world_0"]');
        await pad(page, 'cross');
        await page.locator('[data-card="mod:100"]').waitFor();
        assert(await page.evaluate(() => document.activeElement !== document.body && document.activeElement.getClientRects().length > 0), 'Entering a game dropped controller focus to the document');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: game, mod and file navigation returns to the chosen parent`, async () => {
        const { page, api } = await fixture(viewport);
        await screenshot(page, viewport, 'games');
        await reach(page, '[data-show-details="game:world_0"]'); await pad(page, 'cross');
        await page.waitForFunction(() => document.querySelector('#details-dialog').open);
        assert.equal(await page.locator('#details-title').innerText(), 'Game 01');
        await screenshot(page, viewport, 'game-more-info'); await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.showDetails), 'game:world_0');
        await reach(page, '[data-card="game:world_0"]'); await pad(page, 'cross');
        await page.locator('[data-card="mod:100"]').waitFor();
        assert.equal(await page.evaluate(() => document.activeElement.dataset.card), 'mod:100');
        await screenshot(page, viewport, 'mods');
        await reach(page, '[data-show-details="mod:100"]'); await pad(page, 'cross');
        await page.waitForFunction(() => document.querySelector('#details-dialog').open);
        assert.equal(await page.locator('#details-title').innerText(), 'Mod 01');
        await screenshot(page, viewport, 'mod-more-info'); await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.showDetails), 'mod:100');
        await reach(page, '[data-card="mod:100"]'); await pad(page, 'cross'); await page.locator('[data-file-card="501"]').waitFor();
        assert(await page.evaluate(() => document.activeElement.closest('[data-file-card="501"]') !== null), 'Files opened without focus on the first file');
        await screenshot(page, viewport, 'files');
        await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.card), 'mod:100');
        await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.card), 'game:world_0');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: d-pad directions follow game and mod grid rows`, async () => {
        const { page, api } = await fixture(viewport);
        for (const [first, cardSelector] of [['[data-card="game:world_0"]', '[data-card^="game:"]'], ['[data-card="mod:100"]', '[data-card^="mod:"]']]) {
          await reach(page, first);
          const before = await page.evaluate(() => { const r = document.activeElement.getBoundingClientRect(); return { x: r.x, y: r.y, width: r.width, height: r.height }; });
          await pad(page, 'right');
          const right = await page.evaluate(selector => { const el = document.activeElement, r = el.getBoundingClientRect(); return { card: el.matches(selector), x: r.x, y: r.y, width: r.width, height: r.height }; }, cardSelector);
          assert(right.card && right.x > before.x + before.width / 2 && Math.abs(right.y - before.y) < before.height / 2, 'Right did not choose the card beside this one');
          await pad(page, 'down');
          const below = await page.evaluate(selector => { const el = document.activeElement, r = el.getBoundingClientRect(); return { card: el.matches(selector), x: r.x, y: r.y }; }, cardSelector);
          assert(below.card && below.y > right.y + right.height / 2 && Math.abs(below.x - right.x) < right.width / 2, 'Down moved sideways instead of to the next grid row');
          if (cardSelector.includes('game:')) { await reach(page, first); await pad(page, 'cross'); await page.locator('[data-card="mod:100"]').waitFor(); }
        }
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: all long file details can be read and Back restores focus`, async () => {
        const { page, api } = await fixture(viewport);
        await enterFiles(page); await reach(page, '[data-file-details="501"]'); await pad(page, 'cross');
        await page.waitForFunction(() => document.querySelector('#details-dialog').open);
        await screenshot(page, viewport, 'file-more-info');
        await reach(page, '#details-content');
        const initial = await page.locator('#details-content').evaluate(el => ({ top: el.scrollTop, height: el.clientHeight, total: el.scrollHeight }));
        assert(initial.total > initial.height, 'Long real description should need scrolling');
        for (let step = 0; step < 80; step++) {
          const bottom = await page.locator('#details-content').evaluate(el => el.scrollTop + el.clientHeight >= el.scrollHeight - 2);
          if (bottom) break;
          assert.equal(await page.evaluate(() => document.activeElement.id), 'details-content', 'D-pad left the description before its end');
          await pad(page, 'down');
        }
        assert(await page.locator('#details-content').evaluate(el => el.scrollTop > 0 && el.scrollTop + el.clientHeight >= el.scrollHeight - 2), 'Controller could not reach the end of the description');
        await screenshot(page, viewport, 'file-more-info-end');
        await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.fileDetails), '501');
        assert.equal(await page.locator('#details-dialog').evaluate(el => el.open), false);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: removing, clearing and submitting the selection retain focus`, async () => {
        const { page, api } = await fixture(viewport);
        await enterFiles(page);
        await reach(page, '[data-basket-add="501"]'); await pad(page, 'cross');
        await page.locator('.basket-item').waitFor();
        assert(await activeIsUsable(page));
        await reach(page, '[data-basket-remove]'); await pad(page, 'cross');
        assert.equal(await page.locator('.basket-item').count(), 0); assert(await activeIsUsable(page), 'Removing the last selected file lost focus');
        await reach(page, '[data-basket-add="501"]'); await pad(page, 'cross');
        await reach(page, '#basket-clear'); await pad(page, 'cross');
        assert.equal(await page.locator('.basket-item').count(), 0); assert(await activeIsUsable(page), 'Clearing the selection lost focus');
        await reach(page, '[data-basket-add="501"]'); await pad(page, 'cross');
        await reach(page, '[data-basket-add="502"]'); await pad(page, 'cross');
        await reach(page, '#basket-submit'); await screenshot(page, viewport, 'selected-files'); await pad(page, 'cross');
        await page.waitForFunction(() => document.querySelectorAll('.basket-item').length === 0);
        assert(await activeIsUsable(page), 'Successful download request removed its focused action');
        assert.deepEqual(api.posts.filter(item => item.path === '/api/jobs').map(item => item.body.file_id), [501, 502]);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: tabs, Activity authorization and account actions are reachable`, async () => {
        const { page, api } = await fixture(viewport);
        await reach(page, '#tab-activity'); await pad(page, 'cross');
        await page.locator('#activity-view:not([hidden])').waitFor();
        await reach(page, 'a[data-focus-key="nexus-source"]');
        assert.equal(await page.evaluate(() => document.activeElement.getAttribute('href')), 'https://www.nexusmods.com/world_0/mods/100?tab=files&file_id=501');
        await screenshot(page, viewport, 'activity');
        await page.evaluate(() => { window.ownedOldAuthorization = document.activeElement; });
        api.jobBytes = 22;
        await page.waitForFunction(() => document.querySelector('[data-job-id]')?.textContent.includes('22 bytes'));
        assert.equal(await page.evaluate(() => window.ownedOldAuthorization.isConnected), false, 'This fixture must exercise a real polling rerender');
        assert.equal(await page.evaluate(() => document.activeElement.dataset.focusKey), 'nexus-source', 'Polling lost the authorization action');
        await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.id), 'tab-games');
        await reach(page, '#tab-installed'); await pad(page, 'cross');
        await page.locator('#installed-view:not([hidden])').waitFor(); await screenshot(page, viewport, 'installed');
        await pad(page, 'circle'); await reach(page, '#disconnect-account'); await pad(page, 'cross');
        await page.locator('#sign-in').waitFor(); assert(await activeIsUsable(page), 'Disconnect removed the focused account action');
        await reach(page, '#sign-in'); await pad(page, 'cross', 500);
        await page.locator('#status-banner:not([hidden])').waitFor();
        assert.match(await page.locator('#status-banner').innerText(), /internet|connected/i);
        await screenshot(page, viewport, 'nexus-offline');
        assert.equal(api.posts.filter(item => item.path === '/api/auth/disconnect').length, 1);
        assert.equal(api.posts.filter(item => item.path === '/api/auth/start').length, 1);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: startup failure leaves controller navigation usable`, async () => {
        const { page, api } = await fixture(viewport, { failCapabilities: true });
        await reach(page, '#tab-activity'); await pad(page, 'cross');
        assert.equal(await page.locator('#activity-view').isVisible(), true);
        await pad(page, 'circle');
        assert.equal(await page.evaluate(() => document.activeElement.id), 'tab-games');
        await screenshot(page, viewport, 'startup-connection-error');
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: game selection and refresh are usable with the controller`, async () => {
        const { page, api } = await fixture(viewport, { titles: true });
        await page.locator('[data-title-key]:not(:disabled)').first().waitFor();
        await reach(page, '[data-title-key="' + '1'.repeat(32) + '"]'); await pad(page, 'cross');
        assert.equal(await page.locator('[data-title-key="' + '1'.repeat(32) + '"]').getAttribute('aria-pressed'), 'true');
        await screenshot(page, viewport, 'game-target');
        await reach(page, '#clear-target'); await pad(page, 'cross');
        assert(await activeIsUsable(page), 'Clearing the selected game lost focus');
        await reach(page, '#reload-titles'); await pad(page, 'cross');
        await page.locator('#reload-titles:not(:disabled)').waitFor(); assert(await activeIsUsable(page));
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
      await test(`${viewport.width}: readable TV scale, visible focus and viewport containment`, async () => {
        const { page, api } = await fixture(viewport);
        assert.equal(await page.evaluate(() => document.activeElement.id), 'tab-games', 'Controller has no predictable starting point');
        await reach(page, '[data-card="game:world_0"]');
        const measurements = await page.evaluate(() => {
          const el = document.activeElement, r = el.getBoundingClientRect(), css = getComputedStyle(el);
          return { width: innerWidth, height: innerHeight, documentWidth: document.documentElement.scrollWidth,
            r: { x: r.x, y: r.y, right: r.right, bottom: r.bottom }, outline: css.outlineStyle, outlineWidth: parseFloat(css.outlineWidth), outlineColor: css.outlineColor,
            title: parseFloat(getComputedStyle(el.querySelector('.card-title')).fontSize), secondary: parseFloat(getComputedStyle(el.querySelector('.card-meta')).fontSize),
            actions: [...document.querySelectorAll('.tab,.account-action,.action-button,.secondary-button,.icon-button')].filter(item => item.getClientRects().length && !item.closest('[hidden]')).map(item => ({ name: item.textContent.trim() || item.getAttribute('aria-label'), height: item.getBoundingClientRect().height })) };
        });
        const scale = viewport.width / 1920;
        assert(measurements.title >= 24 * scale && measurements.secondary >= 20 * scale, `TV text is too small: ${JSON.stringify(measurements)}`);
        assert(measurements.actions.every(item => item.height >= 56 * scale), `Some controller targets are too small: ${JSON.stringify(measurements.actions)}`);
        assert(measurements.outline !== 'none' && measurements.outlineWidth >= 3 * scale && measurements.outlineColor !== 'rgba(0, 0, 0, 0)', 'Controller focus has no clear outline');
        assert(measurements.documentWidth <= viewport.width + 1, 'UI overflows horizontally');
        assert(measurements.r.x >= 0 && measurements.r.y >= 0 && measurements.r.right <= viewport.width && measurements.r.bottom <= viewport.height, 'Focused game is outside the visible TV area');
        await pad(page, 'cross'); await page.locator('[data-card="mod:100"]').waitFor();
        await reach(page, '[data-card="mod:100"]'); await pad(page, 'cross'); await page.locator('[data-file-card="501"]').waitFor();
        await reach(page, '[data-basket-add="501"]'); await pad(page, 'cross');
        const basketInstructions = await page.locator('.basket-heading > p').evaluate(el => parseFloat(getComputedStyle(el).fontSize));
        assert(basketInstructions >= 20 * scale, `Selected-files instructions are too small for the TV: ${basketInstructions}px`);
        assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
      });
    }
  } finally {
    await browser.close(); await new Promise(resolve => server.close(resolve));
  }
  console.log(JSON.stringify({ passed, failed, webRoot }));
  fs.writeFileSync(path.join(evidenceRoot, 'RESULTS.json'), JSON.stringify({ passed, failed, webRoot, navigation: 'standard Gamepad API only; d-pad and cross/circle', assets: Object.fromEntries(['app.js', 'style.css', 'index.html'].map(name => [name, crypto.createHash('sha256').update(fs.readFileSync(path.join(webRoot, name))).digest('hex')])), results }, null, 2) + '\n');
  if (failed) process.exitCode = 1;
}
main().catch(error => { console.error(error); process.exitCode = 1; });
