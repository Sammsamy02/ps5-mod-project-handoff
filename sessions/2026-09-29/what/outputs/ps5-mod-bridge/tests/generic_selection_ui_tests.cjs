const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

// Real production assets with owned localhost API fixtures. Every nonlocal
// request is aborted; no account, Nexus, console or game service is contacted.
const webRoot = path.join(__dirname, '..', 'web');
const observed = value => ({ value, status: 'observed', source: 'owned_fixture' });
const titleSnapshot = (suffix = 'one') => ({
  schema_version: 1, status: 'observed', complete: true,
  snapshot_id: (suffix === 'one' ? '1' : '2').repeat(32), source_scope: 'running_processes', reason: null,
  items: [
    { title_key: 'a'.repeat(31) + (suffix === 'one' ? '1' : '2'), title_id: observed('PPSA83071'), display_name: observed('First unrelated title'), build_version: observed('3.2'), build_fingerprint: null },
    { title_key: 'b'.repeat(31) + (suffix === 'one' ? '1' : '2'), title_id: observed('PPSA94082'), display_name: observed('Second unrelated title'), build_version: observed('5.1'), build_fingerprint: { algorithm: 'sha256', value: 'a'.repeat(64), confidence: 'exact_bytes', source: 'owned_fixture', provenance: { method: 'sha256_exact_bytes', byte_length: 10, complete: true, artifact_kind: 'content' } } },
  ],
});
const modRecord = (domain, id) => ({ mod_id: Number(id), game_domain: domain, name: `Independent mod ${id}`, author: '作者 Ω', version: '2.7', summary: 'Owned metadata only', source_url: `https://www.nexusmods.com/${domain}/mods/${id}` });
const defaultInventory = { schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'running_processes', items: [], reason: 'platform_provider_unavailable' };

