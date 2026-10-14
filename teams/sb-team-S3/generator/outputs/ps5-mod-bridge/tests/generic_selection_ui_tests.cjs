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
      // The D-pad moves by geometry: Down walks a column, Left and Right cross to the next one. Every control is reachable.
      const reached = new Set();
      const pad = async index => {
        await page.evaluate(i => {
          if (!window.fixturePad) {
            window.fixturePad = { mapping: 'standard', buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0] };
            Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.fixturePad] });
          }
          window.fixturePad.buttons[i].pressed = true;
        }, index);
        await page.waitForTimeout(80);
        await page.evaluate(i => { window.fixturePad.buttons[i].pressed = false; }, index);
        await page.waitForTimeout(40);
        const here = await page.evaluate(() => document.activeElement.id || (document.activeElement.dataset.titleKey ? 'title' : ''));
        reached.add(here);
        return here;
      };
      const UP = 12, DOWN = 13, LEFT = 14, RIGHT = 15;
      for (let n = 0; n < 12 && !reached.has('title'); n++) await pad(DOWN);
      assert(reached.has('reload-titles'), 'Down walks the console target column, past its refresh button');
      assert(reached.has('title'), 'and reaches the target list');
      await pad(DOWN);
      assert.equal(await pad(RIGHT), 'lookup-submit', 'Right crosses to the lookup column');
      assert.equal(await pad(UP), 'lookup-domain');
      assert.equal(await pad(RIGHT), 'lookup-mod-id', 'Right moves along the row');
      assert.equal(await pad(UP), 'lookup-url');
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"]').focus();
      await page.evaluate(() => { window.fixturePad.buttons[0].pressed = true; });
      await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"][aria-pressed="true"]').waitFor();
      assert.equal(await page.locator('[data-title-key="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1"]').evaluate(element => document.activeElement === element), true);
      await page.evaluate(() => { window.fixturePad.buttons[0].pressed = false; });
      assert.equal(api.posts.length, 0);
      await page.getByRole('button', { name: /Other catalog domain/ }).focus();
      await page.keyboard.press('F6');
      assert.equal(await page.locator('#details-dialog').evaluate(dialog => dialog.open), true);
      assert.equal(await page.getByRole('link', { name: 'Open on Nexus' }).getAttribute('href'), 'https://www.nexusmods.com/other_world-7');
      await page.getByRole('button', { name: 'Back', exact: true }).click();
      assert.match(await page.evaluate(() => document.activeElement.innerText), /Other catalog domain/);
      assert.deepEqual(api.errors, []);
    });

    await test('analysis report shows scoped blockers without claiming format, runtime or installation support', async () => {
      const jobs = [
        { id: 'b'.repeat(32), game: 'quiet2', mod_id: 31, file_id: 902, label: 'Owned analysis', status: 'analyzed', analysis_report: { schema_version: 1, operation: 'archive_inventory', execution_context: 'desktop_preparation', operation_state: 'supported', compatibility_state: 'unverified', reason_codes: ['missing_backend', 'platform_acceptance_missing', 'unknown_target'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } } },
        { id: 'c'.repeat(32), game: 'other_world-7', mod_id: 9, file_id: 902, label: 'Unsupported runtime fixture', status: 'analyzed', analysis_report: { schema_version: 1, operation: 'archive_inventory', execution_context: 'ps5_native', operation_state: 'supported', compatibility_state: 'unsupported', reason_codes: ['platform_acceptance_missing', 'windows_runtime_unavailable'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } } },
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
      assert.match(await second.innerText(), /PS5 application/);
      assert.doesNotMatch(await second.innerText(), /desktop preparation/i);
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      assert.doesNotMatch(await page.locator('body').innerText(), /Compatibility supported|PS5 compatible|Ready to install/i);
    });

    await test('compatibility badge renders every verdict, every reason code, missing lists and escapes host text', async () => {
      const emptyMissing = () => ({ assets: [], classes: [], functions: [], short_class_names: [] });
      const assessment = (verdict, extra = {}) => ({ verdict, reasons: [], console_content_version: null, pc_build: null, selected_file_id: null, checked: 0, present: 0, missing: emptyMissing(), ...extra });
      const allCodes = ['unknown_console_version', 'unknown_build', 'file_ambiguous', 'file_no_fit', 'file_older_build', 'file_newer_than_build', 'file_undated', 'missing_references', 'unverified_references', 'dynamic_references', 'no_index', 'index_for_other_build', 'assessment_failed'];
      const injection = '<img src=x onerror="window.__compatXss=1">&<script>window.__compatXss=2</script>';
      const id = ch => ch.repeat(32);
      const job = (ch, compat, label = `Compat fixture ${ch}`) => { const record = { id: id(ch), label, status: 'analyzed' }; if (compat !== undefined) record.compatibility_assessment = compat; return record; };
      const jobs = [
        job('1', assessment('compatible', { console_content_version: '1.002.003', pc_build: '1.4.2', selected_file_id: 902, checked: 10, present: 10 })),
        job('2', assessment('partially_compatible', { reasons: [{ code: 'missing_references', detail: 'Two classes absent.' }], checked: 12, present: 10, missing: { assets: ['/Game/Art/A'], classes: ['BP_Missing_C'], functions: [], short_class_names: [] } })),
        job('3', assessment('incompatible', { reasons: [{ code: 'file_no_fit', detail: '' }], console_content_version: '9.9' })),
        job('4', assessment('unknown', { reasons: [{ code: 'unknown_console_version', detail: '' }] })),
        job('5', assessment('partially_compatible', { reasons: allCodes.map(code => ({ code, detail: `detail-for-${code}` })) })),
        job('6', assessment('incompatible', { reasons: [{ code: 'dynamic_references', detail: injection }], pc_build: injection, console_content_version: '<b>1</b>', checked: 1, present: 0, missing: { assets: [injection], classes: [injection], functions: [injection], short_class_names: [injection] } })),
        job('7', { ...assessment('compatible'), surprise: true }, 'Malformed unknown key'),
        job('8', assessment('compatible', { reasons: [{ code: 'not_a_real_code', detail: 'x' }] }), 'Malformed bad code'),
        job('9', assessment('compatible', { reasons: [{ code: 'no_index', detail: 'x'.repeat(513) }] }), 'Malformed oversized detail'),
        job('a', assessment('compatible', { missing: { ...emptyMissing(), assets: Array.from({ length: 51 }, (_, n) => `a${n}`) } }), 'Malformed too many names'),
        job('b', assessment('mostly_compatible'), 'Malformed verdict'),
        job('c', undefined, 'Old job without field'),
      ];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = ch => page.locator(`[data-job-id="${id(ch)}"]`);
      await card('1').waitFor();
      // One badge per verdict, with a visible text label and the dedicated class.
      for (const [ch, cls, label] of [['1', 'compat-compatible', 'Compatible'], ['2', 'compat-partial', 'Partially compatible'], ['3', 'compat-incompatible', 'Incompatible'], ['4', 'compat-unknown', 'Unknown']]) {
        const badge = card(ch).locator('.compat-badge');
        assert.equal(await badge.count(), 1, `badge for ${label}`);
        assert.equal(await badge.evaluate((el, name) => el.classList.contains(name), cls), true);
        assert.match(await badge.innerText(), new RegExp(`${label}$`));
        assert.doesNotMatch(await card(ch).innerText(), /Compatibility unverified/);
        // Every verdict except Unknown carries the static-only qualifier, and Unknown has no positive claim to qualify.
        assert.equal(await card(ch).locator('.compat-basis').count(), label === 'Unknown' ? 0 : 1, `basis note for ${label}`);
        if (label !== 'Unknown') assert.equal(await card(ch).locator('.compat-basis').innerText(), 'Static check. Not tested on PS5.', `qualifier wording for ${label}`);
        else assert.doesNotMatch(await card(ch).innerText(), /Static check|Not tested on PS5/);
        assert.equal(await card(ch).locator('.compat-limited, .compat-limit').count(), 0, `${label} without a hard-block report is not limited`);
      }
      assert.doesNotMatch(await card('1').innerText(), /not yet tested in game/);
      assert.match(await card('1').innerText(), /Game version 1\.002\.003 · PC patch 1\.4\.2/);
      assert.match(await card('1').innerText(), /10 of 10 referenced items found/);
      assert.match(await card('3').innerText(), /Game version 9\.9/);
      assert.doesNotMatch(await card('3').innerText(), /PC patch/);
      assert.equal(await card('1').locator('details.compat-missing').count(), 0);
      assert.equal(await card('1').locator('.compat-reasons').count(), 0);
      // Missing lists are collapsed and keep the contract counts.
      const details = card('2').locator('details.compat-missing');
      assert.equal(await details.evaluate(el => el.open), false);
      assert.match(await card('2').locator('summary').innerText(), /^Missing from this game version \(2\)$/);
      assert.match(await card('2').innerText(), /Two classes absent\./);
      assert.equal(await card('2').locator('.compat-missing li').first().isVisible(), false);
      await card('2').locator('summary').click();
      assert.equal(await card('2').locator('.compat-missing li').first().isVisible(), true);
      assert.match(await details.innerText(), /BP_Missing_C/);
      // Every reason code has its own plain-English sentence; the host's detail follows as a separate, secondary clause in parentheses.
      const every = await card('5').locator('.compat-reasons li').allInnerTexts();
      assert.equal(every.length, allCodes.length);
      const sentences = [];
      for (const [index, code] of allCodes.entries()) {
        assert.match(every[index], new RegExp(`\\n\\(detail-for-${code}\\)$`));
        const sentence = every[index].replace(new RegExp(`\\s*\\(detail-for-${code}\\)$`), '');
        assert.doesNotMatch(sentence, /_/, 'raw code is not user text');
        assert.match(sentence, /^[A-Z][^]+\.$/);
        sentences.push(sentence);
      }
      assert.equal(new Set(sentences).size, allCodes.length);
      const clause = card('5').locator('.compat-reasons li').first();
      assert.equal(await clause.locator('.compat-reason-text').innerText(), sentences[0]);
      assert.equal(await clause.locator('.compat-reason-detail').evaluate(el => getComputedStyle(el).display), 'block', 'detail sits on its own line');
      assert.notEqual(await clause.locator('.compat-reason-detail').evaluate(el => getComputedStyle(el).color), await clause.locator('.compat-reason-text').evaluate(el => getComputedStyle(el).color), 'detail is visually secondary');
      assert.equal(await card('2').locator('.compat-reason-detail').innerText(), '(Two classes absent.)', 'the host detail is shown as written, with no period added or removed');
      // Host-provided text is escaped everywhere it appears.
      await card('6').locator('summary').click();
      assert.equal(await card('6').locator('img, script').count(), 0);
      assert.equal(await page.evaluate(() => window.__compatXss), undefined);
      assert.match(await card('6').locator('.compat-reasons').innerText(), /<img src=x onerror="window\.__compatXss=1">&<script>/);
      assert.match(await card('6').locator('summary').innerText(), /\(4\)$/);
      assert.equal(await card('6').locator('.compat-missing li').count(), 4);
      assert.match(await card('6').innerText(), /<b>1<\/b>/);
      // Malformed objects fall back to the unverified text and render no badge or host strings.
      for (const [ch, label] of [['7', 'unknown key'], ['8', 'bad code'], ['9', 'oversized detail'], ['a', 'too many names'], ['b', 'bad verdict']]) {
        assert.equal(await card(ch).locator('.compat-badge, .compat-detail').count(), 0, label);
        assert.match(await card(ch).innerText(), /Compatibility unverified/, label);
      }
      assert.doesNotMatch(await card('9').innerText(), /xxxx/);
      // Absent field keeps the old behavior exactly.
      assert.equal(await card('c').locator('.compat-badge, .compat-detail').count(), 0);
      assert.match(await card('c').innerText(), /Compatibility unverified · unresolved/);
      assert.deepEqual(api.errors, []);
      assert.deepEqual(api.outbound, []);
    });

    await test('a reason detail reads as a separate, secondary clause after the catalog sentence', async () => {
      const emptyMissing = () => ({ assets: [], classes: [], functions: [], short_class_names: [] });
      const assessment = reasons => ({ verdict: 'unknown', reasons, console_content_version: null, pc_build: null, selected_file_id: null, checked: 0, present: 0, missing: emptyMissing() });
      const injection = '<img src=x onerror="window.__detailXss=1">';
      const reasons = [
        { code: 'unknown_console_version', detail: 'no console title was selected for this job' },
        { code: 'unknown_build', detail: 'pc_build for console content version 1.0 is not known' },
        { code: 'unverified_references', detail: '4 references cannot be verified against an incomplete index' },
        { code: 'file_older_build', detail: 'Already a sentence.' },
        { code: 'no_index', detail: '   ' },
        { code: 'dynamic_references', detail: '' },
        { code: 'missing_references', detail: injection },
      ];
      const { page, api } = await fixture({ jobs: [{ id: 'd'.repeat(32), label: 'Detail fixture', status: 'analyzed', compatibility_assessment: assessment(reasons) }] });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const items = page.locator('.compat-reasons li');
      await items.first().waitFor();
      assert.equal(await items.count(), reasons.length);
      const sentence = index => items.nth(index).locator('.compat-reason-text');
      const detail = index => items.nth(index).locator('.compat-reason-detail');
      // The catalog sentence stands alone; the host's own words follow it, unchanged, in parentheses on their own line.
      assert.equal(await sentence(0).innerText(), 'The console game version could not be determined.');
      assert.equal(await detail(0).textContent(), '(no console title was selected for this job)');
      assert.equal(await items.nth(0).innerText(), 'The console game version could not be determined.\n(no console title was selected for this job)');
      assert.equal((await items.nth(0).textContent()).replace(/\s+/g, ' '), 'The console game version could not be determined. (no console title was selected for this job)');
      assert.doesNotMatch(await items.nth(0).innerText(), /determined\.\s*no console/, 'no longer one run-on sentence');
      assert.equal(await detail(1).textContent(), '(pc_build for console content version 1.0 is not known)');
      assert.equal(await detail(2).textContent(), '(4 references cannot be verified against an incomplete index)');
      assert.equal(await detail(3).textContent(), '(Already a sentence.)', 'detail text is shown as written');
      // Empty or blank detail leaves the sentence alone.
      for (const index of [4, 5]) assert.equal(await detail(index).count(), 0);
      // The clause is a quieter block of its own.
      assert.equal(await detail(0).evaluate(el => getComputedStyle(el).display), 'block');
      assert.ok(await detail(0).evaluate(el => parseFloat(getComputedStyle(el).fontSize) < parseFloat(getComputedStyle(el.previousElementSibling).fontSize)), 'smaller than the sentence');
      assert.notEqual(await detail(0).evaluate(el => getComputedStyle(el).color), await sentence(0).evaluate(el => getComputedStyle(el).color));
      // Host text stays escaped, and a clause is plain text with no focus stop.
      assert.equal(await detail(6).textContent(), `(${injection})`);
      assert.equal(await page.locator('.compat-reasons img').count(), 0);
      assert.equal(await page.evaluate(() => window.__detailXss), undefined);
      assert.equal(await page.locator('.compat-reasons').locator('a, button, input, summary, [tabindex]').count(), 0);
      assert.deepEqual(api.errors, []);
    });

    await test('a hard-block analysis outranks a positive verdict; unmeasured acceptance is never hidden by a verdict', async () => {
      const emptyMissing = () => ({ assets: [], classes: [], functions: [], short_class_names: [] });
      const assessment = (verdict, extra = {}) => ({ verdict, reasons: [], console_content_version: '1.0', pc_build: '2.0', selected_file_id: 902, checked: 4, present: 4, missing: emptyMissing(), ...extra });
      const report = (state, codes, context = 'ps5_native') => ({ schema_version: 1, operation: 'archive_inventory', execution_context: context, operation_state: 'supported', compatibility_state: state, reason_codes: codes, install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } });
      const windows = report('unsupported', ['platform_acceptance_missing', 'windows_runtime_unavailable']);
      const managed = report('unsupported', ['managed_runtime_unavailable', 'platform_acceptance_missing']);
      const unverified = report('unverified', ['platform_acceptance_missing', 'unknown_target'], 'desktop_preparation');
      const id = ch => ch.repeat(32);
      const job = (ch, compat, analysis) => { const item = { id: id(ch), label: `Precedence fixture ${ch}`, status: 'analyzed' }; if (compat) item.compatibility_assessment = compat; if (analysis) item.analysis_report = analysis; return item; };
      const jobs = [
        job('1', assessment('compatible'), windows),
        job('2', assessment('partially_compatible', { checked: 6, present: 4 }), managed),
        job('3', assessment('incompatible', { checked: 4, present: 0 }), windows),
        job('4', assessment('unknown', { checked: 0, present: 0 }), windows),
        job('5', assessment('compatible'), unverified),
        job('6', assessment('compatible')),
        job('7', assessment('compatible'), { ...windows, reason_codes: ['platform_acceptance_missing'] }),
        job('8', assessment('unknown', { checked: 0, present: 0 })),
      ];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = ch => page.locator(`[data-job-id="${id(ch)}"]`);
      await card('1').waitFor();
      const acceptance = 'PS5 asset and runtime acceptance has not been measured.';
      const qualifier = 'Static check. Not tested on PS5.';
      // Positive verdict plus a hard block (Windows or managed runtime): the badge reads as limited and is never green or plainly "Compatible".
      for (const ch of ['1', '2']) {
        const box = card(ch);
        const badge = box.locator('.compat-badge');
        assert.equal(await badge.count(), 1, `one badge for ${ch}`);
        assert.equal(await badge.evaluate(el => el.classList.contains('compat-limited')), true);
        assert.equal(await badge.getAttribute('data-compat-limited'), 'true');
        assert.equal(await badge.locator('.compat-glyph').getAttribute('aria-hidden'), 'true');
        assert.match(await badge.innerText(), /Limited by runtime$/);
        assert.equal(await box.locator('.compat-compatible, .compat-partial').count(), 0, 'no positive colour class beside the hard block');
        const text = await box.innerText();
        assert.doesNotMatch(text, /✓|\bCompatible\b|Partially compatible/);
        assert.match(text, /Compatibility unsupported/);
        assert.match(text, ch === '1' ? /Required Windows runtime is unavailable\./ : /Required managed runtime is unavailable\./);
        assert.ok(text.includes(acceptance), 'acceptance line stays');
        assert.ok(text.includes(qualifier), 'qualifier stays');
        assert.match(text, /The runtime this mod needs is not available on PS5, so this content check does not show it can be used there\./);
        assert.match(text, /\d of \d referenced items found/, 'the content facts are still shown');
        assert.equal(await box.locator('.compat-limit').locator('a, button, input, summary, [tabindex]').count(), 0, 'new text takes no focus');
      }
      // Incompatible and Unknown say nothing positive, so a hard block does not change their badge.
      assert.equal(await card('3').locator('.compat-badge').evaluate(el => el.classList.contains('compat-incompatible') && !el.hasAttribute('data-compat-limited')), true);
      assert.equal(await card('3').locator('.compat-basis').innerText(), qualifier);
      assert.equal(await card('4').locator('.compat-badge').evaluate(el => el.classList.contains('compat-unknown') && !el.hasAttribute('data-compat-limited')), true);
      assert.equal(await card('4').locator('.compat-basis').count(), 0);
      for (const ch of ['3', '4']) {
        assert.match(await card(ch).innerText(), /Compatibility unsupported/);
        assert.equal(await card(ch).locator('.compat-limit').count(), 0);
      }
      // A positive verdict with an ordinary unverified report: badge as before, qualifier, and the report's own acceptance line.
      assert.equal(await card('5').locator('.compat-badge').evaluate(el => el.classList.contains('compat-compatible')), true);
      assert.equal(await card('5').locator('.compat-basis').innerText(), qualifier);
      assert.ok((await card('5').innerText()).includes(acceptance));
      assert.equal(await card('5').locator('.compat-acceptance').count(), 0, 'no duplicate when the report lists it');
      assert.doesNotMatch(await card('5').innerText(), /Compatibility unverified|Target compatibility remains unresolved/);
      // Without a usable report the card states the unmeasured acceptance itself, whatever the verdict, and drops the contradicting "unresolved" fallback.
      for (const ch of ['6', '8']) {
        assert.equal(await card(ch).locator('.compat-acceptance').innerText(), acceptance);
        assert.doesNotMatch(await card(ch).innerText(), /Target compatibility remains unresolved|Compatibility unverified/);
        assert.match(await card(ch).innerText(), /Archive analyzed; nothing was installed\./);
        assert.equal(await card(ch).locator('.compat-acceptance').locator('a, button, input, summary, [tabindex]').count(), 0);
      }
      assert.equal(await card('6').locator('.compat-badge').evaluate(el => el.classList.contains('compat-compatible')), true);
      // An inconsistent report (claims unsupported without a runtime reason) is dropped whole, like any malformed record.
      assert.equal(await card('7').locator('.compat-acceptance').count(), 1);
      assert.equal(await card('7').locator('.compat-limited').count(), 0);
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      assert.doesNotMatch(await page.locator('body').innerText(), /Compatibility supported|PS5 compatible|Ready to install|can be installed/i);
      assert.deepEqual(api.errors, []);
      assert.deepEqual(api.outbound, []);
    });

    await test('the Lua runtime limits a positive verdict when blocked or not wired to the console, alone or with a hard-block report', async () => {
      const emptyMissing = () => ({ assets: [], classes: [], functions: [], short_class_names: [] });
      const assessment = (verdict, extra = {}) => ({ verdict, reasons: [], console_content_version: '1.0', pc_build: '2.0', selected_file_id: 902, checked: 4, present: 4, missing: emptyMissing(), ...extra });
      const hardBlock = { schema_version: 1, operation: 'archive_inventory', execution_context: 'ps5_native', operation_state: 'supported', compatibility_state: 'unsupported', reason_codes: ['platform_acceptance_missing', 'windows_runtime_unavailable'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } };
      const runtime = (status, extra = {}) => ({ status, reason: null, runtime: 'ps5-ue4ss-portable', console_wired: false, missing: [], partial: [], missing_modules: [], partial_modules: [], dynamic_require: false, ...extra });
      const blocked = (extra = {}) => runtime('blocked', { missing: ['ExecuteInGameThread'], ...extra });
      const id = ch => ch.repeat(32);
      const job = (ch, compat, runtimeRecord, report) => { const item = { id: id(ch), label: `Lua fixture ${ch}`, status: 'analyzed', compatibility_assessment: compat, runtime_coverage: runtimeRecord }; if (report) item.analysis_report = report; return item; };
      const jobs = [
        job('1', assessment('compatible'), blocked()),                                                  // blocked and not wired
        job('2', assessment('partially_compatible', { checked: 6, present: 4 }), runtime('covered')),    // covered, but not wired to the console
        job('3', assessment('compatible'), blocked({ console_wired: true })),                            // blocked, wired
        job('4', assessment('compatible'), blocked(), hardBlock),                                        // both causes
        job('5', assessment('compatible'), runtime('covered', { console_wired: true })),                 // covered and wired: left alone
        job('6', assessment('partially_compatible', { checked: 6, present: 4 }), runtime('partial', { console_wired: true, partial: ['ExecuteAsync'] })),
        job('7', assessment('incompatible', { checked: 4, present: 0 }), blocked()),
        job('8', assessment('unknown', { checked: 0, present: 0 }), blocked()),
        job('9', assessment('compatible'), runtime('unknown', { reason: 'usage_unavailable', runtime: null, console_wired: null })),
        job('a', assessment('compatible'), { ...blocked(), surprise: true }),                            // malformed: dropped whole, so it limits nothing
        job('b', assessment('compatible'), runtime('covered', { console_wired: true }), hardBlock),      // the report alone limits
      ];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = ch => page.locator(`[data-job-id="${id(ch)}"]`);
      await card('1').waitFor();
      const luaSentence = 'The PS5 Lua runtime cannot run this mod today, so this content check does not show it can be used there.';
      const reportSentence = 'The runtime this mod needs is not available on PS5, so this content check does not show it can be used there.';
      const causes = ch => card(ch).locator('.compat-limit').evaluateAll(els => els.map(el => el.dataset.limitCause));
      const limited = async (ch, expected) => {
        const box = card(ch), badge = box.locator('.compat-badge');
        assert.equal(await badge.count(), 1, `one badge for ${ch}, however many causes`);
        assert.equal(await badge.evaluate(el => el.classList.contains('compat-limited') && el.dataset.compatLimited === 'true'), true, ch);
        assert.match(await badge.innerText(), /Limited by runtime$/);
        assert.equal(await badge.locator('.compat-glyph').getAttribute('aria-hidden'), 'true');
        assert.equal(await box.locator('.compat-compatible, .compat-partial').count(), 0, `no positive colour class for ${ch}`);
        assert.doesNotMatch(await box.locator('.compat-badge, .compat-detail').allInnerTexts().then(parts => parts.join('\n')), /✓|◐|\bCompatible\b|Partially compatible/, ch);
        assert.deepEqual(await causes(ch), expected, `causes for ${ch}`);
        const text = await box.innerText();
        assert.equal(text.includes(luaSentence), expected.includes('lua'), `lua sentence for ${ch}`);
        assert.equal(text.includes(reportSentence), expected.includes('report'), `report sentence for ${ch}`);
        assert.match(text, /Static check\. Not tested on PS5\./);
        assert.match(text, /\d of \d referenced items found/, 'the content facts stay');
        assert.ok(text.includes('PS5 asset and runtime acceptance has not been measured.') || await box.locator('.compat-acceptance').count() === 1, 'the acceptance line stays');
        assert.equal(await box.locator('.compat-limit').locator('a, button, input, summary, [tabindex]').count(), 0, 'new text takes no focus');
        assert.equal(await box.locator('.runtime-coverage').count(), 1, 'the runtime section stays beside the badge');
      };
      await limited('1', ['lua']);
      await limited('2', ['lua']);
      await limited('3', ['lua']);
      await limited('4', ['report', 'lua']);   // both sentences, report first, one badge
      await limited('b', ['report']);
      // Covered or partial coverage wired to the console leaves the verdict, its colour and its words alone.
      for (const [ch, cls, label] of [['5', 'compat-compatible', 'Compatible'], ['6', 'compat-partial', 'Partially compatible']]) {
        const badge = card(ch).locator('.compat-badge');
        assert.equal(await badge.evaluate((el, expected) => el.classList.contains(expected) && !el.hasAttribute('data-compat-limited'), cls), true, ch);
        assert.match(await badge.innerText(), new RegExp(`${label}$`));
        assert.equal(await card(ch).locator('.compat-limit').count(), 0, ch);
      }
      // Incompatible and Unknown say nothing positive, so the runtime does not change their badge.
      assert.equal(await card('7').locator('.compat-badge').evaluate(el => el.classList.contains('compat-incompatible') && !el.hasAttribute('data-compat-limited')), true);
      assert.equal(await card('8').locator('.compat-badge').evaluate(el => el.classList.contains('compat-unknown') && !el.hasAttribute('data-compat-limited')), true);
      for (const ch of ['7', '8', '9', 'a']) assert.equal(await card(ch).locator('.compat-limit').count(), 0, `no limit for ${ch}`);
      // An unchecked runtime and a malformed record limit nothing.
      for (const ch of ['9', 'a']) assert.equal(await card(ch).locator('.compat-badge').evaluate(el => el.classList.contains('compat-compatible') && !el.hasAttribute('data-compat-limited')), true, ch);
      assert.equal(await card('a').locator('.runtime-coverage').count(), 0, 'a malformed record shows no section');
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      assert.doesNotMatch(await page.locator('body').innerText(), /Compatibility supported|PS5 compatible|Ready to install|can be installed/i);
      assert.deepEqual(api.errors, []);
      assert.deepEqual(api.outbound, []);
    });

    await test('compatibility badge and missing list stay reachable by arrow keys, gamepad and survive refresh', async () => {
      const compat = { verdict: 'partially_compatible', reasons: [{ code: 'missing_references', detail: 'Absent.' }], console_content_version: '1.0', pc_build: '2.0', selected_file_id: 902, checked: 3, present: 2, missing: { assets: ['/Game/Missing'], classes: [], functions: [], short_class_names: [] } };
      const jobs = [{ id: 'e'.repeat(32), label: 'Focus fixture', status: 'analyzed', compatibility_assessment: compat }];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const summary = page.locator('[data-job-id="' + 'e'.repeat(32) + '"] summary');
      await summary.waitFor();
      // Arrow-key traversal from the tab strip eventually lands on the summary and keeps going past it.
      await page.getByRole('tab', { name: 'Activity' }).focus();
      let reached = false;
      for (let step = 0; step < 12 && !reached; step++) {
        await page.keyboard.press('ArrowDown');
        reached = await summary.evaluate(el => el === document.activeElement);
      }
      assert.equal(reached, true, 'summary is part of arrow navigation');
      // Nothing sits below the summary, so Down stays on it (the page edge does not wrap); Up leaves it and Down comes back.
      const before = await page.evaluate(() => document.activeElement.outerHTML);
      await page.keyboard.press('ArrowUp');
      assert.notEqual(await page.evaluate(() => document.activeElement.outerHTML), before, 'focus is not trapped on the summary');
      await page.keyboard.press('ArrowDown');
      assert.equal(await summary.evaluate(el => el === document.activeElement), true);
      // Gamepad Cross activates the focused summary like a click.
      await page.evaluate(() => { const buttons = Array.from({ length: 17 }, () => ({ pressed: false })); buttons[0].pressed = true; const pad = { mapping: 'standard', buttons, axes: [0, 0] }; Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [pad] }); window.fixtureGamepad = pad; });
      await page.waitForFunction(() => document.querySelector('details.compat-missing').open);
      await page.evaluate(() => { window.fixtureGamepad.buttons[0].pressed = false; });
      // A job-list change re-renders the card; the open state and the focused summary survive it.
      await page.evaluate(() => { window.__compatMarker = document.querySelector('details.compat-missing'); });
      api.jobs = [{ ...jobs[0], bytes_downloaded: 4096 }];
      await page.waitForFunction(() => document.querySelector('details.compat-missing') !== window.__compatMarker, null, { timeout: 8000 });
      assert.equal(await page.locator('details.compat-missing').evaluate(el => el.open), true);
      assert.equal(await summary.evaluate(el => el === document.activeElement), true);
      assert.deepEqual(api.errors, []);
    });

    await test('PS5 Lua runtime section renders every status, the console line and the static-check qualifier', async () => {
      const record = (status, extra = {}) => ({ status, reason: null, runtime: 'ps5-ue4ss-portable', console_wired: false, missing: [], partial: [], missing_modules: [], partial_modules: [], dynamic_require: false, ...extra });
      const unknown = reason => record('unknown', { reason, runtime: null, console_wired: null });
      const id = ch => ch.repeat(32);
      const job = (ch, runtime, extra = {}) => { const item = { id: id(ch), label: `Runtime fixture ${ch}`, status: 'analyzed', ...extra }; if (runtime !== undefined) item.runtime_coverage = runtime; return item; };
      const compat = { verdict: 'compatible', reasons: [], console_content_version: '1.0', pc_build: '2.0', selected_file_id: 902, checked: 3, present: 3, missing: { assets: [], classes: [], functions: [], short_class_names: [] } };
      const jobs = [
        job('1', record('covered')),
        job('2', record('covered', { console_wired: true })),
        job('3', record('partial', { partial: ['ExecuteAsync'], partial_modules: ['UEHelpers'], dynamic_require: true })),
        job('4', record('blocked', { missing: ['ExecuteInGameThread', 'FindFirstOf'], partial: ['ExecuteAsync'], missing_modules: ['mod/x.y'], partial_modules: ['UEHelpers'] })),
        job('5', unknown('usage_unavailable')),
        job('6', unknown('assessment_failed')),
        job('7', record('blocked', { missing: ['RegisterHook'] }), { compatibility_assessment: compat }),
        job('8', undefined),
      ];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = ch => page.locator(`[data-job-id="${id(ch)}"]`);
      const section = ch => card(ch).locator('.runtime-coverage');
      await card('1').waitFor();
      const consoleLine = 'The PS5 Lua runtime is not running on the console yet, so this mod cannot run there today.';
      const qualifier = "Checked by reading the mod's script, not by running it.";
      const reasonText = { usage_unavailable: "The mod's scripts could not be read for this check.", assessment_failed: 'The runtime check could not run.' };
      for (const [ch, status, label, glyph] of [['1', 'covered', 'Supported functions', '✓'], ['2', 'covered', 'Supported functions', '✓'], ['3', 'partial', 'Partly supported', '◐'], ['4', 'blocked', 'Missing functions', '✕'], ['5', 'unknown', 'Not checked', '?'], ['6', 'unknown', 'Not checked', '?'], ['7', 'blocked', 'Missing functions', '✕']]) {
        const box = section(ch);
        assert.equal(await box.count(), 1, `section for job ${ch}`);
        assert.equal(await box.getAttribute('data-runtime-status'), status);
        assert.equal(await box.getAttribute('role'), 'group');
        assert.equal(await box.getAttribute('aria-label'), 'PS5 Lua runtime');
        assert.match(await box.locator('.runtime-title').innerText(), /^PS5 Lua runtime$/);
        const badge = box.locator('.runtime-badge');
        assert.equal(await badge.count(), 1);
        assert.equal(await badge.evaluate((el, name) => el.classList.contains(`runtime-${name}`), status), true);
        assert.equal(await badge.locator('.runtime-glyph').innerText(), glyph, `glyph for job ${ch}`);
        assert.match(await badge.innerText(), new RegExp(`${label}$`), `label for job ${ch}`);
        assert.equal(await badge.locator('.runtime-glyph').getAttribute('aria-hidden'), 'true');
        // The static-check qualifier is always there; the section never says a mod works or can be installed.
        const text = await box.innerText();
        assert.ok(text.includes(qualifier), `qualifier for job ${ch}`);
        assert.doesNotMatch(text, /\b(works?|working|installable|install|compatible|ready|runs on ps5)\b/i, `no works claim for job ${ch}`);
        assert.equal(await box.locator('a, button, input, summary, [tabindex]').count(), 0, 'plain text takes no focus');
      }
      // The console line follows console_wired === false, whatever the status.
      for (const ch of ['1', '3', '4', '7']) assert.ok((await section(ch).innerText()).includes(consoleLine), `console line for job ${ch}`);
      assert.equal(await section('1').locator('.runtime-wired').count(), 1);
      assert.equal((await section('2').innerText()).includes(consoleLine), false, 'console_wired true has no such line');
      for (const ch of ['5', '6']) assert.equal((await section(ch).innerText()).includes(consoleLine), false, 'unchecked jobs say nothing about the console');
      // Names and reasons.
      assert.equal(await section('1').locator('li, .runtime-group').count(), 0);
      assert.deepEqual(await section('4').locator('.runtime-group-title').allInnerTexts(), ['Functions the runtime lacks', 'Functions it only partly supports', 'Modules the runtime lacks', 'Modules it only partly supports']);
      assert.deepEqual(await section('4').locator('.runtime-names li').allInnerTexts(), ['ExecuteInGameThread', 'FindFirstOf', 'ExecuteAsync', 'mod/x.y', 'UEHelpers']);
      assert.deepEqual(await section('3').locator('.runtime-names li').allInnerTexts(), ['ExecuteAsync', 'UEHelpers']);
      assert.match(await section('3').innerText(), /loaded by name while the mod runs/);
      assert.equal((await section('4').innerText()).includes('loaded by name'), false);
      assert.ok((await section('5').innerText()).includes(reasonText.usage_unavailable));
      assert.ok((await section('6').innerText()).includes(reasonText.assessment_failed));
      assert.equal(await section('5').locator('li').count(), 0);
      assert.equal((await section('5').innerText()).includes(reasonText.assessment_failed), false);
      // It sits next to the compatibility badge on the card, without replacing it.
      assert.equal(await card('7').locator('.compat-badge').count(), 1);
      assert.equal(await card('7').locator('.job-details .compat-detail + .runtime-coverage').count(), 1);
      assert.equal(await card('1').locator('.compat-badge').count(), 0);
      // An old job without the field shows no section at all.
      assert.equal(await section('8').count(), 0);
      assert.match(await card('8').innerText(), /Compatibility unverified · unresolved/);
      // The page offers no Install control and never claims support.
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      assert.doesNotMatch(await page.locator('.runtime-coverage').allInnerTexts().then(parts => parts.join('\n')), /Ready to install|PS5 compatible|will work|runs on PS5/i);
      assert.deepEqual(api.errors, []);
      assert.deepEqual(api.outbound, []);
    });

    await test('PS5 Lua runtime section escapes host text and rejects malformed records fail-closed', async () => {
      const record = (status, extra = {}) => ({ status, reason: null, runtime: 'ps5-ue4ss-portable', console_wired: false, missing: [], partial: [], missing_modules: [], partial_modules: [], dynamic_require: false, ...extra });
      const unknown = reason => record('unknown', { reason, runtime: null, console_wired: null });
      const injection = '<img src=x onerror="window.__runtimeXss=1">&<script>window.__runtimeXss=2</script>';
      const id = ch => ch.repeat(32);
      const job = (ch, runtime, label = `Runtime fixture ${ch}`) => { const item = { id: id(ch), label, status: 'analyzed' }; if (runtime !== undefined) item.runtime_coverage = runtime; return item; };
      const list = (n, make = i => `Name${i}`) => Array.from({ length: n }, (_, i) => make(i));
      const valid = [
        job('1', record('blocked', { missing: [injection], partial: [injection], missing_modules: [injection], partial_modules: [injection] })),
        job('2', record('blocked', { missing: list(64) })),
        job('3', record('blocked', { missing: ['N'.repeat(128)] })),
        job('4', record('partial', { partial: ['ExecuteInGameThread'], optional_missing: ['RegisterLoadMapPreHook'] })),
      ];
      const malformed = [
        ['unknown key', record('blocked', { missing: ['A'], surprise: true })],
        ['missing key', (({ partial_modules, ...rest }) => rest)(record('blocked', { missing: ['A'] }))],
        ['bad status', record('works')],
        ['null status', record(null)],
        ['inherited status', record('constructor')],
        ['bad reason', record('unknown', { reason: 'secret_provider_text', runtime: null, console_wired: null })],
        ['reason on a checked record', record('blocked', { missing: ['A'], reason: 'assessment_failed' })],
        ['unknown without a reason', record('unknown', { runtime: null, console_wired: null })],
        ['unknown with data', { ...unknown('assessment_failed'), missing: ['A'] }],
        ['unknown with a runtime', { ...unknown('assessment_failed'), runtime: 'ps5-ue4ss-portable' }],
        ['unknown with console_wired', { ...unknown('assessment_failed'), console_wired: false }],
        ['covered beside a missing name', record('covered', { missing: ['A'] })],
        ['covered beside a partial name', record('covered', { partial: ['A'] })],
        ['covered with dynamic_require', record('covered', { dynamic_require: true })],
        ['blocked with nothing missing', record('blocked')],
        ['partial beside a missing name', record('partial', { missing_modules: ['m'] })],
        ['too many names', record('blocked', { missing: list(65) })],
        ['too many modules', record('blocked', { missing: ['A'], partial_modules: list(65) })],
        ['oversized name', record('blocked', { missing: ['N'.repeat(129)] })],
        ['empty name', record('blocked', { missing: [''] })],
        ['non-string name', record('blocked', { missing: [7] })],
        ['list not an array', record('blocked', { missing: 'ExecuteInGameThread' })],
        ['console_wired not boolean', record('blocked', { missing: ['A'], console_wired: 'no' })],
        ['null console_wired on a checked record', record('blocked', { missing: ['A'], console_wired: null })],
        ['dynamic_require not boolean', record('blocked', { missing: ['A'], dynamic_require: 1 })],
        ['no runtime id', record('blocked', { missing: ['A'], runtime: null })],
        ['oversized runtime id', record('blocked', { missing: ['A'], runtime: 'r'.repeat(65) })],
        ['empty optional list', record('partial', { partial: ['A'], optional_missing: [] })],
        ['optional beside covered', record('covered', { optional_missing: ['A'] })],
        ['optional on unknown', { ...unknown('assessment_failed'), optional_missing: ['A'] }],
        ['optional name also missing', record('blocked', { missing: ['A'], optional_missing: ['A'] })],
        ['optional not an array', record('partial', { partial: ['A'], optional_missing: 'A' })],
        ['too many optional names', record('partial', { optional_missing: list(65) })],
        ['array', [record('blocked', { missing: ['A'] })]],
        ['string', 'blocked'],
        ['null', null],
      ].map(([label, value], index) => ({ label, ch: String(index).padStart(2, '0'), value }));
      const jobs = [...valid, ...malformed.map(({ label, ch, value }) => ({ id: ('f' + ch).repeat(16), label: `Malformed ${label}`, status: 'analyzed', runtime_coverage: value }))];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      await page.locator(`[data-job-id="${id('1')}"]`).waitFor();
      const card = ch => page.locator(`[data-job-id="${id(ch)}"]`);
      // Hostile names are shown as text, never parsed.
      assert.equal(await card('1').locator('img, script').count(), 0);
      assert.equal(await page.evaluate(() => window.__runtimeXss), undefined);
      const names = await card('1').locator('.runtime-names li').allInnerTexts();
      assert.deepEqual(names, [injection, injection, injection, injection]);
      assert.equal(await page.locator('.runtime-coverage img, .runtime-coverage script').count(), 0);
      // The caps are exact.
      assert.equal(await card('2').locator('.runtime-names li').count(), 64);
      assert.equal((await card('3').locator('.runtime-names li').innerText()).length, 128);
      // Optional functions (called only through pcall) are their own group and do not read as blocking.
      const optionalText = await card('4').locator('.runtime-coverage').innerText();
      assert.match(optionalText, /Partly supported/);
      assert.match(optionalText, /Optional functions the runtime lacks \(the mod keeps running without them\)\s+RegisterLoadMapPreHook/);
      assert.doesNotMatch(optionalText, /Missing functions/);
      // Every malformed record is dropped whole: no section, no host strings, the old unverified text stays.
      for (const { label, ch } of malformed) {
        const box = page.locator(`[data-job-id="${('f' + ch).repeat(16)}"]`);
        assert.equal(await box.count(), 1, label);
        assert.equal(await box.locator('.runtime-coverage').count(), 0, label);
        assert.match(await box.innerText(), /Compatibility unverified/, label);
        assert.doesNotMatch(await box.innerText(), /secret_provider_text|Missing functions|Supported functions|Partly supported|Not checked/, label);
      }
      assert.equal(await page.locator('.runtime-coverage').count(), 4);
      assert.deepEqual(api.errors, []);
      assert.deepEqual(api.outbound, []);
    });

    await test('PS5 Lua runtime section leaves arrow-key focus alone and survives a refresh', async () => {
      const compat = { verdict: 'partially_compatible', reasons: [{ code: 'missing_references', detail: 'Absent.' }], console_content_version: '1.0', pc_build: '2.0', selected_file_id: 902, checked: 3, present: 2, missing: { assets: ['/Game/Missing'], classes: [], functions: [], short_class_names: [] } };
      const runtime = { status: 'blocked', reason: null, runtime: 'ps5-ue4ss-portable', console_wired: false, missing: ['ExecuteInGameThread'], partial: [], missing_modules: [], partial_modules: [], dynamic_require: false };
      const jobs = [{ id: 'e'.repeat(32), label: 'Focus fixture', status: 'analyzed', compatibility_assessment: compat, runtime_coverage: runtime }];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = page.locator('[data-job-id="' + 'e'.repeat(32) + '"]');
      const summary = card.locator('summary');
      await summary.waitFor();
      assert.equal(await card.locator('.runtime-coverage').count(), 1);
      // Arrow keys still reach the compatibility summary and move on past the card; the runtime text is skipped.
      await page.getByRole('tab', { name: 'Activity' }).focus();
      let reached = false;
      for (let step = 0; step < 12 && !reached; step++) {
        await page.keyboard.press('ArrowDown');
        reached = await summary.evaluate(el => el === document.activeElement);
      }
      assert.equal(reached, true, 'summary is part of arrow navigation');
      const before = await page.evaluate(() => document.activeElement.outerHTML);
      await page.keyboard.press('ArrowUp');
      assert.notEqual(await page.evaluate(() => document.activeElement.outerHTML), before, 'focus is not trapped on the summary');
      assert.equal(await page.evaluate(() => !!document.activeElement?.closest('.runtime-coverage')), false, 'focus never lands inside the runtime text');
      await page.keyboard.press('ArrowDown');
      assert.equal(await summary.evaluate(el => el === document.activeElement), true);
      // A job-list change re-renders the card: the focused summary survives and the section is drawn again.
      await page.evaluate(() => { window.__runtimeMarker = document.querySelector('.runtime-coverage'); });
      api.jobs = [{ ...jobs[0], bytes_downloaded: 4096 }];
      await page.waitForFunction(() => document.querySelector('.runtime-coverage') !== window.__runtimeMarker, null, { timeout: 8000 });
      assert.equal(await card.locator('.runtime-coverage').count(), 1);
      assert.match(await card.locator('.runtime-coverage').innerText(), /Missing functions/);
      assert.equal(await summary.evaluate(el => el === document.activeElement), true);
      assert.deepEqual(api.errors, []);
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
