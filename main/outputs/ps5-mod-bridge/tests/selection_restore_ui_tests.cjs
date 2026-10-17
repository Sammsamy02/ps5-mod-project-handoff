const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

// Exercise the production assets with intercepted, localhost-only API replies.
const webRoot = path.join(__dirname, '../web');
const storageKey = 'modbridge.selection.v1';
const observed = value => ({ value, status: 'observed', source: 'owned_fixture' });
const inventory = {
  schema_version: 1, status: 'observed', complete: true, snapshot_id: '1'.repeat(32),
  source_scope: 'running_processes', reason: null,
  items: ['a', 'b'].map((key, i) => ({ title_key: key.repeat(32), title_id: observed(`PPSA9000${i}`), display_name: observed(`Console game ${i}`), build_version: observed('1.0'), build_fingerprint: null })),
};
const savedRow = (id = 101) => ({ game: 'alpha', mod_id: '21', file_id: String(id), idempotency_key: 'mb_' + String(id).padStart(36, 'a'), name: `File ${id}`, modName: 'Owned mod', needsTargetSelection: false });

async function main() {
  const server = http.createServer((req, res) => {
    const name = req.url === '/' ? 'index.html' : req.url.slice(1);
    if (!['index.html', 'app.js', 'style.css'].includes(name)) return res.writeHead(404).end();
    res.writeHead(200, { 'Content-Type': name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript' });
    res.end(fs.readFileSync(path.join(webRoot, name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const options = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) options.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) options.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(options), contexts = [];
  let passed = 0, failed = 0;
  async function fixture(extra = {}) {
    const context = await browser.newContext(); contexts.push(context);
    const api = { posts: [], errors: [], outbound: [], connected: true, ...extra };
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, async route => {
      const req = route.request(), url = new URL(req.url());
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (url.pathname === '/api/capabilities') return json({ csrf_token: 'never-store-csrf', capabilities: { nexus_catalog: true, account_status: true, detected_titles: true, jobs: true, download_analyze: true } });
      if (url.pathname === '/api/account') return json(api.connected ? { status: 'connected', live_service: 'verified', account: { display_name: api.accountName || 'Owned account', credential: 'never-store-account' } } : { status: 'disconnected' });
      if (url.pathname === '/api/auth/disconnect') { api.connected = false; return json({ status: 'disconnected' }); }
      if (url.pathname.startsWith('/api/detected-titles')) return json(api.inventory || inventory);
      if (url.pathname === '/api/games') return json({ items: [{ domain_name: 'alpha', name: 'Alpha world' }] });
      if (/\/mods$/.test(url.pathname)) return json({ items: [{ mod_id: 21, name: api.longLabels ? 'M'.repeat(1000) : 'Owned mod' }] });
      if (/\/files$/.test(url.pathname)) return json({ items: Array.from({ length: 25 }, (_, i) => ({ file_id: i + 101, name: api.longLabels ? 'N'.repeat(1000) : `File ${i + 101}`, signed_url: 'https://private.invalid/?secret=never-store-url' })) });
      if (url.pathname === '/api/jobs' && req.method() === 'GET' && api.blockInitialJobs) {
        api.blockInitialJobs = false;
        await new Promise(resolve => { api.releaseInitialJobs = resolve; });
      }
      if (url.pathname === '/api/jobs' && req.method() === 'POST') {
        const body = req.postDataJSON(); api.posts.push(body);
        if (body.file_id === api.capacityFile) return json({ error: { code: 'job_limit', message: 'Queue full.' } }, 409);
        if (body.file_id === api.failFile) return json({ error: { code: 'busy', message: 'Temporary failure.' } }, 503);
        if (body.file_id === api.blockFile) await new Promise(resolve => { api.release = resolve; });
        return json({ id: String(body.file_id).padStart(32, 'a'), status: 'queued', private_reply: 'never-store-reply' }, 202);
      }
      return json({ items: [] });
    });
    const page = await context.newPage(); page.setDefaultTimeout(1600);
    page.on('pageerror', error => api.errors.push(error.message));
    if (extra.saved !== undefined) await context.addInitScript(({ key, value }) => {
      if (!sessionStorage.getItem('fixture-seeded')) { localStorage.setItem(key, value); sessionStorage.setItem('fixture-seeded', 'yes'); }
    }, { key: storageKey, value: extra.saved });
    if (extra.storageDenied) await context.addInitScript(() => {
      Object.defineProperty(window, 'localStorage', { get() { throw new DOMException('Storage disabled', 'SecurityError'); } });
    });
    await page.goto(origin); await ready(page);
    return { page, api, context };
  }
  async function ready(page) {
    await page.waitForFunction(() => document.querySelector('#title-status')?.textContent.includes('Running titles') && document.querySelector('#catalog-count')?.textContent.includes('1 of 1'));
    await page.waitForTimeout(40);
  }
  async function files(page) {
    if (await page.locator('[data-card="game:alpha"]').count()) { await page.locator('[data-card="game:alpha"]').click(); await page.locator('[data-card="mod:21"]').waitFor(); }
    if (await page.locator('[data-card="mod:21"]').count()) await page.locator('[data-card="mod:21"]').click();
    await page.locator('[data-basket-add="101"]').waitFor();
  }
  const saved = page => page.evaluate(key => JSON.parse(localStorage.getItem(key) || 'null'), storageKey);
  const reload = async page => { await page.reload(); await ready(page); };
  const add = (page, id) => page.locator(`[data-basket-add="${id}"]`).click();
  async function test(name, run) {
    if (process.env.TEST_FILTER && !new RegExp(process.env.TEST_FILTER).test(name)) return;
    try { await run(); passed++; console.log(`PASS ${name}`); }
    catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
    finally { while (contexts.length) await contexts.pop().close(); }
  }
  try {
    await test('reload retains reviewed source files and keys without creating jobs or storing private data', async () => {
      const { page, api } = await fixture(); await files(page); await add(page, 101); await add(page, 102);
      const before = await saved(page); assert.equal(before?.items?.length, 2, 'selected files must be persisted');
      assert.equal(before.version, 1);
      assert.deepEqual(Object.keys(before).sort(), ['items', 'version']);
      assert.deepEqual(Object.keys(before.items[0]).sort(), ['file_id', 'game', 'idempotency_key', 'modName', 'mod_id', 'name', 'needsTargetSelection']);
      assert.doesNotMatch(JSON.stringify(before), /never-store|target_ref|snapshot_id|title_key|authorization|https:/);
      await reload(page);
      assert.equal(await page.locator('.basket-item').count(), 2); assert.deepEqual(await saved(page), before); assert.deepEqual(api.posts, []);
      await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.deepEqual(api.posts.map(item => item.idempotency_key), before.items.map(item => item.idempotency_key));
      assert.equal(await saved(page), null); await reload(page); assert.equal(await page.locator('.basket-item').count(), 0);
      assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
    });
    await test('startup cannot submit a new intent before the saved selection has restored', async () => {
      const { page, api } = await fixture({ blockInitialJobs: true, saved: JSON.stringify({ version: 1, items: [savedRow()] }) });
      await files(page);
      assert.equal(await page.locator('[data-basket-add="101"]').isDisabled(), true);
      assert.equal(await page.locator('[data-file-action="101"]').isDisabled(), true);
      await page.locator('[data-basket-add="101"]').dispatchEvent('click');
      await page.locator('[data-file-action="101"]').dispatchEvent('click');
      assert.deepEqual(api.posts, []); assert.equal((await saved(page)).items[0].idempotency_key, savedRow().idempotency_key);
      api.releaseInitialJobs(); await page.locator('.basket-item').waitFor();
      await page.locator('[data-file-action="101"]').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.equal(api.posts.length, 1); assert.equal(api.posts[0].idempotency_key, savedRow().idempotency_key);
      assert.equal(await saved(page), null); assert.deepEqual(api.errors, []);
    });
    await test('each admitted file is removed from storage while the next request is still pending', async () => {
      const { page, api } = await fixture({ blockFile: 102 }); await files(page); await add(page, 101); await add(page, 102);
      await page.locator('#basket-submit').click();
      await page.waitForFunction(key => JSON.parse(localStorage.getItem(key))?.items.length === 1, storageKey);
      assert.deepEqual((await saved(page)).items.map(item => item.file_id), ['102']);
      while (!api.release) await page.waitForTimeout(10); api.release();
      await page.waitForFunction(key => localStorage.getItem(key) === null, storageKey);
      assert.deepEqual(api.posts.map(item => item.file_id), [101, 102]);
    });
    for (const rejection of ['failFile', 'capacityFile']) await test(`${rejection} survives reload and retries only remaining files with the original key`, async () => {
      const { page, api } = await fixture({ [rejection]: 102 }); await files(page); await add(page, 101); await add(page, 102); await add(page, 103);
      await page.locator('#basket-submit').click(); await page.locator('.basket-status').waitFor();
      const key = api.posts[1].idempotency_key;
      await reload(page); assert.equal(await page.locator('.basket-item').count(), 2); assert.equal(api.posts.length, 2);
      api[rejection] = null; await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.deepEqual(api.posts.map(item => item.file_id), [101, 102, 102, 103]); assert.equal(api.posts[2].idempotency_key, key);
      assert.deepEqual(api.errors, []);
    });
    await test('a direct download failure becomes a restorable selection with the same retry key', async () => {
      const { page, api } = await fixture({ failFile: 101 }); await files(page); await page.locator('[data-file-action="101"]').click();
      await page.locator('#status-banner').getByText(/Retry/).waitFor(); const key = api.posts[0].idempotency_key;
      await reload(page); assert.equal(await page.locator('.basket-item').count(), 1); assert.equal(api.posts.length, 1);
      api.failFile = null; await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.equal(api.posts[1].idempotency_key, key); assert.equal(await saved(page), null);
    });
    await test('target-bound selections require a fresh explicit choice and changed request key after reload', async () => {
      const { page, api } = await fixture();
      await page.locator(`[data-title-key="${'a'.repeat(32)}"]`).click(); await files(page); await add(page, 101);
      const before = await saved(page); assert.equal(before?.items[0].needsTargetSelection, true);
      assert.doesNotMatch(JSON.stringify(before), /target_ref|snapshot_id|title_key|PPSA/);
      await reload(page); assert.equal(await page.locator('.basket-item').count(), 1); assert.equal(await page.locator('#basket-submit').isDisabled(), true);
      assert.match(await page.locator('#selection-basket').innerText(), /choose.*target/i); await page.locator('[data-file-action="101"]').click(); assert.deepEqual(api.posts, []);
      await page.locator(`[data-title-key="${'b'.repeat(32)}"]`).click(); assert.equal(await page.locator('.basket-item').count(), 1);
      await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.notEqual(api.posts[0].idempotency_key, before.items[0].idempotency_key);
      assert.deepEqual(api.posts[0].target_ref, { snapshot_id: '1'.repeat(32), title_key: 'b'.repeat(32) });
    });
    await test('refreshing unavailable targets preserves restored files until a target can be chosen', async () => {
      const { page, api } = await fixture();
      await page.locator(`[data-title-key="${'a'.repeat(32)}"]`).click(); await files(page); await add(page, 101);
      api.inventory = { schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'unavailable', reason: null, items: [] };
      await page.reload(); await page.locator('#title-status').getByText(/unavailable/i).waitFor();
      await page.locator('.basket-item').waitFor(); assert.equal(await page.locator('#basket-submit').isDisabled(), true);
      api.inventory = inventory; await page.locator('#reload-titles').click(); await ready(page);
      assert.equal(await page.locator('.basket-item').count(), 1, 'refresh must keep restored files awaiting target review');
      await page.locator(`[data-title-key="${'a'.repeat(32)}"]`).click();
      assert.equal(await page.locator('#basket-submit').isDisabled(), false); assert.deepEqual(api.posts, []);
    });
    await test('remove and explicit Clear update storage and prevent restore', async () => {
      const { page } = await fixture(); await files(page); await add(page, 101); await add(page, 102);
      await page.locator('[data-basket-remove="alpha:21:101"]').click(); await reload(page);
      assert.deepEqual((await saved(page)).items.map(item => item.file_id), ['102']);
      await page.locator('#basket-clear').click(); assert.equal(await saved(page), null); await reload(page); assert.equal(await page.locator('.basket-item').count(), 0);
    });
    await test('account changes never submit restored selections and disconnect explicitly clears them', async () => {
      const { page, api } = await fixture(); await files(page); await add(page, 101); api.connected = false; await reload(page);
      assert.equal(await page.locator('.basket-item').count(), 1); assert.equal(await page.locator('#basket-submit').isDisabled(), true); assert.deepEqual(api.posts, []);
      api.connected = true; api.accountName = 'Another account'; await reload(page); assert.deepEqual(api.posts, []);
      await page.getByRole('button', { name: 'Disconnect', exact: true }).click(); await page.getByText('Nexus account disconnected.').waitFor();
      assert.equal(await saved(page), null); await reload(page); assert.equal(await page.locator('.basket-item').count(), 0);
    });
    await test('selection and labels remain bounded and stored labels render as text', async () => {
      const { page, api } = await fixture({ longLabels: true }); await files(page);
      for (let id = 101; id <= 125; id++) await add(page, id);
      assert.equal(await page.locator('.basket-item').count(), 24); await page.locator('[data-file-action="125"]').click(); assert.deepEqual(api.posts, []); const data = await saved(page); assert.equal(data?.items.length, 24);
      assert(data.items.every(item => item.name.length <= 160 && item.modName.length <= 160));
      await reload(page); assert.equal(await page.locator('.basket-item').count(), 24); assert.deepEqual(api.posts, []);
      const encoded = JSON.stringify({ version: 1, items: [{ ...savedRow(), name: '<img src=x onerror=alert(1)>' }] });
      await page.evaluate(({ key, value }) => localStorage.setItem(key, value), { key: storageKey, value: encoded }); await reload(page);
      assert.match(await page.locator('.basket-item').innerText(), /<img src=x/); assert.equal(await page.locator('#selection-basket img').count(), 0);
    });
    const invalid = [
      ['invalid JSON', '{'], ['stale version', JSON.stringify({ version: 0, items: [savedRow()] })],
      ['unsafe source', JSON.stringify({ version: 1, items: [{ ...savedRow(), game: '../escape' }] })],
      ['unsafe ID', JSON.stringify({ version: 1, items: [{ ...savedRow(), file_id: '9007199254740992' }] })],
      ['invalid key', JSON.stringify({ version: 1, items: [{ ...savedRow(), idempotency_key: 'secret-value' }] })],
      ['unknown field', JSON.stringify({ version: 1, items: [{ ...savedRow(), target_ref: { snapshot_id: '1'.repeat(32), title_key: 'a'.repeat(32) } }] })],
      ['duplicate identity', JSON.stringify({ version: 1, items: [savedRow(), savedRow()] })],
      ['too many rows', JSON.stringify({ version: 1, items: Array.from({ length: 25 }, (_, i) => savedRow(101 + i)) })],
      ['oversized envelope', ' '.repeat(32769)],
    ];
    for (const [name, raw] of invalid) await test(`${name} is discarded safely without sending jobs`, async () => {
      const { page, api } = await fixture({ saved: raw }); assert.equal(await page.locator('.basket-item').count(), 0);
      assert.equal(await saved(page), null); assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('a failed storage replacement drops the old admitted copy and keeps the remaining request in memory', async () => {
      const { page, api } = await fixture({ capacityFile: 102 }); await files(page); await add(page, 101); await add(page, 102);
      await page.evaluate(() => {
        const original = Storage.prototype.setItem;
        Storage.prototype.setItem = function (key, value) {
          if (this === localStorage) throw new DOMException('Quota exceeded', 'QuotaExceededError');
          return original.call(this, key, value);
        };
      });
      await page.locator('#basket-submit').click(); await page.locator('.basket-status').waitFor();
      assert.equal(await saved(page), null); assert.equal(await page.locator('.basket-item').count(), 1);
      const key = api.posts[1].idempotency_key; api.capacityFile = null;
      await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.equal(api.posts[2].idempotency_key, key); assert.deepEqual(api.posts.map(item => item.file_id), [101, 102, 102]);
      await reload(page); assert.equal(await page.locator('.basket-item').count(), 0); assert.deepEqual(api.errors, []);
    });
    await test('unavailable browser storage keeps in-memory selection and retries usable', async () => {
      const { page, api } = await fixture({ storageDenied: true, failFile: 101 }); await files(page); await add(page, 101);
      await page.locator('#basket-submit').click(); await page.locator('.basket-status').waitFor(); const key = api.posts[0].idempotency_key;
      api.failFile = null; await page.locator('#basket-submit').click(); await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.equal(api.posts[1].idempotency_key, key); assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
    });
    console.log(`selection restore UI: ${passed} passed, ${failed} failed`); if (failed) process.exitCode = 1;
  } finally {
    for (const context of contexts) await context.close();
    await browser.close(); await new Promise(resolve => server.close(resolve));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