async function main() {
  const server = http.createServer((req, res) => {
    const name = req.url === '/' ? 'index.html' : req.url.slice(1);
    if (!['index.html', 'app.js', 'style.css'].includes(name)) return res.writeHead(404).end();
    const type = name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript';
    res.writeHead(200, { 'Content-Type': type }); res.end(fs.readFileSync(path.join(webRoot, name)));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const launch = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(launch);
  let passed = 0, failed = 0;
  const contexts = [];

  async function fixture(options = {}) {
    const context = await browser.newContext({ viewport: { width: 1440, height: 1080 } }); contexts.push(context);
    const api = { inventory: options.inventory || defaultInventory, jobs: options.jobs || [], gets: [], posts: [], refreshes: [], outbound: [], errors: [], staleOnce: false, ...options };
    await context.route('**/*', route => {
      const url = new URL(route.request().url());
      if (url.origin !== origin) { api.outbound.push(url.href); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, async route => {
      const req = route.request(), url = new URL(req.url());
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (req.method() === 'GET') api.gets.push(url.pathname);
      if (url.pathname === '/api/capabilities') return json({ mode: api.preview ? 'console_preview' : 'desktop_preparation', csrf_token: 'owned-fixture-csrf', capabilities: { nexus_catalog: !api.preview, account_status: !api.preview, detected_titles: !api.preview, jobs: !api.preview, installed_records: !api.preview, download_analyze: !api.preview, deploy: false } });
      if (url.pathname === '/api/account') return json({ status: 'connected', live_service: 'verified', account: { display_name: 'Owned fixture' } });
      if (url.pathname === '/api/detected-titles') return json(api.inventory);
      if (url.pathname === '/api/detected-titles/refresh' && req.method() === 'POST') {
        api.refreshes.push({ body: req.postDataJSON(), headers: req.headers() });
        return json(api.inventory);
      }
      if (url.pathname === '/api/games') return json({ items: [{ domain_name: 'other_world-7', id: 830, name: 'Other catalog domain' }, { domain_name: 'quiet2', id: 991, name: 'Second catalog domain' }] });
      if (url.pathname === '/api/installed') return json({ items: [] });
      if (url.pathname === '/api/jobs' && req.method() === 'GET') return json({ items: api.jobs });
      if (url.pathname === '/api/jobs' && req.method() === 'POST') {
        api.posts.push({ body: req.postDataJSON(), headers: req.headers() });
        if (api.staleOnce) { api.staleOnce = false; api.inventory = titleSnapshot('changed'); return json({ error: { code: 'stale_target_ref', message: 'Title snapshot changed.' } }, 409); }
        if (api.failRequests) return json({ error: { code: 'busy', message: 'Owned transient failure.' } }, 503);
        return json({ id: 'a'.repeat(32), status: 'queued' }, 202);
      }
      const detail = /^\/api\/games\/([a-z0-9_-]+)\/mods\/([1-9][0-9]*)$/.exec(url.pathname);
      if (detail) {
        if (detail[2] === '83001' && api.delayFirst) await new Promise(resolve => setTimeout(resolve, 180));
        return json({ item: api.wrongIdentity ? { ...modRecord(detail[1], detail[2]), game_domain: 'wrong-domain' } : modRecord(detail[1], detail[2]) });
      }
      const files = /^\/api\/games\/([a-z0-9_-]+)\/mods\/([1-9][0-9]*)\/files$/.exec(url.pathname);
      if (files) return json({ items: [{ file_id: 902, name: 'Owned payload', file_name: 'owned.zip', version: '2.7', size_kb: 1 }] });
      const latest = /^\/api\/games\/([a-z0-9_-]+)\/mods$/.exec(url.pathname);
      if (latest) return json({ items: [modRecord(latest[1], '77')] });
      return json({ error: { code: 'not_found', message: 'Owned route unavailable.' } }, 404);
    });
    const page = await context.newPage(); page.setDefaultTimeout(700);
    page.on('pageerror', error => api.errors.push(error.message));
    await page.goto(origin);
    await page.getByRole('heading', { name: 'Choose a game' }).waitFor();
    return { page, api, context };
  }
  async function test(name, action) {
    try { await action(); passed++; console.log(`PASS ${name}`); }
    catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
  }
  async function lookup(page, domain = 'unlisted_space-9', id = '83001') {
    await page.getByLabel('Nexus domain', { exact: true }).fill(domain);
    await page.getByLabel('Mod ID', { exact: true }).fill(id);
    await page.getByRole('button', { name: 'Find mod', exact: true }).click();
    await page.getByRole('heading', { name: `Files for Independent mod ${id}`, exact: true }).waitFor();
    await page.getByRole('button', { name: 'Download for analysis', exact: true }).waitFor();
  }

  try {
    await test('unavailable inventory keeps Nexus browsing analysis-only', async () => {
      const { page, api } = await fixture();
      await page.getByRole('heading', { name: 'Console target', exact: true }).waitFor();
      await page.locator('#title-status').getByText(/unavailable/i).waitFor();
      assert.equal(await page.locator('[data-title-key]').count(), 0);
      assert.match(await page.locator('#selected-target').innerText(), /No console target.*analysis only/i);
      await lookup(page);
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.getByText('Analysis request queued.').waitFor();
      assert.equal(Object.hasOwn(api.posts[0].body, 'target_ref'), false);
      assert.deepEqual(api.outbound, []); assert.deepEqual(api.errors, []);
    });

    await test('official URL lookup reaches a nonlatest arbitrary domain and only fixed local routes', async () => {
      const { page, api } = await fixture();
      await page.getByLabel('Nexus mod URL', { exact: true }).fill('https://www.nexusmods.com/unlisted_space-9/mods/83001?tab=files');
      await page.getByRole('button', { name: 'Find mod', exact: true }).click();
      await page.getByRole('heading', { name: 'Files for Independent mod 83001' }).waitFor();
      assert(api.gets.includes('/api/games/unlisted_space-9/mods/83001'));
      assert(api.gets.includes('/api/games/unlisted_space-9/mods/83001/files'));
      assert.equal(api.gets.includes('/api/games/unlisted_space-9/mods'), false);
      assert.deepEqual(api.outbound, []); assert.deepEqual(api.errors, []);
    });

    await test('hostile URLs and unsafe identities produce no detail or external request', async () => {
      const { page, api } = await fixture();
      const urls = [
        'http://www.nexusmods.com/quiet2/mods/31', 'https://www.nexusmods.com.evil.example/quiet2/mods/31',
        'https://person:secret@www.nexusmods.com/quiet2/mods/31', 'https://www.nexusmods.com:8443/quiet2/mods/31',
        'https://www.nexusmods.com/quiet2%2fescape/mods/31', 'https://www.nexusmods.com/quiet2/mods/%33%31',
        'https://www.nexusmods.com/quiet2/../quiet2/mods/31', 'https://www.nexusmods.com/quiet2/mods/031',
        'https://www.nexusmods.com/quiet2/mods/9223372036854775807', 'https://www.nexusmods.com/quiet2/mods/31?api_key=secret',
        'https://www.nexusmods.com/quiet2/mods/31?tab=files&tab=description', 'https://www.nexusmods.com/quiet2/mods/31?redirect=https://evil.example',
      ];
      for (const url of urls) {
        await page.getByLabel('Nexus mod URL', { exact: true }).fill(url);
        await page.getByRole('button', { name: 'Find mod', exact: true }).click();
        assert.match(await page.locator('#status-banner').innerText(), /valid|safe|official/i);
      }
      await page.getByLabel('Nexus mod URL', { exact: true }).fill('');
      await page.getByLabel('Nexus domain', { exact: true }).fill('_bad');
      await page.getByLabel('Mod ID', { exact: true }).fill('31');
      await page.getByRole('button', { name: 'Find mod', exact: true }).click();
      assert.equal(api.gets.some(value => /\/mods\/[0-9]+/.test(value)), false);
      assert.deepEqual(api.outbound, []); assert.deepEqual(api.errors, []);
    });

    await test('metadata identity mismatch and late detail response never replace current selection', async () => {
      const bad = await fixture({ wrongIdentity: true });
      await bad.page.getByLabel('Nexus domain', { exact: true }).fill('quiet2');
      await bad.page.getByLabel('Mod ID', { exact: true }).fill('31');
      await bad.page.getByRole('button', { name: 'Find mod', exact: true }).click();
      await bad.page.locator('#status-banner').getByText(/identity/i).waitFor();
      assert.equal(bad.api.gets.some(value => value.endsWith('/files')), false);
      const { page, api } = await fixture({ delayFirst: true });
      await page.getByLabel('Nexus domain', { exact: true }).fill('unlisted_space-9');
      await page.getByLabel('Mod ID', { exact: true }).fill('83001');
      await page.getByRole('button', { name: 'Find mod', exact: true }).click();
      await page.getByRole('button', { name: /Other catalog domain/ }).click();
      await page.getByRole('heading', { name: 'Latest mods for Other catalog domain' }).waitFor();
      await page.waitForTimeout(220);
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Other catalog domain');
      assert.equal(api.gets.some(value => value === '/api/games/unlisted_space-9/mods/83001/files'), false);
    });

    await test('complete independent target selection sends only opaque references and restores by current IDs', async () => {
      const { page, api } = await fixture({ inventory: titleSnapshot() });
      await page.locator('[data-title-key="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"]').click();
      assert.match(await page.locator('#selected-target').innerText(), /PPSA83071/);
      assert.match(await page.locator('#title-list').innerText(), /Build (?:unknown|unverified)/i);
      await lookup(page, 'quiet2', '731');
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.getByText('Analysis request queued.').waitFor();
      const body = api.posts[0].body;
      assert.deepEqual(body.target_ref, { snapshot_id: '11111111111111111111111111111111', title_key: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1' });
      assert.deepEqual(Object.keys(body).sort(), ['file_id', 'game', 'idempotency_key', 'kind', 'mod_id', 'target_ref']);
      assert.equal(body.game, 'quiet2'); assert.equal(body.mod_id, 731);
      assert.equal(api.posts[0].headers['x-csrf-token'], 'owned-fixture-csrf');
      await page.reload(); await page.locator('#selected-target').getByText(/PPSA83071/).waitFor();
      await page.getByRole('heading', { name: 'Files for Independent mod 731', exact: true }).waitFor();
      api.inventory = titleSnapshot('changed');
      await page.reload(); await page.locator('#selected-target').getByText(/No console target/).waitFor();
      assert.equal(await page.locator('[data-title-key][aria-pressed="true"]').count(), 0);
      assert.deepEqual(api.outbound, []); assert.deepEqual(api.errors, []);
    });

    await test('empty and partial snapshots do not manufacture a selectable console target', async () => {
      for (const inventory of [
        { ...titleSnapshot(), items: [] },
        { ...titleSnapshot(), status: 'partial', complete: false, snapshot_id: null, reason: 'process_capture_incomplete' },
      ]) {
        const { page } = await fixture({ inventory });
        await page.locator('#title-status').waitFor();
        assert.match(await page.locator('#title-status').innerText(), inventory.complete ? /No running titles/i : /partial|incomplete/i);
        assert.equal(await page.locator('[data-title-key]:not(:disabled)').count(), 0);
        assert.match(await page.locator('#selected-target').innerText(), /No console target/);
      }
    });

    await test('stale target 409 refreshes and clears without silently resubmitting or retargeting', async () => {
      const { page, api } = await fixture({ inventory: titleSnapshot(), staleOnce: true });
      await page.locator('[data-title-key="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"]').click();
      await lookup(page);
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.locator('#status-banner').getByText(/changed|stale/i).waitFor();
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2"]').waitFor();
      assert.match(await page.locator('#selected-target').innerText(), /No console target/);
      assert.equal(api.posts.length, 1);
      assert.equal(api.refreshes.length, 1);
      assert.deepEqual(api.refreshes[0].body, {});
      assert.equal(api.refreshes[0].headers['x-csrf-token'], 'owned-fixture-csrf');
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2"]').click();
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.getByText('Analysis request queued.').waitFor();
      assert.equal(api.posts.length, 2);
      assert.notEqual(api.posts[1].body.idempotency_key, api.posts[0].body.idempotency_key);
      assert.deepEqual(api.posts[1].body.target_ref, { snapshot_id: '22222222222222222222222222222222', title_key: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2' });
    });

    await test('controller reaches lookup and target controls while keyboard editing and modal focus survive', async () => {
      const { page, api } = await fixture({ inventory: titleSnapshot() });
      const domain = page.getByLabel('Nexus domain', { exact: true });
      await domain.fill('quiet29'); await page.keyboard.press('Backspace');
      assert.equal(await domain.inputValue(), 'quiet2');
      await page.keyboard.press('ArrowLeft'); await page.keyboard.press('x');
      assert.equal(await domain.inputValue(), 'quietx2');
      assert.equal(await domain.evaluate(element => document.activeElement === element), true);
      await page.getByRole('tab', { name: 'Games', exact: true }).focus();
      const reached = new Set();
      for (let n = 0; n < 30; n++) {
        await page.evaluate(() => {
          if (!window.fixturePad) {
            window.fixturePad = { mapping: 'standard', buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0] };
            Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.fixturePad] });
          }
          window.fixturePad.buttons[13].pressed = true;
        });
        await page.waitForTimeout(235);
        reached.add(await page.evaluate(() => document.activeElement.id || (document.activeElement.dataset.titleKey ? 'title' : '')));
        await page.evaluate(() => { window.fixturePad.buttons[13].pressed = false; });
        if (reached.has('lookup-url') && reached.has('lookup-domain') && reached.has('lookup-mod-id') && reached.has('title')) break;
      }
      assert(reached.has('lookup-url')); assert(reached.has('lookup-domain')); assert(reached.has('lookup-mod-id')); assert(reached.has('title'));
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"]').focus();
      await page.evaluate(() => { window.fixturePad.buttons[0].pressed = true; });
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"][aria-pressed="true"]').waitFor();
      assert.equal(await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"]').evaluate(element => document.activeElement === element), true);
      await page.evaluate(() => { window.fixturePad.buttons[0].pressed = false; });
      assert.equal(api.posts.length, 0);
      await page.getByRole('button', { name: /Other catalog domain/ }).focus();
      await page.keyboard.press('F6');
      assert.equal(await page.locator('#details-dialog').evaluate(dialog => dialog.open), true);
      await page.getByRole('button', { name: 'Back', exact: true }).click();
      assert.match(await page.evaluate(() => document.activeElement.innerText), /Other catalog domain/);
      assert.deepEqual(api.errors, []);
    });

    await test('analysis report shows scoped blockers without claiming format, runtime or installation support', async () => {
      const jobs = [
        { id: 'b'.repeat(32), game: 'quiet2', mod_id: 31, file_id: 902, label: 'Owned analysis', status: 'analyzed', analysis_report: { schema_version: 1, operation: 'archive_inventory', execution_context: 'desktop_preparation', operation_state: 'supported', compatibility_state: 'unverified', reason_codes: ['missing_backend', 'platform_acceptance_missing', 'unknown_target'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } } },
        { id: 'c'.repeat(32), game: 'other_world-7', mod_id: 9, file_id: 902, label: 'Unsupported runtime fixture', status: 'analyzed', analysis_report: { schema_version: 1, operation: 'archive_inventory', execution_context: 'desktop_preparation', operation_state: 'supported', compatibility_state: 'unsupported', reason_codes: ['platform_acceptance_missing', 'windows_runtime_unavailable'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } } },
      ];
      const { page } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const first = page.locator('[data-job-id="' + 'b'.repeat(32) + '"]');
      assert.match(await first.innerText(), /Compatibility unverified/i);
      assert.match(await first.innerText(), /build.*unknown|target.*unknown/i);
      assert.match(await first.innerText(), /backend.*unavailable|missing.*backend/i);
      assert.match(await first.innerText(), /read.only.*inventory|archive inventory/i);
      const second = page.locator('[data-job-id="' + 'c'.repeat(32) + '"]');
      assert.match(await second.innerText(), /Compatibility unsupported/i);
      assert.match(await second.innerText(), /Windows runtime/i);
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      assert.doesNotMatch(await page.locator('body').innerText(), /Compatibility supported|PS5 compatible|Ready to install/i);
    });

    await test('explicit refresh is protected and cached GET never silently captures a new target', async () => {
      const { page, api } = await fixture({ inventory: titleSnapshot() });
      await page.locator('[data-title-key="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"]').click();
      assert.equal(api.refreshes.length, 0);
      api.inventory = titleSnapshot('changed');
      await page.getByRole('button', { name: 'Refresh targets', exact: true }).click();
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2"]').waitFor();
      assert.equal(api.refreshes.length, 1);
      assert.deepEqual(api.refreshes[0].body, {});
      assert.equal(api.refreshes[0].headers['x-csrf-token'], 'owned-fixture-csrf');
      assert.match(await page.locator('#selected-target').innerText(), /No console target/);
      assert.equal(api.posts.length, 0);
    });

    await test('native console preview honors disabled service flags without querying unfinished endpoints', async () => {
      const { page, api } = await fixture({ preview: true });
      await page.locator('#title-status').getByText(/unavailable/i).waitFor();
      assert.equal(await page.locator('.mode-label').innerText(), 'CONSOLE PREVIEW');
      assert.equal(await page.locator('.brand').getAttribute('href'), '/app');
      assert.equal(await page.getByRole('button', { name: 'Find mod', exact: true }).isDisabled(), true);
      assert.equal(await page.getByRole('button', { name: 'Sign in', exact: true }).count(), 0);
      assert.deepEqual(api.gets.sort(), ['/api/capabilities', '/api/detected-titles']);
      assert.deepEqual(api.posts, []); assert.deepEqual(api.outbound, []); assert.deepEqual(api.errors, []);
    });

    await test('retry identity retains the same opaque target and changes when the selected target changes', async () => {
      const { page, api } = await fixture({ inventory: titleSnapshot(), failRequests: true });
      await page.locator('[data-title-key="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"]').click();
      await lookup(page);
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.locator('#status-banner').getByText(/Retry/).waitFor();
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.waitForFunction(() => document.querySelector('[data-file-action]')?.disabled === false);
      assert.equal(api.posts.length, 2);
      assert.equal(api.posts[0].body.idempotency_key, api.posts[1].body.idempotency_key);
      assert.deepEqual(api.posts[0].body.target_ref, api.posts[1].body.target_ref);
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"]').click();
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.waitForFunction(() => document.querySelector('[data-file-action]')?.disabled === false);
      assert.equal(api.posts.length, 3);
      assert.notEqual(api.posts[1].body.idempotency_key, api.posts[2].body.idempotency_key);
      assert.equal(api.posts[2].body.target_ref.title_key, 'b'.repeat(31) + '1');
    });

    await test('duplicate title keys and an unrecognized analysis report publish no positive facts or provider text', async () => {
      const inventory = titleSnapshot(); inventory.items[1].title_key = inventory.items[0].title_key;
      const jobs = [{ id: 'd'.repeat(32), label: 'Invalid analysis fixture', status: 'analyzed', analysis_report: { schema_version: 1, operation: 'archive_inventory', execution_context: 'desktop_preparation', operation_state: 'supported', compatibility_state: 'supported', reason_codes: ['fixture-secret-provider-value'], install_authority: { state: 'supported', reason: 'install_ready', scope: ['all_games'] } } }];
      const { page } = await fixture({ inventory, jobs });
      await page.locator('#title-status').getByText(/could not be verified/i).waitFor();
      assert.equal(await page.locator('[data-title-key]').count(), 0);
      await page.getByRole('tab', { name: 'Activity' }).click();
      assert.match(await page.locator('#activity-content').innerText(), /Compatibility unverified/i);
      assert.doesNotMatch(await page.locator('body').innerText(), /fixture-secret-provider-value|Compatibility supported|all_games|install_ready/);
    });

    console.log(`generic selection UI: ${passed} passed, ${failed} failed`);
    if (failed) process.exitCode = 1;
  } finally {
    for (const context of contexts) await context.close();
    await browser.close(); await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
