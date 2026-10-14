const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

// Production browser assets, owned localhost responses. No live services.
const webRoot = path.join(__dirname, '..', 'web');
const observed = value => ({ value, status: 'observed', source: 'owned_fixture' });
const inventory = {
  schema_version: 1, status: 'observed', complete: true, snapshot_id: '1'.repeat(32),
  source_scope: 'running_processes', reason: null,
  items: ['a', 'b'].map((key, i) => ({ title_key: key.repeat(32), title_id: observed(`PPSA9000${i}`), display_name: observed(`Console game ${i}`), build_version: observed('1.0'), build_fingerprint: null })),
};

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
    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } }); contexts.push(context);
    const api = { gets: [], posts: [], errors: [], outbound: [], failFile: null, failOnce: false, available: true, ...extra };
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, async route => {
      const req = route.request(), url = new URL(req.url());
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      api.gets.push(`${req.method()} ${url.pathname}`);
      if (url.pathname === '/api/capabilities') return json({ mode: 'desktop_preparation', csrf_token: 'basket-csrf', capabilities: { nexus_catalog: true, account_status: true, detected_titles: true, jobs: true, download_analyze: api.available, deploy: false, ...(api.capacity === undefined ? {} : { job_capacity: api.capacity }) } });
      if (url.pathname === '/api/account') return json({ status: 'connected', live_service: 'verified', account: { display_name: 'Owned fixture' } });
      if (url.pathname === '/api/auth/disconnect') return json({ status: 'disconnected' });
      if (url.pathname.startsWith('/api/detected-titles')) return json(inventory);
      if (url.pathname === '/api/games') return json({ items: [{ domain_name: 'zeta', name: 'Zeta world' }, { domain_name: 'alpha', name: 'Alpha world' }] });
      if (/\/mods$/.test(url.pathname)) return json({ items: [{ mod_id: 21, name: 'Zebra textures', author: 'Ada', version: '1.0' }, { mod_id: 22, name: 'Amber camera', author: 'Ben', version: '2.0' },
        ...(api.moreMods ? [{ mod_id: 23, name: 'Cobalt lamps', author: 'Cy', version: '1.1' }, { mod_id: 24, name: 'Dune fog', author: 'Di', version: '1.2' }] : [])] });
      const files = /\/mods\/(21|22|23|24)\/files$/.exec(url.pathname);
      if (files) return json({ items: [{ file_id: Number(files[1]) + 100, name: `Main ${files[1]}`, file_name: `main${files[1]}.zip`, size_kb: 1 }] });
      if (url.pathname === '/api/jobs' && req.method() === 'POST') {
        const body = req.postDataJSON(); api.posts.push({ body, headers: req.headers() });
        if (api.slowRequests) await new Promise(resolve => setTimeout(resolve, 300));
        if (body.file_id === api.capacityFile) return json({ error: { code: 'job_limit', message: 'The local job queue is full.' } }, 409);
        if (api.staleTarget) return json({ error: { code: 'stale_target_ref', message: 'Owned snapshot changed.' } }, 409);
        if (body.file_id === api.failFile && api.failOnce) { api.failOnce = false; return json({ error: { code: 'busy', message: 'Temporary owned error.' } }, 503); }
        return json({ id: String(body.file_id).padStart(32, 'a'), status: 'queued' }, 202);
      }
      if (url.pathname === '/api/jobs') { api.jobPolls = (api.jobPolls || 0) + 1; return json({ items: api.jobs || [] }); }
      return json({ items: [] });
    });
    const page = await context.newPage(); page.setDefaultTimeout(1300);
    page.on('pageerror', error => api.errors.push(error.message));
    await page.goto(origin); await page.getByRole('button', { name: 'Zeta world' }).waitFor();
    return { page, api };
  }
  const add = async (page, name, id) => {
    await page.getByRole('button', { name: new RegExp(name) }).click();
    await page.locator(`[data-basket-add="${id}"]`).click();
    assert.equal(await page.evaluate(() => document.activeElement?.dataset.fileDetails), String(id));
    await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
  };
  async function test(name, run) {
    try { await run(); passed++; console.log(`PASS ${name}`); }
    catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
  }
  try {
    await test('loaded catalog filters and sorts without making a search request', async () => {
      const { page, api } = await fixture();
      await page.getByLabel('Filter loaded games').fill('alpha');
      assert.equal(await page.locator('[data-card^="game:"]').count(), 1);
      assert.match(await page.locator('#catalog-count').innerText(), /1 of 2 loaded games/);
      await page.getByLabel('Filter loaded games').fill('');
      await page.locator('#catalog-sort').click();
      assert.equal(await page.locator('[data-card^="game:"]').first().getAttribute('data-card'), 'game:alpha');
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await page.getByRole('button', { name: /Zebra textures/ }).waitFor();
      await page.getByLabel('Filter loaded latest mods').fill('ada');
      assert.equal(await page.locator('[data-card^="mod:"]').count(), 1);
      assert.match(await page.locator('#catalog-count').innerText(), /1 of 2 loaded latest mods/);
      assert.equal(api.gets.some(url => /search|sort|popular/.test(url)), false);
      if (process.env.UI_MODS_SCREENSHOT_PATH) await page.screenshot({ path: process.env.UI_MODS_SCREENSHOT_PATH, fullPage: true });
      assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, []);
    });
    await test('multiple mods retain target provenance and retry only the uncertain request with the same intent', async () => {
      const { page, api } = await fixture({ failFile: 122, failOnce: true });
      await page.locator('[data-title-key="' + 'a'.repeat(32) + '"]').click();
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122);
      assert.equal(await page.locator('.basket-item').count(), 2);
      assert.match(await page.locator('#selection-basket').innerText(), /PPSA90000/);
      assert.match(await page.locator('#selection-basket').innerText(), /Installation unavailable/);
      if (process.env.UI_BASKET_SCREENSHOT_PATH) await page.screenshot({ path: process.env.UI_BASKET_SCREENSHOT_PATH, fullPage: true });
      await page.locator('#basket-submit').click();
      await page.locator('.basket-status').getByText(/Retry/).waitFor();
      assert.equal(api.posts.length, 2);
      assert.equal(await page.locator('.basket-item').count(), 1);
      await page.locator('#basket-submit').click();
      await page.getByText('Selected downloads requested.').waitFor();
      assert.equal(api.posts.length, 3);
      assert.equal(api.posts[1].body.idempotency_key, api.posts[2].body.idempotency_key);
      assert.notEqual(api.posts[0].body.idempotency_key, api.posts[1].body.idempotency_key);
      assert.deepEqual(api.posts.map(row => row.body.file_id), [121, 122, 122]);
      assert.deepEqual(api.posts[0].body.target_ref, { snapshot_id: '1'.repeat(32), title_key: 'a'.repeat(32) });
      assert.equal(api.posts[0].headers['x-csrf-token'], 'basket-csrf');
      assert.deepEqual(api.errors, []);
    });
    await test('capacity rejection retains the remaining intent without promising retry recovery', async () => {
      const { page, api } = await fixture({ capacityFile: 122 });
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122);
      await page.locator('#basket-submit').click();
      await page.locator('.basket-status').waitFor();
      assert.equal(api.posts.length, 2);
      assert.equal(await page.locator('.basket-item').count(), 1);
      assert.match(await page.locator('.basket-item').innerText(), /Main 22/);
      assert.match(await page.locator('#status-banner').innerText(), /Host job capacity reached.*remaining files have not been requested/i);
      assert.doesNotMatch(await page.locator('#selection-basket').innerText(), /retry/i);
      assert.doesNotMatch(await page.locator('#status-banner').innerText(), /retry|restart/i);
      const retainedKey = api.posts[1].body.idempotency_key;
      api.capacityFile = null;
      await page.locator('#basket-submit').click();
      await page.getByText('Selected downloads requested.').waitFor();
      assert.deepEqual(api.posts.map(row => row.body.file_id), [121, 122, 122]);
      assert.equal(api.posts[2].body.idempotency_key, retainedKey);
      assert.deepEqual(api.errors, []);
    });
    await test('target, account and game changes cannot execute a stale selection', async () => {
      const { page, api } = await fixture();
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await page.getByRole('button', { name: /Zebra textures/ }).click();
      await page.locator('[data-basket-add="121"]').click();
      await page.locator('[data-title-key="' + 'b'.repeat(32) + '"]').click();
      assert.equal(await page.locator('.basket-item').count(), 0);
      assert.equal(await page.locator('[data-basket-add="121"]').isDisabled(), false);
      await page.locator('[data-basket-add="121"]').click();
      await page.getByRole('button', { name: 'Disconnect', exact: true }).click();
      await page.getByText('Nexus account disconnected.').waitFor();
      assert.equal(await page.locator('.basket-item').count(), 0);
      assert.deepEqual(api.posts, []);
      const second = await fixture();
      await second.page.getByRole('button', { name: 'Zeta world' }).click(); await add(second.page, 'Zebra textures', 121);
      await second.page.locator('[data-breadcrumb="games"]').click();
      await second.page.getByRole('button', { name: 'Alpha world' }).click();
      assert.equal(await second.page.locator('.basket-item').count(), 0);
      assert.deepEqual(second.api.posts, []);
    });
    await test('review remove clear and controller activation work without requesting downloads', async () => {
      const { page, api } = await fixture();
      await page.getByRole('button', { name: 'Zeta world' }).click(); await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122);
      await page.getByRole('button', { name: 'Remove Main 21', exact: true }).click();
      assert.equal(await page.locator('.basket-item').count(), 1);
      await page.locator('#basket-clear').focus();
      await page.evaluate(() => { const buttons = Array.from({ length: 17 }, () => ({ pressed: false })); buttons[0].pressed = true; Object.defineProperty(navigator, 'getGamepads', { value: () => [{ mapping: 'standard', buttons, axes: [0, 0] }], configurable: true }); });
      await page.waitForFunction(() => document.querySelector('#selection-basket').hidden);
      assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('missing download capability does not offer an executable selection', async () => {
      const { page, api } = await fixture({ available: false });
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await page.getByRole('button', { name: /Zebra textures/ }).click();
      assert.equal(await page.locator('[data-basket-add="121"]').isDisabled(), true);
      assert.deepEqual(api.posts, []);
    });
    await test('a stale target stops the batch without retargeting or replaying', async () => {
      const { page, api } = await fixture({ staleTarget: true });
      await page.locator('[data-title-key="' + 'a'.repeat(32) + '"]').click();
      await page.getByRole('button', { name: 'Zeta world' }).click(); await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122);
      await page.locator('#basket-submit').click();
      await page.locator('#status-banner').getByText(/target changed/).waitFor();
      assert.equal(api.posts.length, 1);
      assert.equal(await page.locator('.basket-item').count(), 0);
      assert.match(await page.locator('#selected-target').innerText(), /No console target/);
      assert.deepEqual(api.errors, []);
    });
    await test('changing the Nexus game during a request stops the remaining batch', async () => {
      const { page, api } = await fixture({ slowRequests: true });
      await page.getByRole('button', { name: 'Zeta world' }).click(); await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122);
      await page.locator('#basket-submit').click();
      await page.locator('[data-breadcrumb="games"]').click();
      await page.getByRole('button', { name: 'Alpha world' }).click();
      await page.waitForTimeout(400);
      assert.equal(api.posts.length, 1);
      assert.equal(await page.locator('.basket-item').count(), 0);
      assert.deepEqual(api.errors, []);
    });
    // ---- Job capacity (docs/INSTALL-FLOW-UX.md 3.14): the selection never holds more than the host can queue or run ----
    const busyJob = (n, status = 'downloading') => ({ id: String(n).padStart(32, 'c'), kind: 'download_analyze', game: 'zeta', mod_id: 900 + n, file_id: 9000 + n, label: `Busy ${n}`, status, stage: 'requested', compatibility: 'unresolved' });
    const heading = page => page.locator('#basket-heading').innerText().then(text => text.replace(/\s+/g, ' ').trim());
    await test('the selection holds only what the host can queue or run: its limit less the jobs already in use, with the reason', async () => {
      const { page, api } = await fixture({ capacity: { max_queued: 3, max_records: 8 }, moreMods: true, jobs: [busyJob(1), busyJob(2, 'queued'), busyJob(3, 'analyzed'), busyJob(4, 'failed')] });
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await add(page, 'Zebra textures', 121);
      assert.equal(await heading(page), 'Selected files (1/1)', 'two of three are in use: room for one; finished and failed jobs are not');
      assert.match(await page.locator('[data-basket-capacity]').innerText(), /The host takes up to 3 downloads queued or running at once; 2 are in use\./);
      await page.getByRole('button', { name: /Amber camera/ }).click();
      await page.locator('[data-basket-add="122"]').click();
      assert.equal(await page.locator('.basket-item').count(), 1, 'the second file was not added');
      assert.match(await page.locator('#status-banner').innerText(), /The host takes at most 3 downloads queued or running at once and 2 are in use now, so a selection can hold up to 1 file\. Remove a file, or wait for downloads to finish\./);
      assert.equal(await page.locator('[data-basket-add="122"]').getAttribute('aria-pressed'), 'false');
      // A job finishes: the page's own poll (2.5 s) brings the room back and the second file fits.
      api.jobs = [busyJob(1, 'analyzed'), busyJob(2, 'queued'), busyJob(3, 'analyzed')];
      await page.waitForFunction(() => document.querySelector('#basket-heading')?.textContent.includes('(1/2)'), null, { timeout: 5000 });
      await page.locator('[data-basket-add="122"]').click();
      assert.equal(await page.locator('.basket-item').count(), 2);
      assert.equal(await heading(page), 'Selected files (2/2)');
      assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('with no room at all nothing can be selected and the message says what to wait for', async () => {
      const { page, api } = await fixture({ capacity: { max_queued: 2, max_records: 8 }, jobs: [busyJob(1), busyJob(2, 'queued')] });
      await page.getByRole('button', { name: 'Zeta world' }).click(); await page.getByRole('button', { name: /Zebra textures/ }).click();
      await page.locator('[data-basket-add="121"]').click();
      assert.equal(await page.locator('.basket-item').count(), 0);
      assert.match(await page.locator('#status-banner').innerText(), /The host already has 2 downloads queued or running and takes at most 2 at once\. Wait for some to finish, then select more\./);
      await page.locator('[data-file-action="121"]').click();
      assert.equal(await page.locator('.basket-item').count(), 0, 'the one-click Download is held to the same room');
      assert.deepEqual(api.posts, []); assert.deepEqual(api.errors, []);
    });
    await test('a selection larger than the room is not sent; the message counts, and the same selection goes once there is room', async () => {
      const { page, api } = await fixture({ capacity: { max_queued: 4, max_records: 8 }, moreMods: true });
      await page.getByRole('button', { name: 'Zeta world' }).click();
      await add(page, 'Zebra textures', 121); await add(page, 'Amber camera', 122); await add(page, 'Cobalt lamps', 123);
      assert.equal(await heading(page), 'Selected files (3/4)');
      api.jobs = [busyJob(1), busyJob(2, 'queued'), busyJob(3, 'analyzing')];   // three jobs began elsewhere: room for one
      await page.locator('#basket-submit').click();
      await page.locator('#status-banner').getByText(/You selected 3 files, but the host has room for 1 more download right now/).waitFor();
      assert.match(await page.locator('#status-banner').innerText(), /it takes at most 4 queued or running at once, and 3 are in use\)\. Remove 2 files, or wait for downloads to finish, then press Download selected again\./);
      assert.deepEqual(api.posts, [], 'a selection that does not fit asks the host for nothing');
      assert.equal(await page.locator('.basket-item').count(), 3, 'nothing was dropped');
      assert.match(await page.locator('[data-basket-capacity]').innerText(), /This selection is larger than the 1 the host can take now\./);
      assert.equal(await page.locator('#basket-submit').isEnabled(), true, 'the button explains instead of going dead');
      api.jobs = [busyJob(1, 'analyzed'), busyJob(2, 'analyzed'), busyJob(3, 'analyzed')];
      await page.locator('#basket-submit').click();
      await page.getByText('Selected downloads requested.').waitFor();
      assert.deepEqual(api.posts.map(row => row.body.file_id), [121, 122, 123]);
      assert.deepEqual(api.errors, []);
    });
    await test('without a valid job_capacity the ceiling stays 24 and the host is not asked about room', async () => {
      for (const capacity of [undefined, null, {}, { max_queued: 0, max_records: 5 }, { max_queued: 6, max_records: 5 }, { max_queued: '6', max_records: 9 }, { max_queued: 2.5, max_records: 9 },
        { max_queued: 4, max_records: 8, extra: 1 }, { max_queued: 4 }, [4, 8]]) {
        const { page, api } = await fixture({ capacity, moreMods: true, jobs: [busyJob(1), busyJob(2), busyJob(3), busyJob(4), busyJob(5)] });
        await page.getByRole('button', { name: 'Zeta world' }).click(); await add(page, 'Zebra textures', 121);
        assert.equal(await heading(page), 'Selected files (1/24)', JSON.stringify(capacity));
        assert.equal(await page.locator('[data-basket-capacity]').count(), 0, JSON.stringify(capacity));
        const before = api.jobPolls;
        await page.locator('#basket-submit').click(); await page.getByText('Selected downloads requested.').waitFor();
        assert.equal(api.posts.length, 1, JSON.stringify(capacity));
        assert.ok(api.jobPolls - before <= 2, 'only the usual refresh after the request');
        assert.deepEqual(api.errors, []);
      }
    });
    console.log(`selection basket UI: ${passed} passed, ${failed} failed`);
    if (failed) process.exitCode = 1;
  } finally {
    for (const context of contexts) await context.close();
    await browser.close(); await new Promise(resolve => server.close(resolve));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
