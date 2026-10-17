const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

// Mod browsing: search, sort, details and conflict display. Production browser
// assets with owned localhost API fixtures; every nonlocal request is aborted.
const webRoot = path.join(__dirname, '..', 'web');
const browseKey = 'modbridge.browse.v1';
const unavailableInventory = { schema_version: 1, status: 'unavailable', complete: false, snapshot_id: null, source_scope: 'running_processes', items: [], reason: 'platform_provider_unavailable' };

// Title-inventory pages as the page's codec takes them, byte for byte: sorted keys, compact, ASCII only. A test asks for them with the
// fixture option `titles` ({ items, status, issues, fail }); `legacyTitles` is a v1 record for the host without paging.
const canonicalJson = value => JSON.stringify(value, (_key, entry) => entry && typeof entry === 'object' && !Array.isArray(entry) ?
  Object.fromEntries(Object.entries(entry).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)) : entry).replace(/[\u007f-\uffff]/g, ch => '\\u' + ch.charCodeAt(0).toString(16).padStart(4, '0'));
const SNAPSHOT_ID = 'a'.repeat(32);
const titlePage = ({ items = [], status = 'observed', issues = [], scope = 'console_title_metadata' } = {}) => status === 'observed'
  ? { schema_version: 2, kind: 'title_inventory_page', snapshot: { status, complete: true, snapshot_id: SNAPSHOT_ID, source_scope: scope, reason: null, issues },
      page: { index: 0, page_count: 1, item_offset: 0, total_count: items.length, next_cursor: null }, items }
  : { schema_version: 2, kind: 'title_inventory_page', snapshot: { status, complete: false, snapshot_id: null, source_scope: scope,
      reason: status === 'unavailable' ? 'platform_provider_unavailable' : status === 'partial' ? 'provider_observation_incomplete' : 'provider_error', issues }, page: null, items: [] };
const titlesReply = (api, url, route) => {
  const answer = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: canonicalJson(body) });
  if (api.titles.fail) return answer({ code: 'inventory_page_stale', error: 'The target inventory changed. Refresh the targets.' }, 409);
  if (url.pathname === '/api/detected-titles/pages' || url.pathname === '/api/detected-titles/pages/refresh') return answer(titlePage(api.titles));
  if (/^\/api\/detected-titles\/pages\/current\/[a-f0-9]{32}$/.test(url.pathname)) return answer({ schema_version: 2, kind: 'title_inventory_current', snapshot_id: SNAPSHOT_ID, current: true });
  return route.fulfill({ status: 404, contentType: 'application/json', body: '{}' });
};
const games = () => [
  { domain_name: 'zeta', id: 1, name: 'Zeta world', mods: 12 },
  { domain_name: 'alpha', id: 2, name: 'Alpha world', mods: 340 },
  { domain_name: 'cafe', id: 3, name: 'Café Noir', mods: 12 },
  { domain_name: 'plain', id: 4, name: 'Plain game' },
];
const longDescription = Array.from({ length: 120 }, (_, index) => `Description line ${index + 1}`).join('\n');
const zetaMods = () => [
  { mod_id: 30, game_domain: 'zeta', name: 'Zebra textures', author: 'Ada Lovelace', summary: 'Crisp stripes for every surface', version: '1.0', description: longDescription, source_url: 'https://www.nexusmods.com/zeta/mods/30' },
  { mod_id: 12, game_domain: 'zeta', name: 'Amber camera', author: 'Ben Ōkami', summary: 'Warm filmic grading', version: '2.0', description: 'Secretword lives only in a description.', source_url: 'javascript:alert(1)' },
  { mod_id: 45, game_domain: 'zeta', name: 'Café lighting', author: 'Ada Lovelace', summary: 'Soft lamps', version: '0.9', description: '' },
  { mod_id: 7, game_domain: 'zeta', name: 'Beyoncé tribute', author: 'Zoë Quinn', summary: 'Résumé of stage outfits', version: '1.1', description: 'Costumes.' },
  { mod_id: 21, game_domain: 'zeta', name: 'Hostile <img src=x onerror=window.__pwned=1>', author: 'Mallory & Co', summary: '<b>bold</b> summary', version: '1', description: '' },
];
const file = (id, name, extra = {}) => ({ file_id: id, name, file_name: `${name.toLowerCase().replace(/\W+/g, '-')}.zip`, version: '1.0', size_kb: 10, description: '', ...extra });
// API order is deliberately not the order of any sort below.
const zebraFiles = () => [
  file(102, 'Optional extras', { size_kb: 512, category_id: 3, category_name: 'OPTION', uploaded_timestamp: 1710000000, description: 'Extra lamps' }),
  file(104, 'Patch notes', { size_kb: 100, version: '0.5' }),
  file(101, 'Main file', { size_kb: 2048, is_primary: true, category_id: 1, category_name: 'MAIN', uploaded_timestamp: 1700000000, description: 'Primary archive' }),
  file(103, 'Old build', { size_kb: 3145728, category_id: 4, category_name: 'OLD_VERSION', uploaded_timestamp: 1600000000, is_primary: false }),
];
const compat = (verdict = 'partially_compatible') => ({ verdict, reasons: [{ code: 'dynamic_references', detail: '' }], console_content_version: '1.2.3', pc_build: '4.5', selected_file_id: 101, checked: 10, present: 8, missing: { assets: ['/Game/Missing/A'], classes: [], functions: [], short_class_names: [] } });
const conflicts = () => ({
  with: [{ job_id: 'b'.repeat(32), label: 'Amber camera · Main', paths: 3, winner: 'this' }, { job_id: 'c'.repeat(32), label: 'Other mod · file', paths: 1, winner: 'other' }],
  paths_sample: ['Game/Content/Textures/a.uasset', 'Game/Content/b.uasset'], harmless_duplicates: 2,
});
const runtimeCoverage = (extra = {}) => ({ status: 'blocked', reason: null, runtime: 'ps5-ue4ss-portable', console_wired: false, missing: ['ExecuteInGameThread'], partial: ['ExecuteAsync'], missing_modules: [], partial_modules: ['UEHelpers'], dynamic_require: false, ...extra });
const hardBlockReport = () => ({ schema_version: 1, operation: 'archive_inventory', execution_context: 'ps5_native', operation_state: 'supported', compatibility_state: 'unsupported', reason_codes: ['platform_acceptance_missing', 'windows_runtime_unavailable'], install_authority: { state: 'unsupported', reason: 'install_operation_unavailable', scope: [] } });
const job = (id, extra = {}) => ({ id, kind: 'download_analyze', game: 'zeta', mod_id: 30, file_id: 101, label: `Job ${id}`, status: 'analyzed', bytes_downloaded: 1, ...extra });

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
    const context = await browser.newContext({ viewport: extra.viewport || { width: 1920, height: 1080 }, locale: 'en-US', timezoneId: 'UTC' }); contexts.push(context);
    const api = { errors: [], outbound: [], requests: [], posts: [], games: games(), mods: { zeta: zetaMods() }, files: { 30: zebraFiles(), 12: [file(201, 'Only file')] }, jobs: [], ...extra };
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin !== origin) { api.outbound.push(route.request().url()); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, async route => {
      const req = route.request(), url = new URL(req.url());
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      api.requests.push(`${req.method()} ${url.pathname}`);
      // Tests add routes with `get(url)` (a reply { body, status } or undefined, may be async); `jobsError` makes the job list fail.
      if (req.method() === 'GET' && api.get) { const reply = await api.get(url, req); if (reply) return json(reply.body, reply.status ?? 200); }
      if (url.pathname === '/api/capabilities') return json({ mode: api.mode || 'desktop_preparation', csrf_token: 'browse-csrf', capabilities: { nexus_catalog: true, account_status: true, detected_titles: true, jobs: true, download_analyze: true, deploy: false, file_updates: true, ...(api.titles ? { title_inventory_paging: true } : {}), ...api.capabilities } });
      if (url.pathname === '/api/account') return json(api.account || { status: 'connected', live_service: 'verified', account: { display_name: 'Owned fixture' } });
      if (url.pathname.startsWith('/api/detected-titles')) return api.titles ? titlesReply(api, url, route) : json(api.legacyTitles || unavailableInventory);
      if (url.pathname === '/api/games') return json({ items: api.games });
      const mods = /^\/api\/games\/([^/]+)\/mods$/.exec(url.pathname);
      if (mods) return json({ items: api.mods[mods[1]] || [] });
      const files = /^\/api\/games\/[^/]+\/mods\/(\d+)\/files$/.exec(url.pathname);
      if (files) return json({ items: api.files[files[1]] || [] });
      if (url.pathname === '/api/jobs' && req.method() === 'GET') return api.jobsError ? json({ error: { code: api.jobsError, message: 'host text' } }, 500) : json({ items: api.jobs });
      if (req.method() === 'POST') {
        const post = { path: url.pathname, body: req.postDataJSON(), headers: req.headers() };
        api.posts.push(post);
        const reply = api.reply?.(post);
        if (reply) return json(reply.body, reply.status);
      }
      return json({ items: [] });
    });
    if (extra.init) await context.addInitScript(extra.init);
    const page = await context.newPage(); page.setDefaultTimeout(2500);
    page.on('pageerror', error => api.errors.push(error.message));
    if (extra.clock) await page.clock.install();
    await page.goto(origin);
    // A test of a signed-out page names what it waits for instead of the games list.
    if (extra.ready) await extra.ready(page); else await page.getByRole('button', { name: 'Zeta world' }).waitFor();
    return { page, api, context };
  }
  const fixtureWithSeed = seed => fixture({ init: `if (!sessionStorage.getItem('seeded')) { sessionStorage.setItem('seeded', '1'); sessionStorage.setItem('modbridge.browse.v1', ${JSON.stringify(seed)}); }` });
  const clean = async (page, api, outbound = []) => {
    assert.deepEqual(api.errors, []); assert.deepEqual(api.outbound, outbound);
    assert.equal(await page.evaluate(() => window.__pwned), undefined, 'hostile markup must never execute');
  };
  const cards = (page, kind) => page.$$eval(`[data-card^="${kind}:"]`, els => els.map(el => el.dataset.card.slice(el.dataset.card.indexOf(':') + 1)));
  const fileCards = page => page.$$eval('[data-file-card]', els => els.map(el => el.dataset.fileCard));
  const active = page => page.evaluate(() => { const el = document.activeElement; return el.id || el.dataset.card || el.dataset.fileDetails && `file-details:${el.dataset.fileDetails}` || el.getAttribute('aria-label') || el.textContent.trim().slice(0, 40); });
  const openZeta = async page => { await page.getByRole('button', { name: 'Zeta world' }).click(); await page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor(); };
  const openFiles = async (page, name = /Zebra textures/) => { await openZeta(page); await page.getByRole('button', { name }).click(); await page.locator('[data-file-card]').first().waitFor(); };
  const modFilter = page => page.getByLabel('Filter loaded latest mods');
  const gameFilter = page => page.getByLabel('Filter loaded games');
  const count = page => page.locator('#catalog-count').innerText();
  const press = async (page, index, hold = 50, gap = 260) => {
    await page.evaluate(i => {
      if (!window.fixturePad) {
        window.fixturePad = { mapping: 'standard', buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0] };
        Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.fixturePad] });
      }
      window.fixturePad.buttons[i].pressed = true;
    }, index);
    await page.waitForTimeout(hold);
    await page.evaluate(i => { window.fixturePad.buttons[i].pressed = false; }, index);
    await page.waitForTimeout(gap);
  };
  const CROSS = 0, CIRCLE = 1, SQUARE = 2, OPTIONS = 9, UP = 12, DOWN = 13, LEFT = 14, RIGHT = 15, L2 = 6, R2 = 7;
  async function test(name, run) {
    if (process.env.TEST_FILTER && !new RegExp(process.env.TEST_FILTER).test(name)) return;
    try { await run(); passed++; console.log(`PASS ${name}`); }
    catch (error) { failed++; console.error(`FAIL ${name}: ${error.stack || error.message}`); }
    finally { while (contexts.length) await contexts.pop().close(); }
  }

  try {
    await test('search ignores case and diacritics across name, author and summary', async () => {
      const { page, api } = await fixture();
      await gameFilter(page).fill('CAFE');
      assert.deepEqual(await cards(page, 'game'), ['cafe'], 'unaccented query finds Café');
      await gameFilter(page).fill('café noir');
      assert.deepEqual(await cards(page, 'game'), ['cafe']);
      await gameFilter(page).fill('café');
      assert.deepEqual(await cards(page, 'game'), ['cafe'], 'a decomposed accent in the query folds too');
      await gameFilter(page).fill('');
      await openZeta(page);
      const check = async (query, expected, note) => {
        await modFilter(page).fill(query);
        assert.deepEqual((await cards(page, 'mod')).sort(), expected.map(String).sort(), `${note}: "${query}"`);
      };
      await check('ZEBRA', [30], 'case');
      await check('cafe', [45], 'accent in name');
      await check('CAFÉ', [45], 'accent in query');
      await check('beyonce', [7], 'accent in name, plain query');
      await check('résumé', [7], 'accent in summary');
      await check('resume', [7], 'accent in summary, plain query');
      await check('okami', [12], 'author with macron');
      await check('ōkami', [12], 'author with macron in query');
      await check('lovelace', [30, 45], 'author');
      await check('stripes', [30], 'summary');
      await check('  zebra  ', [30], 'surrounding whitespace');
      await check('zzzz', [], 'no match');
      await clean(page, api);
    });

    await test('several words must all match, description is not searched, markup stays inert', async () => {
      const { page, api } = await fixture();
      await openZeta(page);
      const check = async (query, expected) => {
        await modFilter(page).fill(query);
        assert.deepEqual((await cards(page, 'mod')).sort(), expected.map(String).sort(), `"${query}"`);
      };
      await check('ada lamps', [45]);          // author + summary of one mod
      await check('lamps ada', [45]);          // order does not matter
      await check('ada    zebra', [30]);       // runs of whitespace
      await check('ada camera', []);           // both words must hit the same mod
      await check('ada lovelace soft', [45]);
      await check('lov', [30, 45]);            // substrings
      await check('secretword', []);           // description is not part of search
      await check('   ', [30, 12, 45, 7, 21]); // blank query shows everything
      await check('<img', [21]);
      await check('mallory &', [21]);
      assert.equal(await page.locator('#games-content img, #games-content b').count(), 0);
      assert.match(await page.locator('#games-content').innerText(), /<img src=x onerror=window\.__pwned=1>/);
      assert.equal(api.requests.some(url => /search|sort/.test(url)), false, 'filtering never queries the server');
      await clean(page, api);
    });

    await test('count is live, empty results explain themselves and clearing works by pointer, keyboard and controller', async () => {
      const { page, api } = await fixture();
      assert.equal(await page.locator('#catalog-count').getAttribute('aria-live'), 'polite');
      assert.match(await count(page), /^4 of 4 loaded games/);
      assert.equal(await page.locator('#catalog-clear').isHidden(), true, 'nothing to clear yet');
      await gameFilter(page).fill('zeta');
      assert.match(await count(page), /^1 of 4 loaded games/);
      assert.equal(await page.locator('#catalog-clear').isVisible(), true);
      await page.locator('#catalog-clear').click();
      assert.equal(await gameFilter(page).inputValue(), '');
      assert.equal(await active(page), 'catalog-search', 'focus returns to the field');
      assert.equal(await page.locator('#catalog-clear').isHidden(), true);
      assert.match(await count(page), /^4 of 4 loaded games/);
      // No match: message plus a second clear control.
      await gameFilter(page).fill('nothing here');
      assert.deepEqual(await cards(page, 'game'), []);
      assert.match(await page.locator('.no-matches').innerText(), /No games match[\s\S]*nothing here/);
      assert.match(await count(page), /^0 of 4 loaded games/);
      await page.locator('.no-matches [data-clear-search]').focus();
      await press(page, CROSS);
      assert.equal(await gameFilter(page).inputValue(), '');
      assert.equal((await cards(page, 'game')).length, 4);
      // Escape clears a non-empty field before it navigates anywhere.
      await openZeta(page);
      await modFilter(page).fill('zebra');
      await page.keyboard.press('Escape');
      assert.equal(await modFilter(page).inputValue(), '');
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      assert.equal((await cards(page, 'mod')).length, 5);
      await page.keyboard.press('Escape');
      await page.getByRole('heading', { name: 'Choose a game' }).waitFor();
      await clean(page, api);
    });

    await test('each view keeps its own query', async () => {
      const { page, api } = await fixture();
      await gameFilter(page).fill('zeta');
      await openZeta(page);
      assert.equal(await modFilter(page).inputValue(), '', 'the mods query starts empty for a new list');
      await modFilter(page).fill('zebra');
      assert.deepEqual(await cards(page, 'mod'), ['30']);
      await page.getByRole('button', { name: /Zebra textures/ }).click();
      await page.locator('[data-file-card]').first().waitFor();
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      assert.equal(await modFilter(page).inputValue(), 'zebra', 'returning from files keeps the mods query');
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      assert.equal(await gameFilter(page).inputValue(), 'zeta', 'games query survives the round trip');
      assert.deepEqual(await cards(page, 'game'), ['zeta']);
      await openZeta(page);
      assert.equal(await modFilter(page).inputValue(), '', 'a freshly opened game starts unfiltered');
      assert.equal((await cards(page, 'mod')).length, 5);
      await clean(page, api);
    });

    await test('game sorts: name both ways and most mods with name ties and missing counts last', async () => {
      const { page, api } = await fixture();
      const sort = page.locator('#catalog-sort');
      assert.equal(await sort.innerText(), 'Sort: Nexus order');
      assert.deepEqual(await cards(page, 'game'), ['zeta', 'alpha', 'cafe', 'plain']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Name A–Z');
      assert.deepEqual(await cards(page, 'game'), ['alpha', 'cafe', 'plain', 'zeta']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Name Z–A');
      assert.deepEqual(await cards(page, 'game'), ['zeta', 'plain', 'cafe', 'alpha']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Most mods');
      assert.deepEqual(await cards(page, 'game'), ['alpha', 'cafe', 'zeta', 'plain'], '12-mod tie is broken by name; the game without a count is last');
      assert.match(await count(page), /sorted by Most mods/);
      await gameFilter(page).fill('world');
      assert.deepEqual(await cards(page, 'game'), ['alpha', 'zeta'], 'sorting applies to the filtered list');
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Nexus order');
      assert.deepEqual(await cards(page, 'game'), ['zeta', 'alpha']);
      await clean(page, api);
    });

    await test('mod sorts: name, author with name tie-breaker and mod ID', async () => {
      const { page, api } = await fixture();
      await openZeta(page);
      const sort = page.locator('#catalog-sort');
      assert.deepEqual(await cards(page, 'mod'), ['30', '12', '45', '7', '21']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Name A–Z');
      assert.deepEqual(await cards(page, 'mod'), ['12', '7', '45', '21', '30']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Name Z–A');
      assert.deepEqual(await cards(page, 'mod'), ['30', '21', '45', '7', '12']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Author A–Z');
      assert.deepEqual(await cards(page, 'mod'), ['45', '30', '12', '21', '7'], 'Ada Lovelace ties are ordered by mod name');
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Mod ID high to low');
      assert.deepEqual(await cards(page, 'mod'), ['45', '30', '21', '12', '7']);
      await modFilter(page).fill('ada');
      assert.deepEqual(await cards(page, 'mod'), ['45', '30']);
      await sort.click(); assert.equal(await sort.innerText(), 'Sort: Nexus order');
      assert.deepEqual(await cards(page, 'mod'), ['30', '45']);
      await clean(page, api);
    });

    await test('file sorts and file filter are offered only where there is something to order', async () => {
      const { page, api } = await fixture();
      await openFiles(page);
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      assert.equal(await page.locator('#file-tools').isVisible(), true);
      const sort = page.locator('#file-sort');
      const step = async (label, expected) => {
        await sort.click(); assert.equal(await sort.innerText(), `Sort: ${label}`);
        assert.deepEqual(await fileCards(page), expected.map(String), label);
      };
      assert.equal(await sort.innerText(), 'Sort: Nexus order');
      assert.deepEqual(await fileCards(page), ['102', '104', '101', '103']);
      await step('Name A–Z', [101, 103, 102, 104]);
      await step('Name Z–A', [104, 102, 103, 101]);
      await step('Recently uploaded', [102, 101, 103, 104]); // no upload date last
      await step('Largest first', [103, 101, 102, 104]);
      await step('Category', [101, 102, 103, 104]);          // no category last
      await step('Nexus order', [102, 104, 101, 103]);
      assert.match(await page.locator('#file-count').innerText(), /^4 of 4 loaded files · sorted by Nexus order/);
      // Filter by name, file name, version, description and category.
      const fileFilter = page.getByLabel('Filter loaded files');
      for (const [query, expected] of [['extras', [102]], ['old-build.zip', [103]], ['0.5', [104]], ['primary archive', [101]], ['old_version', [103]], ['ZIP', [102, 104, 101, 103]]]) {
        await fileFilter.fill(query);
        assert.deepEqual((await fileCards(page)).sort(), expected.map(String).sort(), query);
      }
      await fileFilter.fill('nope');
      assert.match(await page.locator('.no-matches').innerText(), /No files match/);
      await page.locator('.no-matches [data-clear-search]').click();
      assert.equal((await fileCards(page)).length, 4);
      // Cards show the extra facts the records carry.
      assert.match(await page.locator('[data-file-card="101"]').innerText(), /Main · Primary file · Uploaded Nov 14, 2023/);
      assert.doesNotMatch(await page.locator('[data-file-card="104"]').innerText(), /Uploaded|Primary/);
      // One file: nothing to search or sort.
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      await page.getByRole('button', { name: /Amber camera/ }).click();
      await page.locator('[data-file-card]').first().waitFor();
      assert.equal(await page.locator('#file-tools').isHidden(), true);
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      await clean(page, api);
    });

    await test('sort choice is remembered per view in sessionStorage, validated, and optional', async () => {
      const { page, api } = await fixture();
      const sort = page.locator('#catalog-sort');
      for (let n = 0; n < 3; n++) await sort.click();
      assert.equal(await sort.innerText(), 'Sort: Most mods');
      await openZeta(page);
      assert.equal(await sort.innerText(), 'Sort: Nexus order', 'mods have their own sort');
      for (let n = 0; n < 3; n++) await sort.click();
      assert.equal(await sort.innerText(), 'Sort: Author A–Z');
      assert.deepEqual(await page.evaluate(key => JSON.parse(sessionStorage.getItem(key)), browseKey), { version: 1, sort: { games: 'mods-desc', mods: 'author-asc', files: 'nexus' } });
      await page.reload(); await page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor();
      await page.locator('[data-card^="mod:"]').first().waitFor();
      assert.equal(await sort.innerText(), 'Sort: Author A–Z');
      assert.deepEqual(await cards(page, 'mod'), ['45', '30', '12', '21', '7']);
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      assert.equal(await sort.innerText(), 'Sort: Most mods');
      await clean(page, api);

      // Hostile or stale stored values fall back safely and never throw.
      for (const seed of ['{"version":1,"sort":{"games":"bogus","mods":"id-desc","files":7}}', '{"version":2,"sort":{"games":"mods-desc"}}', 'not json', '[]']) {
        const seeded = await fixtureWithSeed(seed);
        const label = await seeded.page.locator('#catalog-sort').innerText();
        assert.equal(label, 'Sort: Nexus order', `games fall back for ${seed}`);
        if (seed.includes('id-desc')) {
          await seeded.page.getByRole('button', { name: 'Zeta world' }).click();
          await seeded.page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor();
          await seeded.page.locator('[data-card^="mod:"]').first().waitFor();
          assert.equal(await seeded.page.locator('#catalog-sort').innerText(), 'Sort: Mod ID high to low', 'valid entries are kept');
        }
        await clean(seeded.page, seeded.api);
      }

      // Storage that throws must not break sorting.
      const denied = await fixture({ init: () => Object.defineProperty(window, 'sessionStorage', { get() { throw new DOMException('denied', 'SecurityError'); } }) });
      await denied.page.locator('#catalog-sort').click();
      assert.equal(await denied.page.locator('#catalog-sort').innerText(), 'Sort: Name A–Z');
      assert.deepEqual(await cards(denied.page, 'game'), ['alpha', 'cafe', 'plain', 'zeta']);
      assert.deepEqual(denied.api.errors, []);
    });

    await test('a sort whose field no loaded record has is never offered', async () => {
      const { page, api } = await fixture({ games: games().map(({ mods, ...rest }) => rest), mods: { zeta: zetaMods().map(({ author, ...rest }) => rest) }, files: { 30: zebraFiles().map(({ uploaded_timestamp, category_id, category_name, size_kb, ...rest }) => rest) } });
      const seen = [];
      for (let n = 0; n < 4; n++) { seen.push(await page.locator('#catalog-sort').innerText()); await page.locator('#catalog-sort').click(); }
      assert.deepEqual(seen, ['Sort: Nexus order', 'Sort: Name A–Z', 'Sort: Name Z–A', 'Sort: Nexus order'], 'no "Most mods" without counts');
      await openZeta(page);
      seen.length = 0;
      for (let n = 0; n < 5; n++) { seen.push(await page.locator('#catalog-sort').innerText()); await page.locator('#catalog-sort').click(); }
      assert.deepEqual(seen, ['Sort: Nexus order', 'Sort: Name A–Z', 'Sort: Name Z–A', 'Sort: Mod ID high to low', 'Sort: Nexus order'], 'no author sort without authors');
      await page.getByRole('button', { name: /Zebra textures/ }).click(); await page.locator('[data-file-card]').first().waitFor();
      seen.length = 0;
      for (let n = 0; n < 4; n++) { seen.push(await page.locator('#file-sort').innerText()); await page.locator('#file-sort').click(); }
      assert.deepEqual(seen, ['Sort: Nexus order', 'Sort: Name A–Z', 'Sort: Name Z–A', 'Sort: Nexus order'], 'no date, size or category sorts without those fields');
      await clean(page, api);
    });

    await test('mod details list every record field, escape them, and link to Nexus', async () => {
      const { page, api } = await fixture();
      await openZeta(page);
      await page.locator('[data-card="mod:30"]').focus();
      await press(page, OPTIONS);
      const dialog = page.locator('#details-dialog');
      assert.equal(await dialog.evaluate(el => el.open), true);
      assert.equal(await page.locator('#details-title').innerText(), 'Zebra textures');
      const rows = await page.$$eval('#details-content .detail-row', els => Object.fromEntries(els.map(el => [el.querySelector('.detail-label').textContent, el.querySelector('.detail-value').textContent])));
      assert.equal(rows.Author, 'Ada Lovelace');
      assert.equal(rows.Version, '1.0');
      assert.equal(rows.Summary, 'Crisp stripes for every surface');
      assert.match(rows.Description, /^Description line 1\n/);
      assert.equal(rows['Mod ID'], '30');
      assert.equal(rows.Game, 'zeta');
      const link = page.getByRole('link', { name: 'Open on Nexus' });
      assert.equal(await link.getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/30');
      assert.equal(await link.getAttribute('target'), '_blank');
      assert.match(await link.getAttribute('rel'), /noopener/);
      await press(page, CIRCLE);
      assert.equal(await dialog.evaluate(el => el.open), false, 'Circle closes');
      assert.equal(await active(page), 'mod:30', 'focus returns to the card that opened it');
      // A hostile source_url is never followed; escaped record text stays text.
      await page.locator('[data-card="mod:12"]').focus(); await page.keyboard.press('F6');
      assert.equal(await page.getByRole('link', { name: 'Open on Nexus' }).getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/12');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'mod:12');
      await page.locator('[data-card="mod:21"]').focus(); await page.keyboard.press('F6');
      assert.match(await page.locator('#details-title').innerText(), /^Hostile <img src=x onerror=window\.__pwned=1>$/);
      assert.match(await page.locator('#details-content').innerText(), /<b>bold<\/b> summary/);
      assert.equal(await page.locator('#details-dialog img, #details-dialog b').count(), 0);
      await page.getByRole('button', { name: 'Back', exact: true }).click();
      assert.equal(await active(page), 'mod:21');
      // Context menu opens the same page.
      await page.locator('[data-card="mod:7"]').click({ button: 'right' });
      assert.equal(await dialog.evaluate(el => el.open), true);
      assert.equal(await page.locator('#details-content .detail-row').first().innerText().then(text => /Zoë Quinn/.test(text)), true);
      await page.keyboard.press('Escape');
      // Game details too.
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      await page.locator('[data-card="game:alpha"]').focus(); await page.keyboard.press('F6');
      const gameRows = await page.locator('#details-content').innerText();
      assert.match(gameRows, /alpha/); assert.match(gameRows, /340/);
      assert.equal(await page.getByRole('link', { name: 'Open on Nexus' }).getAttribute('href'), 'https://www.nexusmods.com/alpha');
      await page.keyboard.press('Escape');
      await clean(page, api);
    });

    await test('file details show size, category, local date, compatibility and conflicts from the newest job', async () => {
      const jobs = [
        job('old-job', { compatibility_assessment: compat('incompatible') }),
        job('new-job', { compatibility_assessment: compat('compatible'), conflicts: conflicts() }),
        job('other-file', { file_id: 102, conflicts: { with: [], paths_sample: [], harmless_duplicates: 0 } }),
        job('other-mod', { mod_id: 12, file_id: 101, compatibility_assessment: compat('incompatible') }),
        job('bad-only', { file_id: 103, conflicts: { with: 'nope' } }),
      ];
      const { page, api } = await fixture({ jobs });
      await openFiles(page);
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      assert.equal(await page.locator('#details-title').innerText(), 'Main file');
      const rows = await page.$$eval('#details-content .detail-row', els => Object.fromEntries(els.map(el => [el.querySelector('.detail-label').textContent, el.querySelector('.detail-value').innerText])));
      assert.equal(rows['File name'], 'main-file.zip');
      assert.equal(rows.Version, '1.0');
      assert.equal(rows.Size, '2 MB (2,048 KB)');
      assert.equal(rows.Category, 'Main');
      assert.equal(rows['Primary file'], 'Yes');
      assert.match(rows.Uploaded, /^Nov 14, 2023, 10:13 PM$/, 'local, readable date');
      assert.equal(await page.locator('#details-content time').getAttribute('datetime'), '2023-11-14T22:13:20.000Z');
      assert.equal(rows.Description, 'Primary archive');
      assert.equal(rows.Mod, 'Zebra textures · by Ada Lovelace');
      assert.equal(await page.locator('#details-content [data-compat-verdict]').getAttribute('data-compat-verdict'), 'compatible', 'newest assessed job wins');
      assert.match(rows.Compatibility, /Compatible/);
      assert.match(rows.Compatibility, /Static check\. Not tested on PS5\./, 'the dialog badge carries the same static-only qualifier');
      assert.match(rows.Conflicts, /Shares 3 files with Amber camera · Main \(this mod wins\)/);
      assert.match(rows.Conflicts, /Shares 1 file with Other mod · file \(the other mod wins\)/);
      assert.equal(await page.getByRole('link', { name: 'Open on Nexus' }).getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/30?tab=files&file_id=101');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'file-details:101');
      // A file whose only job reports no overlap: conflicts say so, no badge.
      await page.getByRole('button', { name: 'Details for Optional extras' }).click();
      const second = await page.$$eval('#details-content .detail-label', els => els.map(el => el.textContent));
      assert.ok(second.includes('Conflicts') && !second.includes('Compatibility'));
      assert.match(await page.locator('#details-content').innerText(), /No shared files found with other analyzed downloads/);
      assert.match(await page.locator('#details-content').innerText(), /Option/);
      await page.keyboard.press('Escape');
      // No usable job: no Compatibility or Conflicts rows; absent facts are omitted, not invented.
      for (const name of ['Old build', 'Patch notes']) {
        await page.getByRole('button', { name: `Details for ${name}` }).click();
        const labels = await page.$$eval('#details-content .detail-label', els => els.map(el => el.textContent));
        assert.ok(!labels.includes('Compatibility') && !labels.includes('Conflicts'), name);
        if (name === 'Patch notes') assert.ok(!labels.includes('Uploaded') && !labels.includes('Category') && !labels.includes('Primary file') && !labels.includes('Description'));
        else assert.equal(await page.locator('#details-content').innerText().then(text => /primary file\s*no/i.test(text)), true);
        await page.keyboard.press('Escape');
      }
      await clean(page, api);
    });

    await test('file details show the PS5 Lua runtime section from the job that carries a valid runtime_coverage, reusing the card markup', async () => {
      const covered = runtimeCoverage({ status: 'covered', missing: [], partial: [], partial_modules: [] });
      const hostile = '<img src=x onerror=window.__pwned=1>';
      const jobs = [
        job('old-runtime', { runtime_coverage: runtimeCoverage({ missing: ['OldFunction'] }) }),
        job('new-runtime', { compatibility_assessment: compat('compatible'), runtime_coverage: runtimeCoverage() }),
        job('only-runtime', { file_id: 102, runtime_coverage: covered }),
        job('bad-runtime', { file_id: 103, runtime_coverage: { ...runtimeCoverage(), surprise: true } }),
        job('hostile-runtime', { file_id: 104, runtime_coverage: runtimeCoverage({ missing: [hostile], partial: [] }) }),
      ];
      const { page, api } = await fixture({ jobs });
      // The card for the newest job on file 101, kept to prove the dialog shows the very same section.
      await page.getByRole('tab', { name: 'Activity' }).click();
      const cardHtml = await page.locator('[data-job-id="new-runtime"] .runtime-coverage').evaluate(el => el.outerHTML);
      await page.getByRole('tab', { name: 'Games' }).click();
      await openFiles(page);
      const labels = async () => page.$$eval('#details-content .detail-label', els => els.map(el => el.textContent));
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      assert.deepEqual((await labels()).filter(label => ['Compatibility', 'PS5 Lua runtime', 'Conflicts'].includes(label)), ['Compatibility', 'PS5 Lua runtime'], 'newest job wins; section follows the compatibility row');
      const section = page.locator('#details-content .runtime-coverage');
      assert.equal(await section.count(), 1);
      assert.equal(await section.evaluate(el => el.outerHTML), cardHtml, 'same rendering as the job card');
      assert.equal(await section.getAttribute('data-runtime-status'), 'blocked');
      const text = await section.innerText();
      assert.ok(text.includes('ExecuteInGameThread') && !text.includes('OldFunction'));
      assert.ok(text.includes('The PS5 Lua runtime is not running on the console yet, so this mod cannot run there today.'));
      assert.ok(text.includes("Checked by reading the mod's script, not by running it."));
      assert.doesNotMatch(text, /\b(works?|working|installable|install|compatible|ready)\b/i);
      assert.equal(await section.locator('a, button, input, summary, [tabindex]').count(), 0, 'plain text inside the section takes no focus');
      assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
      // The row is a scroll region, like long text, so a controller can reach it: it is one stop, and focus is never trapped on it.
      const region = page.locator('#details-content [aria-label="PS5 Lua runtime"][tabindex="0"]');
      assert.equal(await region.count(), 1);
      await page.getByRole('button', { name: 'Close details' }).focus();
      let reached = false;
      for (let step = 0; step < 12 && !reached; step++) { await page.keyboard.press('ArrowDown'); reached = await region.evaluate(el => el === document.activeElement); }
      assert.equal(reached, true, 'the runtime row is reachable by arrow keys');
      // Like long text, it scrolls to its end first when taller than its box, then hands focus on.
      let guard = 0;
      while (await region.evaluate(el => el === document.activeElement) && guard++ < 40) await page.keyboard.press('ArrowDown');
      assert.equal(await region.evaluate(el => el.scrollTop + el.clientHeight >= el.scrollHeight - 2), true, 'scrolled to the end before focus moved on');
      assert.equal(await region.evaluate(el => el === document.activeElement), false, 'focus moves on from the runtime row');
      guard = 0;
      while (!(await region.evaluate(el => el === document.activeElement)) && guard++ < 6) await page.keyboard.press('ArrowUp');
      assert.equal(await region.evaluate(el => el === document.activeElement), true, 'and comes back');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'file-details:101');
      // A job with only a runtime check still gets the section, and no Compatibility row.
      await page.getByRole('button', { name: 'Details for Optional extras' }).click();
      assert.deepEqual((await labels()).filter(label => ['Compatibility', 'PS5 Lua runtime', 'Conflicts'].includes(label)), ['PS5 Lua runtime']);
      assert.equal(await page.locator('#details-content .runtime-coverage').getAttribute('data-runtime-status'), 'covered');
      await page.keyboard.press('Escape');
      // An invalid record is dropped whole: no row, no host strings.
      await page.getByRole('button', { name: 'Details for Old build' }).click();
      assert.ok(!(await labels()).includes('PS5 Lua runtime'));
      assert.equal(await page.locator('#details-content .runtime-coverage').count(), 0);
      await page.keyboard.press('Escape');
      // Host-provided names are escaped in the dialog too.
      await page.getByRole('button', { name: 'Details for Patch notes' }).click();
      assert.equal(await page.locator('#details-content .runtime-coverage').count(), 1);
      assert.equal(await page.locator('#details-content img').count(), 0);
      assert.match(await page.locator('#details-content .runtime-names').first().innerText(), /<img src=x onerror=window\.__pwned=1>/);
      await page.keyboard.press('Escape');
      await clean(page, api);
    });

    await test('file details show a positive verdict next to a hard-block analysis as limited, never as plain Compatible', async () => {
      const jobs = [job('blocked', { compatibility_assessment: compat('compatible'), analysis_report: hardBlockReport(), runtime_coverage: runtimeCoverage() })];
      const { page, api } = await fixture({ jobs });
      await openFiles(page);
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      const badge = page.locator('#details-content .compat-badge');
      assert.equal(await badge.count(), 1);
      assert.equal(await badge.evaluate(el => el.classList.contains('compat-limited') && el.dataset.compatLimited === 'true'), true);
      assert.match(await badge.innerText(), /Limited by runtime$/);
      assert.equal(await page.locator('#details-content .compat-compatible').count(), 0);
      const row = await page.locator('#details-content .detail-row', { hasText: 'Compatibility' }).locator('.detail-value').innerText();
      assert.doesNotMatch(row, /\bCompatible\b|✓/);
      assert.match(row, /Static check\. Not tested on PS5\./);
      assert.match(row, /The runtime this mod needs is not available on PS5, so this content check does not show it can be used there\./);
      // This job's Lua check is blocked and not wired to the console too: both causes read, the badge once.
      assert.match(row, /The PS5 Lua runtime cannot run this mod today, so this content check does not show it can be used there\./);
      assert.deepEqual(await page.locator('#details-content .compat-limit').evaluateAll(els => els.map(el => el.dataset.limitCause)), ['report', 'lua']);
      assert.ok((await page.locator('#details-content .runtime-coverage').count()) === 1, 'the runtime section sits beside it');
      await page.keyboard.press('Escape');
      await clean(page, api);
    });

    await test('file details limit a positive verdict by the Lua runtime alone: blocked or not wired to the console', async () => {
      const wired = (status, extra = {}) => runtimeCoverage({ status, console_wired: true, missing: status === 'blocked' ? ['ExecuteInGameThread'] : [], partial: [], partial_modules: [], ...extra });
      const jobs = [
        job('blocked-unwired', { file_id: 101, compatibility_assessment: compat('compatible'), runtime_coverage: runtimeCoverage() }),
        job('covered-wired', { file_id: 102, compatibility_assessment: compat('partially_compatible'), runtime_coverage: wired('covered') }),
        job('partial-wired', { file_id: 104, compatibility_assessment: compat('compatible'), runtime_coverage: wired('partial', { partial: ['ExecuteAsync'] }) }),
        job('covered-unwired', { file_id: 103, compatibility_assessment: compat('compatible'), runtime_coverage: wired('covered', { console_wired: false }) }),
      ];
      const { page, api } = await fixture({ jobs });
      await openFiles(page);
      const luaSentence = 'The PS5 Lua runtime cannot run this mod today, so this content check does not show it can be used there.';
      const open = async name => { await page.getByRole('button', { name: `Details for ${name}` }).click(); return page.locator('#details-content .detail-row', { hasText: 'Compatibility' }).locator('.detail-value'); };
      const close = () => page.keyboard.press('Escape');
      for (const name of ['Main file', 'Old build']) {
        const row = await open(name);
        const badge = row.locator('.compat-badge');
        assert.equal(await badge.count(), 1, name);
        assert.equal(await badge.evaluate(el => el.classList.contains('compat-limited') && el.dataset.compatLimited === 'true'), true, name);
        assert.match(await badge.innerText(), /Limited by runtime$/);
        assert.deepEqual(await row.locator('.compat-limit').evaluateAll(els => els.map(el => el.dataset.limitCause)), ['lua'], name);
        assert.equal(await row.locator('.compat-limit').innerText(), luaSentence);
        assert.doesNotMatch(await row.innerText(), /\bCompatible\b|Partially compatible|✓|◐/, name);
        assert.match(await row.innerText(), /Static check\. Not tested on PS5\./);
        assert.match(await row.innerText(), /\d+ of \d+ referenced items found/, 'the content facts stay');
        await close();
      }
      // Covered or partial coverage with the runtime wired to the console leaves the verdict alone.
      for (const [name, cls, label] of [['Optional extras', 'compat-partial', 'Partially compatible'], ['Patch notes', 'compat-compatible', 'Compatible']]) {
        const row = await open(name);
        assert.equal(await row.locator('.compat-badge').evaluate((el, expected) => el.classList.contains(expected) && !el.hasAttribute('data-compat-limited'), cls), true, name);
        assert.match(await row.locator('.compat-badge').innerText(), new RegExp(`${label}$`));
        assert.equal(await row.locator('.compat-limit').count(), 0, name);
        await close();
      }
      await clean(page, api);
    });

    await test('the account shows Premium or Free from is_premium, hints at the free download path, and shows neither when unsure', async () => {
      const connected = extra => ({ status: 'connected', live_service: 'verified', account: { display_name: 'Owned fixture', ...extra } });
      const hint = 'Free account: downloads need the website’s “Slow download” button and a pasted nxm:// link.';
      for (const [name, account, tier] of [['premium', connected({ is_premium: true }), 'Premium'], ['free', connected({ is_premium: false }), 'Free']]) {
        const { page, api } = await fixture({ account });
        await page.locator('#account .account-tier').waitFor();
        assert.equal(await page.locator('#account .account-tier').innerText(), tier, name);
        assert.equal(await page.locator('#account .account-tier').getAttribute('data-account-tier'), tier.toLowerCase());
        assert.match(await page.locator('#account').innerText(), /Nexus · Owned fixture/);
        if (tier === 'Free') {
          assert.equal(await page.locator('#account .account-hint').innerText(), hint);
          assert.match(await page.locator('#account .account-hint').innerText(), /Slow download.*nxm/);
        } else assert.equal(await page.locator('#account .account-hint').count(), 0, 'Premium needs no hint');
        // Plain text: the only control in the account box is still Disconnect.
        assert.equal(await page.locator('#account').locator('a, button, input, select, [tabindex]').count(), 1);
        assert.equal(await page.locator('#account .account-tier, #account .account-hint').locator('a, button, [tabindex]').count(), 0);
        assert.deepEqual(api.errors, []);
      }
      // Fail closed: a missing or non-boolean value names no plan and offers no hint.
      const unsure = [['missing', {}], ['null', { is_premium: null }], ['string true', { is_premium: 'true' }], ['string false', { is_premium: 'false' }], ['number', { is_premium: 1 }], ['zero', { is_premium: 0 }], ['object', { is_premium: {} }], ['array', { is_premium: [true] }]];
      for (const [name, extra] of unsure) {
        const { page, api } = await fixture({ account: connected(extra) });
        await page.locator('#account .account-action').waitFor();
        assert.equal(await page.locator('#account .account-tier, #account .account-hint').count(), 0, name);
        assert.doesNotMatch(await page.locator('#account').innerText(), /Premium|Free|Slow download/, name);
        assert.match(await page.locator('#account').innerText(), /Nexus · Owned fixture/);
        assert.deepEqual(api.errors, []);
      }
      // A record that is not a verified connection shows no plan, whatever it claims.
      for (const [name, account] of [['unverified', { status: 'connected', live_service: 'unverified', account: { display_name: 'Do not show', is_premium: true } }], ['disconnected', { status: 'disconnected', account: { display_name: 'Do not show', is_premium: false } }]]) {
        const { page } = await fixture({ account });
        await page.waitForFunction(() => /not connected/.test(document.querySelector('#account').innerText));
        assert.equal(await page.locator('#account .account-tier, #account .account-hint').count(), 0, name);
        assert.doesNotMatch(await page.locator('#account').innerText(), /Premium|Free|Do not show/, name);
      }
    });

    await test('details are fully controller navigable: long text scrolls, nothing traps focus, Circle closes', async () => {
      const { page, api } = await fixture();
      await openZeta(page);
      // D-pad walk through a short record: every part is a stop and focus wraps.
      await page.locator('[data-card="mod:12"]').focus();
      await press(page, OPTIONS);
      assert.equal(await active(page), 'Close details');
      const seen = [];
      for (let step = 0; step < 8; step++) {
        await press(page, DOWN);
        seen.push(await page.evaluate(() => { const el = document.activeElement; return el.getAttribute('aria-label') || el.textContent.trim().slice(0, 20); }));
        if (seen[seen.length - 1] === 'Close details') break;
      }
      assert.deepEqual(seen, ['Summary', 'Description', 'Open on Nexus', 'Back', 'Close details'], 'every part is reachable and focus wraps');
      await press(page, CIRCLE);
      assert.equal(await page.locator('#details-dialog').evaluate(el => el.open), false, 'Circle closes');
      assert.equal(await active(page), 'mod:12');
      // Long description: arrow keys scroll it to the end before focus moves on.
      await page.locator('[data-card="mod:30"]').focus(); await press(page, OPTIONS);
      const region = page.locator('#details-content [aria-label="Description"]');
      await region.focus();
      assert.equal(await region.evaluate(el => el.scrollHeight > el.clientHeight), true, 'description is scrollable');
      let guard = 0;
      while ((await active(page)) === 'Description' && guard++ < 80) await page.keyboard.press('ArrowDown');
      assert.equal(await region.evaluate(el => el.scrollTop + el.clientHeight >= el.scrollHeight - 2), true, 'scrolled to the end first');
      assert.equal(await active(page), 'details-nexus-link');
      await page.keyboard.press('ArrowUp');
      assert.equal(await active(page), 'Description');
      assert.equal(await region.evaluate(el => el.matches(':focus-visible') && getComputedStyle(el).outlineStyle !== 'none'), true, 'focus ring is visible');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'mod:30');
      // Cross on the link opens Nexus in a new tab (the fixture blocks the request and records it).
      await page.locator('[data-card="mod:30"]').focus(); await press(page, OPTIONS);
      await page.getByRole('link', { name: 'Open on Nexus' }).focus();
      const popup = page.context().waitForEvent('page');
      await press(page, CROSS);
      const opened = await popup;
      for (let n = 0; n < 20 && !api.outbound.length; n++) await page.waitForTimeout(50);
      await opened.close().catch(() => {});
      await clean(page, api, ['https://www.nexusmods.com/zeta/mods/30']);
    });

    await test('conflict warnings appear on activity cards with a collapsed sample that survives refresh', async () => {
      const api0 = { jobs: [job('with-conflicts', { conflicts: conflicts() }), job('harmless-only', { conflicts: { with: [], paths_sample: [], harmless_duplicates: 4 } }), job('nothing', { conflicts: { with: [], paths_sample: [], harmless_duplicates: 0 } })] };
      const { page, api } = await fixture(api0);
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = page.locator('[data-job-id="with-conflicts"]');
      await card.waitFor();
      const lines = await card.locator('.conflict-line').allInnerTexts();
      assert.deepEqual(lines.map(line => line.replace(/^⚠\s*/, '')), ['Shares 3 files with Amber camera · Main (this mod wins)', 'Shares 1 file with Other mod · file (the other mod wins)']);
      assert.equal(await card.locator('.conflict-glyph').first().getAttribute('aria-hidden'), 'true');
      assert.equal(await card.locator('.conflicts').getAttribute('aria-label'), 'File conflicts');
      assert.match(await card.locator('.conflict-note').innerText(), /Also shares 2 harmless duplicate files/);
      const sample = card.locator('details.conflict-paths');
      assert.equal(await sample.evaluate(el => el.open), false, 'sample starts collapsed');
      assert.match(await sample.locator('summary').innerText(), /Sample of shared files \(2\)/);
      assert.deepEqual(await sample.locator('li').allTextContents(), ['Game/Content/Textures/a.uasset', 'Game/Content/b.uasset']);
      // Reachable and operable without a pointer.
      await sample.locator('summary').focus();
      await press(page, CROSS);
      assert.equal(await sample.evaluate(el => el.open), true);
      // A poll that changes the job must keep it open and focused.
      api.jobs[0].bytes_downloaded = 777;
      await page.getByText('777 bytes').waitFor({ timeout: 5000 });
      assert.equal(await page.locator('[data-job-id="with-conflicts"] details.conflict-paths').evaluate(el => el.open), true);
      assert.equal(await page.evaluate(() => document.activeElement.dataset.focusKey), 'conflict-paths');
      // Only harmless duplicates: a note, no warning line. Nothing at all: nothing.
      const harmless = page.locator('[data-job-id="harmless-only"]');
      assert.equal(await harmless.locator('.conflict-line').count(), 0);
      assert.match(await harmless.innerText(), /Also shares 4 harmless duplicate files/);
      assert.equal(await page.locator('[data-job-id="nothing"] .conflicts').count(), 0);
      // Edge sizes the contract allows.
      api.jobs.push(job('max', { conflicts: { with: Array.from({ length: 32 }, (_, n) => ({ job_id: `j${n}`, label: `Mod ${n}`, paths: 1, winner: n % 2 ? 'other' : 'this' })), paths_sample: Array.from({ length: 20 }, (_, n) => n ? `p/${n}.uasset` : 'q'.repeat(4096)), harmless_duplicates: 0 } }));
      await page.locator('[data-job-id="max"]').waitFor({ timeout: 5000 });
      assert.equal(await page.locator('[data-job-id="max"] .conflict-line').count(), 32);
      assert.equal(await page.locator('[data-job-id="max"] .conflict-paths li').count(), 20);
      await clean(page, api);
    });

    await test('invalid conflict reports are ignored without hiding the job', async () => {
      const good = conflicts();
      const mutate = change => { const value = structuredClone(good); const result = change(value); return result === undefined ? value : result; };
      const bad = {
        'extra top-level key': mutate(v => { v.extra = 1; }),
        'missing harmless_duplicates': mutate(v => { delete v.harmless_duplicates; }),
        'with is not a list': mutate(v => { v.with = 'x'; }),
        '33 conflicting jobs': mutate(v => { v.with = Array.from({ length: 33 }, (_, n) => ({ job_id: `j${n}`, label: 'L', paths: 1, winner: 'this' })); }),
        '21 sample paths': mutate(v => { v.paths_sample = Array.from({ length: 21 }, (_, n) => `p${n}`); }),
        'zero paths': mutate(v => { v.with[0].paths = 0; }),
        'negative paths': mutate(v => { v.with[0].paths = -2; }),
        'fractional paths': mutate(v => { v.with[0].paths = 1.5; }),
        'string paths': mutate(v => { v.with[0].paths = '3'; }),
        'unknown winner': mutate(v => { v.with[0].winner = 'both'; }),
        'empty label': mutate(v => { v.with[0].label = '  '; }),
        'numeric label': mutate(v => { v.with[0].label = 5; }),
        'oversized label': mutate(v => { v.with[0].label = 'x'.repeat(257); }),
        'missing job_id': mutate(v => { delete v.with[0].job_id; }),
        'oversized job_id': mutate(v => { v.with[0].job_id = 'j'.repeat(129); }),
        'extra entry key': mutate(v => { v.with[0].extra = true; }),
        'negative harmless count': mutate(v => { v.harmless_duplicates = -1; }),
        'fractional harmless count': mutate(v => { v.harmless_duplicates = 0.5; }),
        'non-string sample path': mutate(v => { v.paths_sample[0] = 4; }),
        'empty sample path': mutate(v => { v.paths_sample[0] = ''; }),
        'oversized sample path': mutate(v => { v.paths_sample[0] = 'p'.repeat(4097); }),
        'null entry': mutate(v => { v.with[0] = null; }),
        'array instead of object': [],
        'string instead of object': 'conflicts',
        'null': null,
      };
      const jobs = Object.keys(bad).map((name, index) => job(`bad-${index}`, { conflicts: bad[name] }));
      jobs.push(job('good', { conflicts: good }));
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      await page.locator('[data-job-id="good"] .conflict-line').first().waitFor();
      for (const [index, name] of Object.keys(bad).entries()) {
        const card = page.locator(`[data-job-id="bad-${index}"]`);
        assert.equal(await card.count(), 1, `${name}: the job still renders`);
        assert.equal(await card.locator('.conflicts, .conflict-line, .conflict-paths').count(), 0, `${name}: report ignored`);
      }
      await clean(page, api);
    });

    await test('conflict labels and paths are escaped, never interpreted', async () => {
      const hostile = { with: [{ job_id: 'x', label: '<img src=x onerror=window.__pwned=1>', paths: 2, winner: 'this' }], paths_sample: ['<script>window.__pwned=1</script>', 'a&b"c\'d'], harmless_duplicates: 0 };
      const { page, api } = await fixture({ jobs: [job('hostile', { conflicts: hostile, label: 'Plain label' })], files: { 30: zebraFiles() } });
      await page.getByRole('tab', { name: 'Activity' }).click();
      const card = page.locator('[data-job-id="hostile"]');
      await card.locator('.conflict-line').waitFor();
      assert.match(await card.locator('.conflict-line').innerText(), /Shares 2 files with <img src=x onerror=window\.__pwned=1> \(this mod wins\)/);
      assert.equal(await page.locator('#activity-content img, #activity-content script').count(), 0);
      await card.locator('summary').click();
      assert.deepEqual(await card.locator('li').allInnerTexts(), ['<script>window.__pwned=1</script>', 'a&b"c\'d']);
      // Same markup in the file details dialog.
      await page.getByRole('tab', { name: 'Games' }).click();
      await openFiles(page);
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      assert.match(await page.locator('#details-content').innerText(), /Shares 2 files with <img src=x onerror=window\.__pwned=1>/);
      assert.equal(await page.locator('#details-dialog img, #details-dialog script').count(), 0);
      await clean(page, api);
    });

    await test('older jobs without conflicts or assessments render exactly as before', async () => {
      const { page, api } = await fixture({ jobs: [job('legacy', { status: 'needs_nexus_authorization' })] });
      await page.getByRole('tab', { name: 'Activity' }).click();
      await page.locator('[data-job-id="legacy"]').waitFor();
      assert.equal(await page.locator('.conflicts, .conflict-line, .compat-badge').count(), 0);
      assert.match(await page.locator('[data-job-id="legacy"]').innerText(), /Compatibility unverified · unresolved/);
      await page.getByRole('tab', { name: 'Games' }).click();
      await openFiles(page);
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      const labels = await page.$$eval('#details-content .detail-label', els => els.map(el => el.textContent));
      assert.ok(!labels.includes('Conflicts') && !labels.includes('Compatibility'));
      await clean(page, api);
    });

    await test('arrow keys and the D-pad reach search, clear, sort and cards by geometry with visible focus', async () => {
      const { page, api } = await fixture();
      await page.locator('#tab-installed').focus();
      const walk = async (key, target, limit = 8) => {
        const seen = [];
        for (let n = 0; n < limit; n++) { await key(); seen.push(await active(page)); if (seen[seen.length - 1] === target) break; }
        return seen;
      };
      const arrow = name => () => page.keyboard.press(name);
      // Down from the tabs reaches the search field; Right crosses the row of tools (an empty query has no clear control);
      // Down from the tools reaches the cards.
      assert.deepEqual(await walk(arrow('ArrowDown'), 'catalog-search'), ['catalog-search']);
      assert.deepEqual(await walk(arrow('ArrowRight'), 'catalog-sort'), ['catalog-sort']);
      await page.keyboard.press('ArrowDown');
      assert.match(await active(page), /^game:/);
      await gameFilter(page).fill('world');
      await page.locator('#tab-installed').focus();
      assert.deepEqual(await walk(arrow('ArrowDown'), 'catalog-search'), ['catalog-search']);
      assert.deepEqual(await walk(arrow('ArrowRight'), 'catalog-sort'), ['catalog-clear', 'catalog-sort']);
      // Focus rings are visible on every control (keyboard modality).
      for (const id of ['#catalog-search', '#catalog-clear', '#catalog-sort']) {
        await page.locator(id).focus(); await page.keyboard.press('Shift+Tab'); await page.keyboard.press('Tab');
        assert.equal(await page.locator(id).evaluate(el => el.matches(':focus-visible') && getComputedStyle(el).outlineStyle !== 'none' && parseFloat(getComputedStyle(el).outlineWidth) >= 2), true, id);
      }
      // D-pad: same path, then Cross clears and cycles.
      await page.locator('#tab-installed').focus();
      assert.deepEqual(await walk(() => press(page, DOWN), 'catalog-search'), ['catalog-search']);
      assert.deepEqual(await walk(() => press(page, RIGHT), 'catalog-sort'), ['catalog-clear', 'catalog-sort']);
      await press(page, CROSS);
      assert.equal(await page.locator('#catalog-sort').innerText(), 'Sort: Name A–Z');
      assert.equal(await active(page), 'catalog-sort', 'focus stays on the sort button');
      await page.locator('#catalog-clear').focus(); await press(page, CROSS);
      assert.equal(await gameFilter(page).inputValue(), '');
      assert.equal(await active(page), 'catalog-search');
      assert.equal((await cards(page, 'game')).length, 4);
      // Up from a card walks back out through the tools, not into a trap.
      await page.locator('[data-card="game:alpha"]').focus();
      await page.keyboard.press('ArrowUp');
      assert.match(await active(page), /^catalog-(search|sort)$/);
      if ((await active(page)) === 'catalog-sort') { await page.keyboard.press('ArrowLeft'); assert.equal(await active(page), 'catalog-search'); }
      // Up from the field goes on to the tabs.
      await page.keyboard.press('ArrowUp');
      assert.match(await active(page), /^tab-/);
      await clean(page, api);
    });

    // ---- Install-flow gaps (docs/INSTALL-FLOW-UX.md section 7, items 2, 3, 4, 5 and 8) ----
    const hex = ch => ch.repeat(32);
    const ref = { snapshot_id: 'a'.repeat(32), title_key: 'b'.repeat(32) };
    const failed = (id, extra = {}) => job(id, { status: 'failed', stage: 'requested', bytes_downloaded: undefined, error: { code: 'download_failed', message: 'host text' }, ...extra });
    const interrupted = (id, extra = {}) => job(id, { status: 'interrupted', stage: 'requested', bytes_downloaded: undefined, error: { code: 'interrupted', message: 'host text' }, ...extra });
    const waiting = (id, extra = {}) => job(id, { status: 'needs_nexus_authorization', stage: 'requested', bytes_downloaded: undefined, error: { code: 'download_authorization', message: 'host text' }, ...extra });
    const nxmLink = (extra = {}) => {
      const v = { game: 'zeta', mod: 30, file: 101, key: 'secret-grant-key', expires: Math.floor(Date.now() / 1000) + 3600, user: 42, ...extra };
      return `nxm://${v.game}/mods/${v.mod}/files/${v.file}?key=${v.key}&expires=${v.expires}&user_id=${v.user}`;
    };
    const bannerText = page => page.locator('#status-banner .banner-text').innerText();
    const card = (page, id) => page.locator(`[data-job-id="${id}"]`);
    const goActivity = async (page, id) => { await page.getByRole('tab', { name: 'Activity' }).click(); await card(page, id).waitFor(); };
    // The user text table lives in app.js; read its keys and values from the source to compare wiring, not wording.
    const catalogFromSource = () => {
      const source = fs.readFileSync(path.join(webRoot, 'app.js'), 'utf8');
      const block = source.slice(source.indexOf('const errorCatalog = Object.freeze({'), source.indexOf('const signInCodes'));
      return Object.fromEntries([...block.matchAll(/^\s{4}([a-z_]+): (?:'([^'\\]*)'|genericErrorText),?$/gm)].map(m => [m[1], m[2] ?? 'Something went wrong. Request it again.']));
    };
    const generic = 'Something went wrong. Request it again.';
    const hostile = '<img src=x onerror=window.__pwned=1>';
    const noInstall = async page => {
      assert.equal(await page.getByRole('button', { name: /install/i }).count(), 0, 'no Install control');
      assert.doesNotMatch(await page.locator('#activity-content').innerText(), /\b(is installed|are installed|PS5.compatible|tested in game|works in game)\b/i);
    };

    await test('every request error code shows the catalog text and never the host message', async () => {
      const catalog = catalogFromSource();
      assert.ok(Object.keys(catalog).length >= 50, 'the catalog covers the host codes');
      assert.equal(catalog.job_limit, 'The job queue is full. Wait for jobs to finish and try again; if it stays full, restart ModBridge.');
      const { page, api } = await fixture({ jobs: [failed(hex('a'))] });
      await goActivity(page, hex('a'));
      const exercise = async (body, status = 409) => {
        api.reply = () => ({ status, body });
        await page.evaluate(() => { document.querySelector('#status-banner').hidden = true; });
        await page.getByRole('button', { name: 'Request again' }).click();
        await page.waitForSelector('#status-banner:not([hidden])');
        return bannerText(page);
      };
      for (const [code, text] of Object.entries(catalog)) assert.equal(await exercise({ error: { code, message: `Conflict ${hostile}` } }), text, code);
      // A native host sends only the HTTP phrase; unknown, malformed or inherited codes never fall back to host text.
      for (const [name, body] of Object.entries({
        'unknown code': { error: { code: 'brand_new_code', message: 'HOST TEXT' } }, 'no code': { error: { message: 'HOST TEXT' } },
        'uppercase code': { error: { code: 'Job_Limit', message: 'HOST TEXT' } }, 'inherited name': { error: { code: 'constructor', message: 'HOST TEXT' } },
        'prototype name': { error: { code: '__proto__', message: 'HOST TEXT' } }, 'array code': { error: { code: ['job_limit'], message: 'HOST TEXT' } }, 'no error': { message: 'HOST TEXT' },
      })) assert.equal(await exercise(body), 'Request failed (409)', name);
      assert.equal(await page.locator('#status-banner img').count(), 0);
      await clean(page, api);
    });

    await test('failed and interrupted jobs get Request again with a new key, the same file and the same target', async () => {
      const jobs = [failed(hex('a'), { target_ref: ref }), interrupted(hex('b')), failed(hex('c'), { file_id: 102 }), job(hex('d')), job(hex('e'), { status: 'queued', stage: 'requested' }),
        job(hex('f'), { status: 'downloading', stage: 'requested' }), waiting(hex('g'))];
      const { page, api } = await fixture({ jobs });
      await goActivity(page, hex('a'));
      assert.deepEqual(await page.$$eval('[data-request-again]', els => els.map(el => el.dataset.requestAgain)), [hex('a'), hex('b'), hex('c')], 'only failed and interrupted jobs');
      const again = id => card(page, id).getByRole('button', { name: /Request(ed|ing)? again|Requesting/ });
      await again(hex('a')).click();
      await page.getByText('Analysis requested again.').waitFor();
      assert.equal(api.posts.length, 1);
      const first = api.posts[0];
      assert.equal(first.path, '/api/jobs');
      assert.deepEqual(Object.keys(first.body).sort(), ['file_id', 'game', 'idempotency_key', 'kind', 'mod_id', 'target_ref']);
      assert.deepEqual({ ...first.body, idempotency_key: '' }, { kind: 'download_analyze', game: 'zeta', mod_id: 30, file_id: 101, idempotency_key: '', target_ref: ref });
      assert.match(first.body.idempotency_key, /^mb_[a-f0-9]{36}$/);
      assert.equal(first.headers['x-csrf-token'], 'browse-csrf');
      // Once requested the button says so and a second press sends nothing.
      assert.equal(await again(hex('a')).innerText(), 'Requested again');
      assert.equal(await again(hex('a')).getAttribute('aria-disabled'), 'true');
      await again(hex('a')).evaluate(button => button.click()); await page.waitForTimeout(150);
      assert.equal(api.posts.length, 1);
      await again(hex('b')).click(); await page.waitForFunction(() => document.querySelector('[data-request-again="' + 'b'.repeat(32) + '"]').textContent === 'Requested again');
      await again(hex('c')).click(); await page.waitForFunction(() => document.querySelector('[data-request-again="' + 'c'.repeat(32) + '"]').textContent === 'Requested again');
      assert.equal(api.posts.length, 3);
      assert.equal('target_ref' in api.posts[1].body, false, 'a job without a target is repeated without one');
      assert.deepEqual([api.posts[1].body.file_id, api.posts[2].body.file_id], [101, 102]);
      const keys = api.posts.map(post => post.body.idempotency_key);
      assert.equal(new Set(keys).size, 3, 'every request has its own key');
      assert.ok(keys.every(key => /^mb_[a-f0-9]{36}$/.test(key)));
      assert.ok(!JSON.stringify(jobs).includes('idempotency'), 'the public job record carries no key to reuse');
      await noInstall(page);
      await clean(page, api);
    });

    await test('Request again fails closed on records that cannot be repeated safely', async () => {
      const { page, api } = await fixture({ jobs: [
        failed(hex('a'), { target_ref: { ...ref, extra: 1 } }), failed(hex('b'), { target_ref: { snapshot_id: 'z', title_key: ref.title_key } }), failed(hex('c'), { target_ref: null }),
        failed('not-an-opaque-id'), failed(hex('d'), { game: 'bad game!' }), failed(hex('e'), { mod_id: '0' }), failed(hex('f'), { file_id: 1.5 }), failed(hex('1'), { mod_id: 9007199254740993 }),
        failed(hex('2')), ] });
      await goActivity(page, hex('2'));
      assert.deepEqual(await page.$$eval('[data-request-again]', els => els.map(el => el.dataset.requestAgain)), [hex('2')]);
      for (const id of [hex('a'), hex('b'), hex('c'), 'not-an-opaque-id', hex('d'), hex('e'), hex('f'), hex('1')]) assert.equal(await card(page, id).locator('[data-request-again]').count(), 0, id);
      assert.equal(api.posts.length, 0);
      await clean(page, api);
    });

    await test('Request again is disabled without a verified account or download capability', async () => {
      for (const extra of [{ account: { status: 'disconnected' } }, { capabilities: { download_analyze: false } }, { capabilities: { jobs: false } }]) {
        const { page, api, context } = await fixture({ jobs: [failed(hex('a'))], ...extra });
        await page.getByRole('tab', { name: 'Activity' }).click();
        if (extra.capabilities?.jobs === false) { assert.equal(await page.locator('[data-job-id]').count(), 0); }
        else assert.equal(await card(page, hex('a')).locator('[data-request-again]').isDisabled(), true);
        assert.equal(api.posts.length, 0);
        await context.close();
      }
    });

    await test('a failed repeat request shows the catalog text and can be tried again', async () => {
      const { page, api } = await fixture({ jobs: [failed(hex('a'), { target_ref: ref })], reply: () => ({ status: 409, body: { error: { code: 'stale_target_ref', message: 'Conflict' } } }) });
      await goActivity(page, hex('a'));
      await page.getByRole('button', { name: 'Request again' }).click();
      await page.waitForSelector('#status-banner:not([hidden])');
      assert.equal(await bannerText(page), 'The console target changed. Refresh the targets and choose again.');
      assert.equal(await page.getByRole('button', { name: 'Request again' }).innerText(), 'Request again', 'the failure leaves the action available');
      api.reply = undefined;
      await page.getByRole('button', { name: 'Request again' }).click();
      await page.getByText('Analysis requested again.').waitFor();
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'success clears the earlier error');
      await clean(page, api);
    });

    await test('job errors render by code through the catalog, never as host text', async () => {
      const raw = `RAW HOST TEXT ${hostile}`;
      const jobs = [failed(hex('a'), { error: { code: 'job_limit', message: raw } }), failed(hex('b'), { error: { code: 'never_heard_of_it', message: raw } }),
        failed(hex('c'), { error: undefined }), failed(hex('d'), { error: raw }), failed(hex('e'), { error: { code: 'constructor', message: raw } }),
        failed(hex('f'), { error: { code: 'download_failed', message: raw } }), interrupted(hex('1'), { error: undefined }), interrupted(hex('2'), { error: { code: 'interrupted', message: raw } }),
        interrupted(hex('3'), { error: { code: 'zzz_unknown', message: raw } }), failed(hex('4'), { error: { code: 'content_hidden', message: raw } })];
      const catalog = catalogFromSource();
      const { page, api } = await fixture({ jobs });
      await goActivity(page, hex('4'));
      const details = async id => card(page, id).locator('.job-details').innerText();
      assert.match(await details(hex('a')), new RegExp(catalog.job_limit.replace(/[.;]/g, '\\$&')));
      assert.match(await details(hex('f')), new RegExp(catalog.download_failed));
      assert.match(await details(hex('4')), new RegExp(catalog.content_hidden.replace(/[.,]/g, '\\$&')));
      for (const id of [hex('b'), hex('c'), hex('d'), hex('e')]) assert.match(await details(id), new RegExp(generic), id);
      for (const id of [hex('1'), hex('2'), hex('3')]) assert.match(await details(id), /ModBridge stopped before this job finished/, id);
      assert.doesNotMatch(await page.locator('#activity-content').innerText(), /RAW HOST TEXT|Details unavailable|Please try again/);
      assert.equal(await page.locator('#activity-content img').count(), 0);
      await clean(page, api);
    });

    await test('reconnect_required offers Sign in on the banner and on the job, reusing the existing flow', async () => {
      const authStart = post => post.path === '/api/auth/start' ? { status: 200, body: { authorization_url: 'https://users.nexusmods.com/oauth/authorize?fixture=1' } } : undefined;
      // Banner: a request that comes back reconnect_required.
      let fixtureResult = await fixture({ capabilities: { nexus_auth_start: true }, jobs: [failed(hex('a'), { error: { code: 'reconnect_required', message: 'raw' } })],
        reply: post => post.path === '/api/jobs' ? { status: 401, body: { error: { code: 'reconnect_required', message: 'Unauthorized' } } } : authStart(post) });
      let { page, api } = fixtureResult;
      await goActivity(page, hex('a'));
      assert.equal(await card(page, hex('a')).getByRole('button', { name: 'Sign in' }).count(), 1, 'the job card offers Sign in');
      assert.match(await card(page, hex('a')).locator('.job-details').innerText(), /Reconnect your Nexus account to continue\./);
      await page.getByRole('button', { name: 'Request again' }).click();
      await page.waitForSelector('#status-banner:not([hidden])');
      assert.equal(await bannerText(page), 'Reconnect your Nexus account to continue.');
      assert.deepEqual(await page.locator('#status-banner button').allInnerTexts(), ['Sign in', 'Dismiss']);
      // Reachable by arrow keys (controller path) and activated with Cross.
      await page.locator('#tab-activity').focus();
      await page.keyboard.press('ArrowDown');
      assert.match(await active(page), /^banner-(sign-in|dismiss)$/, 'Down from the tabs reaches the banner’s buttons, which sit under its text');
      if ((await active(page)) !== 'banner-sign-in') await page.keyboard.press('ArrowLeft');
      assert.equal(await active(page), 'banner-sign-in');
      await page.keyboard.press('Enter');
      for (let n = 0; n < 40 && !api.outbound.length; n++) await page.waitForTimeout(50);
      assert.deepEqual(api.outbound, ['https://users.nexusmods.com/oauth/authorize?fixture=1']);
      assert.equal(api.posts.filter(post => post.path === '/api/auth/start').length, 1);
      assert.equal(api.posts.find(post => post.path === '/api/auth/start').headers['x-csrf-token'], 'browse-csrf');
      await page.context().close();
      // The job's own button starts the same flow.
      ({ page, api } = await fixture({ capabilities: { nexus_auth_start: true }, jobs: [interrupted(hex('b'), { error: { code: 'credentials', message: 'raw' } })], reply: authStart }));
      await goActivity(page, hex('b'));
      await card(page, hex('b')).getByRole('button', { name: 'Sign in' }).click();
      for (let n = 0; n < 40 && !api.outbound.length; n++) await page.waitForTimeout(50);
      assert.deepEqual(api.outbound, ['https://users.nexusmods.com/oauth/authorize?fixture=1']);
      await page.context().close();
      // Hosts that cannot start a sign-in never offer one; other codes never do.
      ({ page, api } = await fixture({ jobs: [failed(hex('c'), { error: { code: 'reconnect_required', message: 'raw' } })],
        reply: () => ({ status: 401, body: { error: { code: 'reconnect_required', message: 'Unauthorized' } } }) }));
      await goActivity(page, hex('c'));
      await page.getByRole('button', { name: 'Request again' }).click();
      await page.waitForSelector('#status-banner:not([hidden])');
      assert.equal(await page.getByRole('button', { name: 'Sign in' }).count(), 0);
      assert.deepEqual(api.outbound, []);
      await clean(page, api);
    });

    await test('only sign-in codes offer Sign in', async () => {
      const codes = ['reconnect_required', 'credentials', 'authorization', 'authorization_cancelled', 'auth_cancelled', 'job_limit', 'busy', 'download_failed', 'provider', 'download_authorization', 'not_found'];
      const { page, api } = await fixture({ capabilities: { nexus_auth_start: true }, jobs: codes.map((code, index) => failed(String(index + 1).padStart(32, '0'), { error: { code, message: 'raw' } })) });
      await page.getByRole('tab', { name: 'Activity' }).click();
      await page.locator('[data-job-id]').first().waitFor();
      const offered = await page.$$eval('[data-job-id]', cards => cards.filter(el => el.querySelector('[data-job-sign-in]')).length);
      assert.equal(offered, 5);
      await clean(page, api);
    });

    await test('byte counts read as KB, MB or GB while small counts stay exact', async () => {
      const sizes = [[0, '0 bytes'], [1, '1 byte'], [22, '22 bytes'], [1023, '1023 bytes'], [1024, '1 KB (1,024 bytes)'], [1536, '1.5 KB (1,536 bytes)'],
        [5 * 1024 * 1024 + 300000, '5.3 MB (5,542,880 bytes)'], [1073741824, '1 GB (1,073,741,824 bytes)'], [1610612736, '1.5 GB (1,610,612,736 bytes)']];
      const bad = [-1, 1.5, '4096', null, NaN, Infinity, {}, [], true, 2 ** 60];
      const jobs = [...sizes.map(([bytes], index) => job(`size${index}`, { bytes_downloaded: bytes })), ...bad.map((bytes, index) => job(`bad${index}`, { bytes_downloaded: bytes }))];
      const { page, api } = await fixture({ jobs });
      await page.getByRole('tab', { name: 'Activity' }).click();
      await card(page, 'size0').waitFor();
      for (const [index, [, text]] of sizes.entries()) assert.equal((await card(page, `size${index}`).locator('.job-state').nth(1).innerText()).trim(), text);
      for (const index of bad.keys()) assert.equal((await card(page, `bad${index}`).locator('.job-state').nth(1).innerText()).trim(), '', `bad${index} shows no size`);
      await clean(page, api);
    });

    await test('queued jobs show an estimated queue position derived from the job list', async () => {
      const queued = id => job(id, { status: 'queued', stage: 'requested', bytes_downloaded: undefined });
      const { page, api } = await fixture({ jobs: [job(hex('0')), queued(hex('a')), job(hex('1'), { status: 'downloading', stage: 'requested', bytes_downloaded: undefined }), queued(hex('b')), failed(hex('2')), queued(hex('c'))] });
      await goActivity(page, hex('c'));
      const progress = id => card(page, id).locator('.job-progress').innerText();
      assert.equal(await progress(hex('a')), 'Estimated queue position 2 · 1 job ahead');
      assert.equal(await progress(hex('b')), 'Estimated queue position 3 · 2 jobs ahead');
      assert.equal(await progress(hex('c')), 'Estimated queue position 4 · 3 jobs ahead');
      assert.equal(await card(page, hex('0')).locator('.job-progress').count(), 0);
      assert.equal(await card(page, hex('2')).locator('.job-progress').count(), 0);
      // Nothing running: the first queued job is next.
      api.jobs = [queued(hex('a')), queued(hex('b'))];
      await page.waitForFunction(() => document.querySelectorAll('[data-job-id]').length === 2, null, { timeout: 6000 });
      assert.equal(await progress(hex('a')), 'Estimated queue position 1 · next to run');
      assert.equal(await progress(hex('b')), 'Estimated queue position 2 · 1 job ahead');
      await clean(page, api);
    });

    await test('running jobs show elapsed time that ticks in place without re-rendering the card', async () => {
      const running = status => job(hex('a'), { status, stage: 'requested', bytes_downloaded: undefined });
      const { page, api } = await fixture({ clock: true, jobs: [running('downloading')] });
      await goActivity(page, hex('a'));
      const text = () => page.locator('[data-elapsed-job]').innerText();
      // waitForFunction polls with requestAnimationFrame, which the page's fake clock holds back.
      const until = async check => { for (let n = 0; n < 120; n++) { if (await page.evaluate(check)) return; await new Promise(resolve => setTimeout(resolve, 50)); } assert.fail('condition not reached'); };
      assert.match(await text(), /^0:0[0-2]$/);
      assert.match(await card(page, hex('a')).locator('.job-progress').innerText(), /Running for 0:0\d · timed by this page from when it first saw the job running\. The host reports no download progress\./);
      assert.equal(await page.locator('[data-elapsed-job]').getAttribute('aria-live'), 'off', 'a ticking clock is not announced');
      await page.evaluate(() => { window.__card = document.querySelector('[data-job-id]'); });
      await page.clock.fastForward(65000);
      await until(() => /^1:0[5-9]$/.test(document.querySelector('[data-elapsed-job]').textContent));
      assert.equal(await page.evaluate(() => window.__card === document.querySelector('[data-job-id]')), true, 'the card was not rebuilt');
      // Moving on to analyzing keeps the same clock; a finished job shows none.
      api.jobs = [running('analyzing')];
      await page.clock.runFor(2600);
      await until(() => document.querySelector('[data-job-id] .job-pill .pill-label')?.textContent === 'Analyzing archive');
      assert.match(await text(), /^1:(0[5-9]|1\d)$/, 'the clock continues across running states instead of restarting');
      api.jobs = [job(hex('a'))];
      await page.clock.runFor(2600);
      await until(() => !document.querySelector('[data-elapsed-job]'));
      assert.equal(await page.locator('.job-progress').count(), 0);
      await clean(page, api);
    });

    await test('a job that waits for a Nexus link offers a paste field and resumes with it', async () => {
      const link = nxmLink();
      const { page, api } = await fixture({ jobs: [waiting(hex('a'), { game: 'zeta', mod_id: 30, file_id: 101 })] });
      api.reply = post => {
        if (post.path !== `/api/jobs/${hex('a')}/resume`) return undefined;
        api.jobs = [job(hex('a'), { status: 'queued', stage: 'requested', bytes_downloaded: undefined })];
        return { status: 202, body: api.jobs[0] };
      };
      await goActivity(page, hex('a'));
      const input = page.getByLabel('Nexus download link');
      assert.equal(await input.getAttribute('type'), 'text');
      assert.match(await card(page, hex('a')).innerText(), /choose “Download with manager”, copy the nxm:\/\/ link it offers, and paste it here/);
      await input.fill(`  ${link}\n`);
      await card(page, hex('a')).getByRole('button', { name: 'Continue download' }).click();
      await page.getByText('Download requested with your Nexus link.').waitFor();
      assert.equal(api.posts.length, 1);
      assert.equal(api.posts[0].path, `/api/jobs/${hex('a')}/resume`);
      assert.deepEqual(api.posts[0].body, { nxm: link }, 'the trimmed link and nothing else');
      assert.equal(api.posts[0].headers['x-csrf-token'], 'browse-csrf');
      // The job requeued, so the form is gone, and the link is nowhere in the page or its storage.
      await page.waitForFunction(() => !document.querySelector('[data-nxm-form]'), null, { timeout: 6000 });
      const leaks = await page.evaluate(() => JSON.stringify([document.documentElement.outerHTML, Object.entries(localStorage), Object.entries(sessionStorage), document.cookie]));
      assert.ok(!leaks.includes('secret-grant-key') && !leaks.includes('nxm://zeta'), 'the pasted link is not retained');
      assert.equal(await card(page, hex('a')).locator('[data-request-again]').count(), 0);
      await clean(page, api);
    });

    await test('the paste field checks game, mod, file, expiry and shape before anything is sent', async () => {
      const { page, api } = await fixture({ jobs: [waiting(hex('a'))] });
      await goActivity(page, hex('a'));
      const input = page.getByLabel('Nexus download link'), status = card(page, hex('a')).locator('.nxm-status');
      const submit = async value => { await input.fill(value); await card(page, hex('a')).getByRole('button', { name: 'Continue download' }).click(); await status.waitFor(); return (await status.innerText()).trim(); };
      const invalid = 'That is not a Nexus link. Copy the nxm:// link from “Download with manager” again.';
      const other = 'That link is for a different file. Use the link from this job’s Nexus file page.';
      const expired = 'That link has expired. Open the Nexus file page and copy a new one.';
      const ok = nxmLink({ key: 'k1' });
      const cases = {
        'other game': [nxmLink({ game: 'other' }), other], 'other mod': [nxmLink({ mod: 31 }), other], 'other file': [nxmLink({ file: 102 }), other],
        'expired': [nxmLink({ expires: 1000 }), expired], 'just expired': [nxmLink({ expires: Math.floor(Date.now() / 1000) - 1 }), expired],
        'empty': ['', invalid], 'https link': [ok.replace('nxm://', 'https://'), invalid], 'upper-case scheme': [ok.replace('nxm://', 'NXM://'), invalid],
        'no query': ['nxm://zeta/mods/30/files/101', invalid], 'extra parameter': [ok + '&extra=1', invalid], 'duplicate parameter': [ok + '&key=again', invalid],
        'missing user': [ok.replace(/&user_id=42/, ''), invalid], 'blank key': [nxmLink({ key: '' }), invalid], 'zero expiry': [nxmLink({ expires: 0 }), invalid],
        'leading-zero mod': [ok.replace('/mods/30/', '/mods/030/'), invalid], 'fragment': [ok + '#x', invalid], 'inner space': [ok.replace('k1', 'k 1'), invalid],
        'collection link': ['nxm://zeta/collections/abc/revisions/1', invalid], 'path trick': [ok.replace('/files/101', '/files/101/../102'), invalid],
        'huge': [ok + '&' + 'x'.repeat(5000), invalid], 'html': [hostile, invalid],
      };
      for (const [name, [value, message]] of Object.entries(cases)) {
        assert.equal(await submit(value), message, name);
        assert.ok(!(await status.innerText()).includes('k1'), 'the message never echoes the link');
        assert.equal(await input.getAttribute('aria-invalid'), 'true');
      }
      assert.equal(api.posts.length, 0, 'nothing was sent');
      assert.equal(await page.locator('#activity-content img').count(), 0);
      await clean(page, api);
    });

    await test('a refused resume keeps the draft and shows catalog text; the next try can succeed', async () => {
      const { page, api } = await fixture({ jobs: [waiting(hex('a'))] });
      const link = nxmLink({ key: 'draftkey' });
      let attempts = 0;
      api.reply = post => {
        if (!post.path.endsWith('/resume')) return undefined;
        attempts += 1;
        if (attempts === 1) return { status: 400, body: { error: { code: 'download_authorization', message: 'raw host text' } } };
        if (attempts === 2) return { status: 403, body: { error: { code: 'auth_refused', message: 'Forbidden' } } };
        api.jobs = [job(hex('a'), { status: 'queued', stage: 'requested', bytes_downloaded: undefined })]; return { status: 202, body: api.jobs[0] };
      };
      await goActivity(page, hex('a'));
      const input = page.getByLabel('Nexus download link'), status = card(page, hex('a')).locator('.nxm-status');
      await input.fill(link);
      await input.press('Enter');
      await page.waitForFunction(() => document.querySelector('.nxm-status').textContent.length > 0);
      assert.equal((await status.innerText()).trim(), catalogFromSource().download_authorization);
      assert.equal(await input.inputValue(), link, 'the draft stays for a retry');
      assert.equal(await page.evaluate(() => document.activeElement.id), `nxm-${hex('a')}`, 'focus stays in the field');
      await input.press('Enter');
      await page.waitForFunction(() => document.querySelector('.nxm-status').textContent.startsWith('This action is not allowed right now'));
      await input.press('Enter');
      await page.getByText('Download requested with your Nexus link.').waitFor();
      assert.equal(api.posts.length, 3);
      assert.equal(api.posts.every(post => post.body.nxm === link), true);
      assert.ok(!JSON.stringify(await page.evaluate(() => [localStorage, sessionStorage])).includes('draftkey'));
      await clean(page, api);
    });

    await test('the pasted draft, focus and caret survive a job refresh and a running clock', async () => {
      const { page, api } = await fixture({ jobs: [waiting(hex('a')), job(hex('b'), { status: 'downloading', stage: 'requested', bytes_downloaded: undefined })] });
      await goActivity(page, hex('a'));
      const input = page.getByLabel('Nexus download link');
      await input.focus();
      await page.keyboard.type('nxm://zeta/mo');
      await page.evaluate(() => { const field = document.activeElement; field.setSelectionRange(5, 5); });
      api.jobs = [waiting(hex('a')), job(hex('b'), { status: 'analyzing', stage: 'downloaded', bytes_downloaded: 4096, sha256: 'c'.repeat(64) })];
      await page.getByText('4 KB (4,096 bytes)').waitFor({ timeout: 6000 });
      assert.equal(await input.inputValue(), 'nxm://zeta/mo');
      assert.deepEqual(await page.evaluate(() => [document.activeElement.id, document.activeElement.selectionStart, document.activeElement.selectionEnd]), [`nxm-${hex('a')}`, 5, 5]);
      // A draft belongs to a waiting job: once the job moves on it is dropped.
      api.jobs = [job(hex('a'), { status: 'queued', stage: 'requested', bytes_downloaded: undefined }), job(hex('b'))];
      await page.waitForFunction(() => !document.querySelector('[data-nxm-form]'), null, { timeout: 6000 });
      api.jobs = [waiting(hex('a')), job(hex('b'))];
      await page.waitForFunction(() => document.querySelector('[data-nxm-form]'), null, { timeout: 6000 });
      assert.equal(await page.getByLabel('Nexus download link').inputValue(), '', 'an old draft is not carried to a new wait');
      await clean(page, api);
    });

    await test('the paste field and its button are reachable with arrows and the D-pad, and release focus on Up and Down', async () => {
      const { page, api } = await fixture({ jobs: [waiting(hex('a'))] });
      api.reply = post => post.path.endsWith('/resume') ? { status: 202, body: job(hex('a'), { status: 'queued', stage: 'requested' }) } : undefined;
      await goActivity(page, hex('a'));
      await card(page, hex('a')).getByRole('link', { name: 'Open Nexus file page' }).focus();
      await page.keyboard.press('ArrowDown');
      assert.equal(await page.evaluate(() => document.activeElement.id), `nxm-${hex('a')}`, 'the field follows the link');
      await page.keyboard.type('abc');
      assert.equal(await page.getByLabel('Nexus download link').inputValue(), 'abc', 'typing stays in the field');
      await page.keyboard.press('ArrowDown');
      assert.equal(await active(page), 'Continue download', 'Down leaves the field');
      await page.keyboard.press('ArrowUp');
      assert.equal(await page.evaluate(() => document.activeElement.id), `nxm-${hex('a')}`);
      await page.keyboard.press('ArrowUp');
      assert.equal(await active(page), 'Open Nexus file page', 'Up leaves the field too');
      // D-pad: same order; Cross on the button submits the (valid) link.
      await page.getByLabel('Nexus download link').fill(nxmLink());
      await card(page, hex('a')).getByRole('link', { name: 'Open Nexus file page' }).focus();
      await press(page, DOWN); assert.equal(await page.evaluate(() => document.activeElement.id), `nxm-${hex('a')}`);
      await press(page, DOWN); assert.equal(await active(page), 'Continue download');
      await press(page, CROSS);
      await page.getByText('Download requested with your Nexus link.').waitFor();
      assert.equal(api.posts.length, 1);
      assert.equal(await page.evaluate(() => document.activeElement.dataset.requestAgain), undefined);
      await clean(page, api);
    });

    await test('the paste field is withheld for waiting jobs whose identity cannot be verified', async () => {
      const { page, api } = await fixture({ jobs: [waiting('legacy'), waiting(hex('a'), { game: 'bad game!' }), waiting(hex('b'), { mod_id: 0 }), waiting(hex('c'), { file_id: 'x' }), waiting(hex('d'))] });
      await goActivity(page, hex('d'));
      assert.deepEqual(await page.$$eval('[data-nxm-form]', forms => forms.map(form => form.dataset.nxmForm)), [hex('d')]);
      assert.equal(await card(page, 'legacy').locator('input').count(), 0);
      assert.equal(await card(page, hex('a')).getByText(/Nexus file page is unavailable/).count(), 1);
      assert.equal(api.posts.length, 0);
      await clean(page, api);
    });

    await test('no new control claims an install, PS5 compatibility or an in-game test', async () => {
      const jobs = [failed(hex('a')), interrupted(hex('b')), waiting(hex('c')), job(hex('d'), { status: 'queued', stage: 'requested' }), job(hex('e'), { status: 'downloading', stage: 'requested' }), job(hex('f'), { compatibility_assessment: compat('unknown') })];
      const { page, api } = await fixture({ jobs, capabilities: { nexus_auth_start: true } });
      await goActivity(page, hex('f'));
      await noInstall(page);
      const labels = await page.$$eval('#activity-content button, #activity-content a, #activity-content input', els => els.map(el => (el.textContent || el.getAttribute('placeholder') || '').trim()));
      assert.ok(labels.every(label => !/install|compatible|tested|works/i.test(label)), labels.join(' | '));
      await clean(page, api);
    });

    // ---- Newer-file check (docs/INSTALL-FLOW-UX.md 3.7) ----
    // A fake of GET /api/jobs/{id}/update: records the requests, may hold the reply back with `gate`.
    const updateApi = () => {
      const fake = { requests: [], reply: null, gate: null };
      fake.get = async url => {
        const match = /^\/api\/jobs\/([^/]+)\/update$/.exec(url.pathname);
        if (!match) return undefined;
        fake.requests.push(match[1]);
        if (fake.gate) await fake.gate;
        return typeof fake.reply === 'function' ? fake.reply(match[1]) : fake.reply;
      };
      return fake;
    };
    const newerReply = (extra = {}, steps = 1) => ({ status: 200, body: { status: 'newer', steps, latest: { file_id: 105, name: 'Main file v2', version: '2.0', category_name: 'UPDATE', uploaded_timestamp: 1710000000, ...extra } } });
    const currentReply = { status: 200, body: { status: 'current', steps: 0, latest: { file_id: 101, name: 'Main file', version: '1.0', category_name: 'MAIN', uploaded_timestamp: 1700000000 } } };
    const unknownReply = { status: 200, body: { status: 'unknown', steps: 0, latest: null } };
    const focusKey = page => page.evaluate(() => document.activeElement?.dataset?.focusKey || '');
    const updateText = (page, id) => card(page, id).locator('.job-update').innerText();

    await test('Check for newer file is hidden when the host does not advertise file_updates', async () => {
      for (const capabilities of [{ file_updates: false }, { file_updates: 'yes' }]) {
        const { page, context } = await fixture({ jobs: [job(hex('a')), failed(hex('b'))], capabilities });
        await goActivity(page, hex('a'));
        assert.equal(await page.locator('[data-update-check]').count(), 0, JSON.stringify(capabilities));
        await context.close();
      }
    });

    await test('Check for newer file is offered on analyzed, failed and interrupted jobs with valid ids only', async () => {
      const jobs = [job(hex('a')), failed(hex('b')), interrupted(hex('c')), job(hex('d'), { status: 'queued', stage: 'requested' }), job(hex('e'), { status: 'downloading', stage: 'requested' }),
        job(hex('f'), { status: 'analyzing', stage: 'downloaded' }), waiting(hex('1')), job(hex('2'), { mod_id: 0 }), job(hex('3'), { file_id: '12x' }), job('not-an-opaque-id'), job(hex('4'), { game: 'bad game!' })];
      const fake = updateApi();
      const { page, api } = await fixture({ jobs, get: fake.get });
      await goActivity(page, hex('a'));
      assert.deepEqual(await page.$$eval('[data-update-check]', els => els.map(el => el.dataset.updateCheck)), [hex('a'), hex('b'), hex('c')]);
      assert.deepEqual(await page.$$eval('[data-update-check]', els => els.map(el => el.textContent)), ['Check for newer file', 'Check for newer file', 'Check for newer file']);
      assert.equal(await page.locator('.job-update .update-line').count(), 0, 'no verdict before the button is pressed');
      assert.deepEqual(fake.requests, [], 'nothing is requested until the button is pressed');
      await noInstall(page);
      await clean(page, api);
    });

    await test('a newer linked file shows its name and version, offers a new job for it and a Nexus link, and keeps focus and the result across refreshes', async () => {
      const fake = updateApi(); fake.reply = newerReply();
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      const { page, api } = await fixture({ jobs: [job(hex('a'), { target_ref: ref }), job(hex('b'), { file_id: 102, mod_id: 12 })], get: fake.get });
      await goActivity(page, hex('a'));
      const box = card(page, hex('a'));
      const check = box.locator('[data-update-check]');
      // Reached like every other control: by the D-pad, then pressed with Cross.
      await page.locator('#tab-activity').focus();
      for (let n = 0; n < 12 && (await focusKey(page)) !== 'update-check'; n++) await press(page, DOWN);
      assert.equal(await focusKey(page), 'update-check');
      assert.equal(await page.evaluate(() => document.activeElement.closest('[data-job-id]').dataset.jobId), hex('a'));
      await press(page, CROSS);
      // In flight: disabled, labelled, focus stays on the pressed control through the re-render, and a second press sends nothing.
      await page.waitForFunction(() => document.activeElement?.textContent === 'Checking…');
      assert.equal(await check.isDisabled(), true);
      assert.equal(await focusKey(page), 'update-check');
      await check.evaluate(button => button.click()); await page.waitForTimeout(100);
      assert.deepEqual(fake.requests, [hex('a')], 'one request in flight');
      release();
      const line = box.locator('.update-line');
      await line.waitFor();
      assert.equal(await line.innerText(), 'Newer file linked by the author: Main file v2 (version 2.0)');
      assert.equal(await box.locator('.update-result').getAttribute('data-update-status'), 'newer');
      assert.match(await box.locator('.update-meta').innerText(), /^Update · Uploaded Mar 9, 2024$/);
      assert.equal(await check.innerText(), 'Check for newer file');
      assert.equal(await check.isDisabled(), false);
      assert.equal(await focusKey(page), 'update-check', 'focus stays on the pressed control after the result arrives');
      assert.equal(await page.locator('#announcer').textContent(), 'Newer file linked by the author: Main file v2 (version 2.0)', 'the result is announced');
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'a verdict is not an error');
      const link = box.getByRole('link', { name: 'Open newer file on Nexus' });
      assert.equal(await link.getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/30?tab=files&file_id=105');
      assert.equal(await link.getAttribute('target'), '_blank');
      assert.equal(await link.getAttribute('rel'), 'noopener noreferrer');
      // Another job's card is untouched.
      assert.equal(await card(page, hex('b')).locator('.update-line').count(), 0);
      // A job-list refresh rebuilds the card: the result and focus survive it.
      await page.evaluate(() => { window.__card = document.querySelector('[data-job-id]'); });
      api.jobs[0] = { ...api.jobs[0], bytes_downloaded: 4096 };
      await page.waitForFunction(() => window.__card !== document.querySelector('[data-job-id]'), null, { timeout: 8000 });
      assert.equal(await line.innerText(), 'Newer file linked by the author: Main file v2 (version 2.0)');
      assert.equal(await focusKey(page), 'update-check');
      // The newer file becomes a new job exactly like Request again: new key, same game, mod and target, only the file differs.
      const download = box.locator('[data-download-newer]');
      assert.equal(await download.innerText(), 'Download newer file for analysis');
      await download.focus();
      await press(page, CROSS);
      await page.getByText('Analysis requested for the newer file.').waitFor();
      assert.equal(api.posts.length, 1);
      assert.equal(api.posts[0].path, '/api/jobs');
      assert.deepEqual({ ...api.posts[0].body, idempotency_key: '' }, { kind: 'download_analyze', game: 'zeta', mod_id: 30, file_id: 105, idempotency_key: '', target_ref: ref });
      assert.match(api.posts[0].body.idempotency_key, /^mb_[a-f0-9]{36}$/);
      assert.equal(api.posts[0].headers['x-csrf-token'], 'browse-csrf');
      assert.equal(await download.innerText(), 'Newer file requested');
      assert.equal(await download.getAttribute('aria-disabled'), 'true');
      assert.equal(await focusKey(page), 'update-download', 'focus stays on the pressed control');
      await download.evaluate(button => button.click()); await page.waitForTimeout(100);
      assert.equal(api.posts.length, 1, 'a second press sends nothing');
      // Request again on the job's own file would use a different, older key and file: nothing else was posted.
      assert.equal(fake.requests.length, 1);
      await noInstall(page);
      await clean(page, api);
    });

    await test('current, unknown and a failed check read as stated and never as up to date', async () => {
      const fake = updateApi();
      const { page, api } = await fixture({ jobs: [job(hex('a')), failed(hex('b'))], get: fake.get });
      await goActivity(page, hex('a'));
      const press_ = async (id, reply) => { fake.reply = reply; await card(page, id).getByRole('button', { name: 'Check for newer file' }).click(); await card(page, id).locator('.update-line').waitFor(); };
      await press_(hex('a'), currentReply);
      assert.equal(await card(page, hex('a')).locator('.update-line').innerText(), 'No newer file linked by the author.');
      assert.equal(await card(page, hex('a')).locator('.update-line').getAttribute('data-update-status'), 'current');
      assert.equal(await card(page, hex('a')).getByRole('button', { name: /Download newer/ }).count(), 0);
      assert.equal(await card(page, hex('a')).getByRole('link', { name: /newer file/ }).count(), 0);
      assert.equal(await page.locator('#announcer').textContent(), 'No newer file linked by the author.');
      // The same control checks again: unknown replaces current.
      await press_(hex('a'), unknownReply);
      await page.waitForFunction(() => document.querySelector('[data-job-id] .update-line').dataset.updateStatus === 'unknown');
      assert.equal(await card(page, hex('a')).locator('.update-line').innerText(), 'Could not tell whether a newer file exists.');
      // A failed check shows the catalog text, never host text, and never a verdict.
      await press_(hex('b'), { status: 429, body: { error: { code: 'rate_limit', message: `HOST TEXT ${hostile}` } } });
      assert.equal(await card(page, hex('b')).locator('.update-line').innerText(), 'Could not check for updates. Nexus asked ModBridge to slow down. Wait a minute and try again.');
      assert.equal(await card(page, hex('b')).locator('.update-line').getAttribute('data-update-status'), 'failed');
      await press_(hex('b'), { status: 404, body: { error: { code: 'not_found', message: 'Not Found' } } });
      await page.waitForFunction(() => /no longer exists/.test(document.querySelector('[data-job-id="' + 'b'.repeat(32) + '"] .update-line').textContent));
      assert.equal(await card(page, hex('b')).locator('.update-line').innerText(), 'Could not check for updates. That item no longer exists. Refresh the list.');
      await press_(hex('b'), { status: 500, body: { error: { code: 'brand_new_code', message: 'HOST TEXT' } } });
      await page.waitForFunction(() => /Request failed/.test(document.querySelector('[data-job-id="' + 'b'.repeat(32) + '"] .update-line').textContent));
      assert.equal(await card(page, hex('b')).locator('.update-line').innerText(), 'Could not check for updates. Request failed (500)');
      assert.equal(await card(page, hex('b')).locator('.update-line').getAttribute('data-update-status'), 'failed');
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'a failed check is shown inline, not as a banner');
      // Whatever happened, nothing on the page claims the file is up to date or the latest.
      assert.doesNotMatch(await page.locator('#activity-content').innerText(), /up to date|up-to-date|\blatest\b|current version|newest/i);
      assert.doesNotMatch(await page.locator('#activity-content').innerText(), /HOST TEXT/);
      await clean(page, api);
    });

    await test('a newer file name, version and category are shown as text, never parsed; missing ones read plainly', async () => {
      const fake = updateApi();
      const { page, api } = await fixture({ jobs: [job(hex('a')), job(hex('b'))], get: fake.get });
      await goActivity(page, hex('a'));
      fake.reply = newerReply({ name: hostile, version: '<b>2</b>&', category_name: '<script>window.__pwned=2</script>', uploaded_timestamp: null });
      await card(page, hex('a')).getByRole('button', { name: 'Check for newer file' }).click();
      await card(page, hex('a')).locator('.update-line').waitFor();
      assert.equal(await card(page, hex('a')).locator('.update-line').innerText(), `Newer file linked by the author: ${hostile} (version <b>2</b>&)`);
      assert.equal(await card(page, hex('a')).locator('.update-meta').innerText(), '<script>window.__pwned=2</script>');
      assert.equal(await card(page, hex('a')).locator('.job-update img, .job-update script, .job-update b').count(), 0);
      fake.reply = newerReply({ name: null, version: null, category_name: null, uploaded_timestamp: null });
      await card(page, hex('b')).getByRole('button', { name: 'Check for newer file' }).click();
      await card(page, hex('b')).locator('.update-line').waitFor();
      assert.equal(await card(page, hex('b')).locator('.update-line').innerText(), 'Newer file linked by the author: Unnamed file (version unavailable)');
      assert.equal(await card(page, hex('b')).locator('.update-meta').count(), 0);
      await clean(page, api);
    });

    await test('a reply that is not exactly the documented shape is no answer: the check reads as failed', async () => {
      const good = () => structuredClone(newerReply().body);
      const mutate = change => { const value = good(); const result = change(value); return result === undefined ? value : result; };
      const bad = {
        'extra top-level key': mutate(v => { v.extra = 1; }), 'missing steps': mutate(v => { delete v.steps; }), 'missing latest': mutate(v => { delete v.latest; }),
        'unknown status': mutate(v => { v.status = 'latest'; }), 'status not a string': mutate(v => { v.status = 1; }), 'inherited status': mutate(v => { v.status = 'constructor'; }),
        'extra latest key': mutate(v => { v.latest.url = 'https://example.test'; }), 'missing latest key': mutate(v => { delete v.latest.version; }),
        'file_id zero': mutate(v => { v.latest.file_id = 0; }), 'file_id string': mutate(v => { v.latest.file_id = '105'; }), 'file_id fractional': mutate(v => { v.latest.file_id = 1.5; }),
        'file_id too large for a browser': mutate(v => { v.latest.file_id = 9007199254740993; }), 'file_id negative': mutate(v => { v.latest.file_id = -105; }),
        'name too long': mutate(v => { v.latest.name = 'n'.repeat(1025); }), 'name a number': mutate(v => { v.latest.name = 7; }), 'version too long': mutate(v => { v.latest.version = 'v'.repeat(257); }),
        'version an object': mutate(v => { v.latest.version = {}; }), 'category too long': mutate(v => { v.latest.category_name = 'c'.repeat(257); }), 'category a number': mutate(v => { v.latest.category_name = 3; }),
        'timestamp zero': mutate(v => { v.latest.uploaded_timestamp = 0; }), 'timestamp negative': mutate(v => { v.latest.uploaded_timestamp = -5; }), 'timestamp string': mutate(v => { v.latest.uploaded_timestamp = '1710000000'; }),
        'timestamp fractional': mutate(v => { v.latest.uploaded_timestamp = 1.5; }), 'steps negative': mutate(v => { v.steps = -1; }), 'steps too many': mutate(v => { v.steps = 65; }),
        'steps a string': mutate(v => { v.steps = '1'; }), 'steps fractional': mutate(v => { v.steps = 1.5; }), 'newer with no steps': mutate(v => { v.steps = 0; }),
        'newer that is the job’s own file': mutate(v => { v.latest.file_id = 101; }), 'newer with no latest': mutate(v => { v.latest = null; }),
        'current with steps': mutate(v => { v.status = 'current'; v.latest.file_id = 101; v.steps = 1; }), 'current at another file': mutate(v => { v.status = 'current'; v.steps = 0; }),
        'unknown with a latest': mutate(v => { v.status = 'unknown'; v.steps = 0; }), 'latest an array': mutate(v => { v.latest = [105]; }),
        array: [good()], string: 'newer', number: 5, null: null,
      };
      const fake = updateApi();
      const { page, api } = await fixture({ jobs: [job(hex('a'))], get: fake.get });
      await goActivity(page, hex('a'));
      const button = card(page, hex('a')).getByRole('button', { name: 'Check for newer file' });
      for (const [name, body] of Object.entries(bad)) {
        fake.reply = { status: 200, body }; const before = fake.requests.length;
        await button.click();
        for (let n = 0; n < 60 && fake.requests.length === before; n++) await page.waitForTimeout(25);
        await page.waitForFunction(() => document.querySelector('.update-line')?.dataset.updateStatus === 'failed' && !document.querySelector('[data-update-check][aria-disabled]'));
        assert.equal(await card(page, hex('a')).locator('.update-line').innerText(), 'Could not check for updates. The host sent a reply this page could not verify.', name);
        assert.equal(await card(page, hex('a')).getByRole('button', { name: /Download newer/ }).count(), 0, name);
      }
      // The same checks accept the good reply.
      fake.reply = { status: 200, body: good() };
      await button.click();
      await card(page, hex('a')).locator('.update-result').waitFor();
      assert.match(await card(page, hex('a')).locator('.update-line').innerText(), /^Newer file linked by the author: Main file v2/);
      await clean(page, api);
    });

    await test('a check that finishes after its job left the list leaves nothing behind', async () => {
      const fake = updateApi(); fake.reply = newerReply();
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      const { page, api } = await fixture({ jobs: [job(hex('a'))], get: fake.get });
      await goActivity(page, hex('a'));
      await card(page, hex('a')).getByRole('button', { name: 'Check for newer file' }).click();
      api.jobs = [job(hex('b'))];
      await card(page, hex('b')).waitFor({ timeout: 8000 });
      release(); await page.waitForTimeout(200);
      assert.equal(await card(page, hex('b')).locator('.update-line').count(), 0);
      assert.equal(await page.locator('.update-line').count(), 0);
      await clean(page, api);
    });

    // ---- Installed view: plain text for runtime states and blocker codes (section 7, item 9) ----
    const serviceFixture = (mods, games) => ({ capabilities: { mod_service_records: true }, get: url => url.pathname === '/api/mod-service' ? { body: { ok: true, result: { contract: 'modforge-service/1' } } } : url.pathname === '/api/mod-service/records' ? { body: { ok: true, result: { mods, games } } } : undefined });
    const gameRecord = (title, state, detail) => ({ title_id: title, installed: { mods: [], generation: 0, load_list_sha256: 'a'.repeat(64) }, runtime: { state, ...(detail === undefined ? {} : { detail }) } });
    const preparedMod = blockers => ({ download_id: 'dl-1', title_id: 'PPSA00001', state: 'prepared', source: { name: 'Fixture mod' }, blockers });

    await test('Installed shows every runtime state as plain text, never the raw state', async () => {
      const states = { 'not-started': 'Not started', activated: 'Enabled', 'engine-ready': 'Engine ready', 'mods-started': 'Mods started (loader report)', failed: 'Loader failed', stopped: 'Game stopped' };
      const { page, api } = await fixture({ ...serviceFixture([], Object.keys(states).map((state, index) => gameRecord(`PPSA0000${index}`, state, state === 'failed' ? `<img src=x onerror=window.__pwned=1> line one\nline two` : undefined))) });
      await page.getByRole('tab', { name: 'Installed' }).click();
      await page.locator('#installed-content .card').first().waitFor();
      const cards = await page.locator('#installed-content .card').allInnerTexts();
      assert.equal(cards.length, 6);
      for (const [index, [raw, label]] of Object.entries(states).entries()) {
        assert.match(cards[index], new RegExp(`Runtime report: ${label.replace(/[()]/g, '\\$&')} · Live game process unverified`), raw);
        if (raw.includes('-')) assert.doesNotMatch(cards[index], new RegExp(raw), `${raw} is not shown raw`);
      }
      assert.doesNotMatch(cards.join('\n'), /\brunning\b|\bactivated\b/i, 'no raw token, and nothing claims the game runs');
      assert.doesNotMatch(cards.join('\n'), /not-started|engine-ready|mods-started/);
      // A host detail is a quieter parenthesised clause, escaped, on one line.
      assert.match(cards[4], /\(<img src=x onerror=window\.__pwned=1> line one line two\)/);
      assert.equal(await page.locator('#installed-content img').count(), 0);
      assert.equal(await page.locator('#installed-content .installed-detail').count(), 1);
      const source = fs.readFileSync(path.join(webRoot, 'app.js'), 'utf8');
      assert.match(source, /const runtimeStateText = Object\.freeze\(/);
      await clean(page, api);
    });

    await test('Installed shows blocker codes as catalog sentences; unknown codes read one generic sentence and never raw text', async () => {
      const known = ['unresolved', 'additional_action_required', 'no_single_preparation_handler', 'preparation_limit', 'stopped', 'refused', 'storage_failed', 'incoming_mod_overlap',
        'source_identity', 'chunk_id', 'package_id', 'directory_path', 'generation_refused', 'source_identity_or_limit_refused', 'preparation_stopped', 'preparation_refused',
        'windows_runtime_unavailable', 'managed_runtime_unavailable', 'missing_backend', 'installation_conflict'];
      const unknown = ['totally_new_code', 'constructor', '__proto__', 'Preparation_Limit', ''];
      const blockers = [...known.map(code => ({ code, message: code === 'preparation_limit' ? 'over by 3 MiB <b>x</b>' : '' })), ...unknown.map(code => ({ code, message: `RAW HOST TEXT ${hostile}` }))];
      // The validator needs a non-empty code: the empty one is refused with the whole record, so it is tested apart below.
      const accepted = blockers.filter(blocker => blocker.code);
      const { page, api } = await fixture({ ...serviceFixture([preparedMod(accepted)], []) });
      await page.getByRole('tab', { name: 'Installed' }).click();
      await page.locator('.installed-blocker').first().waitFor();
      const lines = await page.locator('.installed-blocker').allInnerTexts();
      assert.equal(lines.length, known.length + unknown.length - 1);
      const generic = 'Preparation is blocked for a reason this page does not recognize.';
      assert.equal(lines.filter(line => line === generic).length, unknown.length - 1, 'one generic sentence per unknown code');
      assert.equal(lines.filter(line => line !== generic).length, known.length, 'every known code has its own sentence');
      assert.equal(new Set(lines).size, known.length + 1, 'every known sentence differs, plus the one generic sentence');
      const text = await page.locator('#installed-content').innerText();
      // Single words ("refused") also occur inside plain sentences, so only the snake_case codes and the unknown ones are searched for.
      for (const code of [...known.filter(code => code.includes('_')), ...unknown.filter(Boolean)]) assert.equal(text.includes(code), false, `${code} is never shown`);
      for (const code of known) assert.equal(lines.includes(code), false, `${code} is never a whole line`);
      assert.doesNotMatch(text, /RAW HOST TEXT/);
      assert.match(text, /\(over by 3 MiB <b>x<\/b>\)/, 'a known code keeps the host message as a quieter clause');
      assert.match(text, /Preparation blocked/);
      assert.equal(await page.locator('#installed-content img, #installed-content b').count(), 0);
      const source = fs.readFileSync(path.join(webRoot, 'app.js'), 'utf8');
      assert.match(source, /const blockerText = Object\.freeze\(/);
      await clean(page, api);
    });

    await test('Installed refuses a record with an unknown runtime state or a blocker without a code, as before', async () => {
      for (const records of [[[], [gameRecord('PPSA00001', 'running')]], [[preparedMod([{ code: '', message: 'x' }])], []]]) {
        const { page, api } = await fixture({ ...serviceFixture(...records) });
        await page.getByRole('tab', { name: 'Installed' }).click();
        await page.getByText('Service records unavailable', { exact: true }).waitFor();
        assert.doesNotMatch(await page.locator('#installed-content').innerText(), /running|Runtime report|Loader failed/);
        await page.context().close();
      }
    });

    // ---- Accessibility (section 7, item 13) ----
    const forcePoll = async (page, api) => {
      // Visiting a tab reads the job list at once; wait until that request has come back.
      const count = () => api.requests.filter(request => request === 'GET /api/jobs').length, before = count();
      await page.getByRole('tab', { name: 'Games' }).click(); await page.getByRole('tab', { name: 'Activity' }).click();
      for (let n = 0; n < 100 && count() === before; n++) await page.waitForTimeout(25);
      await page.waitForTimeout(120);
    };
    const watch = (page, selector) => page.evaluate(selector => { window.__seen = []; new MutationObserver(() => window.__seen.push(document.querySelector(selector).textContent)).observe(document.querySelector(selector), { childList: true, subtree: true, characterData: true }); }, selector);
    const seen = page => page.evaluate(() => window.__seen);

    await test('no list is aria-live: one visually hidden status announcer replaces them', async () => {
      const { page, api } = await fixture({ jobs: [job(hex('a')), failed(hex('b'))] });
      await goActivity(page, hex('a'));
      for (const id of ['#games-content', '#activity-content', '#installed-content']) {
        assert.equal(await page.locator(id).evaluate(el => el.hasAttribute('aria-live') || el.hasAttribute('role')), false, `${id} is not a live region`);
      }
      // Nothing inside the lists is live either, except a ticking clock, which is explicitly off.
      assert.deepEqual(await page.$$eval('#games-content [aria-live], #activity-content [aria-live], #installed-content [aria-live]', els => els.map(el => el.getAttribute('aria-live'))), []);
      const announcer = page.locator('#announcer');
      assert.equal(await page.locator('[id="announcer"]').count(), 1);
      assert.equal(await announcer.getAttribute('role'), 'status');
      assert.equal(await announcer.getAttribute('aria-live'), 'polite');
      assert.equal(await announcer.getAttribute('aria-atomic'), 'true');
      assert.equal(await announcer.getAttribute('aria-hidden'), null, 'it stays in the accessibility tree');
      const box = await announcer.evaluate(el => { const style = getComputedStyle(el), rect = el.getBoundingClientRect(); return { position: style.position, overflow: style.overflow, width: rect.width, height: rect.height, display: style.display, visibility: style.visibility }; });
      assert.deepEqual(box, { position: 'absolute', overflow: 'hidden', width: 1, height: 1, display: 'block', visibility: 'visible' });
      assert.equal(await announcer.textContent(), '', 'nothing is read out for the list read at start');
      await clean(page, api);
    });

    await test('the announcer says a job status change once, and nothing for unchanged polls', async () => {
      const first = job(hex('a'), { status: 'queued', stage: 'requested', label: 'Readable UI · Main file' });
      const { page, api } = await fixture({ jobs: [first] });
      await goActivity(page, hex('a'));
      await watch(page, '#announcer');
      // A forced poll can be skipped when the timer's own poll is in flight, so a change waits for its message instead of assuming it.
      const until = count => page.waitForFunction(wanted => window.__seen.length >= wanted, count, { timeout: 8000 });
      await forcePoll(page, api); await forcePoll(page, api);
      assert.deepEqual(await seen(page), [], 'polls that change nothing announce nothing');
      api.jobs = [{ ...first, status: 'downloading' }]; await forcePoll(page, api); await until(1);
      api.jobs = [{ ...first, status: 'analyzing', stage: 'downloaded' }]; await forcePoll(page, api); await until(2); await forcePoll(page, api);
      api.jobs = [{ ...first, status: 'analyzed', stage: 'analyzed', bytes_downloaded: 10 }]; await forcePoll(page, api); await until(3); await forcePoll(page, api);
      assert.deepEqual(await seen(page), ['Readable UI · Main file: downloading', 'Readable UI · Main file: checking archive contents', 'Readable UI · Main file: analysis complete']);
      // A second job appears queued (its toast said so) then fails; several changes in one poll are one message.
      api.jobs = [{ ...first, status: 'analyzed' }, job(hex('b'), { status: 'queued', stage: 'requested', label: 'Second' })]; await forcePoll(page, api);
      await page.waitForFunction(() => document.querySelectorAll('[data-job-id]').length === 2, null, { timeout: 8000 }); await forcePoll(page, api);
      assert.equal((await seen(page)).length, 3, 'a job first seen queued says nothing');
      api.jobs = [{ ...first, status: 'failed', error: { code: 'download_failed' } }, job(hex('b'), { status: 'downloading', stage: 'requested', label: 'Second' }), job(hex('c'), { status: 'failed', stage: 'requested', label: 'Third', error: { code: 'download_failed' } })]; await forcePoll(page, api); await until(4); await forcePoll(page, api);
      const all = await seen(page);
      assert.equal(all.length, 4, 'one message for the whole poll');
      assert.equal(all[3], 'Readable UI · Main file: could not complete. Second: downloading. Third: could not complete');
      assert.equal(await page.locator('#announcer').textContent(), all[3]);
      // A label with markup is read as text.
      api.jobs = [job(hex('d'), { status: 'failed', label: hostile, error: { code: 'download_failed' } })]; await forcePoll(page, api); await until(5);
      assert.equal(await page.locator('#announcer img').count(), 0);
      assert.equal(await page.locator('#announcer').textContent(), `${hostile}: could not complete`);
      // More than three changes in one poll: the first three, then a count.
      api.jobs = Array.from({ length: 5 }, (_, n) => job(`many${n}`, { status: 'failed', label: `Job ${n}`, error: { code: 'download_failed' } })); await forcePoll(page, api); await until(6);
      assert.equal(await page.locator('#announcer').textContent(), 'Job 0: could not complete. Job 1: could not complete. Job 2: could not complete. 2 more jobs changed');
      await clean(page, api);
    });

    await test('an error stays until it is dismissed, is a role=alert, and its Dismiss button is reached and pressed by controller', async () => {
      const { page, api } = await fixture({ jobs: [failed(hex('a'), { target_ref: ref })], reply: () => ({ status: 409, body: { error: { code: 'stale_target_ref', message: 'Conflict' } } }) });
      await goActivity(page, hex('a'));
      const banner = page.locator('#status-banner');
      assert.equal(await banner.getAttribute('role'), 'alert');
      assert.equal(await banner.getAttribute('aria-live'), null, 'role=alert already is the live region');
      await page.locator('[data-request-again]').focus();
      await press(page, CROSS);
      await page.waitForSelector('#status-banner:not([hidden])');
      assert.equal(await bannerText(page), 'The console target changed. Refresh the targets and choose again.');
      assert.equal(await page.locator('#toast').isHidden(), true, 'an error is not a toast');
      // No timer takes it away: it outlives the 3.6 s toast and the job polls in between.
      await page.waitForTimeout(4300);
      assert.equal(await banner.isVisible(), true);
      assert.equal(await bannerText(page), 'The console target changed. Refresh the targets and choose again.');
      const dismiss = banner.getByRole('button', { name: 'Dismiss' });
      assert.equal(await dismiss.count(), 1);
      // Up from the control that was focused reaches it (the banner sits before the list in focus order); Cross dismisses it.
      assert.equal(await focusKey(page), 'request-again');
      for (let n = 0; n < 6 && (await page.evaluate(() => document.activeElement?.id)) !== 'banner-dismiss'; n++) await press(page, UP);
      assert.equal(await page.evaluate(() => document.activeElement?.id), 'banner-dismiss');
      await press(page, CROSS);
      assert.equal(await banner.isHidden(), true);
      assert.equal(await banner.innerText(), '');
      assert.equal(await focusKey(page), 'request-again', 'focus goes back to where it was');
      // Arrow keys reach it too, and Enter dismisses it.
      await page.locator('[data-request-again]').focus(); await press(page, CROSS);
      await page.waitForSelector('#status-banner:not([hidden])');
      await page.locator('#tab-activity').focus();
      for (let n = 0; n < 6 && (await page.evaluate(() => document.activeElement?.id)) !== 'banner-dismiss'; n++) await page.keyboard.press('ArrowDown');
      assert.equal(await page.evaluate(() => document.activeElement?.id), 'banner-dismiss');
      assert.equal(await dismiss.evaluate(el => el.matches(':focus-visible') && getComputedStyle(el).outlineStyle !== 'none'), true, 'focus ring is visible');
      await page.keyboard.press('Enter');
      assert.equal(await banner.isHidden(), true);
      await clean(page, api);
    });

    await test('a repeated identical error is not announced again, and a dismissed poll error stays dismissed until the poll succeeds', async () => {
      const { page, api } = await fixture({ jobs: [job(hex('a'))] });
      await goActivity(page, hex('a'));
      const banner = page.locator('#status-banner');
      api.jobsError = 'network';
      await forcePoll(page, api);
      await banner.waitFor({ state: 'visible' });
      assert.equal(await bannerText(page), 'Nexus could not be reached. Try again.');
      await watch(page, '#status-banner');
      await forcePoll(page, api); await forcePoll(page, api); await forcePoll(page, api);
      assert.deepEqual(await seen(page), [], 'the same error up again changes nothing in the alert');
      assert.equal(await banner.isVisible(), true, 'it stays up');
      // Dismissed: the same failing poll does not raise it again.
      await banner.getByRole('button', { name: 'Dismiss' }).click();
      assert.equal(await banner.isHidden(), true);
      await forcePoll(page, api); await forcePoll(page, api);
      assert.equal(await banner.isHidden(), true, 'a dismissed poll error stays dismissed');
      // A poll that succeeds forgets the dismissal; the next failure is shown again.
      api.jobsError = ''; await forcePoll(page, api);
      assert.equal(await banner.isHidden(), true);
      api.jobsError = 'network'; await forcePoll(page, api);
      await banner.waitFor({ state: 'visible' });
      // A different error replaces it (one banner, newest wins).
      api.jobsError = 'rate_limit'; await forcePoll(page, api);
      await page.waitForFunction(() => /slow down/.test(document.querySelector('#status-banner .banner-text')?.textContent || ''));
      assert.equal(await page.locator('#status-banner .banner-text').count(), 1);
      await clean(page, api);
    });

    await test('a success is still a toast; focus stays where it was and nothing else becomes a live region', async () => {
      const { page, api } = await fixture({ jobs: [failed(hex('a'))] });
      await goActivity(page, hex('a'));
      await page.locator('[data-request-again]').focus(); await press(page, CROSS);
      await page.getByText('Analysis requested again.').waitFor();
      const toast = page.locator('#toast');
      assert.equal(await toast.isVisible(), true);
      assert.equal(await toast.getAttribute('role'), 'status');
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      assert.equal(await focusKey(page), 'request-again');
      // Exactly one hidden announcer plus the visible toast and the lookup form's own status line; the lists are not live.
      assert.deepEqual(await page.$$eval('[role="status"]', els => els.map(el => el.id).sort()), ['announcer', 'lookup-status', 'toast']);
      await clean(page, api);
    });

    // ---- TV type scale, safe area, controller and status cues (section 6; section 7, items 12, 13 and 15) ----
    const FULL_HD = { width: 1920, height: 1080 }, HD = { width: 1280, height: 720 };
    const longName = `Long${'X'.repeat(150)}`;
    const observedValue = value => ({ value, status: 'observed', source: 'owned_fixture' });
    const titleInventory = { schema_version: 1, status: 'observed', complete: true, snapshot_id: '1'.repeat(32), source_scope: 'running_processes', reason: null,
      items: [{ title_key: 'a'.repeat(32), title_id: observedValue('PPSA83071'), display_name: observedValue(`First unrelated title ${longName}`), build_version: observedValue('3.2'), build_fingerprint: null }] };
    // Every text node that is drawn, and every field value or placeholder, with its computed size. Closed <details> are opened first.
    const smallText = (page, floor = 18) => page.evaluate(floor => {
      for (const details of document.querySelectorAll('details')) details.open = true;
      const bad = [], shown = el => !!el && el.getClientRects().length > 0 && el.checkVisibility({ checkVisibilityCSS: true }) && !el.closest('.visually-hidden, [hidden]');
      const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
      for (let node = walker.nextNode(); node; node = walker.nextNode()) {
        const el = node.parentElement;
        if (!node.nodeValue.trim() || !el || el.closest('script, style, template') || !shown(el)) continue;
        const size = parseFloat(getComputedStyle(el).fontSize);
        if (size < floor) bad.push(`${size}px ${el.tagName.toLowerCase()}.${el.className}: ${node.nodeValue.trim().slice(0, 40)}`);
      }
      for (const el of document.querySelectorAll('input, select, textarea, button')) if (shown(el) && parseFloat(getComputedStyle(el).fontSize) < floor) bad.push(`${getComputedStyle(el).fontSize} control ${el.tagName.toLowerCase()}#${el.id}`);
      return bad;
    }, floor);
    // Horizontal scroll, anything drawn past the safe margins, and controls that overlap one another (the open dialog hides the page behind it).
    const layoutProblems = page => page.evaluate(() => {
      const problems = [], root = document.documentElement, safeX = innerWidth * .05 - 1, dialog = document.querySelector('#details-dialog');
      if (root.scrollWidth > innerWidth) problems.push(`horizontal scroll ${root.scrollWidth} > ${innerWidth}`);
      const scope = dialog.open ? dialog : document.querySelector('.shell');
      for (const el of scope.querySelectorAll('*')) {
        if (el.closest('.visually-hidden, [hidden]') || !el.getClientRects().length) continue;
        const box = el.getBoundingClientRect();
        if (box.width && (box.left < safeX || box.right > innerWidth - safeX)) problems.push(`outside the safe margin: ${el.tagName.toLowerCase()}.${el.className} ${Math.round(box.left)}..${Math.round(box.right)}`);
      }
      const stops = [...scope.querySelectorAll('button, a[href], input, select, summary, [tabindex="0"]')].filter(el => el.getClientRects().length && !el.closest('[hidden]'));
      // What is scrolled out of a scroll box is not drawn, so a box is cut down to the scroll boxes around it.
      const visibleBox = el => {
        let box = el.getBoundingClientRect();
        for (let parent = el.parentElement; parent && box; parent = parent.parentElement) {
          if (getComputedStyle(parent).overflowY === 'visible') continue;
          const clip = parent.getBoundingClientRect(), left = Math.max(box.left, clip.left), top = Math.max(box.top, clip.top), right = Math.min(box.right, clip.right), bottom = Math.min(box.bottom, clip.bottom);
          box = right > left && bottom > top ? { left, top, right, bottom } : null;
        }
        return box;
      };
      const boxes = stops.map(el => [el, visibleBox(el)]).filter(([, box]) => box);
      for (let i = 0; i < boxes.length; i++) for (let j = i + 1; j < boxes.length; j++) {
        const [a, x] = boxes[i], [b, y] = boxes[j];
        if (a.contains(b) || b.contains(a)) continue;
        const wide = Math.min(x.right, y.right) - Math.max(x.left, y.left), high = Math.min(x.bottom, y.bottom) - Math.max(x.top, y.top);
        if (wide > 2 && high > 2) problems.push(`overlap: ${a.tagName.toLowerCase()}.${a.className} and ${b.tagName.toLowerCase()}.${b.className}`);
      }
      return problems;
    });
    const richJobs = () => [
      job(hex('1'), { status: 'queued', bytes_downloaded: undefined }),
      job(hex('2'), { status: 'downloading', bytes_downloaded: undefined, progress: { unit: 'files', done: 3, total: 8 } }),
      job(hex('3'), { status: 'analyzing', bytes_downloaded: undefined, label: longName }),
      job(hex('4'), { file_id: 101, compatibility_assessment: { ...compat(), missing: { assets: [`/Game/${longName}`], classes: ['ClassX'], functions: [], short_class_names: [] }, reasons: [{ code: 'dynamic_references', detail: `host detail ${longName}` }] },
        runtime_coverage: runtimeCoverage({ missing: [longName.slice(0, 120), 'ExecuteInGameThread'] }), conflicts: { ...conflicts(), paths_sample: [`Game/Content/${longName}.uasset`] } }),
      waiting(hex('5')), failed(hex('6')), interrupted(hex('7')),
    ];
    const richFixture = async (extra = {}) => {
      const mods = [...zetaMods(), { mod_id: 90, game_domain: 'zeta', name: longName, author: longName, summary: longName, version: '1.0', description: longName }];
      const files = [...zebraFiles(), file(190, longName, { description: longName, category_name: 'MAIN', uploaded_timestamp: 1700000000 })];
      return fixture({ viewport: FULL_HD, capabilities: { nexus_auth_start: true }, mods: { zeta: mods }, files: { 30: files, 12: [file(201, 'Only file')] }, jobs: richJobs(),
        get: url => url.pathname.startsWith('/api/detected-titles') ? { body: titleInventory } : undefined, ...extra });
    };
    // Each view of the app, in turn, for the checks above: games, mods, files with a selection, the file dialog, Activity, Installed.
    const eachView = async (page, check) => {
      await check('games');
      await page.getByRole('button', { name: 'Zeta world' }).click(); await page.locator('[data-card^="mod:"]').first().waitFor();
      await check('mods');
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await page.locator('[data-basket-add]').first().click(); await page.locator('#selection-basket .basket-item').first().waitFor();
      await check('files and selection');
      await page.getByRole('button', { name: 'Details for Main file' }).click();
      await check('file dialog');
      await page.keyboard.press('Escape');
      await page.getByRole('tab', { name: 'Activity' }).click(); await card(page, hex('4')).waitFor();
      await check('activity');
      await page.getByRole('tab', { name: 'Installed' }).click();
      await check('installed');
    };

    await test('TV type scale: body 24px, secondary 20px, no visible text under 18px in any view or dialog', async () => {
      const { page, api } = await richFixture();
      const sizes = await page.evaluate(() => { const root = getComputedStyle(document.documentElement), size = selector => parseFloat(getComputedStyle(document.querySelector(selector)).fontSize); return { tokens: ['--text-floor', '--text-secondary', '--text-body'].map(name => root.getPropertyValue(name).trim()), body: size('body'), tab: size('.tab'), pill: size('.pill'), meta: size('.card-meta'), footer: size('.footer'), hint: size('.controller-hint'), eyebrow: size('.eyebrow') }; });
      assert.deepEqual(sizes, { tokens: ['18px', '20px', '24px'], body: 24, tab: 24, pill: 18, meta: 20, footer: 18, hint: 18, eyebrow: 18 });
      await eachView(page, async name => assert.deepEqual(await smallText(page), [], `text under 18px in ${name}`));
      // Notices: an error banner and a success toast.
      await page.getByRole('tab', { name: 'Activity' }).click();
      api.reply = post => post.path === '/api/jobs' ? { status: 503, body: { error: { code: 'busy', message: 'x' } } } : undefined;
      await page.locator('[data-request-again]').first().click();
      await page.waitForSelector('#status-banner:not([hidden])');
      assert.deepEqual(await smallText(page), [], 'text under 18px in the error banner');
      await page.locator('#banner-dismiss').click();
      api.reply = post => post.path === '/api/jobs' ? { status: 202, body: job(hex('9'), { status: 'queued', stage: 'requested' }) } : undefined;
      await page.locator('[data-request-again]').first().click();
      await page.locator('#toast:not([hidden])').waitFor();
      assert.deepEqual(await smallText(page), [], 'text under 18px in the toast');
      await clean(page, api);
    });

    await test('TV type scale: the Installed view and its host details stay at 18px or more', async () => {
      const { page, api } = await fixture({ ...serviceFixture([preparedMod([{ code: 'preparation_limit', message: 'over by 3 MiB' }, { code: 'mystery', message: 'x' }])], [gameRecord('PPSA00001', 'failed', 'loader said something')]) });
      await page.getByRole('tab', { name: 'Installed' }).click();
      await page.locator('.installed-detail').first().waitFor();
      assert.deepEqual(await smallText(page), []);
      assert.equal(await page.locator('.installed-detail').first().evaluate(el => getComputedStyle(el).fontSize), '18px');
      await clean(page, api);
    });

    await test('the type scale is defined once: every font size in style.css is a token, a clamp, or at least 18px', async () => {
      const css = fs.readFileSync(path.join(webRoot, 'style.css'), 'utf8').replace(/\/\*[\s\S]*?\*\//g, '');
      const sizes = [...css.matchAll(/(?<![-\w])font-size:\s*([^;}]+)/g)].map(match => match[1].trim());
      assert.ok(sizes.length > 40, 'font sizes are found');
      const tokens = [...css.matchAll(/--text-(floor|secondary|body|heading):\s*(\d+)px/g)].map(match => [match[1], Number(match[2])]);
      assert.deepEqual(tokens, [['floor', 18], ['secondary', 20], ['body', 24], ['heading', 28]], 'each size is declared once');
      for (const size of sizes) {
        if (/^var\(--text-[a-z]+\)$/.test(size) || /^clamp\(/.test(size)) continue;
        assert.match(size, /^\d+px$/, `font-size ${size} is a token or a plain pixel size`);
        assert.ok(parseInt(size, 10) >= 18, `font-size ${size} is under 18px`);
      }
      // The fill of a progress bar is set from script because the host's content security policy allows no inline style attribute.
      assert.doesNotMatch(fs.readFileSync(path.join(webRoot, 'app.js'), 'utf8'), /\sstyle="/);
    });

    for (const [name, viewport] of [['1920x1080', FULL_HD], ['1280x720', HD]]) {
      await test(`${name}: a 5% safe margin, no horizontal scroll, no overlapping controls, long names wrap`, async () => {
        const { page, api } = await richFixture({ viewport });
        await eachView(page, async view => assert.deepEqual(await layoutProblems(page), [], view));
        // The page keeps 5% free above the header and below the footer.
        await page.getByRole('tab', { name: 'Games' }).click();
        const edges = await page.evaluate(() => { const top = document.querySelector('.topbar').getBoundingClientRect().top + scrollY, bottom = document.documentElement.scrollHeight - (document.querySelector('.footer').getBoundingClientRect().bottom + scrollY); return { top, bottom, rowEnd: innerHeight * .05 }; });
        assert.ok(edges.top >= edges.rowEnd - 1, `top margin ${edges.top}`); assert.ok(edges.bottom >= edges.rowEnd - 1, `bottom margin ${edges.bottom}`);
        const padding = await page.evaluate(() => { const style = getComputedStyle(document.body); return [style.paddingTop, style.paddingRight, style.paddingBottom, style.paddingLeft]; });
        assert.deepEqual(padding, [`${viewport.height * .05}px`, `${viewport.width * .05}px`, `${viewport.height * .05}px`, `${viewport.width * .05}px`], 'the margin is 5% of the screen on every side');
        await clean(page, api);
      });
    }

    // A grid of cards walked with the arrows and the D-pad.
    const manyGames = count => Array.from({ length: count }, (_, index) => ({ domain_name: `g${index}`, id: index + 1, name: index === 0 ? 'Zeta world' : `Game ${index}`, mods: index }));
    const columnsOf = page => page.$$eval('[data-card^="game:"]', els => { const tops = els.map(el => Math.round(el.getBoundingClientRect().top)); return tops.filter(top => top === tops[0]).length; });
    for (const [name, viewport] of [['1920x1080', FULL_HD], ['1280x720', HD]]) {
      await test(`${name}: Up and Down move between rows and Left and Right along a row of cards, by geometry, with arrows and D-pad`, async () => {
        const { page, api } = await fixture({ viewport, games: manyGames(11) });
        const ids = (await cards(page, 'game')), n = ids.length, cols = await columnsOf(page);
        assert.ok(cols >= 3 && cols < n, `a grid of ${cols} columns`);
        const at = () => active(page);
        const below = i => Math.min((Math.floor(i / cols) + 1) * cols + (i % cols), n - 1);
        await page.locator('[data-card="game:g0"]').focus();
        // Keys. Down is the card below, not the next one in the document; Right is the next card in the row.
        await page.keyboard.press('ArrowDown'); assert.equal(await at(), `game:${ids[cols]}`);
        await page.keyboard.press('ArrowRight'); assert.equal(await at(), `game:${ids[cols + 1]}`);
        await page.keyboard.press('ArrowUp'); assert.equal(await at(), `game:${ids[1]}`);
        await page.keyboard.press('ArrowLeft'); assert.equal(await at(), `game:${ids[0]}`);
        // The end of a row carries on to the start of the next one, and back.
        await page.locator(`[data-card="game:${ids[cols - 1]}"]`).focus();
        await page.keyboard.press('ArrowRight'); assert.equal(await at(), `game:${ids[cols]}`, 'Right at the end of a row');
        await page.keyboard.press('ArrowLeft'); assert.equal(await at(), `game:${ids[cols - 1]}`, 'Left at the start of a row');
        // A short last row: Down from the last column lands on the nearest card of that row.
        const last = Math.floor((n - 1) / cols) - 1;
        await page.locator(`[data-card="game:${ids[last * cols + cols - 1]}"]`).focus();
        await page.keyboard.press('ArrowDown'); assert.equal(await at(), `game:${ids[below(last * cols + cols - 1)]}`);
        // D-pad: the same moves.
        await page.locator('[data-card="game:g0"]').focus();
        await press(page, RIGHT); assert.equal(await at(), `game:${ids[1]}`);
        await press(page, DOWN); assert.equal(await at(), `game:${ids[1 + cols]}`);
        await press(page, LEFT); assert.equal(await at(), `game:${ids[cols]}`);
        await press(page, UP); assert.equal(await at(), `game:${ids[0]}`);
        assert.equal(await page.evaluate(() => document.activeElement.matches(':focus-visible')), true, 'the focus ring shows');
        await clean(page, api);
      });
    }

    // Every control of a view can be reached from the first tab with the four directions alone (arrow keys; the D-pad calls the same code).
    const unreachable = async (page, view) => {
      const total = await page.evaluate(view => {
        document.querySelectorAll('[data-probe]').forEach(el => el.removeAttribute('data-probe'));
        const stops = [...document.querySelectorAll('.topbar a[href],.topbar button:not(:disabled),.tabs button:not(:disabled),#status-banner button:not(:disabled),#' + view + '-view button:not(:disabled),#' + view + '-view a[href],#' + view + '-view summary,#' + view + '-view input:not(:disabled)')]
          .filter(el => el.getClientRects().length && !el.closest('[hidden]'));
        stops.forEach((el, index) => { el.dataset.probe = String(index); });
        return stops.length;
      }, view);
      const edges = [], noRing = [];
      for (let index = 0; index < total; index++) {
        const row = [];
        for (const key of ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']) {
          await page.locator(`[data-probe="${index}"]`).focus(); await page.keyboard.press(key);
          // Wherever the keys land, the focus ring is drawn (an outline of at least 2px).
          const landed = await page.evaluate(() => { const el = document.activeElement, style = getComputedStyle(el); return { probe: el.dataset.probe ?? null, ring: el.matches(':focus-visible') && style.outlineStyle !== 'none' && parseFloat(style.outlineWidth) >= 2, name: `${el.tagName.toLowerCase()}#${el.id}.${el.className}` }; });
          row.push(landed.probe);
          if (!landed.ring) noRing.push(landed.name);
        }
        edges.push(row);
      }
      const seen = new Set([0]), queue = [0];
      while (queue.length) for (const next of edges[queue.shift()]) if (next !== null && !seen.has(Number(next))) { seen.add(Number(next)); queue.push(Number(next)); }
      return { total, noRing, missing: Array.from({ length: total }, (_, index) => index).filter(index => !seen.has(index)) };
    };
    await test('every control of the Games, files and Activity views is reachable from the tabs with the four directions', async () => {
      const { page, api } = await richFixture();
      let result = await unreachable(page, 'games');
      assert.ok(result.total > 10, `${result.total} stops`); assert.deepEqual(result.missing, [], 'games'); assert.deepEqual(result.noRing, [], 'focus rings in games');
      await page.getByRole('button', { name: 'Zeta world' }).click(); await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await page.locator('[data-basket-add]').first().click();
      result = await unreachable(page, 'games'); assert.deepEqual(result.missing, [], 'files and selection'); assert.deepEqual(result.noRing, [], 'focus rings in files');
      await page.getByRole('tab', { name: 'Activity' }).click(); await card(page, hex('4')).waitFor();
      result = await unreachable(page, 'activity'); assert.ok(result.total > 10); assert.deepEqual(result.missing, [], 'activity'); assert.deepEqual(result.noRing, [], 'focus rings in activity');
      // The same in the file dialog: every control the arrows reach shows its ring.
      await page.getByRole('tab', { name: 'Games' }).click(); await page.getByRole('button', { name: 'Details for Main file' }).click();
      for (let n = 0; n < 8; n++) {
        await page.keyboard.press('ArrowDown');
        assert.equal(await page.evaluate(() => { const style = getComputedStyle(document.activeElement); return document.activeElement.matches(':focus-visible') && style.outlineStyle !== 'none' && parseFloat(style.outlineWidth) >= 2; }), true, `dialog stop ${n}`);
      }
      await clean(page, api);
    });

    await test('L2 and R2 jump to the top and bottom of the current list; Square and Options act on the card in focus', async () => {
      const failedJobs = [failed(hex('a')), waiting(hex('b')), job(hex('c'))];
      const fake = updateApi(); fake.reply = unknownReply;
      const { page, api } = await fixture({ jobs: failedJobs, get: fake.get });
      const label = () => active(page);
      // Mods list: from a card, from the tabs.
      await openZeta(page);
      await page.locator('[data-card="mod:45"]').focus();
      await press(page, L2); assert.equal(await label(), 'mod:30');
      await press(page, R2); assert.equal(await label(), 'mod:21');
      await page.locator('#tab-games').focus();
      await press(page, R2); assert.equal(await label(), 'mod:21', 'from the tabs it is the view’s main list');
      await press(page, L2); assert.equal(await label(), 'mod:30');
      // Square on a mod card has no quick action.
      await press(page, SQUARE); assert.equal(await label(), 'mod:30'); assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      // Files: the breadcrumbs are not list items; the last stop is the last file's download button.
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await page.locator('[data-file-details="104"]').focus();
      await press(page, L2); assert.equal(await label(), 'file-details:102');
      await press(page, R2);
      assert.equal(await page.evaluate(() => document.activeElement === [...document.querySelectorAll('[data-file-card]')].at(-1).querySelector('.download-file')), true);
      // Square adds the file to the selection (not a download); again does nothing; on a selected row it removes it.
      await page.locator('[data-file-card="101"] .download-file').focus();
      await press(page, SQUARE);
      await page.locator('#selection-basket .basket-item').waitFor();
      assert.equal(await page.locator('[data-basket-add="101"]').getAttribute('aria-pressed'), 'true');
      assert.equal(api.posts.length, 0, 'Square never requests a download');
      assert.equal(await label(), 'file-details:101', 'focus returns to the file after the list redraws');
      await press(page, SQUARE); assert.equal(await page.locator('#selection-basket .basket-item').count(), 1, 'nothing more to add');
      await page.locator('[data-file-card="102"] .download-file').focus(); await press(page, SQUARE);
      assert.equal(await page.locator('#selection-basket .basket-item').count(), 2);
      await page.locator('[data-basket-remove]').first().focus();
      await press(page, L2); assert.equal(await page.evaluate(() => document.activeElement === document.querySelector('[data-basket-remove]')), true, 'L2 is the first row of the selection');
      await press(page, R2); assert.equal(await label(), 'basket-clear', 'R2 is the last control of the selection');
      await page.locator('[data-basket-remove]').first().focus(); await press(page, SQUARE);
      assert.equal(await page.locator('#selection-basket .basket-item').count(), 1, 'Square on a selected row removes it');
      // Options on a file opens its details; Circle returns to where it came from.
      await page.locator('[data-file-details="103"]').focus(); await press(page, OPTIONS);
      assert.equal(await page.locator('#details-dialog').evaluate(el => el.open), true);
      await press(page, R2); assert.equal(await label(), 'Back', 'in a dialog R2 is the last control');
      await press(page, L2); assert.equal(await label(), 'Close details');
      await press(page, CIRCLE); assert.equal(await label(), 'file-details:103');
      // Activity: L2 and R2 within the jobs; Options goes to the first action of the card; Square checks for a newer file.
      await page.getByRole('tab', { name: 'Activity' }).click(); await card(page, hex('a')).waitFor();
      await page.locator('[data-update-check]').first().focus();
      await press(page, OPTIONS); assert.equal(await focusKey(page), 'request-again', 'Options focuses the card’s first action');
      await press(page, R2); assert.equal(await page.evaluate(() => document.activeElement === [...document.querySelectorAll('[data-job-id]')].at(-1).querySelector('[data-update-check]')), true, 'R2 is the last job’s last action');
      await press(page, L2); assert.equal(await focusKey(page), 'request-again', 'L2 is the first job’s first action');
      const before = fake.requests.length;
      await press(page, SQUARE);
      for (let n = 0; n < 40 && fake.requests.length === before; n++) await page.waitForTimeout(25);
      assert.equal(fake.requests.length, before + 1, 'Square on a job card is its quick action, a newer-file check');
      assert.equal(api.posts.length, 0, 'and not “Request again”');
      assert.equal(await focusKey(page), 'request-again', 'focus stays on the card');
      await clean(page, api);
    });

    await test('Options on a job card with no actions says so and leaves focus alone', async () => {
      const { page, api } = await fixture({ capabilities: { file_updates: false }, jobs: [job(hex('a'), { compatibility_assessment: compat() })] });
      await goActivity(page, hex('a'));
      await page.locator('[data-job-id] summary').focus();
      await press(page, OPTIONS);
      await page.locator('#toast:not([hidden])').waitFor();
      assert.equal(await page.locator('#toast').innerText(), 'This job has no actions.');
      assert.equal(await page.evaluate(() => document.activeElement.tagName), 'SUMMARY');
      assert.equal(await page.locator('#details-dialog').evaluate(el => el.open), false);
      await clean(page, api);
    });

    await test('each direction has its own repeat timer: no shared delay, a hold repeats, a stick moves along both axes', async () => {
      const { page, api } = await fixture({ clock: true, viewport: FULL_HD, games: manyGames(60) });
      // The installed clock keeps running in real time until it is paused; from here only runFor moves it, in whole 16 ms frames.
      await page.clock.pauseAt(Date.now() + 5000);
      await page.evaluate(() => {
        window.fixturePad = { mapping: 'standard', buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0] };
        Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.fixturePad] });
        window.__moves = []; document.addEventListener('focusin', event => window.__moves.push(event.target.dataset.card || event.target.id));
      });
      const ids = await cards(page, 'game'), cols = await columnsOf(page), set = (index, value) => page.evaluate(([i, v]) => { window.fixturePad.buttons[i].pressed = v; }, [index, value]);
      const moves = () => page.evaluate(() => window.__moves.slice());
      const reset = async id => { await page.locator(`[data-card="game:${id}"]`).focus(); await page.evaluate(() => { window.__moves = []; }); };
      await reset(ids[0]);
      // Hold Down: one move at once, none for the first 400 ms, then one every 140 ms or so.
      await set(DOWN, true); await page.clock.runFor(48);
      assert.deepEqual(await moves(), [`game:${ids[cols]}`], 'a press moves at once');
      await page.clock.runFor(300); assert.equal((await moves()).length, 1, 'no repeat inside the first delay');
      await page.clock.runFor(150); assert.equal((await moves()).length, 2, 'the first repeat comes after the delay');
      await page.clock.runFor(560);
      const held = (await moves()).length;
      assert.ok(held >= 5 && held <= 7, `a hold repeats about every 140 ms (${held} moves in 1.06 s)`);
      await set(DOWN, false); await page.clock.runFor(48);
      const settled = (await moves()).length; await page.clock.runFor(500);
      assert.equal((await moves()).length, settled, 'releasing stops the repeat');
      // Another direction is not held up by the first one's timer: Right while Down was pressed a moment ago.
      await reset(ids[0]);
      await set(DOWN, true); await page.clock.runFor(64);
      await set(RIGHT, true); await page.clock.runFor(64);
      assert.deepEqual(await moves(), [`game:${ids[cols]}`, `game:${ids[cols + 1]}`], 'Right moves at once while Down is held');
      await set(DOWN, false); await set(RIGHT, false); await page.clock.runFor(48);
      // Turning round at once is not swallowed either, nor is a second quick tap.
      await reset(ids[2 * cols]);
      await set(UP, true); await page.clock.runFor(48); await set(UP, false); await page.clock.runFor(32);
      await set(DOWN, true); await page.clock.runFor(48); await set(DOWN, false); await page.clock.runFor(32);
      await set(DOWN, true); await page.clock.runFor(48); await set(DOWN, false); await page.clock.runFor(32);
      assert.deepEqual(await moves(), [`game:${ids[cols]}`, `game:${ids[2 * cols]}`, `game:${ids[3 * cols]}`], 'Up, then Down, then Down again, each at once');
      // A stick held diagonally moves along both axes in the same frame (down first, then right).
      await reset(ids[0]);
      await page.evaluate(() => { window.fixturePad.axes = [0.9, 0.9]; }); await page.clock.runFor(20);
      assert.deepEqual(await moves(), [`game:${ids[cols]}`, `game:${ids[cols + 1]}`]);
      await page.evaluate(() => { window.fixturePad.axes = [0, 0]; }); await page.clock.runFor(20);
      // Buttons act once per press: Cross held for a second cycles the sort once.
      await page.locator('#catalog-sort').focus();
      const sortLabel = () => page.locator('#catalog-sort').innerText();
      const first = await sortLabel();
      await set(CROSS, true); await page.clock.runFor(1000); await set(CROSS, false); await page.clock.runFor(32);
      const second = await sortLabel();
      assert.notEqual(second, first);
      await set(CROSS, true); await page.clock.runFor(1000); await set(CROSS, false); await page.clock.runFor(32);
      assert.notEqual(await sortLabel(), second);
      assert.notEqual(await sortLabel(), first, 'two presses, two steps');
      await clean(page, api);
    });

    const glyphOf = { queued: '…', downloading: '↓', analyzing: '↻', analyzed: '≡', needs_nexus_authorization: '!', failed: '×', interrupted: '‖', unavailable: '?' };
    const labelOf = { queued: 'Queued', downloading: 'Downloading', analyzing: 'Analyzing archive', analyzed: 'Analysis complete', needs_nexus_authorization: 'Nexus authorization needed', failed: 'Could not complete', interrupted: 'Interrupted', unavailable: 'Status unavailable' };
    await test('job status pills carry a glyph per state beside their words; the glyph is hidden from screen readers', async () => {
      const statuses = Object.keys(glyphOf).filter(status => status !== 'unavailable');
      const jobs = [...statuses.map((status, index) => job(hex(String(index + 1)), { status, bytes_downloaded: undefined })), job(hex('a'), { status: 'bogus' }), job(hex('b'), { status: 'constructor' }), job(hex('c'), { status: 'unavailable' })];
      const { page, api } = await fixture({ jobs });
      await goActivity(page, hex('1'));
      const pills = await page.$$eval('[data-job-id] .job-pill', els => els.map(el => ({ status: el.dataset.jobStatus, glyph: el.querySelector('.pill-glyph').textContent, hidden: el.querySelector('.pill-glyph').getAttribute('aria-hidden'), label: el.querySelector('.pill-label').textContent, text: el.innerText.replace(/\s+/g, ' ').trim(), listed: el.closest('[data-job-id]').dataset.jobId })));
      assert.equal(pills.length, jobs.length);
      for (const pill of pills) {
        assert.equal(pill.hidden, 'true', `${pill.status} glyph is aria-hidden`);
        assert.equal(pill.glyph, glyphOf[pill.status], pill.status);
        assert.equal(pill.label, labelOf[pill.status], 'the words stay beside the glyph');
        assert.equal(pill.text, `${glyphOf[pill.status]} ${labelOf[pill.status]}`);
      }
      assert.deepEqual(pills.map(pill => pill.status), [...statuses, 'unavailable', 'unavailable', 'unavailable']);
      assert.equal(new Set(Object.values(glyphOf)).size, Object.keys(glyphOf).length, 'every state has its own glyph');
      // Verdict glyphs mean a verdict only: no job state borrows them.
      for (const verdictGlyph of ['✓', '◐', '✕', '⚠']) assert.equal(Object.values(glyphOf).includes(verdictGlyph), false, verdictGlyph);
      // The accessible name of a pill is its words only.
      assert.equal(await page.locator('[data-job-status="failed"]').evaluate(el => [...el.childNodes].filter(node => !(node.nodeType === 1 && node.getAttribute('aria-hidden') === 'true')).map(node => node.textContent).join('')), 'Could not complete');
      await clean(page, api);
    });

    await test('indeterminate steps are aria-busy; a progressbar appears only where a total is known, and nothing shows a percentage', async () => {
      const running = (id, status, progress) => job(id, { status, bytes_downloaded: undefined, ...(progress === undefined ? {} : { progress }) });
      const invalid = [{ unit: 'files', done: 9, total: 8 }, { unit: 'files', done: 1, total: 0 }, { unit: 'percent', done: 1, total: 2 }, { unit: 'files', done: '3', total: 8 }, { unit: 'files', done: -1, total: 8 },
        { unit: 'files', done: 1.5, total: 8 }, { unit: 'files', done: 1, total: 8, extra: 1 }, { unit: 'files', done: 1 }, [3, 8], 'half', 7, null, { unit: 'bytes', done: 1e300, total: 2e300 }, { unit: 'files', done: 1, total: 'many' }];
      const jobs = [running(hex('1'), 'queued'), running(hex('2'), 'downloading'), running(hex('3'), 'analyzing'), job(hex('4')),
        running(hex('5'), 'downloading', { unit: 'files', done: 3, total: 8 }), running(hex('6'), 'analyzing', { unit: 'bytes', done: 1024, total: 4096 }),
        running(hex('7'), 'downloading', { unit: 'files', done: 5, total: null }), running(hex('8'), 'downloading', { unit: 'files', done: 1, total: 1 }),
        job(hex('9'), { progress: { unit: 'files', done: 3, total: 8 } }), ...invalid.map((progress, index) => running(`bad${String(index).padStart(2, '0')}`.padEnd(32, 'x'), 'downloading', progress))];
      const { page, api } = await fixture({ jobs });
      await goActivity(page, hex('1'));
      const busy = id => card(page, id).locator('[aria-busy="true"]').count();
      assert.equal(await busy(hex('1')), 0, 'a queued job is waiting, not busy');
      assert.equal(await busy(hex('2')), 1); assert.equal(await busy(hex('3')), 1);
      assert.equal(await card(page, hex('2')).locator('.job-progress').getAttribute('aria-busy'), 'true');
      assert.equal(await busy(hex('4')), 0, 'a finished job is not busy');
      for (const id of [hex('1'), hex('2'), hex('3'), hex('4')]) assert.equal(await card(page, id).locator('[role="progressbar"]').count(), 0, 'no total, no progressbar');
      assert.match(await card(page, hex('2')).locator('.job-progress').innerText(), /The host reports no download progress\./);
      // A known total: the values, the words, and a fill that matches.
      const bar = id => card(page, id).locator('[role="progressbar"]');
      assert.deepEqual(await bar(hex('5')).evaluate(el => ({ min: el.getAttribute('aria-valuemin'), max: el.getAttribute('aria-valuemax'), now: el.getAttribute('aria-valuenow'), text: el.getAttribute('aria-valuetext'), label: el.getAttribute('aria-label') })),
        { min: '0', max: '8', now: '3', text: '3 of 8 files', label: 'Files processed' });
      assert.equal(await card(page, hex('5')).locator('.progress-words').innerText(), '3 of 8 files');
      const fill = await card(page, hex('5')).locator('.progress-fill').evaluate(el => el.getBoundingClientRect().width / el.parentElement.clientWidth);
      assert.ok(fill > .35 && fill < .4, `the fill is 3/8 (${fill})`);
      assert.match(await card(page, hex('5')).locator('.job-progress').innerText(), /^Running for 0:0\d · timed by this page from when it first saw the job running\.$/, 'the page no longer says the host reports no progress');
      assert.deepEqual(await bar(hex('6')).evaluate(el => [el.getAttribute('aria-valuemax'), el.getAttribute('aria-valuenow'), el.getAttribute('aria-valuetext'), el.getAttribute('aria-label')]), ['4096', '1024', '1 KB (1,024 bytes) of 4 KB (4,096 bytes)', 'Bytes processed']);
      assert.equal(await bar(hex('8')).evaluate(el => el.firstElementChild.getBoundingClientRect().width / el.clientWidth > .99), true, 'a complete count fills the bar');
      assert.equal(await card(page, hex('5')).locator('.job-progress').getAttribute('aria-busy'), 'true', 'a running step is still busy');
      // An unknown total is words only: no bar, no fraction.
      assert.equal(await bar(hex('7')).count(), 0);
      assert.equal(await card(page, hex('7')).locator('.job-count').innerText(), '5 files so far. The total is not known.');
      // A finished job and every malformed record show no count at all.
      assert.equal(await card(page, hex('9')).locator('.job-count, [role="progressbar"]').count(), 0);
      for (let index = 0; index < invalid.length; index++) {
        const id = `bad${String(index).padStart(2, '0')}`.padEnd(32, 'x');
        assert.equal(await card(page, id).locator('.job-count, [role="progressbar"]').count(), 0, JSON.stringify(invalid[index]));
        assert.equal(await card(page, id).locator('.job-progress').getAttribute('aria-busy'), 'true');
      }
      assert.equal(await page.locator('[role="progressbar"]').count(), 3);
      assert.doesNotMatch(await page.locator('#activity-content').innerText(), /%|\bpercent\b|\bETA\b/i, 'no percentage and no estimate anywhere');
      await clean(page, api);
    });

    await test('a newer-file check in flight marks its row busy and clears it when the answer arrives', async () => {
      const fake = updateApi(); fake.reply = currentReply;
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      const { page, api } = await fixture({ jobs: [job(hex('a'))], get: fake.get });
      await goActivity(page, hex('a'));
      assert.equal(await card(page, hex('a')).locator('.job-update').getAttribute('aria-busy'), null);
      await card(page, hex('a')).getByRole('button', { name: 'Check for newer file' }).click();
      await card(page, hex('a')).locator('.job-update[aria-busy="true"]').waitFor();
      release();
      await card(page, hex('a')).locator('.update-line').waitFor();
      assert.equal(await card(page, hex('a')).locator('.job-update').getAttribute('aria-busy'), null);
      await clean(page, api);
    });

    // ---- Mod requirements (docs/INSTALL-FLOW-UX.md 3.8): what the author listed on Nexus, never a verdict ----
    // A fake of GET /api/games/{game}/mods/{mod}/requirements: records "game/mod" per request, may hold the reply back with `gate`.
    const requirementsApi = () => {
      const fake = { requests: [], reply: null, gate: null };
      fake.get = async url => {
        const match = /^\/api\/games\/([^/]+)\/mods\/(\d+)\/requirements$/.exec(url.pathname);
        if (!match) return undefined;
        fake.requests.push(`${match[1]}/${match[2]}`);
        if (fake.gate) await fake.gate;
        return typeof fake.reply === 'function' ? fake.reply(match[1], Number(match[2])) : fake.reply;
      };
      return fake;
    };
    const nexusRow = (extra = {}) => ({ id: 'rel-1', external_requirement: false, game: 'zeta', mod_id: '5', name: 'Amber camera framework', notes: 'Install it first.', url: 'https://www.nexusmods.com/zeta/mods/5', satisfaction: 'unresolved', ...extra });
    const externalRow = (extra = {}) => ({ id: 'rel-2', external_requirement: true, game: 'External', mod_id: 'v1', name: 'UE4SS', notes: 'Needed for scripts.', url: 'https://github.com/UE4SS-RE/RE-UE4SS', satisfaction: 'unresolved', ...extra });
    const dlcRow = (extra = {}) => ({ game: 'zeta', id: 'dlc-1', name: 'Expansion One', notes: 'Story pack', satisfaction: 'unresolved', ...extra });
    const requirementsBody = (extra = {}) => ({ game: 'zeta', mod_id: 30, discovery_complete: true, nexus_requirements: [nexusRow(), externalRow()], dlc_requirements: [dlcRow()], file_version_constraints_provided: false, installation_supported: false, ...extra });
    const requirementsReply = (extra = {}) => ({ status: 200, body: requirementsBody(extra) });
    const requirementsFixture = (fake, extra = {}) => fixture({ capabilities: { mod_requirements: true }, get: fake.get, ...extra });
    const introSentence = 'The author lists these on Nexus. ModBridge has not checked whether any of them are installed or compatible.';
    const invalidSentence = 'The host sent a reply this page could not verify.';
    const codeSentences = { content_hidden: 'Your Nexus content filter hides this file. Change the filter on Nexus, or pick another file.', reconnect_required: 'Reconnect your Nexus account to continue.',
      payload: 'Nexus sent a reply that could not be used. Try again later.', busy: 'ModBridge is busy with another operation. Wait a moment and try again.' };
    const openModSheet = async (page, id = 30) => { await page.locator(`[data-card="mod:${id}"]`).focus(); await page.keyboard.press('F6'); await page.locator('#details-dialog[open]').waitFor(); };
    const openFileSheet = async (page, name = 'Main file') => { await page.getByRole('button', { name: `Details for ${name}` }).click(); await page.locator('#details-dialog[open]').waitFor(); };
    const reqSection = page => page.locator('#details-content [data-requirements]');
    const reqStatus = page => reqSection(page).locator('[data-requirements-status]');
    const reqSettled = page => page.locator('#details-content [data-requirements-status]:not([data-requirements-status="loading"])').waitFor();
    const reqText = async page => (await reqSection(page).innerText()).replace(/\s+/g, ' ').trim();
    const sheetFocus = page => page.evaluate(() => { const el = document.activeElement; return el.dataset.focusKey || el.getAttribute('aria-label') || el.id || el.textContent.trim().slice(0, 30); });

    await test('requirements: Nexus, outside and DLC rows each read "Not checked"; links only where allowed; text stays text', async () => {
      const fake = requirementsApi();
      fake.reply = requirementsReply({
        nexus_requirements: [
          nexusRow(),
          nexusRow({ id: 'rel-b', mod_id: '6', name: 'No link mod', notes: '', url: '' }),
          nexusRow({ id: 'rel-c', mod_id: '7', name: 'Odd host mod', notes: '', url: 'https://example.test/zeta/mods/7' }),
          externalRow(),
          externalRow({ id: 'rel-3', name: 'Tool without link', notes: '', url: '' }),
          externalRow({ id: 'rel-4', name: 'Hostile <img src=x onerror=window.__pwned=1>', notes: '<b>bold</b> & "quoted"\u202e\u0007 tail', game: '<i>x</i>', mod_id: '', url: 'https://example.test/a?b=c&d=<e>' }),
        ],
        dlc_requirements: [dlcRow(), dlcRow({ id: 'dlc-2', name: '', notes: '' })],
      });
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      assert.deepEqual(fake.requests, ['zeta/30']);
      assert.equal(await page.locator('#details-content .detail-label', { hasText: 'Requirements listed by the author' }).count(), 1);
      assert.equal(await reqSection(page).locator('.req-title').innerText(), 'Requirements listed on Nexus');
      assert.equal(await reqStatus(page).innerText(), introSentence);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'listed');
      assert.deepEqual(await reqSection(page).locator('.req-name').allInnerTexts(), ['Amber camera framework', 'No link mod', 'Odd host mod', 'UE4SS', 'Tool without link', 'Hostile <img src=x onerror=window.__pwned=1>', 'DLC: Expansion One', 'DLC: unnamed']);
      assert.deepEqual(await reqSection(page).locator('.req-kind').allInnerTexts(), ['Nexus mod', 'Nexus mod', 'Nexus mod', 'Not on Nexus', 'Not on Nexus', 'Not on Nexus', 'DLC', 'DLC']);
      assert.deepEqual(await reqSection(page).locator('.req-state').allInnerTexts(), Array(8).fill('Not checked'), 'one chip per row, always "Not checked"');
      assert.deepEqual(await reqSection(page).locator('.req-row').evaluateAll(rows => rows.map(row => row.querySelector('.req-notes')?.textContent ?? null)),
        ['Install it first.', null, null, 'Needed for scripts.', null, '<b>bold</b> & "quoted"   tail', 'Story pack', null], 'notes only when written; control and bidi characters become spaces');
      const links = await reqSection(page).locator('.req-link').evaluateAll(els => els.map(el => ({ text: el.textContent.replace(/\s+/g, ' ').trim(), href: el.getAttribute('href'), target: el.target, rel: el.rel, label: el.getAttribute('aria-label'), row: el.closest('.req-row').querySelector('.req-name').textContent })));
      assert.deepEqual(links.map(link => link.row), ['Amber camera framework', 'UE4SS', 'Hostile <img src=x onerror=window.__pwned=1>'], 'no link for an empty url, an internal row off nexusmods.com, or a DLC');
      assert.deepEqual(links[0], { text: 'Open on Nexus', href: 'https://www.nexusmods.com/zeta/mods/5', target: '_blank', rel: 'noopener noreferrer', label: 'Open on Nexus: Amber camera framework', row: 'Amber camera framework' });
      assert.deepEqual(links.slice(1).map(link => [link.text, link.href, link.target, link.rel]), [
        ['Open external link github.com', 'https://github.com/UE4SS-RE/RE-UE4SS', '_blank', 'noopener noreferrer'],
        ['Open external link example.test', 'https://example.test/a?b=c&d=%3Ce%3E', '_blank', 'noopener noreferrer']], 'an outside link is labelled as external and names its host');
      assert.match(links[1].label, /^Open external link for UE4SS \(github\.com\)$/);
      // Text is text: nothing the author typed became an element, and every row's markup is closed.
      assert.equal(await page.locator('#details-content img, #details-content b, #details-content i, #details-content script').count(), 0);
      assert.equal(await page.locator('#announcer').innerText(), 'Requirements listed by the author: 8 listed on Nexus. None are checked.');
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'a list that loads quietly raises nothing');
      // Information only: no control in the section acts on a requirement.
      assert.equal(await reqSection(page).locator('button').count(), 0);
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'mod:30');
      // The same mod's file sheet shows the same list from the page's own copy: the host was asked once.
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await openFileSheet(page);
      await reqSettled(page);
      assert.equal(await reqSection(page).locator('.req-row').count(), 8);
      assert.deepEqual(fake.requests, ['zeta/30']);
      assert.deepEqual((await page.locator('#details-content .detail-label').allTextContents()).slice(-2), ['Mod', 'Requirements listed by the author'], 'the section follows the mod row');
      await clean(page, api);
    });

    await test('requirements: an empty set says so in the allowed words and claims nothing else', async () => {
      const fake = requirementsApi();
      fake.reply = requirementsReply({ nexus_requirements: [], dlc_requirements: [] });
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'empty');
      assert.equal(await reqText(page), 'No requirements listed The author listed no requirements on Nexus. That does not mean the mod needs nothing.');
      assert.equal(await reqSection(page).locator('.req-row, .req-state, .req-scroll, button, a').count(), 0);
      assert.equal(await page.locator('#announcer').innerText(), 'No requirements listed by the author on Nexus.');
      await clean(page, api);
    });

    await test('requirements: a failed request shows the catalog text and Retry, never an empty list, and Retry keeps focus', async () => {
      const fake = requirementsApi();
      fake.reply = { status: 400, body: { error: { code: 'rate_limit', message: 'HOST TEXT must not show' } } };
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      assert.equal(await reqSection(page).locator('.req-title').innerText(), 'Could not load requirements');
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'failed');
      assert.equal(await reqStatus(page).innerText(), 'Nexus asked ModBridge to slow down. Wait a minute and try again.');
      assert.doesNotMatch(await page.locator('#details-content').innerText(), /HOST TEXT|No requirements listed/);
      assert.equal(await reqSection(page).locator('.req-row, .req-state').count(), 0);
      assert.equal(await page.locator('#announcer').innerText(), 'Could not load requirements.');
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'the optional section raises no page-wide error');
      // A failure is kept: reopening shows it again without asking the host; only Retry asks.
      await page.keyboard.press('Escape'); await openModSheet(page);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'failed');
      assert.deepEqual(fake.requests, ['zeta/30']);
      // Retry: the control stays (as "Retrying…", disabled for the page but still a stop) and keeps focus; a second press does nothing.
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      fake.reply = requirementsReply();
      await page.getByRole('button', { name: 'Retry' }).click();
      await page.locator('[data-requirements-status="loading"]').waitFor();
      assert.equal(await page.locator('[data-requirements-status="loading"]').getAttribute('aria-busy'), 'true');
      assert.equal(await sheetFocus(page), 'req-retry');
      assert.deepEqual(await page.locator('[data-focus-key="req-retry"]').evaluate(el => [el.textContent, el.getAttribute('aria-disabled')]), ['Retrying…', 'true']);
      await page.keyboard.press('Enter');
      assert.deepEqual(fake.requests, ['zeta/30', 'zeta/30'], 'one retry in flight, not two');
      release();
      await page.locator('[data-requirements-status="listed"]').waitFor();
      assert.equal(await page.evaluate(() => document.activeElement.classList.contains('req-scroll')), true, 'focus moves on to the list it replaced');
      assert.equal(await reqSection(page).locator('.req-row').count(), 3);
      assert.equal(await reqSection(page).locator('[data-requirements-retry]').count(), 0);
      await clean(page, api);
    });

    await test('requirements: unlisted codes and an unreachable host read generically; host text is never shown', async () => {
      const fake = requirementsApi();
      fake.reply = { status: 500, body: { error: { code: 'weird_host_code', message: 'secret host text' } } };
      const { page, api, context } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      assert.equal(await reqStatus(page).innerText(), 'Request failed (500)');
      await page.keyboard.press('Escape');
      await context.route(`${origin}/api/games/**/requirements`, route => route.abort());
      await openModSheet(page);
      await page.getByRole('button', { name: 'Retry' }).click();
      await page.locator('[data-requirements-retry]').waitFor();
      assert.equal(await reqStatus(page).innerText(), 'ModBridge could not be reached. Try again.');
      await context.unroute(`${origin}/api/games/**/requirements`);
      for (const code of ['content_hidden', 'reconnect_required', 'payload', 'busy']) {
        fake.reply = { status: code === 'busy' ? 409 : 400, body: { error: { code, message: 'secret host text' } } };
        await page.getByRole('button', { name: 'Retry' }).click();
        await page.locator('[data-requirements-retry]').waitFor();
        assert.equal(await reqStatus(page).innerText(), codeSentences[code], code);
      }
      assert.doesNotMatch(await page.locator('#details-content').innerText(), /secret host text/);
      await clean(page, api, []);
    });

    await test('requirements: the section is hidden, and the host never asked, unless mod_requirements is exactly true', async () => {
      for (const capabilities of [{ mod_requirements: false }, { mod_requirements: 'yes' }, { mod_requirements: 1 }, {}]) {
        const fake = requirementsApi(); fake.reply = requirementsReply();
        const { page, api, context } = await fixture({ capabilities, get: fake.get });
        await openFiles(page);
        await openFileSheet(page);
        assert.equal(await page.locator('#details-content [data-requirements], #details-content .req-section').count(), 0, JSON.stringify(capabilities));
        assert.doesNotMatch(await page.locator('#details-dialog').innerText(), /Requirements|Not checked/, JSON.stringify(capabilities));
        await page.keyboard.press('Escape');
        await context.close();
        assert.deepEqual(fake.requests, [], JSON.stringify(capabilities));
        assert.equal(api.requests.some(request => /requirements/.test(request)), false, JSON.stringify(capabilities));
      }
      // Only mods and files are asked about: a game sheet has no section even when the host advertises the route.
      const fake = requirementsApi(); fake.reply = requirementsReply();
      const { page, api } = await requirementsFixture(fake);
      await page.locator('[data-card="game:zeta"]').focus(); await page.keyboard.press('F6');
      await page.locator('#details-dialog[open]').waitFor();
      assert.equal(await page.locator('#details-content [data-requirements]').count(), 0);
      await page.keyboard.press('Escape');
      assert.deepEqual(fake.requests, []);
      await clean(page, api);
    });

    await test('requirements: a mod id the host cannot take is not asked; the largest one is', async () => {
      const fake = requirementsApi(); fake.reply = (game, mod) => requirementsReply({ mod_id: mod });
      const mods = [...zetaMods(), { mod_id: 2147483648, game_domain: 'zeta', name: 'Huge id', author: 'A', summary: 's', version: '1' }, { mod_id: 2147483647, game_domain: 'zeta', name: 'Largest id', author: 'A', summary: 's', version: '1' }];
      const { page, api } = await requirementsFixture(fake, { mods: { zeta: mods } });
      await openZeta(page);
      await openModSheet(page, 2147483648);
      assert.equal(await page.locator('#details-content [data-requirements]').count(), 0);
      await page.keyboard.press('Escape');
      assert.deepEqual(fake.requests, []);
      await openModSheet(page, 2147483647);
      await reqSettled(page);
      assert.deepEqual(fake.requests, ['zeta/2147483647']);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'listed');
      await clean(page, api);
    });

    await test('requirements: every reply that is not exactly the documented shape is a failed request, never a partial list', async () => {
      const many = (make, count) => Array.from({ length: count }, (_, index) => make(index));
      const cases = [
        ['an extra top-level key', body => ({ ...body, apikey: 'x' })],
        ['a missing top-level key', body => { const { installation_supported: _, ...rest } = body; return rest; }],
        ['another game echoed', body => ({ ...body, game: 'alpha' })],
        ['another mod echoed', body => ({ ...body, mod_id: 31 })],
        ['mod_id as a string', body => ({ ...body, mod_id: '30' })],
        ['discovery_complete false', body => ({ ...body, discovery_complete: false })],
        ['discovery_complete missing', body => { const { discovery_complete: _, ...rest } = body; return rest; }],
        ['discovery_complete as a string', body => ({ ...body, discovery_complete: 'true' })],
        ['file constraints claimed', body => ({ ...body, file_version_constraints_provided: true })],
        ['installation claimed', body => ({ ...body, installation_supported: true })],
        ['a list that is not an array', body => ({ ...body, nexus_requirements: {} })],
        ['a null DLC list', body => ({ ...body, dlc_requirements: null })],
        ['satisfaction "met" on a Nexus row', body => ({ ...body, nexus_requirements: [nexusRow({ satisfaction: 'met' })] })],
        ['satisfaction "installed" on a DLC row', body => ({ ...body, dlc_requirements: [dlcRow({ satisfaction: 'installed' })] })],
        ['satisfaction missing on a row', body => ({ ...body, nexus_requirements: [(({ satisfaction: _, ...rest }) => rest)(nexusRow())] })],
        ['satisfaction true', body => ({ ...body, nexus_requirements: [nexusRow({ satisfaction: true })] })],
        ['an extra key on a Nexus row', body => ({ ...body, nexus_requirements: [nexusRow({ installed: true })] })],
        ['an extra key on a DLC row', body => ({ ...body, dlc_requirements: [dlcRow({ url: 'https://example.test/' })] })],
        ['a row that is not an object', body => ({ ...body, nexus_requirements: ['x'] })],
        ['external_requirement as a string', body => ({ ...body, nexus_requirements: [nexusRow({ external_requirement: 'false' })] })],
        ['a Nexus row with a padded mod id', body => ({ ...body, nexus_requirements: [nexusRow({ mod_id: '05' })] })],
        ['a Nexus row with a numeric mod id', body => ({ ...body, nexus_requirements: [nexusRow({ mod_id: 5 })] })],
        ['a Nexus row with an empty mod id', body => ({ ...body, nexus_requirements: [nexusRow({ mod_id: '' })] })],
        ['a Nexus row with a mod id past 63 bits', body => ({ ...body, nexus_requirements: [nexusRow({ mod_id: '9223372036854775808' })] })],
        ['a Nexus row with a bad domain', body => ({ ...body, nexus_requirements: [nexusRow({ game: 'Bad Game' })] })],
        ['an http link', body => ({ ...body, nexus_requirements: [nexusRow({ url: 'http://www.nexusmods.com/zeta/mods/5' })] })],
        ['a javascript link', body => ({ ...body, nexus_requirements: [externalRow({ url: 'javascript:alert(1)' })] })],
        ['a data link', body => ({ ...body, nexus_requirements: [externalRow({ url: 'data:text/html,x' })] })],
        ['a scheme-relative link', body => ({ ...body, nexus_requirements: [externalRow({ url: '//example.test/x' })] })],
        ['a relative link', body => ({ ...body, nexus_requirements: [externalRow({ url: 'www.nexusmods.com/zeta/mods/5' })] })],
        ['a link with credentials', body => ({ ...body, nexus_requirements: [externalRow({ url: 'https://user:pw@example.test/' })] })],
        ['a link with a port', body => ({ ...body, nexus_requirements: [externalRow({ url: 'https://example.test:8443/' })] })],
        ['a link with a space', body => ({ ...body, nexus_requirements: [externalRow({ url: 'https://example.test/a b' })] })],
        ['a link with a backslash', body => ({ ...body, nexus_requirements: [externalRow({ url: 'https://example.test\\evil/' })] })],
        ['a link that is not text', body => ({ ...body, nexus_requirements: [externalRow({ url: null })] })],
        ['a link over 8192 characters', body => ({ ...body, nexus_requirements: [externalRow({ url: `https://example.test/${'x'.repeat(8200)}` })] })],
        ['duplicate relation ids', body => ({ ...body, nexus_requirements: [nexusRow(), externalRow({ id: 'rel-1' })] })],
        ['an empty relation id', body => ({ ...body, nexus_requirements: [nexusRow({ id: '' })] })],
        ['a numeric relation id', body => ({ ...body, nexus_requirements: [nexusRow({ id: 1 })] })],
        ['a name over 1024 characters', body => ({ ...body, nexus_requirements: [nexusRow({ name: 'x'.repeat(1025) })] })],
        ['a name that is not text', body => ({ ...body, nexus_requirements: [nexusRow({ name: 5 })] })],
        ['notes over 65536 characters', body => ({ ...body, nexus_requirements: [nexusRow({ notes: 'x'.repeat(65537) })] })],
        ['notes that are null', body => ({ ...body, nexus_requirements: [nexusRow({ notes: null })] })],
        ['more than 1024 Nexus rows', body => ({ ...body, nexus_requirements: many(index => nexusRow({ id: `rel-${index}`, mod_id: String(index + 1) }), 1025) })],
        ['more than 1024 DLC rows', body => ({ ...body, dlc_requirements: many(index => dlcRow({ id: `dlc-${index}` }), 1025) })],
        ['a DLC row with a bad domain', body => ({ ...body, dlc_requirements: [dlcRow({ game: 'Not a domain' })] })],
        ['a DLC row without an id', body => ({ ...body, dlc_requirements: [dlcRow({ id: '' })] })],
        ['a DLC name over 1024 characters', body => ({ ...body, dlc_requirements: [dlcRow({ name: 'x'.repeat(1025) })] })],
        ['an array instead of an object', () => []],
        ['null instead of an object', () => null],
        ['an empty object', () => ({})],
      ];
      const fake = requirementsApi();
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page);
      for (const [index, [name, mutate]] of cases.entries()) {
        fake.reply = { status: 200, body: mutate(requirementsBody()) };
        if (index === 0) await openModSheet(page); else await page.locator('[data-requirements-retry]').click();
        await page.locator('[data-requirements-retry]').waitFor();
        assert.equal(fake.requests.length, index + 1, name);
        assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'failed', name);
        assert.equal(await reqStatus(page).innerText(), invalidSentence, name);
        assert.equal(await reqSection(page).locator('.req-row, .req-state, .req-link, .req-scroll').count(), 0, `${name}: no partial list`);
      }
      // The harness is not failing everything: the unmodified reply is accepted.
      fake.reply = requirementsReply();
      await page.locator('[data-requirements-retry]').click();
      await page.locator('[data-requirements-status="listed"]').waitFor();
      assert.equal(await reqSection(page).locator('.req-row').count(), 3);
      await clean(page, api);
    });

    await test('requirements: edge rows the host may send are accepted (default port, empty name, empty notes, one list empty)', async () => {
      const fake = requirementsApi();
      fake.reply = requirementsReply({ nexus_requirements: [externalRow({ url: 'https://example.test:443/x', name: '', notes: '', game: '', mod_id: '' }), nexusRow({ id: 'rel-9', mod_id: '9223372036854775807', name: '', notes: '', url: 'https://nexusmods.com/zeta/mods/9223372036854775807' })], dlc_requirements: [] });
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'listed');
      assert.deepEqual(await reqSection(page).locator('.req-name').allInnerTexts(), ['Unnamed requirement', 'Unnamed Nexus mod']);
      assert.deepEqual(await reqSection(page).locator('.req-link').evaluateAll(els => els.map(el => el.getAttribute('href'))), ['https://example.test/x', 'https://nexusmods.com/zeta/mods/9223372036854775807']);
      await clean(page, api);
    });

    await test('requirements: the wording rules hold in every state; no verdict word, glyph or green anywhere', async () => {
      const forbidden = /\b(installed|met|satisfied|missing|ok|okay|required|requires|dependenc(?:y|ies)|prerequisites?|complete|completed|everything|ready)\b|[✓✔✅☑]/i;
      const fake = requirementsApi();
      // Downloaded and queued: the file has an analyzed job and sits in the selection, and its rows still say "Not checked".
      const { page, api } = await requirementsFixture(fake, { jobs: [job(hex('a'))] });
      fake.reply = { status: 400, body: { error: { code: 'network', message: 'x' } } };
      await openFiles(page);
      await page.locator('[data-basket-add="101"]').click();
      const download = page.locator('[data-file-action="101"]');
      const before = [await download.isEnabled(), await download.innerText()];
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      await openFileSheet(page);
      await page.locator('[data-requirements-status="loading"]').waitFor();
      const states = {};
      states.loading = await reqText(page);
      release(); await reqSettled(page);
      states.failed = await reqText(page);
      assert.deepEqual([await download.isEnabled(), await download.innerText()], before, 'a failed list changes nothing about Download');
      fake.gate = null; fake.reply = requirementsReply(); await page.getByRole('button', { name: 'Retry' }).click(); await page.locator('[data-requirements-status="listed"]').waitFor();
      states.listed = (await reqText(page)).replace(introSentence, '');
      assert.equal(await page.locator('#details-content .req-state').count(), 3);
      assert.deepEqual(await page.locator('#details-content .req-state').allInnerTexts(), Array(3).fill('Not checked'), 'a downloaded, queued file\'s rows are still "Not checked"');
      assert.deepEqual([await download.isEnabled(), await download.innerText()], before, 'a listed set changes nothing about Download');
      for (const [state, text] of Object.entries(states)) assert.doesNotMatch(text, forbidden, state);
      assert.doesNotMatch(await page.locator('#details-content').innerText(), /\b(Installed|Met|Satisfied|Missing)\b|[✓✔✅☑]/, 'nowhere in the sheet');
      assert.deepEqual((await page.locator('#details-content .detail-label').allInnerTexts()).filter(label => /^(required|dependenc|prerequisite)/i.test(label.trim())), [], 'no heading calls the list required');
      assert.equal(await reqSection(page).locator('button').count(), 0);
      assert.doesNotMatch(await reqSection(page).locator('a').allInnerTexts().then(texts => texts.join(' | ')), /download|install|prepare|add|queue/i, 'no control acts on a requirement');
      // The chip is neutral: never a green fill, text or border.
      const colors = await page.locator('#details-content .req-state').first().evaluate(el => { const style = getComputedStyle(el); return [style.color, style.backgroundColor, style.borderTopColor]; });
      for (const color of colors) {
        const [r, g, b] = color.match(/\d+(\.\d+)?/g).map(Number);
        assert.ok(!(g > r + 8 && g > b + 8), `${color} must not be green`);
      }
      // The announcer and the whole page use the same words.
      assert.doesNotMatch(await page.locator('#announcer').innerText(), forbidden);
      await clean(page, api);
    });

    await test('requirements: Retry and links are focus stops reached by arrows, the D-pad and Cross, and a long list is a scroll region', async () => {
      const fake = requirementsApi();
      fake.reply = { status: 400, body: { error: { code: 'timeout', message: 'x' } } };
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page); await openModSheet(page);
      await reqSettled(page);
      // Keyboard: Down from Close walks the sheet in order and reaches Retry (a long description scrolls before it hands focus on); Enter presses it.
      const seen = [await sheetFocus(page)];
      for (let n = 0; n < 40 && seen.at(-1) !== 'Back'; n++) { await page.keyboard.press('ArrowDown'); const key = await sheetFocus(page); if (key !== seen.at(-1)) seen.push(key); }
      assert.deepEqual(seen, ['Close details', 'Summary', 'Description', 'req-retry', 'details-nexus-link', 'Back']);
      await page.keyboard.press('ArrowUp'); await page.keyboard.press('ArrowUp');
      assert.equal(await sheetFocus(page), 'req-retry');
      fake.reply = requirementsReply({ nexus_requirements: [nexusRow(), externalRow(), nexusRow({ id: 'rel-3', name: 'Third', url: '' }), externalRow({ id: 'rel-4', name: 'Fourth', url: '' })] });
      await page.keyboard.press('Enter');
      await page.locator('[data-requirements-status="listed"]').waitFor();
      assert.equal(await page.evaluate(() => document.activeElement.classList.contains('req-scroll')), true);
      assert.equal(await page.locator('.req-scroll').getAttribute('tabindex'), '0');
      assert.equal(await page.locator('.req-scroll').getAttribute('role'), 'group');
      assert.equal(await page.locator('.req-scroll').evaluate(el => el.scrollHeight > el.clientHeight), true, 'five rows do not fit: the list scrolls, and a controller can focus it');
      const links = [];
      for (let n = 0; n < 14 && !links.includes('Back'); n++) { await page.keyboard.press('ArrowDown'); const key = await sheetFocus(page); if (!links.length || links.at(-1) !== key) links.push(key); }
      assert.deepEqual(links.filter(key => key.startsWith('req-link') || key === 'details-nexus-link' || key === 'Back'), ['req-link-0', 'req-link-1', 'details-nexus-link', 'Back'], 'both links are reached, then the sheet\'s own actions');
      await page.keyboard.press('Escape');
      // Controller: the same walk with the D-pad on a fresh failure, and Cross presses Retry.
      const second = requirementsApi(); second.reply = { status: 400, body: { error: { code: 'timeout', message: 'x' } } };
      const { page: pad, api: padApi } = await requirementsFixture(second);
      await openZeta(pad); await pad.locator('[data-card="mod:30"]').focus(); await press(pad, OPTIONS);
      await pad.locator('#details-dialog[open]').waitFor(); await reqSettled(pad);
      await press(pad, R2);
      assert.equal(await sheetFocus(pad), 'Back', 'R2 jumps to the sheet\'s last control');
      await press(pad, UP); await press(pad, UP);
      assert.equal(await sheetFocus(pad), 'req-retry', 'two D-pad presses up from Back reach Retry');
      second.reply = requirementsReply();
      await press(pad, CROSS);
      await pad.locator('[data-requirements-status="listed"]').waitFor();
      assert.deepEqual(second.requests, ['zeta/30', 'zeta/30'], 'Cross asked once more');
      const keys = [];
      for (let n = 0; n < 12 && !keys.includes('Back'); n++) { await press(pad, DOWN); keys.push(await sheetFocus(pad)); }
      assert.ok(keys.includes('req-link-0') && keys.includes('req-link-1'), `D-pad reaches both links: ${keys.join(', ')}`);
      assert.equal(keys.at(-1), 'Back');
      await press(pad, CIRCLE);
      assert.equal(await pad.locator('#details-dialog').evaluate(el => el.open), false, 'Circle closes');
      assert.equal(await active(pad), 'mod:30');
      await clean(page, api); await clean(pad, padApi);
    });

    await test('requirements: the host is asked once per game and mod for the session, and a late reply is not lost', async () => {
      const fake = requirementsApi(); fake.reply = (game, mod) => requirementsReply({ mod_id: mod });
      let release; fake.gate = new Promise(resolve => { release = resolve; });
      const { page, api } = await requirementsFixture(fake);
      await openZeta(page);
      // Open, close and reopen while the first request is still in flight: still one request.
      await openModSheet(page);
      await page.locator('[data-requirements-status="loading"]').waitFor();
      await page.keyboard.press('Escape');
      await openModSheet(page);
      await page.locator('[data-requirements-status="loading"]').waitFor();
      assert.deepEqual(fake.requests, ['zeta/30']);
      // Close, let the reply arrive with no sheet open, reopen: drawn from the page's copy at once.
      await page.keyboard.press('Escape');
      release(); fake.gate = null;
      await page.waitForTimeout(150);
      await openModSheet(page);
      assert.equal(await reqStatus(page).getAttribute('data-requirements-status'), 'listed');
      assert.equal(await reqSection(page).locator('.req-row').count(), 3);
      await page.keyboard.press('Escape');
      // Through the files and back, for the mod and for its file: the same one request.
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await openFileSheet(page, 'Main file'); await page.keyboard.press('Escape');
      await openFileSheet(page, 'Optional extras'); assert.equal(await reqSection(page).locator('.req-row').count(), 3); await page.keyboard.press('Escape');
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click(); await page.locator('[data-card^="mod:"]').first().waitFor();
      await openModSheet(page); assert.equal(await reqSection(page).locator('.req-row').count(), 3); await page.keyboard.press('Escape');
      assert.deepEqual(fake.requests, ['zeta/30'], 'one request for zeta/30 in the whole session');
      // Another mod is another request, once.
      await openModSheet(page, 12); await reqSettled(page); await page.keyboard.press('Escape');
      await openModSheet(page, 12); await page.keyboard.press('Escape');
      assert.deepEqual(fake.requests, ['zeta/30', 'zeta/12']);
      // The reply is checked against the mod it was asked for: another mod's list is refused.
      fake.reply = () => requirementsReply({ mod_id: 99 });
      await openModSheet(page, 7); await reqSettled(page);
      assert.equal(await reqStatus(page).innerText(), invalidSentence);
      await clean(page, api);
    });

    await test('requirements: the section fits the TV safe area and type floor at 1920x1080 and 1280x720, with long text', async () => {
      const longNote = `${'Long note words '.repeat(60)}${'U'.repeat(200)}`;
      const body = (rows = 6) => requirementsReply({
        nexus_requirements: Array.from({ length: rows }, (_, index) => index % 2 ? externalRow({ id: `rel-${index}`, name: `${longName} ${index}`, notes: longNote, url: `https://example.test/${'p'.repeat(150)}` }) : nexusRow({ id: `rel-${index}`, mod_id: String(index + 1), name: `${longName} ${index}`, notes: longNote })),
        dlc_requirements: [dlcRow({ name: longName, notes: longNote })],
      });
      for (const viewport of [FULL_HD, HD]) {
        const fake = requirementsApi(); fake.reply = body();
        const { page, api } = await requirementsFixture(fake, { viewport });
        await openZeta(page); await openModSheet(page); await reqSettled(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, listed`);
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, listed`);
        // Focus the list and a link inside it: still inside the safe area, nothing overlaps.
        await page.locator('.req-scroll').focus();
        await page.locator('.req-link').first().focus();
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, link focused`);
        await page.keyboard.press('Escape');
        fake.reply = { status: 400, body: { error: { code: 'rate_limit', message: 'x' } } };
        await openModSheet(page, 12); await reqSettled(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, failed`);
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, failed`);
        await clean(page, api);
        await page.context().close();
      }
    });

    // ---- Select required mods (docs/INSTALL-FLOW-UX.md 3.14): the main file of each Nexus mod the author lists, added to the selection ----
    const mainOf = (id, name, extra = {}) => file(id, name, { is_primary: true, category_id: 1, category_name: 'MAIN', ...extra });
    const optionOf = (id, name, extra = {}) => file(id, name, { is_primary: false, category_id: 3, category_name: 'OPTION', ...extra });
    const selectFiles = () => ({
      30: [mainOf(301, 'Zebra main'), optionOf(302, 'Zebra extras')],
      5: [mainOf(51, 'Camera main')],
      6: [mainOf(61, 'Lamp main'), file(62, 'Lamp old', { is_primary: false, category_id: 4, category_name: 'OLD_VERSION' })],
      7: [mainOf(71, 'Fog main'), mainOf(72, 'Fog other main')],
      8: [mainOf(81, 'Retry main')],
      10: [file(101, 'No hints at all')],
      11: [],
    });
    // One row of every kind, in the order the author might list them.
    const selectRows = () => [
      nexusRow({ id: 'r1', mod_id: '5', name: 'Amber camera framework' }),
      nexusRow({ id: 'r2', mod_id: '6', name: 'Lamp pack', notes: '', url: '' }),
      externalRow({ id: 'r3', name: 'UE4SS' }),
      nexusRow({ id: 'r4', mod_id: '7', name: 'Fog twins' }),
      nexusRow({ id: 'r5', mod_id: '8', name: 'Unreachable mod' }),
      nexusRow({ id: 'r6', mod_id: '10', name: 'Unhinted mod' }),
      nexusRow({ id: 'r7', game: 'alpha', mod_id: '9', name: 'Other game mod', url: 'https://www.nexusmods.com/alpha/mods/9' }),
      nexusRow({ id: 'r8', mod_id: '11', name: 'Empty files mod' }),
    ];
    // The requirements fake plus the files route: it records each lookup ("game/mod") and fails the mods named in `failMods` as the host would.
    const selectApi = () => {
      const fake = requirementsApi(), requirementsGet = fake.get;
      fake.fileRequests = []; fake.failMods = new Set([8]);
      fake.get = async url => {
        const files = /^\/api\/games\/([^/]+)\/mods\/(\d+)\/files$/.exec(url.pathname);
        if (!files) return requirementsGet(url);
        fake.fileRequests.push(`${files[1]}/${files[2]}`);
        return fake.failMods.has(Number(files[2])) ? { status: 502, body: { error: { code: 'provider_unavailable', message: 'host text must never show' } } } : undefined;
      };
      fake.reply = requirementsReply({ nexus_requirements: selectRows(), dlc_requirements: [dlcRow()] });
      return fake;
    };
    const selectFixture = (fake, extra = {}) => requirementsFixture(fake, { files: selectFiles(), ...extra });
    const selectRun = async page => { await page.locator('[data-req-select-run]').click(); await page.locator('[data-req-select-status="done"]').waitFor(); };
    const outcomesOf = page => page.locator('[data-req-outcome]').evaluateAll(rows => rows.map(row => [row.dataset.reqOutcome, row.querySelector('.req-name').textContent, row.querySelector('.req-notes').textContent]));
    const selectSummary = page => page.locator('[data-req-select-status="done"]').innerText();
    const basketRows = page => page.locator('.basket-item').evaluateAll(items => items.map(item => item.querySelector('p').textContent));
    const verdictWords = /\b(installed|satisfied|met|missing|ok|fulfilled|resolved|complete)\b|[✓✔]/i;

    await test('select required mods: adds the mod\'s main file and each listed Nexus mod\'s, says why the rest could not be added, and downloads nothing', async () => {
      const fake = selectApi();
      const { page, api } = await selectFixture(fake);
      await openZeta(page); await openModSheet(page); await reqSettled(page);
      assert.equal(await page.locator('[data-req-select-run]').innerText(), 'Select required mods');
      assert.equal(await page.locator('[data-req-select-note]').innerText(), 'Adds the main file of each Nexus mod the author lists, together with this mod’s main file, to your selection. Nothing downloads until you press Download selected.');
      assert.deepEqual(fake.fileRequests, [], 'nothing is asked of the host until the button is pressed');
      assert.equal(await page.locator('#details-content [data-requirements] button').count(), 0, 'the author\'s list itself still has no control');
      await selectRun(page);
      // The mod itself first, then each row as the author listed it; outside, DLC and other-game rows ask nothing.
      assert.deepEqual(fake.fileRequests, ['zeta/30', 'zeta/5', 'zeta/6', 'zeta/7', 'zeta/8', 'zeta/10', 'zeta/11']);
      assert.deepEqual(await outcomesOf(page), [
        ['added', 'This mod', 'Added to your selection: Zebra main'],
        ['added', 'Amber camera framework', 'Added to your selection: Camera main'],
        ['added', 'Lamp pack', 'Added to your selection: Lamp main'],
        ['external', 'UE4SS', 'Not on Nexus, so there is nothing to select.'],
        ['choose', 'Fog twins', 'Choose a file. The author has marked more than one main file. Open the mod and use Add to selection.'],
        ['failed', 'Unreachable mod', 'Could not look up its files. Nexus could not be reached. Try again.'],
        ['choose', 'Unhinted mod', 'Choose a file. Nexus did not say which file is the main one. Open the mod and use Add to selection.'],
        ['other_game', 'Other game mod', 'Belongs to another game, so it cannot join this selection.'],
        ['choose', 'Empty files mod', 'Choose a file. The author has marked no main file. Open the mod and use Add to selection.'],
        ['dlc', 'DLC: Expansion One', 'A game expansion, so there is nothing to select.'],
      ]);
      assert.equal(await selectSummary(page), 'Added 3 files to your selection. 7 items could not be added.');
      assert.equal(await page.locator('#announcer').innerText(), 'Added 3 files to your selection. 7 items could not be added.');
      assert.doesNotMatch(await page.locator('#details-content').innerText(), /host text/);
      // The selection holds exactly the three main files, for the right mods, with fresh intent keys; no download was asked for.
      assert.deepEqual(await basketRows(page), ['Zebra textures · zeta · mod 30 / file 301', 'Amber camera framework · zeta · mod 5 / file 51', 'Lamp pack · zeta · mod 6 / file 61']);
      assert.equal(await page.locator('#basket-heading').innerText().then(text => text.replace(/\s+/g, ' ').trim()), 'Selected files (3/24)');
      assert.deepEqual(api.posts, []);
      // Wording rules of 3.8: the author's rows read "Not checked" still, and nothing here says installed, met or satisfied.
      assert.deepEqual(await reqSection(page).locator('.req-state').allInnerTexts(), Array(9).fill('Not checked'));
      const sheet = (await page.locator('#details-content').innerText()).replace(introSentence, '');
      assert.doesNotMatch(sheet, verdictWords);
      assert.equal(await sheetFocus(page), 'req-select', 'focus stays on the button that was pressed');
      // A second press adds nothing: what is already selected is said so, and the host is asked again for the rest only as before.
      await selectRun(page);
      assert.equal(await selectSummary(page), 'Nothing new was added to your selection. 7 items could not be added.');
      assert.deepEqual((await outcomesOf(page)).slice(0, 3).map(row => row[0]), ['already', 'already', 'already']);
      assert.equal((await outcomesOf(page))[1][2], 'Already in your selection: Camera main');
      assert.equal(await page.locator('.basket-item').count(), 3);
      assert.deepEqual(api.posts, []);
      await clean(page, api);
    });

    await test('select required mods: a file sheet adds that very file, files already requested are skipped, and a retry fills a failed lookup', async () => {
      const fake = selectApi();
      const { page, api } = await selectFixture(fake);
      await openFiles(page);
      assert.deepEqual(fake.fileRequests, ['zeta/30'], 'opening the mod asked for its files');
      api.jobs = [job('a'.repeat(32), { mod_id: 6, file_id: 61, status: 'analyzed' }), job('b'.repeat(32), { mod_id: 5, file_id: 51, status: 'failed' }), job('c'.repeat(32), { mod_id: 7, file_id: 71, status: 'queued' })];
      await openFileSheet(page, 'Zebra extras'); await reqSettled(page);
      assert.equal(await page.locator('[data-req-select-note]').innerText(), 'Adds the main file of each Nexus mod the author lists, together with this file, to your selection. Nothing downloads until you press Download selected.');
      await selectRun(page);
      assert.deepEqual(fake.fileRequests, ['zeta/30', 'zeta/5', 'zeta/6', 'zeta/7', 'zeta/8', 'zeta/10', 'zeta/11'], 'the open mod\'s files were not asked for again');
      const outcomes = await outcomesOf(page);
      assert.deepEqual(outcomes.slice(0, 3), [
        ['added', 'This file', 'Added to your selection: Zebra extras'],
        ['added', 'Amber camera framework', 'Added to your selection: Camera main'],    // its earlier job failed: it is not "requested"
        ['requested', 'Lamp pack', 'Already requested: Lamp main'],                     // a finished job for the file
      ]);
      assert.equal(outcomes[4][0], 'choose', 'a queued job for one of two main files does not make the mod\'s main file unique');
      assert.equal(await selectSummary(page), 'Added 2 files to your selection. 7 items could not be added.');
      assert.deepEqual(await basketRows(page), ['Zebra textures · zeta · mod 30 / file 302', 'Amber camera framework · zeta · mod 5 / file 51']);
      // Remove one from the selection; the next press adds just that one and reports the rest as already selected.
      await page.keyboard.press('Escape');
      await page.getByRole('button', { name: 'Remove Camera main', exact: true }).click();
      assert.equal(await page.locator('.basket-item').count(), 1);
      fake.failMods.clear();
      await openFileSheet(page, 'Zebra extras'); await reqSettled(page);
      assert.equal(await page.locator('[data-req-select-status]').count(), 0, 'a new sheet starts without the last result');
      await selectRun(page);
      const again = await outcomesOf(page);
      assert.deepEqual(again.slice(0, 3).map(row => row[0]), ['already', 'added', 'requested']);
      assert.deepEqual(again.find(row => row[1] === 'Unreachable mod'), ['added', 'Unreachable mod', 'Added to your selection: Retry main'], 'the lookup that failed succeeds now');
      assert.deepEqual(await basketRows(page), ['Zebra textures · zeta · mod 30 / file 302', 'Amber camera framework · zeta · mod 5 / file 51', 'Unreachable mod · zeta · mod 8 / file 81']);
      assert.deepEqual(api.posts, []);
      await clean(page, api);
    });

    await test('select required mods: the selection is built whole or not at all, within the host\'s room, and the host\'s own capacity decides', async () => {
      for (const [queued, expected] of [[2, 'none'], [3, 'all']]) {
        const fake = selectApi(); fake.reply = requirementsReply({ nexus_requirements: [selectRows()[0], selectRows()[1]], dlc_requirements: [] });
        const { page, api } = await selectFixture(fake, { capabilities: { mod_requirements: true, job_capacity: { max_queued: queued, max_records: 10 } } });
        await openZeta(page); await openModSheet(page); await reqSettled(page);
        await selectRun(page);
        if (expected === 'none') {
          assert.equal(await selectSummary(page), 'Nothing was added: this would make 3 files and the host can take 2 more downloads now. Remove some files or wait for downloads to finish.');
          assert.deepEqual((await outcomesOf(page)).map(row => row[0]), ['full', 'full', 'full']);
          assert.equal((await outcomesOf(page))[0][2], 'Not added: the selection would be larger than the host can take now.');
          assert.equal(await page.locator('.basket-item').count(), 0, 'no half-built selection');
        } else {
          assert.equal(await selectSummary(page), 'Added 3 files to your selection.');
          assert.equal(await page.locator('#basket-heading').innerText().then(text => text.replace(/\s+/g, ' ').trim()), 'Selected files (3/3)');
        }
        assert.deepEqual(api.posts, []);
        await clean(page, api);
        await page.context().close();
      }
    });

    await test('select required mods: it is offered only where it can work, and is a control a controller reaches and presses', async () => {
      const cases = [
        ['only outside and DLC rows', {}, requirementsReply({ nexus_requirements: [externalRow()], dlc_requirements: [dlcRow()] })],
        ['no rows at all', {}, requirementsReply({ nexus_requirements: [], dlc_requirements: [] })],
        ['downloads are off', { capabilities: { mod_requirements: true, download_analyze: false } }, null],
        ['jobs are off', { capabilities: { mod_requirements: true, jobs: false } }, null],
      ];
      for (const [label, extra, reply] of cases) {
        const fake = selectApi(); if (reply) fake.reply = reply;
        const { page, api } = await selectFixture(fake, extra);
        await openZeta(page); await openModSheet(page); await reqSettled(page);
        assert.equal(await page.locator('[data-req-select-run]').count(), 0, label);
        assert.equal(await page.locator('[data-req-select]').innerText(), '', label);
        assert.deepEqual(fake.fileRequests, [], label);
        await clean(page, api);
        await page.context().close();
      }
      // A failed requirements request offers nothing to select; once it loads, the button appears.
      const fake = selectApi(); fake.reply = { status: 400, body: { error: { code: 'rate_limit', message: 'x' } } };
      const { page, api } = await selectFixture(fake);
      await openZeta(page); await openModSheet(page); await reqSettled(page);
      assert.equal(await page.locator('[data-req-select-run]').count(), 0);
      fake.reply = requirementsReply({ nexus_requirements: [selectRows()[0]], dlc_requirements: [] });
      await page.locator('[data-requirements-retry]').click();
      await page.locator('[data-req-select-run]').waitFor();
      // Reached with the keyboard's Tab order and pressed with Cross on the pad.
      await page.locator('[data-req-select-run]').focus();
      assert.equal(await sheetFocus(page), 'req-select');
      await press(page, CROSS);
      await page.locator('[data-req-select-status="done"]').waitFor();
      assert.equal(await selectSummary(page), 'Added 2 files to your selection.');
      assert.equal(await sheetFocus(page), 'req-select');
      // The result is a scroll region the pad can stand on, and the sheet fits the TV at both sizes.
      assert.equal(await page.locator('.req-select .req-scroll').getAttribute('tabindex'), '0');
      await clean(page, api);
      await page.context().close();
      for (const viewport of [FULL_HD, HD]) {
        const wide = selectApi();
        const view = await selectFixture(wide, { viewport });
        await openZeta(view.page); await openModSheet(view.page); await reqSettled(view.page); await selectRun(view.page);
        assert.deepEqual(await smallText(view.page), [], `${viewport.width}: type floor with a result`);
        assert.deepEqual(await layoutProblems(view.page), [], `${viewport.width}: layout with a result`);
        await clean(view.page, view.api);
        await view.page.context().close();
      }
    });

    // ---- Nexus search (docs/INSTALL-FLOW-UX.md 3.9): GET /api/games/{game}/mods/search?q=<text>&page=<n> ----
    // A fake of the route: records each request (game, text, page, raw query string) and may hold a reply back per text with `holds`.
    const searchApi = () => {
      const fake = { requests: [], reply: null, holds: new Map() };
      fake.get = async url => {
        const match = /^\/api\/games\/([^/]+)\/mods\/search$/.exec(url.pathname);
        if (!match) return undefined;
        const text = url.searchParams.get('q'), page = Number(url.searchParams.get('page'));
        fake.requests.push({ game: match[1], text, page, raw: url.search });
        if (fake.holds.has(text)) await fake.holds.get(text);
        return typeof fake.reply === 'function' ? fake.reply({ game: match[1], text, page }) : fake.reply;
      };
      return fake;
    };
    const hit = (id, name, extra = {}) => ({ mod_id: id, game_id: 1, game_domain: 'zeta', name, author: 'Ada Lovelace', summary: `About ${name}`, description: '', version: '1.0', contains_adult_content: false, source_url: `https://www.nexusmods.com/zeta/mods/${id}`, ...extra });
    const searchBody = (items, extra = {}) => ({ items, total: items.length, page: 1, pages: items.length ? 1 : 0, ...extra });
    const searchReply = (items, extra = {}) => ({ status: 200, body: searchBody(items, extra) });
    // Two listed mods per page of a result that matched 45 (three pages), or 5000 (the 50-page cap).
    const pagedReply = (total = 45) => ({ page }) => ({ status: 200, body: { items: [hit(page * 10 + 1, `Page ${page} first`), hit(page * 10 + 2, `Page ${page} second`)], total, page, pages: Math.min(50, Math.ceil(total / 20)) } });
    const searchFixture = (fake, extra = {}) => fixture({ capabilities: { mod_search: true }, get: fake.get, ...extra });
    const searchField = page => page.locator('#nexus-search-input');
    const searchButton = page => page.locator('#nexus-search-submit');
    const searchMessage = page => page.locator('#nexus-search-message').innerText();
    const runSearch = async (page, text) => { await searchField(page).fill(text); await searchField(page).press('Enter'); };
    const resultsReady = page => page.locator('#games-content h2.content-title', { hasText: 'Search results from Nexus' }).waitFor();
    const statusBlock = (page, name) => page.locator(`#games-content [data-search-status="${name}"]`);
    const subtitle = page => page.locator('#games-view .subheading').innerText();
    const pagerOf = (page, where = 'top') => ({ prev: page.locator(`[data-search-pager="${where}"] [data-search-page="prev"]`), next: page.locator(`[data-search-pager="${where}"] [data-search-page="next"]`), label: page.locator(`[data-search-pager="${where}"] .search-page`) });
    const settledPage = (page, n, of) => page.locator('[data-search-pager="top"] .search-page:not([aria-busy])', { hasText: `Page ${n} of ${of}` }).waitFor();
    const searchFocus = page => page.evaluate(() => document.activeElement?.dataset?.searchFocus || document.activeElement?.id || '');
    const latestRequests = api => api.requests.filter(request => /^GET \/api\/games\/[^/]+\/mods$/.test(request)).length;
    const rateLimited = { status: 400, body: { error: { code: 'rate_limit', message: 'host text must never show' } } };
    const couldNotSearch = 'Could not search Nexus';
    const refusedSentence = 'The host sent a reply this page could not verify.';

    await test('search: the field and button appear only with a game chosen and mod_search exactly true; the local filter keeps its own label', async () => {
      for (const [label, capabilities] of [['absent', {}], ['false', { mod_search: false }], ['the string "true"', { mod_search: 'true' }], ['1', { mod_search: 1 }], ['null', { mod_search: null }]]) {
        const fake = searchApi(); fake.reply = searchReply([hit(30, 'Zebra textures')]);
        const { page, api } = await fixture({ capabilities, get: fake.get });
        assert.equal(await page.locator('#nexus-search').isHidden(), true, `${label}: no game chosen`);
        await openZeta(page);
        assert.equal(await page.locator('#nexus-search').isHidden(), true, `${label}: game chosen`);
        assert.equal(await searchButton(page).isHidden(), true, label);
        assert.equal(await modFilter(page).isVisible(), true, `${label}: the local filter is still there`);
        assert.match(await count(page), /loaded latest mods · .*filters apply to this list only/, label);
        assert.deepEqual(fake.requests, [], `${label}: the host is never asked`);
        await clean(page, api);
      }
      const fake = searchApi(); fake.reply = searchReply([hit(30, 'Zebra textures')]);
      const { page, api } = await searchFixture(fake);
      assert.equal(await page.locator('#nexus-search').isHidden(), true, 'no game, no search');
      await openZeta(page);
      assert.equal(await page.locator('#nexus-search').isVisible(), true);
      assert.equal(await page.locator('#nexus-search').getAttribute('role'), 'search');
      assert.equal(await page.locator('#nexus-search-label').innerText(), 'Search Nexus by mod name');
      assert.equal(await searchButton(page).innerText(), 'Search Nexus');
      assert.equal(await searchField(page).getAttribute('maxlength'), '100');
      assert.equal(await page.locator('#nexus-search-hint').innerText(), 'This searches Nexus for this game by mod name. The filter below only narrows the latest mods already loaded.');
      assert.equal(await page.locator('#catalog-search-label').innerText(), 'Filter loaded latest mods', 'the filter is never called a search of Nexus');
      // Two controls, two labels: typing in one never touches the other, and the filter never asks the host.
      await modFilter(page).fill('zebra');
      assert.deepEqual(await cards(page, 'mod'), ['30']);
      assert.equal(await searchField(page).inputValue(), '');
      assert.deepEqual(fake.requests, []);
      // A mod's files have no search (the box belongs to the game's mod list).
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      assert.equal(await page.locator('#nexus-search').isHidden(), true);
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      assert.equal(await page.locator('#nexus-search').isVisible(), true);
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      assert.equal(await page.locator('#nexus-search').isHidden(), true);
      await clean(page, api);
    });

    await test('search: the field and button are reached with the arrows and the D-pad, and the field gives up Up and Down', async () => {
      const fake = searchApi(); fake.reply = searchReply([hit(30, 'Zebra textures'), hit(12, 'Amber camera')]);
      const { page, api } = await searchFixture(fake);
      await openZeta(page);
      // Every control of the view, the new ones included, is reached from the tabs with the four directions, each with a focus ring.
      const result = await unreachable(page, 'games');
      assert.ok(result.total > 8, `${result.total} stops`); assert.deepEqual(result.missing, [], 'every stop is reachable'); assert.deepEqual(result.noRing, []);
      // Arrow keys: tab -> field (below it) -> button (right, the caret is at the end of an empty field) -> back to the field.
      await page.locator('#tab-games').focus();
      await page.keyboard.press('ArrowDown'); assert.equal(await active(page), 'nexus-search-input');
      await page.keyboard.press('ArrowRight'); assert.equal(await active(page), 'nexus-search-submit');
      await page.keyboard.press('ArrowLeft'); assert.equal(await active(page), 'nexus-search-input');
      // The field gives up Up and Down like the filter does; typed text stays and keys that edit stay in the field.
      await searchField(page).fill('amber');
      await page.keyboard.press('ArrowLeft'); assert.equal(await active(page), 'nexus-search-input', 'Left inside the text edits the caret');
      await page.keyboard.press('End');
      await page.keyboard.press('ArrowDown'); assert.notEqual(await active(page), 'nexus-search-input', 'Down leaves the field');
      assert.equal(await searchField(page).inputValue(), 'amber');
      await searchField(page).focus(); await page.keyboard.press('ArrowUp'); assert.match(await active(page), /^tab-/, 'Up leaves the field for the tabs');
      // The D-pad calls the same code.
      await searchField(page).focus();
      await press(page, RIGHT); assert.equal(await active(page), 'nexus-search-submit', 'D-pad Right from the field reaches the button');
      await press(page, LEFT); assert.equal(await active(page), 'nexus-search-input');
      await press(page, DOWN); assert.notEqual(await active(page), 'nexus-search-input', 'D-pad Down leaves the field');
      await press(page, UP); assert.equal(await active(page), 'nexus-search-input', 'and Up comes back');
      await searchButton(page).focus(); await press(page, UP); assert.match(await active(page), /^tab-/);
      assert.deepEqual(fake.requests, [], 'moving around asks nothing');
      await clean(page, api);
    });

    await test('search: Cross on the button, Enter in the field and a click each send one request with the trimmed text and page 1', async () => {
      const fake = searchApi(); fake.reply = searchReply([hit(30, 'Zebra textures')]);
      const { page, api } = await searchFixture(fake);
      await openZeta(page);
      await searchField(page).fill('  amber    lamp ');
      assert.equal(await searchButton(page).getAttribute('aria-disabled'), 'false');
      await searchField(page).press('Enter'); await resultsReady(page);
      assert.deepEqual(fake.requests, [{ game: 'zeta', text: 'amber lamp', page: 1, raw: '?q=amber%20lamp&page=1' }], 'Enter: runs of spaces collapse, the text is percent-encoded, one q and one page');
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      // Cross on the button (the field itself ignores Cross: pressing it only focuses).
      await searchField(page).fill('zebra'); await searchField(page).focus(); await press(page, CROSS);
      assert.equal(fake.requests.length, 1, 'Cross in the field sends nothing');
      await searchButton(page).focus(); await press(page, CROSS); await resultsReady(page);
      assert.deepEqual(fake.requests.at(-1), { game: 'zeta', text: 'zebra', page: 1, raw: '?q=zebra&page=1' });
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      await searchField(page).fill('caf\u00e9 & <b>'); await searchButton(page).click(); await resultsReady(page);
      assert.deepEqual(fake.requests.at(-1), { game: 'zeta', text: 'caf\u00e9 & <b>', page: 1, raw: '?q=caf%C3%A9%20%26%20%3Cb%3E&page=1' }, 'click: UTF-8 text is percent-encoded, & and < cannot start another field');
      assert.equal(fake.requests.length, 3);
      assert.equal(api.posts.length, 0, 'a search never creates a job');
      await clean(page, api);
    });

    await test('search: text under 2 or over 100 characters, or with control characters, is refused with a message and no request', async () => {
      const fake = searchApi(); fake.reply = searchReply([hit(30, 'Zebra textures')]);
      const { page, api } = await searchFixture(fake);
      await openZeta(page);
      const tooShort = 'Type at least 2 characters to search Nexus.', tooLong = 'Search text can be at most 100 characters.', control = 'Search text cannot contain control characters.';
      const setValue = value => searchField(page).evaluate((el, text) => { el.value = text; el.dispatchEvent(new Event('input', { bubbles: true })); }, value);
      assert.equal(await searchButton(page).getAttribute('aria-disabled'), 'true', 'nothing typed: the button is inert');
      for (const [value, message] of [['', tooShort], ['a', tooShort], ['   a   ', tooShort], ['\u00a0\u00a0', tooShort], ['x'.repeat(101), tooLong], [`  ${'y'.repeat(100)}z `, tooLong], ['ab\tcd', control], ['ab\u0085cd', control], ['ab\u2028cd', control], ['ab\u0007cd', control]]) {
        await setValue(value);
        await searchField(page).press('Enter');
        assert.equal(await searchMessage(page), message, JSON.stringify(value).slice(0, 30));
        await searchButton(page).click({ force: true });
        assert.equal(await searchMessage(page), message, `button: ${JSON.stringify(value).slice(0, 30)}`);
        assert.deepEqual(fake.requests, [], `no request for ${JSON.stringify(value).slice(0, 30)}`);
        assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
        assert.equal(await page.locator('#status-banner').isHidden(), true, 'the refusal is a message by the field, not a banner');
        assert.equal(await page.locator('#announcer').innerText(), message, 'and it is said through the one announcer');
      }
      assert.equal(await page.locator('#nexus-search-message').getAttribute('role'), null, 'the form adds no live region of its own');
      assert.deepEqual(await page.$$eval('[role="status"]', els => els.map(el => el.id).sort()), ['announcer', 'lookup-status', 'toast']);
      // The button is focusable while inert, so a controller can read the reason; Cross on it says it again.
      await setValue('a'); assert.equal(await searchButton(page).getAttribute('aria-disabled'), 'true');
      await searchButton(page).focus(); assert.equal(await active(page), 'nexus-search-submit');
      await press(page, CROSS); assert.equal(await searchMessage(page), tooShort); assert.equal(fake.requests.length, 0);
      // Typing again clears the message; two characters (counting an emoji once, as the host does) are enough.
      await searchField(page).fill('ab'); assert.equal(await searchMessage(page), ''); assert.equal(await searchButton(page).getAttribute('aria-disabled'), 'false');
      await setValue('\u{1F600}\u{1F600}'); await searchField(page).press('Enter'); await resultsReady(page);
      assert.deepEqual(fake.requests, [{ game: 'zeta', text: '\u{1F600}\u{1F600}', page: 1, raw: '?q=%F0%9F%98%80%F0%9F%98%80&page=1' }]);
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      // Exactly 100 characters after trimming is allowed; the field itself stops typing at 100.
      await setValue(`  ${'w'.repeat(100)}  `); await searchField(page).press('Enter'); await resultsReady(page);
      assert.equal(fake.requests.at(-1).text, 'w'.repeat(100));
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      await searchField(page).fill(''); await searchField(page).focus(); await page.keyboard.type('v'.repeat(120));
      assert.equal((await searchField(page).inputValue()).length, 100, 'typing stops at 100');
      assert.equal(fake.requests.length, 2);
      await clean(page, api);
    });

    await test('search: results replace the mod grid with the very same cards; the subtitle and count read as specified; filter and sort are off', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = searchReply([hit(30, 'Zebra textures', { author: 'Ada Lovelace', summary: 'Crisp stripes for every surface', version: '1.0' }), hit(77, 'Lamp pack')], { total: 137, pages: 7 });
      await openZeta(page);
      const latestCard = await page.locator('[data-card="mod:30"]').evaluate(el => el.outerHTML);
      const latestIds = await cards(page, 'mod'), latestCount = latestRequests(api);
      await modFilter(page).fill('zebra'); await page.locator('#catalog-sort').click();
      await runSearch(page, 'lamp');
      await resultsReady(page);
      assert.equal(await page.locator('#games-view h1').innerText(), 'Search results for Zeta world');
      assert.equal(await subtitle(page), 'Results come from a Nexus search for lamp. 137 mods matched.');
      assert.equal(await page.locator('#games-content h2.content-title').innerText(), 'Search results from Nexus');
      assert.deepEqual(await cards(page, 'mod'), ['30', '77']);
      // The card is the latest-mods card, unchanged: the same markup for the same record.
      assert.equal(await page.locator('[data-card="mod:30"]').evaluate(el => el.outerHTML), latestCard);
      assert.match(await page.locator('[data-card="mod:77"]').innerText(), /Lamp pack[\s\S]*By Ada Lovelace · 1\.0[\s\S]*About Lamp pack[\s\S]*Compatibility unresolved/);
      // Local filter and sort belong to the latest list: hidden here, remembered for when it comes back, and nothing filters the results.
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      assert.equal(await page.locator('#nexus-search').isVisible(), true);
      assert.equal(await page.locator('#nexus-search-hint').isHidden(), true, 'the line that tells the filter and the search apart is not needed while the filter is hidden');
      assert.deepEqual(fake.requests, [{ game: 'zeta', text: 'lamp', page: 1, raw: '?q=lamp&page=1' }]);
      assert.equal(latestRequests(api), latestCount, 'the latest list is not asked again');
      assert.doesNotMatch(await page.locator('#games-view').innerText(), /not a search result|Latest mods from Nexus/);
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /compatible with|installed|safe|works/i, 'a result is a Nexus listing, nothing more');
      assert.equal(await page.locator('#games-content [data-basket-add], #games-content .download-file').count(), 0, 'no download starts from a result');
      assert.equal(await page.locator('#announcer').innerText(), 'Search results from Nexus: 137 mods matched, page 1 of 7.');
      assert.deepEqual(api.posts, []);
      // Back: the latest list returns as it was (filter and sort kept) without a request, and says it is not a search.
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      assert.equal(await subtitle(page), 'Latest mods from Nexus · this is not a search result.');
      assert.equal(await modFilter(page).inputValue(), 'zebra');
      assert.deepEqual(await cards(page, 'mod'), ['30']);
      assert.equal(await page.locator('#catalog-sort').innerText(), 'Sort: Name A–Z');
      assert.equal(await page.locator('#nexus-search-hint').isVisible(), true, 'the hint that tells filter and search apart is back with the filter');
      await modFilter(page).fill(''); assert.deepEqual((await cards(page, 'mod')).sort(), [...latestIds].sort());
      assert.equal(latestRequests(api), latestCount); assert.equal(fake.requests.length, 1);
      await clean(page, api);
    });

    await test('search: Previous and Next never go past the first page, the last page, or page 50', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = pagedReply(45);
      await openZeta(page);
      await runSearch(page, 'paged'); await resultsReady(page);
      assert.equal(await pagerOf(page).label.innerText(), 'Page 1 of 3');
      assert.equal(await pagerOf(page, 'bottom').label.innerText(), 'Page 1 of 3');
      for (const where of ['top', 'bottom']) { assert.equal(await pagerOf(page, where).prev.isDisabled(), true, `${where}: no Previous on page 1`); assert.equal(await pagerOf(page, where).next.isDisabled(), false); }
      assert.deepEqual(await cards(page, 'mod'), ['11', '12']);
      await pagerOf(page).next.click(); await settledPage(page, 2, 3);
      assert.deepEqual(await cards(page, 'mod'), ['21', '22']);
      assert.equal(await subtitle(page), 'Results come from a Nexus search for paged. 45 mods matched.');
      await pagerOf(page, 'bottom').next.click(); await settledPage(page, 3, 3);
      assert.deepEqual(await cards(page, 'mod'), ['31', '32']);
      for (const where of ['top', 'bottom']) { assert.equal(await pagerOf(page, where).next.isDisabled(), true, `${where}: no Next on the last page`); assert.equal(await pagerOf(page, where).prev.isDisabled(), false); }
      await pagerOf(page).prev.click(); await settledPage(page, 2, 3);
      await pagerOf(page).prev.click(); await settledPage(page, 1, 3);
      assert.equal(await pagerOf(page).prev.isDisabled(), true);
      assert.deepEqual(fake.requests.map(request => request.page), [1, 2, 3, 2, 1]);
      assert.ok(fake.requests.every(request => request.text === 'paged'));
      // One page of results: both buttons are inert.
      fake.reply = searchReply([hit(30, 'Zebra textures')]);
      await runSearch(page, 'one page'); await settledPage(page, 1, 1);
      assert.equal(await pagerOf(page).prev.isDisabled(), true); assert.equal(await pagerOf(page).next.isDisabled(), true);
      // A result that spans more than 50 pages is capped at 50: Next stops at 50 and page 51 is never asked.
      fake.requests.length = 0; fake.reply = pagedReply(5000);
      await runSearch(page, 'huge'); await settledPage(page, 1, 50);
      for (let n = 2; n <= 50; n++) { await pagerOf(page).next.click(); await settledPage(page, n, 50); }
      assert.equal(await pagerOf(page).next.isDisabled(), true, 'no Next on page 50');
      assert.equal(await pagerOf(page, 'bottom').next.isDisabled(), true);
      assert.deepEqual(fake.requests.map(request => request.page), Array.from({ length: 50 }, (_, index) => index + 1));
      assert.ok(Math.max(...fake.requests.map(request => request.page)) <= 50);
      assert.match(await subtitle(page), /5000 mods matched\.$/);
      await clean(page, api);
    });

    await test('search: the pager is inert while a page loads, so a second press asks nothing; focus stays on the control that was pressed', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = pagedReply(45);
      await openZeta(page);
      await runSearch(page, 'paged'); await resultsReady(page);
      let release; fake.holds.set('paged', new Promise(resolve => { release = resolve; }));
      const bottomNext = pagerOf(page, 'bottom').next;
      await bottomNext.focus(); await press(page, CROSS);
      await page.locator('[data-search-status="loading"]').waitFor();
      assert.equal(await statusBlock(page, 'loading').innerText(), 'Searching Nexus for paged…');
      assert.equal(await subtitle(page), 'Searching Nexus for paged…');
      assert.equal(await searchFocus(page), 'next-bottom', 'focus stays on the button that was pressed');
      for (const where of ['top', 'bottom']) for (const direction of ['prev', 'next']) assert.equal(await pagerOf(page, where)[direction].getAttribute('aria-disabled'), 'true', `${where} ${direction} is inert while loading`);
      assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0, 'no stale cards under the new page label');
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods matched|No mods to show/, 'loading is never an empty list');
      await pagerOf(page, 'bottom').next.dispatchEvent('click'); await pagerOf(page).prev.dispatchEvent('click');
      await page.waitForTimeout(100);
      assert.equal(fake.requests.length, 2, 'presses during the load asked nothing');
      release(); await settledPage(page, 2, 3);
      assert.equal(await searchFocus(page), 'next-bottom', 'and it is still there when the page arrives');
      assert.deepEqual(await cards(page, 'mod'), ['21', '22']);
      // On the last page the pressed Next is gone, so focus goes to the first result, not to the page.
      fake.holds.clear();
      await press(page, CROSS); await settledPage(page, 3, 3);
      assert.equal(await searchFocus(page), '', 'focus is on a card');
      assert.equal(await active(page), 'mod:31');
      await clean(page, api);
    });

    await test('search: Back to latest mods, Circle, Escape and an emptied box leave the search without a request', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = searchReply([hit(77, 'Lamp pack')]);
      await openZeta(page);
      let latest = latestRequests(api);
      const leftOk = async note => {
        assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world', note);
        assert.equal((await cards(page, 'mod')).length, 5, note);
        assert.equal(latestRequests(api), latest, `${note}: the latest list was not asked for again`);
        assert.equal(await page.locator('#catalog-tools').isVisible(), true, note);
      };
      await runSearch(page, 'lamp'); await resultsReady(page);
      await page.getByRole('button', { name: 'Back to latest mods' }).click(); await leftOk('Back button');
      assert.equal(await searchField(page).inputValue(), '', 'leaving clears the box'); assert.equal(await active(page), 'nexus-search-submit', 'focus goes to the button, not into the text field');
      assert.equal(await page.locator('#announcer').innerText(), 'Showing the latest mods again.');
      await runSearch(page, 'lamp'); await resultsReady(page);
      await searchButton(page).focus(); await press(page, CIRCLE); await leftOk('Circle');
      await runSearch(page, 'lamp'); await resultsReady(page);
      await page.locator('[data-card="mod:77"]').focus(); await page.keyboard.press('Escape'); await leftOk('Escape on a card');
      await runSearch(page, 'lamp'); await resultsReady(page);
      await searchField(page).focus(); await page.keyboard.press('Escape');
      assert.equal(await searchField(page).inputValue(), '', 'Escape clears the field first'); await leftOk('Escape in the field');
      await page.keyboard.press('Escape'); await page.getByRole('heading', { name: 'Choose a game' }).waitFor();
      await openZeta(page); latest = latestRequests(api);
      await runSearch(page, 'lamp'); await resultsReady(page);
      await searchField(page).fill(''); await leftOk('an emptied box');
      assert.equal(fake.requests.length, 5);
      // Circle from results goes to the latest mods, and once more to the games: nothing is skipped.
      await runSearch(page, 'lamp'); await resultsReady(page);
      await page.locator('[data-card="mod:77"]').focus(); await press(page, CIRCLE); await leftOk('Circle on a card');
      await press(page, CIRCLE); await page.getByRole('heading', { name: 'Choose a game' }).waitFor();
      assert.equal(await page.locator('#nexus-search').isHidden(), true);
      // The game opens again with an empty box and its latest list: the search is not kept after leaving the game.
      await openZeta(page);
      assert.equal(await searchField(page).inputValue(), '');
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      await clean(page, api);
    });

    await test('search: wording is exactly the specified one for results, nothing matched, nothing listable and a failure', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      await openZeta(page);
      // total 0: "No mods matched", the text and no Retry; it is a successful answer.
      fake.reply = searchReply([]);
      await runSearch(page, 'nothing here'); await statusBlock(page, 'empty').waitFor();
      assert.equal(await statusBlock(page, 'empty').locator('h2').innerText(), 'No mods matched');
      assert.equal(await statusBlock(page, 'empty').locator('p').innerText(), 'Nexus found no mods whose name matches nothing here for this game. Try fewer or different words.');
      assert.equal(await subtitle(page), 'Results come from a Nexus search for nothing here. 0 mods matched.');
      assert.equal(await page.locator('[data-search-retry]').count(), 0);
      assert.equal(await page.locator('[data-search-pager]').count(), 0, 'no pages when nothing matched');
      assert.equal(await page.getByRole('button', { name: 'Back to latest mods' }).count(), 1);
      assert.equal(await page.locator('#announcer').innerText(), 'No mods matched.');
      // total above 0, no items: "No mods to show on this page", the pager stays (other pages may list mods), no Retry.
      fake.reply = searchReply([], { total: 41, pages: 3 });
      await runSearch(page, 'hidden things'); await statusBlock(page, 'none-listed').waitFor();
      assert.equal(await statusBlock(page, 'none-listed').locator('h2').innerText(), 'No mods to show on this page');
      assert.equal(await statusBlock(page, 'none-listed').locator('p').innerText(), 'Nexus matched 41 mods, but none can be listed here (hidden by your content settings or not published).');
      assert.equal(await subtitle(page), 'Results come from a Nexus search for hidden things. 41 mods matched.');
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods matched/);
      assert.equal(await page.locator('[data-search-retry]').count(), 0);
      assert.equal(await pagerOf(page).label.innerText(), 'Page 1 of 3'); assert.equal(await pagerOf(page).next.isDisabled(), false);
      // One mod reads in the singular.
      fake.reply = searchReply([hit(77, 'Lamp pack')]);
      await runSearch(page, 'lamp'); await resultsReady(page);
      assert.equal(await subtitle(page), 'Results come from a Nexus search for lamp. 1 mod matched.');
      // A failure is never an empty list: its own title, the catalog text for the code, Retry; host text is never shown.
      const cases = [['rate_limit', 400, 'Nexus asked ModBridge to slow down. Wait a minute and try again.'], ['network', 400, 'Nexus could not be reached. Try again.'],
        ['timeout', 400, 'Nexus took too long to answer. Try again.'], ['payload', 400, 'Nexus sent a reply that could not be used. Try again later.'],
        ['size_limit', 400, 'Nexus sent a reply that could not be used. Try again later.'], ['content_hidden', 400, 'Your Nexus content filter hides this file. Change the filter on Nexus, or pick another file.'],
        ['reconnect_required', 400, 'Reconnect your Nexus account to continue.'], ['credentials', 400, 'Reconnect your Nexus account to continue.'],
        ['busy', 409, 'ModBridge is busy with another operation. Wait a moment and try again.'], ['operation_failed', 500, 'Something went wrong. Request it again.'],
        ['invalid_input', 400, 'The host did not accept this request. Try again; if it keeps failing, restart ModBridge.'], ['made_up_code', 500, 'Request failed (500)']];
      for (const [code, status, sentence] of cases) {
        fake.reply = { status, body: { error: { code, message: 'host text must never show' } } };
        await runSearch(page, `fail ${code}`); await statusBlock(page, 'failed').waitFor();
        assert.equal(await statusBlock(page, 'failed').locator('h2').innerText(), couldNotSearch, code);
        assert.equal(await statusBlock(page, 'failed').locator('p').innerText(), sentence, code);
        assert.equal(await page.getByRole('button', { name: 'Retry', exact: true }).count(), 1, code);
        assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0, code);
        assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods matched|No mods to show|host text must never show/, code);
        assert.equal(await subtitle(page), `Could not search Nexus for fail ${code}.`);
        assert.equal(await page.locator('#announcer').innerText(), 'Could not search Nexus.');
      }
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'no page-wide error for a search');
      // Retry asks again for the same text and page and keeps its place: "Retrying…" holds focus, the results then take it.
      let release; fake.holds.set('fail made_up_code', new Promise(resolve => { release = resolve; }));
      const asked = fake.requests.length;
      await page.getByRole('button', { name: 'Retry', exact: true }).focus(); await press(page, CROSS);
      await page.locator('[data-search-focus="retry"][aria-disabled="true"]').waitFor();
      assert.equal(await page.locator('[data-search-focus="retry"]').innerText(), 'Retrying…');
      assert.equal(await searchFocus(page), 'retry');
      assert.equal(await statusBlock(page, 'loading').innerText(), 'Searching Nexus for fail made_up_code…');
      fake.reply = searchReply([hit(77, 'Lamp pack')]); release();
      await resultsReady(page);
      assert.equal(fake.requests.length, asked + 1); assert.deepEqual(fake.requests.at(-1), { game: 'zeta', text: 'fail made_up_code', page: 1, raw: '?q=fail%20made_up_code&page=1' });
      assert.equal(await subtitle(page), 'Results come from a Nexus search for fail made_up_code. 1 mod matched.');
      assert.equal(await active(page), 'mod:77', 'focus goes to the first result');
      assert.equal(await page.locator('[data-search-status="failed"]').count(), 0);
      await clean(page, api);
    });

    await test('search: a failed page keeps its page number for Retry and the text for Back', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = pagedReply(45);
      await openZeta(page);
      await runSearch(page, 'paged'); await resultsReady(page);
      fake.reply = rateLimited;
      await pagerOf(page).next.click(); await statusBlock(page, 'failed').waitFor();
      assert.equal(await page.locator('[data-search-pager]').count(), 0, 'a failed page offers Retry, not stale paging');
      fake.reply = pagedReply(45);
      await page.getByRole('button', { name: 'Retry', exact: true }).click(); await settledPage(page, 2, 3);
      assert.deepEqual(fake.requests.map(request => request.page), [1, 2, 2], 'Retry asks for page 2 again, not page 1 or 3');
      assert.deepEqual(await cards(page, 'mod'), ['21', '22']);
      await clean(page, api);
    });

    await test('search: every reply that is not exactly the documented page is a failed request, never a partial list', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      await openZeta(page);
      const good = () => searchBody([hit(30, 'Zebra textures'), hit(12, 'Amber camera')]);
      const withItem = item => ({ ...good(), items: [hit(30, 'Zebra textures'), item] });
      const without = (record, key) => { const copy = { ...record }; delete copy[key]; return copy; };
      const many = n => Array.from({ length: n }, (_, index) => hit(1000 + index, `Mod ${index}`));
      const cases = {
        'an extra key': { ...good(), extra: 1 }, 'a missing items key': without(good(), 'items'), 'a missing pages key': without(good(), 'pages'), 'a missing total': without(good(), 'total'), 'a missing page': without(good(), 'page'),
        'an array body': [], 'a null body': null, 'a string body': 'ok', 'items an object': { ...good(), items: {} }, 'items null': { ...good(), items: null },
        'a negative total': { ...good(), total: -1 }, 'a fractional total': { ...good(), total: 2.5 }, 'a string total': { ...good(), total: '2' }, 'a total above a GraphQL Int': { ...good(), total: 2147483648, pages: 50 },
        'a page that is not the one asked': { ...good(), page: 2 }, 'a string page': { ...good(), page: '1' }, 'page 0': { ...good(), page: 0 },
        'pages that do not follow from total': { ...good(), pages: 3 }, 'pages 0 with results': { ...good(), pages: 0 }, 'pages above 50': { ...good(), total: 2000, pages: 100 }, 'pages a string': { ...good(), pages: '1' },
        'more than 20 items': { items: many(21), total: 60, page: 1, pages: 3 }, 'more items than total': { ...good(), total: 1, pages: 1 },
        'an item that is not an object': withItem('x'), 'a null item': withItem(null), 'an array item': withItem([]),
        'an item without a mod id': withItem(without(hit(1, 'No id'), 'mod_id')), 'an item without a name': withItem(without(hit(1, 'x'), 'name')),
        'a string mod id': withItem(hit('12', 'String id')), 'a zero mod id': withItem(hit(0, 'Zero id')), 'a negative mod id': withItem(hit(-3, 'Negative id')), 'a fractional mod id': withItem(hit(1.5, 'Fraction id')), 'a mod id above a GraphQL Int': withItem(hit(2147483648, 'Big id')),
        'an item of another game': withItem(hit(5, 'Elsewhere', { game_domain: 'alpha' })), 'an item with an unknown field': withItem(hit(5, 'Extra', { price: 5 })),
        'a name that is not text': withItem(hit(5, 7)), 'an author that is not text': withItem(hit(5, 'Odd', { author: 3 })), 'a summary that is not text': withItem(hit(5, 'Odd', { summary: null })),
        'an adult flag that is not a boolean': withItem(hit(5, 'Odd', { contains_adult_content: 'no' })), 'a game id that is not an integer': withItem(hit(5, 'Odd', { game_id: '1' })),
        'a summary over the host bound': withItem(hit(5, 'Odd', { summary: 'x'.repeat(65537) })), 'a name over the host bound': withItem(hit(5, 'x'.repeat(1025))),
        'the same mod twice': { ...good(), items: [hit(30, 'Zebra textures'), hit(30, 'Zebra again')] },
        'items on a page past the end': { items: [hit(30, 'Zebra textures')], total: 2, page: 2, pages: 1 },
      };
      let number = 0;
      for (const [label, body] of Object.entries(cases)) {
        fake.reply = { status: 200, body };
        await runSearch(page, `case ${++number}`);
        await statusBlock(page, 'failed').waitFor();
        assert.equal(await statusBlock(page, 'failed').locator('p').innerText(), refusedSentence, label);
        assert.equal(await statusBlock(page, 'failed').locator('h2').innerText(), couldNotSearch, label);
        assert.equal(await page.getByRole('button', { name: 'Retry', exact: true }).count(), 1, label);
        assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0, `${label}: no partial list`);
        assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods matched|No mods to show|mods matched/, label);
      }
      assert.ok(number > 40);
      // The page that was asked for may be a page past the end (a normal answer with no items), a single listed hit may carry fewer fields
      // than the full record (the cards need only an id and a name), and the host's own full record is accepted.
      for (const [label, body, ready] of [
        ['a page past the end', { items: [], total: 2, page: 1, pages: 1 }, () => statusBlock(page, 'none-listed').waitFor()],
        ['records with only the cards fields', { ...good(), items: [{ mod_id: 30, name: 'Zebra textures', author: 'Ada', summary: 'Stripes' }, { mod_id: 12, name: 'Amber camera' }] }, () => resultsReady(page)],
        ['the largest id, an adult flag, a description', { ...good(), items: [hit(2147483647, 'Biggest', { contains_adult_content: true, description: 'More text' })], total: 1 }, () => resultsReady(page)],
        ['the largest page of the largest result', { items: [hit(30, 'Zebra textures')], total: 2147483647, page: 1, pages: 50 }, () => resultsReady(page)],
      ]) {
        fake.reply = { status: 200, body };
        await runSearch(page, `ok ${++number}`); await ready();
        assert.equal(await page.locator('[data-search-status="failed"]').count(), 0, label);
      }
      await clean(page, api);
    });

    await test('search: a late reply for an older search, a left search or another game is ignored', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake, { games: [...games()], mods: { zeta: zetaMods(), alpha: [{ mod_id: 99, game_domain: 'alpha', name: 'Alpha mod', author: 'A', summary: 's', version: '1' }] } });
      fake.reply = ({ text }) => text === 'slow failure' ? rateLimited : searchReply([hit(text === 'older' ? 30 : 12, text === 'older' ? 'Older result' : 'Newer result')]);
      await openZeta(page);
      const hold = text => { let release; fake.holds.set(text, new Promise(resolve => { release = resolve; })); return release; };
      const settle = () => page.waitForTimeout(250);
      // An older text arriving after a newer one.
      let release = hold('older');
      await runSearch(page, 'older'); await page.locator('[data-search-status="loading"]').waitFor();
      await runSearch(page, 'newer'); await resultsReady(page);
      assert.deepEqual(await cards(page, 'mod'), ['12']);
      release(); await settle();
      assert.deepEqual(await cards(page, 'mod'), ['12'], 'the older reply did not replace the newer one');
      assert.equal(await subtitle(page), 'Results come from a Nexus search for newer. 1 mod matched.');
      assert.equal(await page.locator('#announcer').innerText(), 'Search results from Nexus: 1 mod matched, page 1 of 1.');
      // An older failure arriving after a newer success shows nothing.
      release = hold('slow failure');
      await runSearch(page, 'slow failure'); await page.locator('[data-search-status="loading"]').waitFor();
      await runSearch(page, 'newer again'); await resultsReady(page);
      release(); await settle();
      assert.equal(await page.locator('[data-search-status="failed"]').count(), 0); assert.equal(await subtitle(page), 'Results come from a Nexus search for newer again. 1 mod matched.');
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      // A reply arriving after the user left the search (Back, then an emptied box) never brings the results back.
      fake.holds.clear();
      release = hold('older');
      await runSearch(page, 'older'); await page.locator('[data-search-status="loading"]').waitFor();
      await page.getByRole('button', { name: 'Back to latest mods' }).click();
      release(); await settle();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world'); assert.equal((await cards(page, 'mod')).length, 5);
      release = hold('older');
      await runSearch(page, 'older'); await page.locator('[data-search-status="loading"]').waitFor();
      await searchField(page).fill('');
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      release(); await settle();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world'); assert.equal((await cards(page, 'mod')).length, 5);
      // A reply for a game that was left is dropped; the other game opens with its own latest list.
      release = hold('older');
      await runSearch(page, 'older'); await page.locator('[data-search-status="loading"]').waitFor();
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      await page.getByRole('button', { name: 'Alpha world' }).click(); await page.locator('[data-card="mod:99"]').waitFor();
      release(); await settle();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Alpha world'); assert.deepEqual(await cards(page, 'mod'), ['99']);
      assert.equal(await searchField(page).inputValue(), '');
      // A page that arrives after a newer search for another text is dropped too (paging within one text, then a new text).
      fake.holds.clear(); fake.reply = pagedReply(45);
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click(); await openZeta(page);
      await runSearch(page, 'paged'); await resultsReady(page);
      release = hold('paged'); await pagerOf(page).next.click(); await page.locator('[data-search-status="loading"]').waitFor();
      fake.holds.delete('paged'); fake.reply = searchReply([hit(12, 'Newer result')]);
      await runSearch(page, 'other'); await resultsReady(page);
      release(); await settle();
      assert.deepEqual(await cards(page, 'mod'), ['12']); assert.equal(await subtitle(page), 'Results come from a Nexus search for other. 1 mod matched.');
      await clean(page, api);
    });

    await test('search: names, authors, summaries and the typed text are text, never markup', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      const evil = '<img src=x onerror=window.__pwned=1>';
      fake.reply = searchReply([hit(30, `Hostile ${evil}`, { author: `<b>Mallory</b> & ${evil}`, summary: `<script>window.__pwned=1</script> ${evil}`, version: `"><${evil}` })], { total: 1 });
      await openZeta(page);
      await runSearch(page, `<b>x</b> ${evil}`); await resultsReady(page);
      assert.equal(await page.locator('#games-content img, #games-content b, #games-content script, #games-view .subheading *').count(), 0);
      const text = await page.locator('#games-content').innerText();
      assert.match(text, /Hostile <img src=x onerror=window\.__pwned=1>/); assert.match(text, /<b>Mallory<\/b> &/);
      assert.equal(await subtitle(page), 'Results come from a Nexus search for <b>x</b> <img src=x onerror=window.__pwned=1>. 1 mod matched.');
      // The empty and failure states echo the text as text too.
      fake.reply = searchReply([]);
      await runSearch(page, `<i>none</i> ${evil}`); await statusBlock(page, 'empty').waitFor();
      assert.equal(await page.locator('#games-content i, #games-content img').count(), 0);
      assert.equal(await statusBlock(page, 'empty').locator('p').innerText(), `Nexus found no mods whose name matches <i>none</i> ${evil} for this game. Try fewer or different words.`);
      fake.reply = rateLimited; await runSearch(page, `<u>x</u> ${evil}`); await statusBlock(page, 'failed').waitFor();
      assert.equal(await page.locator('#games-view u, #games-view img').count(), 0);
      // The details sheet of a result shows the same text as text.
      fake.reply = searchReply([hit(30, `Hostile ${evil}`, { author: `<b>Mallory</b>`, summary: `<i>sum</i> ${evil}` })], { total: 1 });
      await runSearch(page, 'again'); await resultsReady(page);
      await page.locator('[data-card="mod:30"]').focus(); await page.keyboard.press('F6'); await page.locator('#details-dialog[open]').waitFor();
      assert.equal(await page.locator('#details-content img, #details-content b, #details-content i').count(), 0);
      assert.match(await page.locator('#details-content').innerText(), /<b>Mallory<\/b>/);
      await clean(page, api);
    });

    await test('search: a result opens its files and details like a latest mod, and going back shows the results without asking Nexus', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = searchReply([hit(30, 'Zebra textures', { summary: 'Crisp stripes', description: '' }), hit(12, 'Amber camera')], { total: 2 });
      await openZeta(page);
      await runSearch(page, 'camera'); await resultsReady(page);
      // The details sheet is the mod sheet: author, version and summary from the hit; no description row (a hit carries none).
      await page.locator('[data-card="mod:30"]').focus(); await page.keyboard.press('F6'); await page.locator('#details-dialog[open]').waitFor();
      assert.equal(await page.locator('#details-title').innerText(), 'Zebra textures');
      const labels = await page.locator('#details-content .detail-label').allTextContents();
      assert.deepEqual(labels.filter(label => ['Author', 'Version', 'Summary', 'Description', 'Mod ID', 'Game'].includes(label)), ['Author', 'Version', 'Summary', 'Mod ID', 'Game']);
      assert.match(await page.locator('#details-content').innerText(), /Crisp stripes/);
      assert.equal(await page.locator('#details-nexus-link').getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/30');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'mod:30', 'focus returns to the card');
      // Choosing it opens its files; the search form hides, the breadcrumb returns to the results.
      const searches = fake.requests.length, lists = latestRequests(api);
      await page.locator('[data-card="mod:30"]').click(); await page.locator('[data-file-card]').first().waitFor();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Files for Zebra textures');
      assert.ok(api.requests.includes('GET /api/games/zeta/mods/30/files'));
      assert.equal(await page.locator('#nexus-search').isHidden(), true);
      assert.deepEqual(api.posts, []);
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      assert.equal(await subtitle(page), 'Results come from a Nexus search for camera. 2 mods matched.');
      assert.deepEqual(await cards(page, 'mod'), ['30', '12']);
      assert.equal(await searchField(page).inputValue(), 'camera');
      // Circle in the files returns to the results (not to the latest list), Circle again leaves the search.
      await page.locator('[data-card="mod:12"]').click(); await page.locator('[data-file-card]').first().waitFor();
      await press(page, CIRCLE); assert.equal(await page.locator('#games-view h1').innerText(), 'Search results for Zeta world');
      await press(page, CIRCLE); assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world');
      assert.equal(fake.requests.length, searches, 'none of this asked Nexus again'); assert.equal(latestRequests(api), lists);
      // A search is not kept across a reload: the page starts at the latest mods of the chosen game.
      await runSearch(page, 'camera'); await resultsReady(page);
      await page.reload(); await page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor();
      assert.equal(await searchField(page).inputValue(), '');
      await clean(page, api);
    });

    await test('search: the controller walks field, button, results and pager; L2 and R2 jump to Back and the last pager button', async () => {
      const fake = searchApi(), { page, api } = await searchFixture(fake);
      fake.reply = pagedReply(45);
      await openZeta(page);
      await searchField(page).focus(); await press(page, RIGHT); assert.equal(await active(page), 'nexus-search-submit');
      await searchField(page).fill('paged'); await searchButton(page).focus(); await press(page, CROSS); await resultsReady(page);
      await press(page, L2); assert.equal(await searchFocus(page), 'back', 'L2: the first control of the list is Back to latest mods');
      assert.equal(await pagerOf(page).prev.isDisabled(), true, 'Previous is inert on page 1, so it takes no focus');
      await press(page, RIGHT); assert.equal(await searchFocus(page), 'next-top', 'Right from Back skips the inert Previous');
      await page.locator('[data-card="mod:11"]').focus(); await press(page, R2); assert.equal(await searchFocus(page), 'next-bottom', 'R2: the last control is the bottom Next');
      await press(page, CROSS); await settledPage(page, 2, 3);
      assert.equal(await searchFocus(page), 'next-bottom');
      await press(page, LEFT); assert.equal(await searchFocus(page), 'prev-bottom');
      await press(page, CROSS); await settledPage(page, 1, 3);
      assert.equal(await searchFocus(page), '', 'Previous is inert on page 1 again, so focus goes to the first result'); assert.equal(await active(page), 'mod:11');
      // Up and Down walk the cards as a grid; Cross on a card opens its files.
      await press(page, CROSS); await page.getByRole('heading', { name: 'Files for Page 1 first' }).waitFor();
      assert.ok(api.requests.includes('GET /api/games/zeta/mods/11/files'));
      assert.deepEqual(fake.requests.map(request => request.page), [1, 2, 1]);
      await clean(page, api);
    });

    await test('search: the field, results and states fit the TV safe area and type floor at 1920x1080 and 1280x720', async () => {
      for (const viewport of [FULL_HD, HD]) {
        const fake = searchApi(), { page, api } = await searchFixture(fake, { viewport });
        await openZeta(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, field`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, field`);
        await runSearch(page, 'x'); await searchButton(page).click({ force: true });
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, refusal`);
        fake.reply = ({ page: number }) => searchReply([...Array.from({ length: 8 }, (_, index) => hit(100 + index, `Result ${index} ${longName}`, { author: longName, summary: longName })), hit(7, '<b>Short</b>')], { total: 4000, page: number, pages: 50 });
        await runSearch(page, `${longName} words`); await resultsReady(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, results`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, results`);
        await page.locator('[data-search-pager="bottom"] [data-search-page="next"]').focus();
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, pager focused`);
        for (const [label, reply, status] of [['empty', searchReply([]), 'empty'], ['none listed', searchReply([], { total: 41, pages: 3 }), 'none-listed'], ['failed', rateLimited, 'failed']]) {
          fake.reply = reply; await runSearch(page, `${longName} ${label}`); await statusBlock(page, status).waitFor();
          assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, ${label}`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, ${label}`);
        }
        await clean(page, api);
        await page.context().close();
      }
    });

    // ---- All mods (docs/INSTALL-FLOW-UX.md 3.10): GET /api/games/{game}/mods/browse?sort=<key>&page=<n> ----
    // A fake of the route: records each request (game, sort, page, raw query string) and may hold a reply back per "sort:page".
    const browseApi = () => {
      const fake = { requests: [], reply: null, holds: new Map() };
      fake.get = async url => {
        const match = /^\/api\/games\/([^/]+)\/mods\/browse$/.exec(url.pathname);
        if (!match) return undefined;
        const sort = url.searchParams.get('sort'), page = Number(url.searchParams.get('page'));
        fake.requests.push({ game: match[1], sort, page, raw: url.search });
        const hold = fake.holds.get(`${sort}:${page}`);
        if (hold) await hold;
        return typeof fake.reply === 'function' ? fake.reply({ game: match[1], sort, page }) : fake.reply;
      };
      return fake;
    };
    const sortIndex = { downloads: 1, endorsements: 2, updated: 3, name: 4 };
    // Two listed mods per page of a listing that counts `total` (45: three pages; 5000: the 50-page cap). Ids and names say sort and page.
    const browsePageReply = (total = 45) => ({ game, sort, page }) => ({ status: 200, body: { items: [hit(sortIndex[sort] * 1000 + page * 10 + 1, `${sort} p${page} first`, { game_domain: game }), hit(sortIndex[sort] * 1000 + page * 10 + 2, `${sort} p${page} second`, { game_domain: game })], total, page, pages: Math.min(50, Math.ceil(total / 20)), sort } });
    const browseBody = (items, extra = {}) => ({ items, total: items.length, page: 1, pages: items.length ? 1 : 0, sort: 'downloads', ...extra });
    // A page with nothing to list that still echoes the sort and page asked for, as the host does.
    const browseNone = (total, pages) => ({ sort, page }) => ({ status: 200, body: { items: [], total, page, pages, sort } });
    // Every mod id has one file, so a listed mod can be opened.
    const anyFiles = new Proxy({}, { get: (_, id) => typeof id === 'string' && /^[0-9]+$/.test(id) ? [file(Number(id) + 5, 'Some file')] : undefined });
    const browseFixture = (fake, extra = {}) => fixture({ capabilities: { mod_browse: true }, get: fake.get, files: anyFiles, ...extra });
    const rateLimitedText = 'Nexus asked ModBridge to slow down. Wait a minute and try again.';
    const browseId = (sort, page, n) => `mod:${sortIndex[sort] * 1000 + page * 10 + n}`;
    const openZetaAll = async page => { await page.getByRole('button', { name: 'Zeta world' }).click(); await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor(); };
    const browseStatusOf = (page, name) => page.locator(`#games-content [data-browse-status="${name}"]`);
    const browsePager = (page, where = 'top') => ({ prev: page.locator(`[data-browse-pager="${where}"] [data-browse-page="prev"]`), next: page.locator(`[data-browse-pager="${where}"] [data-browse-page="next"]`), label: page.locator(`[data-browse-pager="${where}"] .search-page`) });
    const browseSettled = (page, n, of) => page.locator('[data-browse-pager="top"] .search-page:not([aria-busy])', { hasText: `Page ${n} of ${of}` }).waitFor();
    const browseFocus = page => page.evaluate(() => document.activeElement?.dataset?.browseFocus || document.activeElement?.id || '');
    const sortButton = (page, label) => page.locator('[data-browse-sort]', { hasText: label });
    const pressedLabels = page => page.locator('[data-browse-sort][aria-pressed="true"], [data-browse-view][aria-pressed="true"]').allInnerTexts();
    const browseSubtitleText = (sorted, total) => `Results come from Nexus, sorted ${sorted}. Nexus counts ${total} mods for this game.`;
    const latestListRequests = api => api.requests.filter(request => request === 'GET /api/games/zeta/mods').length;

    await test('browse: All mods shows only when mod_browse is exactly true; otherwise the latest list and its labels are unchanged', async () => {
      for (const [label, capabilities] of [['absent', {}], ['false', { mod_browse: false }], ['the string "true"', { mod_browse: 'true' }], ['1', { mod_browse: 1 }], ['null', { mod_browse: null }]]) {
        const fake = browseApi(); fake.reply = { status: 200, body: browseBody([hit(30, 'Zebra textures')]) };
        const { page, api } = await fixture({ capabilities, get: fake.get });
        await openZeta(page);
        assert.equal(await page.locator('[data-browse-view], [data-browse-sort]').count(), 0, label);
        assert.equal(await page.locator('#games-view h1').innerText(), 'Latest mods for Zeta world', label);
        assert.equal(await page.locator('#games-view .subheading').innerText(), 'Latest mods from Nexus · this is not a search result.', label);
        assert.equal(await page.locator('#catalog-tools').isVisible(), true, label);
        assert.match(await count(page), /loaded latest mods/, label);
        assert.deepEqual(fake.requests, [], `${label}: the browse route is never asked`);
        await clean(page, api);
      }
    });

    await test('browse: picking a game shows All mods, most downloaded first, with the four sorts and the two views; only the browse route is asked', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      assert.deepEqual(fake.requests, [{ game: 'zeta', sort: 'downloads', page: 1, raw: '?sort=downloads&page=1' }]);
      assert.equal(latestListRequests(api), 0, 'the newest-added list is not asked for until it is chosen');
      assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Zeta world');
      assert.equal(await subtitle(page), browseSubtitleText('most downloaded first', 45));
      assert.equal(await page.locator('#games-content h2.content-title').innerText(), 'All mods from Nexus');
      assert.deepEqual(await cards(page, 'mod'), ['1011', '1012']);
      assert.deepEqual(await page.locator('[data-browse-view]').allInnerTexts(), ['All mods', 'Newest added']);
      assert.deepEqual(await page.locator('[data-browse-sort]').allInnerTexts(), ['Most downloaded', 'Most endorsed', 'Recently updated', 'Name A–Z']);
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most downloaded']);
      assert.equal(await page.locator('.browse-controls [role="group"]').count(), 2, 'two labelled groups');
      assert.equal(await page.locator('[aria-label="Sort all mods"]').count(), 1);
      assert.equal(await browsePager(page).label.innerText(), 'Page 1 of 3');
      assert.equal(await page.locator('#announcer').innerText(), 'All mods from Nexus, most downloaded first: page 1 of 3.');
      // The local filter and sort belong to a list already loaded; they are off here and the search form says what it searches.
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      assert.equal(await page.locator('#nexus-search').isHidden(), true, 'no mod_search on this host');
      // Wording: a listing from Nexus, nothing about a mod working or being safe.
      assert.doesNotMatch(await page.locator('#games-view').innerText(), /works|working|safe|installed|ready/i);
      assert.deepEqual(api.posts, [], 'browsing never creates a job');
      await clean(page, api);
    });

    await test('browse: each sort asks page 1 of its own sort with exactly sort and page, and a sort change from a later page starts over', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      for (const [label, sort, sorted] of [['Most endorsed', 'endorsements', 'most endorsed first'], ['Recently updated', 'updated', 'recently updated first'], ['Name A–Z', 'name', 'name, A to Z'], ['Most downloaded', 'downloads', 'most downloaded first']]) {
        await sortButton(page, label).click();
        await page.locator(`[data-card="${browseId(sort, 1, 1)}"]`).waitFor();
        assert.deepEqual(fake.requests.at(-1), { game: 'zeta', sort, page: 1, raw: `?sort=${sort}&page=1` });
        assert.deepEqual(await pressedLabels(page), ['All mods', label]);
        assert.equal(await subtitle(page), browseSubtitleText(sorted, 45));
        assert.equal(await page.locator('#announcer').innerText(), `All mods from Nexus, ${sorted}: page 1 of 3.`);
        assert.equal(await browseFocus(page), `sort-${sort}`, 'focus stays on the pressed sort');
      }
      assert.equal(fake.requests.length, 5);
      // Pressing the pressed sort again asks nothing.
      await sortButton(page, 'Most downloaded').click();
      assert.equal(fake.requests.length, 5);
      // From page 3 of one sort, another sort is page 1 of that sort; and back, page 1 again (never a page the sort does not have).
      await sortButton(page, 'Most endorsed').click(); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      await browsePager(page).next.click(); await browseSettled(page, 2, 3); await browsePager(page).next.click(); await browseSettled(page, 3, 3);
      await sortButton(page, 'Name A–Z').click(); await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).waitFor();
      assert.deepEqual(fake.requests.at(-1), { game: 'zeta', sort: 'name', page: 1, raw: '?sort=name&page=1' });
      assert.equal(await browsePager(page).label.innerText(), 'Page 1 of 3');
      assert.deepEqual(api.posts, []);
      await clean(page, api);
    });

    await test('browse: Previous and Next stay inside the pages, are inert while a page loads, and the sort holds across pages', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      const top = browsePager(page), bottom = browsePager(page, 'bottom');
      assert.equal(await top.prev.isDisabled(), true); assert.equal(await bottom.prev.isDisabled(), true);
      assert.equal(await top.next.isDisabled(), false);
      await sortButton(page, 'Most endorsed').click(); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      // While a page loads the pager keeps its count, Next is inert (aria-disabled) and a second press asks nothing.
      let release; fake.holds.set('endorsements:2', new Promise(resolve => { release = resolve; }));
      await top.next.click();
      await page.locator('[data-browse-status="loading"]').waitFor();
      assert.equal(await top.label.innerText(), 'Page 2 of 3'); assert.equal(await top.label.getAttribute('aria-busy'), 'true');
      assert.equal(await top.next.getAttribute('aria-disabled'), 'true'); assert.equal(await top.prev.getAttribute('aria-disabled'), 'true');
      assert.equal(await page.locator('[data-browse-sort][aria-pressed="true"]').innerText(), 'Most endorsed');
      assert.equal(await subtitle(page), 'Loading mods from Nexus, most endorsed first…');
      await bottom.next.click({ force: true }); await top.next.click({ force: true });
      assert.equal(fake.requests.filter(request => request.page === 2).length, 1, 'a second press asks nothing');
      assert.equal(await browseFocus(page), 'next-top', 'focus stays on the pressed control');
      release(); await browseSettled(page, 2, 3);
      assert.deepEqual(await cards(page, 'mod'), ['2021', '2022']);
      await top.next.click(); await browseSettled(page, 3, 3);
      assert.equal(await top.next.isDisabled(), true, 'no Next past the last page'); assert.equal(await bottom.next.isDisabled(), true);
      assert.equal(await browseFocus(page), '', 'the pressed Next is gone (inert on the last page): focus moves to the first result');
      assert.equal(await active(page), 'mod:2031');
      await top.prev.click(); await browseSettled(page, 2, 3);
      assert.deepEqual(fake.requests.map(request => `${request.sort}:${request.page}`), ['downloads:1', 'endorsements:1', 'endorsements:2', 'endorsements:3', 'endorsements:2']);
      // Past 50 pages is never offered: a listing of 5000 mods has 50 pages, the pager stops there, and the subtitle says what is reachable.
      fake.reply = browsePageReply(5000);
      await sortButton(page, 'Recently updated').click(); await page.locator(`[data-card="${browseId('updated', 1, 1)}"]`).waitFor();
      assert.equal(await top.label.innerText(), 'Page 1 of 50');
      assert.equal(await subtitle(page), 'Results come from Nexus, sorted recently updated first. Nexus counts 5,000 mods for this game. Only the first 1,000 can be browsed here; search by name to find others.');
      for (let target = 2; target <= 50; target++) { await top.next.click(); await browseSettled(page, target, 50); }
      assert.equal(await top.next.isDisabled(), true);
      assert.equal(fake.requests.at(-1).page, 50);
      assert.ok(fake.requests.every(request => request.page >= 1 && request.page <= 50));
      await clean(page, api);
    });

    await test('browse: Newest added is the latest list, loaded once on first choice, with its filter; All mods returns without a request', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      await sortButton(page, 'Name A–Z').click(); await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).waitFor();
      await browsePager(page).next.click(); await browseSettled(page, 2, 3);
      assert.equal(latestListRequests(api), 0);
      await page.getByRole('button', { name: 'Newest added' }).click();
      await page.getByRole('heading', { name: 'Newest added mods for Zeta world' }).waitFor();
      await page.locator('[data-card="mod:30"]').waitFor();
      assert.equal(latestListRequests(api), 1);
      assert.deepEqual(await cards(page, 'mod'), ['30', '12', '45', '7', '21']);
      assert.equal(await subtitle(page), 'Newest added mods from Nexus · this is not a search result.');
      assert.equal(await page.locator('#games-content h2.content-title').innerText(), 'Newest added mods from Nexus');
      assert.deepEqual(await pressedLabels(page), ['Newest added']);
      assert.equal(await page.locator('[data-browse-sort]').count(), 0, 'the sorts belong to All mods');
      assert.equal(await page.locator('[data-browse-pager]').count(), 0);
      assert.equal(await page.locator('#catalog-search-label').innerText(), 'Filter loaded newest mods');
      assert.match(await count(page), /5 of 5 loaded newest mods/);
      await page.getByLabel('Filter loaded newest mods').fill('zebra');
      assert.deepEqual(await cards(page, 'mod'), ['30']);
      // Back to All mods: the same sort and page, no request; Newest again: no second latest request, its filter text is kept.
      await page.getByRole('button', { name: 'All mods' }).click();
      await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor();
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Name A–Z']);
      assert.equal(await browsePager(page).label.innerText(), 'Page 2 of 3');
      assert.deepEqual(await cards(page, 'mod'), ['4021', '4022']);
      assert.equal(await page.locator('#catalog-tools').isHidden(), true);
      await page.getByRole('button', { name: 'Newest added' }).click();
      assert.deepEqual(await cards(page, 'mod'), ['30']);
      assert.equal(latestListRequests(api), 1); assert.equal(fake.requests.length, 3);
      await clean(page, api);
    });

    await test('browse: a failed Newest added load reads the catalog text with Retry and leaves All mods alone', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      let failing = true, held;
      const { page, api } = await browseFixture(fake, { get: async (url, request) => {
        if (/^\/api\/games\/zeta\/mods$/.test(url.pathname)) { if (held) await held; if (failing) return { status: 400, body: { error: { code: 'rate_limit', message: 'host text must never show' } } }; }
        return fake.get(url, request);
      } });
      await openZetaAll(page);
      let release; held = new Promise(resolve => { release = resolve; });
      await page.getByRole('button', { name: 'Newest added' }).click();
      await browseStatusOf(page, 'loading').waitFor();
      assert.equal(await browseStatusOf(page, 'loading').innerText(), 'Loading the newest mods from Nexus…');
      assert.equal(await page.locator('#catalog-tools').isHidden(), true, 'no filter over a list that has not arrived');
      assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0);
      release(); held = null;
      await browseStatusOf(page, 'failed').waitFor();
      assert.equal(await browseStatusOf(page, 'failed').locator('h2').innerText(), 'Could not load the newest mods');
      assert.equal(await browseStatusOf(page, 'failed').locator('p').innerText(), rateLimitedText);
      assert.doesNotMatch(await page.locator('body').innerText(), /host text must never show/);
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      failing = false;
      await page.getByRole('button', { name: 'Retry' }).click();
      await page.locator('[data-card="mod:30"]').waitFor();
      assert.equal(latestListRequests(api), 2);
      assert.equal(await page.locator('#catalog-tools').isVisible(), true);
      await page.getByRole('button', { name: 'All mods' }).click();
      assert.deepEqual(await cards(page, 'mod'), ['1011', '1012']);
      assert.equal(fake.requests.length, 1);
      await clean(page, api);
    });

    await test('browse: a failed page reads the catalog text with Retry for the same sort and page; the other view and the sorts still work', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      await sortButton(page, 'Most endorsed').click(); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      await browsePager(page).next.click(); await browseSettled(page, 2, 3);
      fake.reply = { status: 400, body: { error: { code: 'rate_limit', message: 'host text must never show' } } };
      await browsePager(page).next.click();
      await browseStatusOf(page, 'failed').waitFor();
      assert.equal(await browseStatusOf(page, 'failed').locator('h2').innerText(), 'Could not load mods from Nexus');
      assert.equal(await browseStatusOf(page, 'failed').locator('p').innerText(), rateLimitedText);
      assert.equal(await subtitle(page), 'Could not load mods from Nexus.');
      assert.equal(await page.locator('#announcer').innerText(), 'Could not load mods from Nexus.');
      assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0, 'a failure is never an empty list');
      assert.equal(await page.locator('[data-browse-pager]').count(), 0, 'a failed page shows Retry only, no pager');
      assert.doesNotMatch(await page.locator('body').innerText(), /No mods found|host text must never show/);
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'the list raises no page-wide error');
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most endorsed'], 'the sort and the views stay where they were');
      // Retry asks the same sort and page once, shows "Retrying…" and keeps focus; when Nexus answers the page replaces the failure.
      let release; fake.holds.set('endorsements:3', new Promise(resolve => { release = resolve; }));
      fake.reply = browsePageReply();
      const before = fake.requests.length;
      await page.locator('[data-browse-retry]').focus(); await page.locator('[data-browse-retry]').click();
      await page.getByRole('button', { name: 'Retrying…' }).waitFor();
      assert.equal(await browseFocus(page), 'retry');
      release(); await browseSettled(page, 3, 3);
      assert.deepEqual(fake.requests.slice(before).map(request => `${request.sort}:${request.page}`), ['endorsements:3']);
      assert.equal(await page.locator('[data-browse-status="failed"]').count(), 0);
      // A failed sort change can be left by choosing another sort.
      fake.reply = { status: 400, body: { error: { code: 'network', message: 'x' } } };
      await sortButton(page, 'Name A–Z').click(); await browseStatusOf(page, 'failed').waitFor();
      assert.equal(await browseStatusOf(page, 'failed').locator('p').innerText(), 'Nexus could not be reached. Try again.');
      fake.reply = browsePageReply();
      await sortButton(page, 'Recently updated').click(); await page.locator(`[data-card="${browseId('updated', 1, 1)}"]`).waitFor();
      // No answer at all (the host is gone): the page says so and never an empty list.
      await page.context().route(`${new URL(page.url()).origin}/api/games/zeta/mods/browse**`, route => route.abort());
      await sortButton(page, 'Name A–Z').click(); await browseStatusOf(page, 'failed').waitFor();
      assert.equal(await browseStatusOf(page, 'failed').locator('p').innerText(), 'ModBridge could not be reached. Try again.');
      await clean(page, api);
    });

    await test('browse: every reply that is not exactly the documented page is a failed request, never a partial list', async () => {
      const item = id => hit(id, `Mod ${id}`);
      const good = ({ sort, page }) => ({ items: [item(11), item(12)], total: 45, page, pages: 3, sort });
      const cases = [['an extra key', () => ({ ...good({ sort: 'downloads', page: 1 }), apikey: 'x' })], ['no sort', ({ page }) => { const { sort, ...rest } = good({ sort: 'downloads', page }); return rest; }],
        ['another sort echoed', ({ page }) => good({ sort: 'name', page })], ['a sort that is not a string', ({ page }) => good({ sort: 1, page })],
        ['another page', () => good({ sort: 'downloads', page: 2 })], ['pages that do not follow total', ({ sort, page }) => ({ ...good({ sort, page }), pages: 4 })],
        ['pages above 50', ({ sort, page }) => ({ ...good({ sort, page }), total: 5000, pages: 250 })], ['a negative total', ({ sort, page }) => ({ ...good({ sort, page }), total: -1 })],
        ['a string total', ({ sort, page }) => ({ ...good({ sort, page }), total: '45' })], ['more mods than a page holds', ({ sort, page }) => ({ ...good({ sort, page }), total: 1, pages: 1, items: [item(11), item(12)] })],
        ['a mod twice', ({ sort, page }) => ({ ...good({ sort, page }), items: [item(11), item(11)] })], ['a mod of another game', ({ sort, page }) => ({ ...good({ sort, page }), items: [hit(11, 'Elsewhere', { game_domain: 'other' }), item(12)] })],
        ['a mod id of 0', ({ sort, page }) => ({ ...good({ sort, page }), items: [item(0), item(12)] })], ['a string mod id', ({ sort, page }) => ({ ...good({ sort, page }), items: [{ ...item(11), mod_id: '11' }, item(12)] })],
        ['an unknown mod key', ({ sort, page }) => ({ ...good({ sort, page }), items: [{ ...item(11), secret: 1 }, item(12)] })], ['items that are not an array', ({ sort, page }) => ({ ...good({ sort, page }), items: 'x' })],
        ['an array', () => []], ['null', () => null], ['a string', () => 'x']];
      for (const [label, make] of cases) {
        const fake = browseApi(); fake.reply = args => ({ status: 200, body: make(args) });
        const { page, api } = await browseFixture(fake);
        await page.getByRole('button', { name: 'Zeta world' }).click();
        await browseStatusOf(page, 'failed').waitFor();
        assert.equal(await browseStatusOf(page, 'failed').locator('p').innerText(), 'The host sent a reply this page could not verify.', label);
        assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0, `${label}: no partial list`);
        await clean(page, api); await page.context().close();
      }
    });

    await test('browse: an empty listing and a page with nothing listable are successful answers with their own words and no Retry', async () => {
      const fake = browseApi(); fake.reply = browseNone(0, 0);
      const { page, api } = await browseFixture(fake);
      await page.getByRole('button', { name: 'Zeta world' }).click(); await browseStatusOf(page, 'empty').waitFor();
      assert.equal(await browseStatusOf(page, 'empty').locator('h2').innerText(), 'No mods found');
      assert.equal(await browseStatusOf(page, 'empty').locator('p').innerText(), 'Nexus returned no mods for this game.');
      assert.equal(await page.locator('[data-browse-retry]').count(), 0);
      assert.equal(await page.locator('#announcer').innerText(), 'No mods found.');
      assert.equal(await subtitle(page), 'Results come from Nexus, sorted most downloaded first. Nexus counts 0 mods for this game.');
      fake.reply = browseNone(41, 3);
      await sortButton(page, 'Most endorsed').click(); await browseStatusOf(page, 'none-listed').waitFor();
      assert.equal(await browseStatusOf(page, 'none-listed').locator('h2').innerText(), 'No mods to show on this page');
      assert.equal(await browseStatusOf(page, 'none-listed').locator('p').innerText(), 'Nexus counted 41 mods, but none can be listed here (hidden by your content settings or not published).');
      assert.equal(await page.locator('[data-browse-retry]').count(), 0);
      assert.equal(await browsePager(page).label.innerText(), 'Page 1 of 3', 'the pager stays so the next page can be tried');
      assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods found/);
      fake.reply = { status: 200, body: browseBody([hit(5, 'Only one')], { total: 1 }) };
      await sortButton(page, 'Most downloaded').click(); await page.locator('[data-card="mod:5"]').waitFor();
      assert.equal(await subtitle(page), 'Results come from Nexus, sorted most downloaded first. Nexus counts 1 mod for this game.');
      assert.equal(await page.locator('[data-browse-pager]').count(), 2, 'one page: both pagers show Page 1 of 1');
      assert.equal(await browsePager(page).next.isDisabled(), true);
      await clean(page, api);
    });

    await test('browse: a late reply for an older sort or page, a left view or another game is ignored', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      const hold = key => { let release; fake.holds.set(key, new Promise(resolve => { release = resolve; })); return release; };
      const settle = () => page.waitForTimeout(150);
      // Name A-Z is slow, Most endorsed is asked after it and wins; the slow reply arrives last and is dropped.
      let release = hold('name:1');
      await sortButton(page, 'Name A–Z').click(); await page.locator('[data-browse-status="loading"]').waitFor();
      fake.holds.delete('name:1');
      await sortButton(page, 'Most endorsed').click(); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      release(); await settle();
      assert.deepEqual(await cards(page, 'mod'), ['2011', '2012']);
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most endorsed']);
      // A page that arrives after the game was left is dropped, and the other game starts clean.
      release = hold('endorsements:2');
      await browsePager(page).next.click(); await page.locator('[data-browse-status="loading"]').waitFor();
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click(); await page.getByRole('button', { name: 'Alpha world' }).waitFor();
      release(); await settle();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Choose a game');
      await page.getByRole('button', { name: 'Alpha world' }).click(); await page.getByRole('heading', { name: 'All mods for Alpha world' }).waitFor();
      await browseSettled(page, 1, 3);
      assert.equal(fake.requests.at(-1).game, 'alpha');
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most downloaded'], 'a new game starts at All mods, most downloaded');
      // A reply for All mods that arrives while Newest added shows does not replace the newest list; it is there when All mods is chosen.
      release = hold('downloads:1');
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click(); await page.getByRole('button', { name: 'Zeta world' }).click();
      await browseStatusOf(page, 'loading').waitFor();
      await page.getByRole('button', { name: 'Newest added' }).click(); await page.locator('[data-card="mod:30"]').waitFor();
      release(); await settle();
      assert.deepEqual(await cards(page, 'mod'), ['30', '12', '45', '7', '21']);
      await page.getByRole('button', { name: 'All mods' }).click();
      assert.deepEqual(await cards(page, 'mod'), ['1011', '1012']);
      await clean(page, api);
    });

    await test('browse: names, authors and summaries are text, never markup; the details sheet of a listed mod works', async () => {
      const fake = browseApi(), evil = '<img src=x onerror=window.__pwned=1>';
      fake.reply = { status: 200, body: browseBody([hit(30, `Hostile ${evil}`, { author: `<b>Mallory</b> & ${evil}`, summary: `<script>window.__pwned=1</script> ${evil}`, version: `"><${evil}` })], { total: 1 }) };
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      assert.equal(await page.locator('#games-content img, #games-content b, #games-content script, #games-view .subheading *').count(), 0);
      assert.match(await page.locator('#games-content').innerText(), /Hostile <img src=x onerror=window\.__pwned=1>/);
      await page.locator('[data-card="mod:30"]').focus(); await page.keyboard.press('F6'); await page.locator('#details-dialog[open]').waitFor();
      assert.equal(await page.locator('#details-title').innerText(), `Hostile ${evil}`);
      assert.equal(await page.locator('#details-content img, #details-content b').count(), 0);
      assert.match(await page.locator('#details-content').innerText(), /<b>Mallory<\/b> &/);
      assert.equal(await page.locator('#details-nexus-link').getAttribute('href'), 'https://www.nexusmods.com/zeta/mods/30');
      await page.keyboard.press('Escape');
      assert.equal(await active(page), 'mod:30', 'focus returns to the card');
      await clean(page, api);
    });

    await test('browse: a listed mod opens its files and going back keeps the sort and page without asking; search returns to the view it began in', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const searched = searchApi(); searched.reply = searchReply([hit(30, 'Zebra textures')], { total: 1 });
      const { page, api } = await fixture({ capabilities: { mod_browse: true, mod_search: true }, files: anyFiles, get: async (url, request) => (await fake.get(url, request)) || searched.get(url, request) });
      await openZetaAll(page);
      await sortButton(page, 'Most endorsed').click(); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      await browsePager(page).next.click(); await browseSettled(page, 2, 3);
      const asked = fake.requests.length;
      await page.locator(`[data-card="${browseId('endorsements', 2, 2)}"]`).click(); await page.locator('[data-file-card]').first().waitFor();
      assert.ok(api.requests.includes('GET /api/games/zeta/mods/2022/files'));
      assert.equal(await page.locator('#games-view h1').innerText(), 'Files for endorsements p2 second');
      assert.equal(await page.locator('[data-browse-view]').count(), 0, 'no mod list controls over a mod’s files');
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Zeta world');
      assert.equal(await browsePager(page).label.innerText(), 'Page 2 of 3');
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most endorsed']);
      // Circle in the files goes back to the page too.
      await page.locator(`[data-card="${browseId('endorsements', 2, 1)}"]`).click(); await page.locator('[data-file-card]').first().waitFor();
      await press(page, CIRCLE); assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Zeta world');
      assert.equal(fake.requests.length, asked, 'none of this asked Nexus again');
      // Search from All mods: Back returns to the same page; Circle leaves the results the same way.
      assert.equal(await page.locator('#nexus-search-hint').isHidden(), true);
      await runSearch(page, 'zebra'); await resultsReady(page);
      assert.equal(await page.getByRole('button', { name: 'Back to all mods' }).count(), 1);
      assert.equal(await page.getByRole('button', { name: 'Back to latest mods' }).count(), 0);
      await press(page, CIRCLE); assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Zeta world');
      assert.equal(await browsePager(page).label.innerText(), 'Page 2 of 3');
      // From Newest added it returns there, and the hint about the filter comes back.
      await page.getByRole('button', { name: 'Newest added' }).click(); await page.locator('[data-card="mod:30"]').waitFor();
      assert.equal(await page.locator('#nexus-search-hint').innerText(), 'This searches Nexus for this game by mod name. The filter below only narrows the newest mods already loaded.');
      await runSearch(page, 'zebra'); await resultsReady(page);
      await page.getByRole('button', { name: 'Back to newest mods' }).first().click();
      await page.getByRole('heading', { name: 'Newest added mods for Zeta world' }).waitFor();
      assert.equal(fake.requests.length, asked);
      await clean(page, api);
    });

    await test('browse: the view is not kept across a reload, and a looked-up mod goes back to All mods', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake, { get: async (url, request) => {
        const lookup = /^\/api\/games\/zeta\/mods\/([0-9]+)$/.exec(url.pathname);
        if (lookup) return { status: 200, body: { item: hit(Number(lookup[1]), lookup[1] === '30' ? 'Zebra textures' : `Looked up ${lookup[1]}`) } };
        return fake.get(url, request);
      } });
      await openZetaAll(page);
      await sortButton(page, 'Name A–Z').click(); await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).waitFor();
      await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).click(); await page.locator('[data-file-card]').first().waitFor();
      // A reload while a mod is open restores that mod (by lookup) and, behind it, All mods of the game.
      await page.reload(); await page.locator('[data-file-card]').first().waitFor();
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor();
      assert.deepEqual(await pressedLabels(page), ['All mods', 'Most downloaded'], 'the sort is not kept');
      // Find a Nexus mod by id on a browse host: its files open; back shows All mods, not a list that holds only that mod.
      await page.locator('.breadcrumbs [data-breadcrumb="games"]').click();
      await page.locator('#lookup-domain').fill('zeta'); await page.locator('#lookup-mod-id').fill('30'); await page.locator('#lookup-submit').click();
      await page.locator('[data-file-card]').first().waitFor();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Files for Zebra textures');
      await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
      await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor();
      assert.deepEqual(await cards(page, 'mod'), ['1011', '1012']);
      await page.getByRole('button', { name: 'Newest added' }).click(); await page.locator('[data-card="mod:30"]').waitFor();
      assert.deepEqual(await cards(page, 'mod'), ['30', '12', '45', '7', '21'], 'Newest added is the latest list, not the looked-up mod');
      await clean(page, api);
    });

    await test('browse: the controller walks the views, sorts, cards and pager; L2 and R2 jump to the first and last control; Square does nothing on a card', async () => {
      const fake = browseApi(); fake.reply = browsePageReply();
      const { page, api } = await browseFixture(fake);
      await openZetaAll(page);
      // Every control of the view is reached from the tabs with the four directions, each with a focus ring.
      const reach = await unreachable(page, 'games');
      assert.ok(reach.total > 8, `${reach.total} stops`); assert.deepEqual(reach.missing, [], 'every stop is reachable'); assert.deepEqual(reach.noRing, []);
      // Left and Right walk a row of the selector; Down goes to the row of sorts, then the pager, then the cards.
      await page.locator('[data-browse-view="all"]').focus();
      await press(page, RIGHT); assert.equal(await browseFocus(page), 'view-newest');
      await press(page, LEFT); assert.equal(await browseFocus(page), 'view-all');
      await press(page, RIGHT); await press(page, RIGHT); assert.equal(await browseFocus(page), 'sort-downloads', 'at the end of the views Right carries on to the sorts');
      await press(page, RIGHT); assert.equal(await browseFocus(page), 'sort-endorsements');
      await press(page, CROSS); await page.locator(`[data-card="${browseId('endorsements', 1, 1)}"]`).waitFor();
      assert.equal(await browseFocus(page), 'sort-endorsements', 'Cross picks the sort and focus stays on it');
      await press(page, RIGHT); await press(page, RIGHT); assert.equal(await browseFocus(page), 'sort-name');
      await press(page, CROSS); await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).waitFor();
      assert.equal(await browseFocus(page), 'sort-name');
      // L2 and R2: the first control of the list is the first view button, the last is the bottom Next.
      await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).focus();
      await press(page, L2); assert.equal(await browseFocus(page), 'view-all');
      await press(page, R2); assert.equal(await browseFocus(page), 'next-bottom');
      await press(page, CROSS); await browseSettled(page, 2, 3);
      assert.equal(await browseFocus(page), 'next-bottom', 'Cross on Next holds focus while the page changes');
      await press(page, LEFT); assert.equal(await browseFocus(page), 'prev-bottom');
      await press(page, CROSS); await browseSettled(page, 1, 3);
      // Cross on a card opens its files; Square (quick action) on a mod card does nothing; Options opens the details sheet.
      await page.locator(`[data-card="${browseId('name', 1, 1)}"]`).focus();
      const before = fake.requests.length;
      await press(page, SQUARE);
      assert.equal(await page.locator('#details-dialog[open]').count(), 0); assert.equal(await page.locator('[data-file-card]').count(), 0); assert.deepEqual(api.posts, []);
      await press(page, OPTIONS); await page.locator('#details-dialog[open]').waitFor(); await press(page, CIRCLE);
      await press(page, CROSS); await page.locator('[data-file-card]').first().waitFor();
      assert.equal(fake.requests.length, before);
      await press(page, CIRCLE); await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor();
      await clean(page, api);
    });

    await test('browse: the selector, cards, pager and every state fit the TV safe area and type floor at 1920x1080 and 1280x720', async () => {
      for (const viewport of [FULL_HD, HD]) {
        const fake = browseApi();
        fake.reply = ({ sort, page: number }) => ({ status: 200, body: { items: [...Array.from({ length: 19 }, (_, index) => hit(100 + index, `Mod ${index} ${longName}`, { author: longName, summary: longName })), hit(7, '<b>Short</b>')], total: 4000, page: number, pages: 50, sort } });
        const { page, api } = await browseFixture(fake, { viewport });
        await openZetaAll(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, All mods`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, All mods`);
        await page.locator('[data-browse-pager="bottom"] [data-browse-page="next"]').focus();
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, pager focused`);
        await sortButton(page, 'Recently updated').focus();
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, sort focused`);
        await page.getByRole('button', { name: 'Newest added' }).click(); await page.locator('[data-card="mod:30"]').waitFor();
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, Newest added`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, Newest added`);
        await page.getByRole('button', { name: 'All mods' }).click();
        for (const [label, reply, status] of [['empty', browseNone(0, 0), 'empty'], ['none listed', browseNone(41, 3), 'none-listed'],
                                              ['failed', { status: 400, body: { error: { code: 'rate_limit', message: 'x' } } }, 'failed']]) {
          fake.reply = reply; await sortButton(page, label === 'empty' ? 'Most endorsed' : label === 'none listed' ? 'Name A–Z' : 'Most downloaded').click(); await browseStatusOf(page, status).waitFor();
          assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, ${label}`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, ${label}`);
        }
        await clean(page, api);
        await page.context().close();
      }
    });

    // ---- Sign in with a personal API key (docs/INSTALL-FLOW-UX.md 3.11): POST /api/auth/key {"api_key": string} ----
    const SECRET = 'owned-personal-key-0123456789abcdef';
    const signedOutAccount = () => ({ credentials_present: false, live_service: 'not_checked', status: 'disconnected', console_translation: 'unverified' });
    const connectedAccount = (extra = {}) => ({ credentials_present: true, live_service: 'verified', status: 'connected', console_translation: 'unverified',
      account: { user_id: 42, display_name: 'Key person', is_premium: false, is_supporter: false, adult_content: null, ...extra } });
    const keyPanel = page => page.locator('#key-sign-in');
    const keyField = page => page.locator('#key-input');
    const keyButton = page => page.locator('#key-submit');
    const keyMessageText = page => page.locator('#key-sign-in-message').innerText();
    const keyReady = page => keyPanel(page).waitFor({ state: 'visible' });
    // A signed-out host that takes keys. `accept` decides the reply to a POST of the key; the account record flips on success.
    const keyFixture = (extra = {}) => {
      const holder = {};
      return fixture({ capabilities: { nexus_key_sign_in: true }, account: signedOutAccount(), ready: keyReady, ...extra, reply: post => {
        if (post.path === '/api/auth/key') {
          const reply = (extra.accept || (() => ({ status: 200, body: connectedAccount() })))(post);
          if (reply.status === 200 && reply.body?.status === 'connected' && reply.body.account && !Array.isArray(reply.body.account)) holder.api.account = reply.body;
          return reply;
        }
        if (post.path === '/api/auth/disconnect') { holder.api.account = signedOutAccount(); return { status: 200, body: { status: 'disconnected' } }; }
        return undefined;
      } }).then(result => { holder.api = result.api; return result; });
    };
    const keyPosts = api => api.posts.filter(post => post.path === '/api/auth/key');
    const accountText = async page => (await page.locator('#account').innerText()).replace(/\s+/g, ' ');
    const failure = (status, code) => ({ status, body: { error: { code, message: 'host text must never show' } } });

    await test('key sign-in: the form shows only signed out, on a host that advertises nexus_key_sign_in exactly true, and asks for no catalog first', async () => {
      for (const [label, capabilities] of [['absent', {}], ['false', { nexus_key_sign_in: false }], ['the string "true"', { nexus_key_sign_in: 'true' }], ['1', { nexus_key_sign_in: 1 }], ['null', { nexus_key_sign_in: null }]]) {
        const { page, api } = await fixture({ capabilities, account: signedOutAccount(), ready: page => page.locator('#account').getByText('Nexus account not connected').waitFor() });
        assert.equal(await keyPanel(page).isHidden(), true, label);
        assert.equal(await page.locator('#sign-in').count(), 0, `${label}: no Sign in button: this host starts no sign-in at all`);
        await clean(page, api); await page.context().close();
      }
      // Signed in: hidden (and never shown after the account answers verified).
      let { page, api } = await fixture({ capabilities: { nexus_key_sign_in: true } });
      assert.equal(await keyPanel(page).isHidden(), true); await clean(page, api); await page.context().close();
      // The account's answer is awaited: nothing flashes while "Checking Nexus account…" is showing.
      let release; const held = new Promise(resolve => { release = resolve; });
      ({ page, api } = await fixture({ capabilities: { nexus_key_sign_in: true }, account: signedOutAccount(), ready: async () => {}, get: async url => { if (url.pathname === '/api/account') await held; } }));
      await page.locator('#account').getByText('Checking Nexus account…').waitFor();
      assert.equal(await keyPanel(page).isHidden(), true);
      release(); await keyReady(page);
      // Signed out: visible, no error banner, and the catalog was not asked for (it would only fail without an account).
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'a first run is not an error');
      assert.ok(!api.requests.includes('GET /api/games'), 'no catalog request while signed out');
      assert.equal(await accountText(page), 'Nexus account not connected Sign in');
      assert.match(await page.locator('#games-content').innerText(), /No games available/);
      await clean(page, api); await page.context().close();
      // Nexus unreachable (account status "error"): the key can still be replaced.
      ({ page, api } = await fixture({ capabilities: { nexus_key_sign_in: true }, account: { status: 'error', credentials_present: true, live_service: 'unavailable' }, ready: keyReady }));
      assert.equal(await keyPanel(page).isVisible(), true);
      await clean(page, api);
    });

    await test('key sign-in: the field is masked and not autofilled, the heading and sentence say where the key is, and the key stays out of the page', async () => {
      const { page, api } = await keyFixture();
      assert.equal(await keyPanel(page).locator('h2').innerText(), 'Sign in with your Nexus API key');
      assert.equal(await keyField(page).getAttribute('type'), 'password');
      for (const [name, value] of [['autocomplete', 'off'], ['autocapitalize', 'off'], ['spellcheck', 'false'], ['maxlength', '512']]) assert.equal(await keyField(page).getAttribute(name), value, name);
      assert.equal(await page.getByLabel('Nexus API key', { exact: true }).count(), 1, 'the field has a visible label');
      assert.equal(await page.locator('#key-sign-in-help').innerText(), 'To find it, open the Nexus Mods website, go to your account settings, choose API Keys, and copy your Personal API Key. ModBridge keeps it in private storage on this device and sends it only to Nexus, so you stay signed in next time.');
      assert.equal(await keyField(page).getAttribute('aria-describedby'), 'key-sign-in-help key-sign-in-message');
      assert.equal(await page.locator('#key-sign-in-message').getAttribute('role'), 'alert');
      assert.equal(await keyButton(page).innerText(), 'Sign in');
      await keyField(page).fill(SECRET);
      assert.equal(await keyField(page).inputValue(), SECRET);
      assert.ok(!(await page.content()).includes(SECRET), 'the typed key is in no attribute or text of the page');
      assert.ok(!(await page.locator('body').innerText()).includes(SECRET));
      assert.equal(await page.evaluate(secret => location.href.includes(secret) || document.cookie.includes(secret) || JSON.stringify(sessionStorage).includes(secret) || JSON.stringify(localStorage).includes(secret), SECRET), false);
      // Typing is not sending: nothing was asked, and the controller hint is unchanged.
      assert.deepEqual(keyPosts(api), []);
      await clean(page, api);
    });

    await test('key sign-in: success posts exactly {api_key} with the CSRF token, empties the field, refreshes the account and loads the games; Disconnect brings the form back', async () => {
      const { page, api } = await keyFixture();
      await keyField(page).fill(`  ${SECRET}  `);
      await keyField(page).press('Enter');
      await page.locator('#account').getByText('Nexus · Key person').waitFor();
      const posts = keyPosts(api);
      assert.equal(posts.length, 1);
      assert.deepEqual(posts[0].body, { api_key: SECRET }, 'trimmed, and nothing else in the body');
      assert.equal(posts[0].headers['x-csrf-token'], 'browse-csrf');
      assert.match(posts[0].headers['content-type'], /^application\/json/);
      assert.equal(await keyPanel(page).isHidden(), true);
      assert.equal(await keyField(page).inputValue(), '', 'the field is emptied');
      assert.equal(await page.locator('#account .account-tier').innerText(), 'Free');
      assert.equal(await page.locator('#toast').innerText(), 'Signed in to Nexus.');
      assert.equal(await page.locator('#announcer').innerText(), 'Signed in to Nexus.');
      await page.getByRole('button', { name: 'Zeta world' }).waitFor();
      assert.equal(api.requests.filter(request => request === 'GET /api/games').length, 1, 'the catalog is asked for once, after sign-in');
      assert.equal(await active(page), 'game:zeta', 'focus moves on from the form that went away, to the first game');
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      assert.ok(!(await page.content()).includes(SECRET));
      assert.equal(await page.evaluate(secret => JSON.stringify(sessionStorage).includes(secret) || JSON.stringify(localStorage).includes(secret), SECRET), false);
      // The games open as for any signed-in account.
      await page.getByRole('button', { name: 'Zeta world' }).click(); await page.locator('[data-card^="mod:"]').first().waitFor();
      // Disconnect from inside a game: the form is back, empty, and the games list is gone with the account (the next sign-in asks again).
      await page.getByRole('button', { name: 'Disconnect' }).click();
      await keyReady(page);
      assert.equal(await keyField(page).inputValue(), '');
      assert.equal(await page.locator('#games-view h1').innerText(), 'Choose a game');
      assert.equal(await page.locator('[data-card]').count(), 0);
      assert.match(await page.locator('#games-content').innerText(), /No games available/);
      assert.equal(await accountText(page), 'Nexus account not connected Sign in');
      await clean(page, api);
    });

    await test('key sign-in: every failure reads the catalog text beside the field, keeps the field masked and filled, raises no banner, and can be retried', async () => {
      let reply = failure(400, 'key_rejected');
      const { page, api } = await keyFixture({ accept: () => reply });
      const cases = [[failure(400, 'key_rejected'), 'Nexus did not accept that API key. Copy the whole Personal API Key from your Nexus account settings and try again.'],
        [failure(429, 'too_many_attempts'), 'Too many failed sign-in attempts. Wait ten minutes, then try again.'],
        [failure(400, 'invalid_input'), 'The host did not accept this request. Try again; if it keeps failing, restart ModBridge.'],
        [failure(401, 'session_required'), 'Your local session has expired. Reload this page.'], [failure(403, 'origin_refused'), 'The request was blocked because it did not come from this app. Reload this page.'],
        [failure(409, 'busy'), 'ModBridge is busy with another operation. Wait a moment and try again.'], [failure(400, 'rate_limit'), 'Nexus asked ModBridge to slow down. Wait a minute and try again.'],
        [failure(400, 'network'), 'Nexus could not be reached. Try again.'], [failure(400, 'timeout'), 'Nexus took too long to answer. Try again.'], [failure(400, 'payload'), 'Nexus sent a reply that could not be used. Try again later.'],
        [failure(500, 'operation_failed'), 'Something went wrong. Request it again.'], [failure(500, 'a_code_nobody_listed'), 'Request failed (500)'],
        [{ status: 200, body: { status: 'disconnected' } }, 'The host sent a reply this page could not verify.'],
        [{ status: 200, body: { status: 'connected', live_service: 'verified', account: [] } }, 'The host sent a reply this page could not verify.'],
        [{ status: 200, body: { status: 'connected', live_service: 'unavailable', account: {} } }, 'The host sent a reply this page could not verify.']];
      await keyField(page).fill(SECRET);
      for (const [answer, text] of cases) {
        reply = answer;
        await keyButton(page).click();
        await page.locator('#key-sign-in-message', { hasText: /\S/ }).waitFor();
        assert.equal(await keyMessageText(page), text, JSON.stringify(answer.body).slice(0, 60));
        assert.equal(await keyField(page).getAttribute('type'), 'password');
        assert.equal(await keyField(page).inputValue(), SECRET, 'the field keeps what was typed, so a typo can be fixed');
        assert.equal(await keyPanel(page).isVisible(), true);
        assert.equal(await page.locator('#status-banner').isHidden(), true, 'no page-wide error');
        assert.equal(await accountText(page), 'Nexus account not connected Sign in');
        assert.equal(await keyButton(page).getAttribute('aria-disabled'), 'false');
        assert.doesNotMatch(await page.locator('body').innerText(), /host text must never show/);
        await page.locator('#key-sign-in-message').evaluate(el => { el.textContent = ''; });
      }
      assert.equal(keyPosts(api).length, cases.length);
      assert.ok(keyPosts(api).every(post => post.body.api_key === SECRET));
      // The host does not answer at all.
      await page.context().route(`${new URL(page.url()).origin}/api/auth/key`, route => route.abort());
      await keyButton(page).click();
      await page.locator('#key-sign-in-message', { hasText: /\S/ }).waitFor();
      assert.equal(await keyMessageText(page), 'ModBridge could not be reached. Try again.');
      await page.context().unroute(`${new URL(page.url()).origin}/api/auth/key`);
      // A later good answer signs in, from the same field.
      reply = { status: 200, body: connectedAccount() };
      await keyButton(page).click();
      await page.locator('#account').getByText('Nexus · Key person').waitFor();
      assert.equal(await keyPanel(page).isHidden(), true);
      assert.equal(api.requests.filter(request => request === 'GET /api/games').length, 1, 'the catalog was asked for only after the good key');
      await clean(page, api);
    });

    await test('key sign-in: an empty key, a key with a space, a long key or a non-ASCII key is refused by the page with no request; a form in flight is inert and sends once', async () => {
      let release; const held = new Promise(resolve => { release = resolve; });
      const { page, api } = await keyFixture();
      // The host answers the key only when this test lets it (the earlier route, the fixture's own, answers it then).
      await page.context().route(`${new URL(page.url()).origin}/api/auth/key`, async route => { await held; await route.fallback(); });
      const set = value => keyField(page).evaluate((el, text) => { el.value = text; }, value);
      for (const [value, message] of [['', 'Type or paste your Nexus API key first.'], ['   ', 'Type or paste your Nexus API key first.'], ['two words', 'An API key has no spaces and is at most 512 characters.'],
                                      ['k'.repeat(513), 'An API key has no spaces and is at most 512 characters.'], ['café-key', 'An API key has no spaces and is at most 512 characters.'], ['tab\tkey', 'An API key has no spaces and is at most 512 characters.']]) {
        await set(value); await keyButton(page).click();
        assert.equal(await keyMessageText(page), message, JSON.stringify(value).slice(0, 30));
        assert.equal(await page.locator('#announcer').innerText(), message);
      }
      assert.deepEqual(keyPosts(api), [], 'no request for any of them');
      let sent = 0; page.on('request', request => { if (request.method() === 'POST' && request.url().endsWith('/api/auth/key')) sent++; });
      await set('k'.repeat(512)); await keyField(page).focus(); await keyButton(page).click();
      await page.locator('#key-submit[aria-disabled="true"]').waitFor();
      assert.equal(await keyMessageText(page), '', 'the longest key is not refused by the page');
      assert.equal(await keyButton(page).innerText(), 'Signing in…');
      assert.equal(sent, 1);
      await keyButton(page).click({ force: true }); await keyField(page).press('Enter');
      await page.waitForTimeout(100);
      assert.equal(sent, 1, 'a form in flight sends nothing more');
      release(); await page.locator('#account').getByText('Nexus · Key person').waitFor();
      assert.equal(keyPosts(api).length, 1);
      assert.equal(keyPosts(api)[0].body.api_key.length, 512);
      await clean(page, api);
    });

    await test('key sign-in: the controller reaches the field and button, the field gives up Up and Down, and Cross on the button signs in', async () => {
      const { page, api } = await keyFixture();
      const result = await unreachable(page, 'games');
      assert.ok(result.total >= 6, `${result.total} stops`); assert.deepEqual(result.missing, [], 'every stop is reachable'); assert.deepEqual(result.noRing, []);
      await page.locator('#tab-games').focus();
      await page.keyboard.press('ArrowDown'); assert.equal(await active(page), 'key-input', 'Down from the tabs reaches the field first');
      await page.keyboard.press('ArrowRight'); assert.equal(await active(page), 'key-submit', 'Right at the end of an empty field reaches the button');
      await page.keyboard.press('ArrowLeft'); assert.equal(await active(page), 'key-input');
      await keyField(page).fill('abc');
      await page.keyboard.press('ArrowLeft'); assert.equal(await active(page), 'key-input', 'Left inside the text edits the caret');
      await page.keyboard.press('End'); await page.keyboard.press('ArrowDown'); assert.notEqual(await active(page), 'key-input', 'Down leaves the field');
      assert.equal(await keyField(page).inputValue(), 'abc');
      await keyField(page).focus(); await page.keyboard.press('ArrowUp'); assert.match(await active(page), /^tab-/, 'Up leaves the field for the tabs');
      // The D-pad calls the same code; Cross on the field only focuses it (a field takes no Cross), Cross on the button submits.
      await keyField(page).focus(); await keyField(page).fill(SECRET);
      await press(page, UP); assert.match(await active(page), /^tab-/);
      await press(page, DOWN); assert.equal(await active(page), 'key-input');
      await press(page, CROSS); assert.deepEqual(keyPosts(api), [], 'Cross in the field sends nothing');
      await press(page, DOWN); assert.notEqual(await active(page), 'key-input');
      await keyButton(page).focus(); await press(page, CROSS);
      await page.locator('#account').getByText('Nexus · Key person').waitFor();
      assert.equal(keyPosts(api).length, 1);
      await clean(page, api);
    });

    await test('key sign-in: Sign in in the bar and in an error banner moves to the key field on a host with no OAuth, from any tab; with OAuth available it keeps starting OAuth', async () => {
      let { page, api } = await keyFixture({ jobsError: 'reconnect_required' });
      // The jobs list failed with a sign-in code: the banner offers Sign in, and it goes to the field.
      await page.locator('#status-banner #banner-sign-in').waitFor();
      await page.locator('#status-banner #banner-sign-in').click();
      assert.equal(await active(page), 'key-input');
      await page.getByRole('tab', { name: 'Activity' }).click();
      assert.equal(await page.locator('#games-view').isHidden(), true);
      await page.locator('#sign-in').click();
      assert.equal(await page.locator('#games-view').isVisible(), true, 'the Games tab comes back');
      assert.equal(await active(page), 'key-input');
      assert.deepEqual(api.posts.filter(post => post.path === '/api/auth/start'), [], 'no OAuth on this host');
      await clean(page, api); await page.context().close();
      // Both: the bar's Sign in is the OAuth one; the form is there as well.
      ({ page, api } = await fixture({ capabilities: { nexus_key_sign_in: true, nexus_auth_start: true }, account: signedOutAccount(), ready: keyReady,
        reply: post => post.path === '/api/auth/start' ? { status: 409, body: { error: { code: 'configuration', message: 'x' } } } : undefined }));
      await page.locator('#sign-in').click();
      await page.locator('#status-banner').getByText('This is not available in this build.').waitFor();
      assert.equal(api.posts.filter(post => post.path === '/api/auth/start').length, 1);
      assert.notEqual(await active(page), 'key-input');
      assert.equal(await keyPanel(page).isVisible(), true);
      await clean(page, api); await page.context().close();
      // OAuth only: no key form.
      ({ page, api } = await fixture({ capabilities: { nexus_auth_start: true }, account: signedOutAccount(), ready: page => page.locator('#sign-in').waitFor() }));
      assert.equal(await keyPanel(page).isHidden(), true);
      await clean(page, api);
    });

    await test('key sign-in: the form and its messages fit the TV safe area and type floor at 1920x1080 and 1280x720', async () => {
      for (const viewport of [FULL_HD, HD]) {
        const { page, api } = await keyFixture({ viewport, accept: () => failure(429, 'too_many_attempts') });
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, form`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, form`);
        await keyField(page).fill(SECRET); await keyButton(page).click(); await page.locator('#key-sign-in-message', { hasText: /\S/ }).waitFor();
        await keyField(page).focus();
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, message`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, message`);
        await keyButton(page).focus();
        assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, button focused`);
        await clean(page, api); await page.context().close();
      }
    });

    // ---- Your games (docs/INSTALL-FLOW-UX.md 3.12): the console's titles above the Nexus games, each tied to its Nexus game ----
    const titleToken = n => n.toString(16).padStart(32, '0');
    const meta = (value, source = 'homebrew_dump') => value == null ? { value: null, status: 'unavailable', source: null } : { value, status: 'observed', source };
    const titleRow = (n, id, name, version, source = 'homebrew_dump') => ({ title_key: titleToken(n + 1), title_id: meta(id, source), display_name: meta(name, source), build_version: meta(version, source), build_fingerprint: null });
    const yourNexusGames = () => [
      { domain_name: 'zeta', id: 1, name: 'Zeta world', mods: 12 },
      { domain_name: 'alpha', id: 2, name: 'Alpha world', mods: 340 },
      { domain_name: 'silenthilltownfall', id: 3, name: 'Silent Hill: Townfall', mods: 7 },
      { domain_name: 'twinpeaks', id: 4, name: 'Twin Peaks', mods: 1 },
      { domain_name: 'twinpeaks2', id: 5, name: 'Twin-Peaks', mods: 1 },
      { domain_name: 'cafe', id: 6, name: 'Café Noir', mods: 3 },
    ];
    const fillerGames = count => Array.from({ length: count }, (_, n) => ({ domain_name: `filler${n}`, id: 100 + n, name: `Filler game ${n}` }));
    // One of each kind: the confirmed table, an exact normalized name, an ambiguous name, a substring, a typo, an unknown source word.
    const yourItems = () => [
      titleRow(1, 'PPSA33286', 'SILENT HILL: Townfall', '02.003.000'),
      titleRow(2, 'PPSA11111', 'Zeta World™', '1.00', 'user_metadata_cache'),
      titleRow(3, 'PPSA22222', 'Twin Peaks', '1.0', 'system_private_metadata_cache'),
      titleRow(4, 'PPSA33333', 'Zeta', null),
      titleRow(5, 'PPSA44444', 'Alpha wrld', '1.0'),
      titleRow(6, 'PPSA55555', 'Unknown Game', '9.9', 'something_else'),
    ];
    const yourReady = page => page.locator('[data-your-game]').first().waitFor();
    const yourFixture = (titles, extra = {}) => fixture({ titles, games: yourNexusGames(), ready: yourReady, ...extra });
    const yourCard = (page, id) => page.locator(`[data-your-game="${id}"]`);
    const yourIds = page => page.$$eval('[data-your-game]', els => els.map(el => el.dataset.yourGame));
    const yourButtons = async (page, id) => (await yourCard(page, id).locator('button').allInnerTexts()).map(text => text.trim());
    const yourFocus = page => page.evaluate(() => document.activeElement?.dataset?.ygFocus || document.activeElement?.id || '');
    const choiceStore = page => page.evaluate(() => localStorage.getItem('modbridge.title-games.v1'));
    const seedChoices = raw => ({ init: `if (!sessionStorage.getItem('yg-seeded')) { sessionStorage.setItem('yg-seeded', '1'); localStorage.setItem('modbridge.title-games.v1', ${JSON.stringify(raw)}); }` });
    const titlesSettled = page => page.waitForFunction(() => !/Checking/.test(document.querySelector('#title-status')?.textContent || ''));
    const yourBrowseFake = (domainBody = 'zeta') => { const fake = browseApi(); fake.reply = { status: 200, body: browseBody([hit(30, 'Zebra textures', { game_domain: domainBody })]) }; return fake; };
    const nexusGameRequests = api => api.requests.filter(request => /^GET \/api\/games\/[^/]+\/mods/.test(request));

    await test('your games: the console titles list first with name, id, version and source, above All Nexus games and the unchanged Nexus grid', async () => {
      const { page, api } = await yourFixture({ items: yourItems() });
      await page.getByRole('heading', { name: 'Your games', level: 2 }).waitFor();
      assert.equal(await page.locator('#games-view h1').innerText(), 'Choose a game');
      assert.equal(await page.locator('#all-games-heading').innerText(), 'All Nexus games');
      // Sorted by name (the host sends title id order), every title one card.
      assert.deepEqual(await yourIds(page), ['PPSA44444', 'PPSA33286', 'PPSA22222', 'PPSA55555', 'PPSA33333', 'PPSA11111']);
      assert.deepEqual(await page.evaluate(() => {
        const at = id => document.querySelector(id), before = (a, b) => !!(a.compareDocumentPosition(b) & Node.DOCUMENT_POSITION_FOLLOWING);
        return [before(at('#your-games'), at('#all-games-heading')), before(at('#all-games-heading'), at('#catalog-tools')), before(at('#catalog-tools'), at('#games-content'))];
      }), [true, true, true], 'Your games, then the All Nexus games heading, then the filter tools and the grid');
      const townfall = await yourCard(page, 'PPSA33286').innerText();
      assert.match(townfall, /SILENT HILL: Townfall/); assert.match(townfall, /PPSA33286/); assert.match(townfall, /Version 02\.003\.000/); assert.match(townfall, /Dump/);
      assert.match(await yourCard(page, 'PPSA11111').innerText(), /Version 1\.00\s*·\s*Installed/);
      assert.match(await yourCard(page, 'PPSA22222').innerText(), /Installed/);
      assert.match(await yourCard(page, 'PPSA33333').innerText(), /Version unavailable/);
      assert.equal(await yourCard(page, 'PPSA55555').locator('.your-game-source').count(), 0, 'an unknown source shows no source word');
      assert.equal(await yourCard(page, 'PPSA33286').locator('.your-game-source').innerText(), 'Dump');
      const note = await page.locator('#your-games').innerText();
      assert.match(note, /Installed: listed in the console's app data, which can outlive an uninstall/);
      assert.doesNotMatch(note, /\b(supports?|supported|moddable|compatible)\b/i, 'the section never says a game supports mods');
      // The Nexus grid keeps its own cards and nothing of Your games leaks into them.
      assert.deepEqual(await cards(page, 'game'), ['zeta', 'alpha', 'silenthilltownfall', 'twinpeaks', 'twinpeaks2', 'cafe']);
      assert.equal(await page.locator('#games-content [data-your-game]').count(), 0);
      await clean(page, api);
    });

    await test('your games: the confirmed table, an exact normalized name and a remembered choice match; ambiguous, partial and misspelt names do not', async () => {
      const { page, api } = await yourFixture({ items: yourItems() });
      const confirmed = yourCard(page, 'PPSA33286');
      assert.match(await confirmed.locator('.your-game-match').innerText(), /^Nexus game: Silent Hill: Townfall\s*Confirmed match$/);
      assert.deepEqual(await yourButtons(page, 'PPSA33286'), ['Browse mods'], 'a confirmed match has nothing to change');
      // Exact after normalizing: the trade-mark sign and case go.
      assert.match(await yourCard(page, 'PPSA11111').locator('.your-game-match').innerText(), /^Nexus game: Zeta world\s*Same name$/);
      assert.deepEqual(await yourButtons(page, 'PPSA11111'), ['Browse mods', 'Change']);
      // Two Nexus games share the normalized name "twin peaks": no match. A substring and a typo are no match either.
      for (const id of ['PPSA22222', 'PPSA33333', 'PPSA44444', 'PPSA55555']) {
        assert.equal(await yourCard(page, id).locator('.your-game-match').innerText(), 'No Nexus game chosen yet.', id);
        assert.deepEqual(await yourButtons(page, id), ['Choose the Nexus game'], id);
      }
      await clean(page, api);
    });

    await test('your games: names are compared after dropping marks, accents, case and punctuation, never fuzzily', async () => {
      const items = [titleRow(1, 'N1', 'ÁLPHA   World®'), titleRow(2, 'N2', 'Café-Noir'), titleRow(3, 'N3', 'zeta WORLD (TM)'),
        titleRow(4, 'N4', 'Alpha World Remastered'), titleRow(5, 'N5', 'Alpha'), titleRow(6, 'N6', 'Zeta worlds'), titleRow(7, 'N7', '!!!')];
      const { page, api } = await yourFixture({ items });
      const line = async id => (await yourCard(page, id).locator('.your-game-match').innerText()).replace(/\s+/g, ' ');
      assert.equal(await line('N1'), 'Nexus game: Alpha world Same name');
      assert.equal(await line('N2'), 'Nexus game: Café Noir Same name');
      assert.equal(await line('N3'), 'Nexus game: Zeta world Same name');
      for (const id of ['N4', 'N5', 'N6', 'N7']) assert.equal(await line(id), 'No Nexus game chosen yet.', id);
      await clean(page, api);
    });

    await test('your games: Choose the Nexus game filters the loaded games, shows at most 12, saves the pick, and Clear choice forgets it', async () => {
      const { page, api } = await yourFixture({ items: yourItems() }, { games: [...yourNexusGames(), ...fillerGames(20)] });
      const card = yourCard(page, 'PPSA33333');
      await card.getByRole('button', { name: 'Choose the Nexus game' }).click();
      const filter = card.getByLabel('Filter Nexus games');
      await filter.waitFor();
      assert.equal(await yourFocus(page), 'PPSA33333|filter');
      assert.equal(await card.locator('[data-yg-pick]').count(), 12, 'at most 12 games at a time');
      assert.equal(await card.locator('[data-yg-count]').innerText(), 'Showing 12 of 26 Nexus games. Type to narrow the list.');
      // The filter folds accents and case, matches name or domain, and typing keeps the focus in the field.
      await filter.fill('CAFE');
      assert.deepEqual(await card.locator('[data-yg-pick]').evaluateAll(els => els.map(el => el.dataset.ygPick)), ['cafe']);
      assert.equal(await yourFocus(page), 'PPSA33333|filter');
      await filter.fill('zzz');
      assert.equal(await card.locator('[data-yg-pick]').count(), 0);
      assert.equal(await card.locator('[data-yg-count]').innerText(), 'No Nexus game matches that.');
      await filter.fill('alpha');
      await card.locator('[data-yg-pick="alpha"]').click();
      // Matched at once, remembered, focus on the card's Browse mods.
      assert.equal(await card.locator('[data-yg-filter]').count(), 0);
      assert.match(await card.locator('.your-game-match').innerText(), /^Nexus game: Alpha world\s*Your choice$/);
      assert.deepEqual(await yourButtons(page, 'PPSA33333'), ['Browse mods', 'Change', 'Clear choice']);
      assert.equal(await yourFocus(page), 'PPSA33333|browse');
      assert.deepEqual(JSON.parse(await choiceStore(page)), { version: 1, choices: { PPSA33333: 'alpha' } });
      await page.reload(); await yourReady(page);
      assert.match(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), /Nexus game: Alpha world/, 'remembered across a reload');
      // Change: pick another game; the choice is replaced, not added.
      await yourCard(page, 'PPSA33333').getByRole('button', { name: 'Change' }).click();
      await yourCard(page, 'PPSA33333').getByLabel('Filter Nexus games').fill('zeta');
      await yourCard(page, 'PPSA33333').locator('[data-yg-pick="zeta"]').click();
      assert.match(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), /Nexus game: Zeta world/);
      assert.deepEqual(JSON.parse(await choiceStore(page)), { version: 1, choices: { PPSA33333: 'zeta' } });
      // Clear choice removes just that title's choice and the whole record when it was the last.
      await yourCard(page, 'PPSA33333').getByRole('button', { name: 'Clear choice' }).click();
      assert.equal(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), 'No Nexus game chosen yet.');
      assert.equal(await choiceStore(page), null);
      assert.equal(await yourFocus(page), 'PPSA33333|choose');
      await page.reload(); await yourReady(page);
      assert.equal(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), 'No Nexus game chosen yet.');
      await clean(page, api);
    });

    await test('your games: Escape and Cancel close the chooser and return to the control that opened it; a pick of another title keeps both choices', async () => {
      const { page, api } = await yourFixture({ items: yourItems() });
      const card = yourCard(page, 'PPSA33333');
      await card.getByRole('button', { name: 'Choose the Nexus game' }).click();
      await card.getByLabel('Filter Nexus games').fill('alp');
      await page.keyboard.press('Escape');
      assert.equal(await card.locator('.your-game-chooser').count(), 0);
      assert.equal(await yourFocus(page), 'PPSA33333|choose');
      assert.equal(await choiceStore(page), null, 'closing saves nothing');
      await card.getByRole('button', { name: 'Choose the Nexus game' }).click();
      assert.equal(await card.getByLabel('Filter Nexus games').inputValue(), '', 'a reopened chooser starts empty');
      await card.getByRole('button', { name: 'Cancel' }).click();
      assert.equal(await card.locator('.your-game-chooser').count(), 0);
      assert.equal(await yourFocus(page), 'PPSA33333|choose');
      // Two titles, two choices; the older one stays.
      await card.getByRole('button', { name: 'Choose the Nexus game' }).click(); await card.locator('[data-yg-pick="alpha"]').click();
      await yourCard(page, 'PPSA44444').getByRole('button', { name: 'Choose the Nexus game' }).click(); await yourCard(page, 'PPSA44444').locator('[data-yg-pick="zeta"]').click();
      assert.deepEqual(JSON.parse(await choiceStore(page)), { version: 1, choices: { PPSA33333: 'alpha', PPSA44444: 'zeta' } });
      assert.equal(await page.locator('.your-game-chooser').count(), 0);
      await clean(page, api);
    });

    await test('your games: a remembered choice is read strictly; any bad record is dropped whole, a good one is used, and the table still wins', async () => {
      const choices = count => Object.fromEntries(Array.from({ length: count }, (_, n) => [`T${n}`, 'alpha']));
      const bad = [['not JSON', '{nope'], ['an array', '[]'], ['another version', JSON.stringify({ version: 2, choices: { PPSA33333: 'alpha' } })],
        ['an extra field', JSON.stringify({ version: 1, choices: { PPSA33333: 'alpha' }, extra: 1 })],
        ['choices as an array', JSON.stringify({ version: 1, choices: ['alpha'] })],
        ['a domain that is not one', JSON.stringify({ version: 1, choices: { PPSA33333: 'Alpha World!' } })],
        ['a domain that is a number', JSON.stringify({ version: 1, choices: { PPSA33333: 3 } })],
        ['one bad title id beside a good entry', JSON.stringify({ version: 1, choices: { PPSA33333: 'alpha', 'bad id': 'zeta' } })],
        ['129 entries', JSON.stringify({ version: 1, choices: { PPSA33333: 'alpha', ...choices(128) } })],
        ['an oversized record', JSON.stringify({ version: 1, choices: { PPSA33333: 'alpha' } }) + ' '.repeat(40000)]];
      for (const [label, raw] of bad) {
        const { page, api } = await yourFixture({ items: yourItems() }, seedChoices(raw));
        assert.equal(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), 'No Nexus game chosen yet.', label);
        assert.equal(await choiceStore(page), null, `${label}: the bad record is removed`);
        await clean(page, api); await page.context().close();
      }
      const good = JSON.stringify({ version: 1, choices: { PPSA33333: 'alpha', PPSA33286: 'alpha', GONE1: 'zeta' } });
      const { page, api } = await yourFixture({ items: yourItems() }, seedChoices(good));
      assert.match(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), /Nexus game: Alpha world\s*Your choice/);
      assert.match(await yourCard(page, 'PPSA33286').locator('.your-game-match').innerText(), /Nexus game: Silent Hill: Townfall\s*Confirmed match/, 'the confirmed table outranks a remembered choice');
      assert.equal(await yourCard(page, 'GONE1').count(), 0, 'a choice for a title that is not listed shows nothing');
      assert.equal(await choiceStore(page), good);
      await clean(page, api);
    });

    await test('your games: Browse mods opens the Nexus game exactly like picking it in the list (All mods on a browse host, latest mods otherwise)', async () => {
      // Via Your games.
      let fake = yourBrowseFake();
      let { page, api } = await yourFixture({ items: yourItems() }, { capabilities: { mod_browse: true }, get: fake.get, files: anyFiles });
      await yourCard(page, 'PPSA11111').getByRole('button', { name: 'Browse mods' }).click();
      await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor();
      const viaYours = { browse: fake.requests, games: nexusGameRequests(api) };
      assert.deepEqual(viaYours.browse.map(({ game, sort, page }) => ({ game, sort, page })), [{ game: 'zeta', sort: 'downloads', page: 1 }]);
      assert.equal(await page.locator('#your-games').isHidden(), true, 'Your games steps aside once a game is open');
      assert.equal(await page.locator('#all-games-heading').isHidden(), true);
      assert.equal(await page.locator('[data-your-game]').count(), 0);
      assert.equal(await page.locator('.breadcrumbs').innerText().then(text => text.replace(/\s+/g, ' ')), 'Games › Zeta world');
      await clean(page, api); await page.context().close();
      // Via the Nexus list: the same requests.
      fake = yourBrowseFake();
      ({ page, api } = await yourFixture({ items: yourItems() }, { capabilities: { mod_browse: true }, get: fake.get, files: anyFiles }));
      await page.locator('[data-card="game:zeta"]').click();
      await page.getByRole('heading', { name: 'All mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor();
      assert.deepEqual({ browse: fake.requests, games: nexusGameRequests(api) }, viaYours);
      await clean(page, api); await page.context().close();
      // A host without mod_browse: the latest mods list, as the list gives it.
      ({ page, api } = await yourFixture({ items: yourItems() }));
      await yourCard(page, 'PPSA11111').getByRole('button', { name: 'Browse mods' }).click();
      await page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor(); await page.locator('[data-card^="mod:"]').first().waitFor();
      assert.deepEqual(nexusGameRequests(api), ['GET /api/games/zeta/mods']);
      assert.equal(await page.locator('#your-games').isHidden(), true);
      // Back (the breadcrumb) shows the list again with Your games first.
      await page.locator('[data-breadcrumb="games"]').click();
      await yourReady(page);
      assert.equal(await page.locator('#all-games-heading').isVisible(), true);
      await clean(page, api);
    });

    await test('your games: a confirmed or remembered domain missing from the Nexus list still opens; the list not loaded blocks only the chooser', async () => {
      const without = yourNexusGames().filter(game => game.domain_name !== 'silenthilltownfall');
      let fake = yourBrowseFake('silenthilltownfall');
      let { page, api } = await yourFixture({ items: yourItems() }, { games: without, capabilities: { mod_browse: true }, get: fake.get, files: anyFiles });
      assert.match(await yourCard(page, 'PPSA33286').locator('.your-game-match').innerText(), /^Nexus game: silenthilltownfall\s*Confirmed match$/, 'the domain, since the list has no name for it');
      await yourCard(page, 'PPSA33286').getByRole('button', { name: 'Browse mods' }).click();
      await page.getByRole('heading', { name: 'All mods for SILENT HILL: Townfall' }).waitFor();
      assert.deepEqual(fake.requests.map(({ game, sort, page }) => ({ game, sort, page })), [{ game: 'silenthilltownfall', sort: 'downloads', page: 1 }]);
      await clean(page, api); await page.context().close();
      // A remembered domain the list lacks.
      fake = yourBrowseFake('ghostgame');
      ({ page, api } = await yourFixture({ items: yourItems() }, { ...seedChoices(JSON.stringify({ version: 1, choices: { PPSA33333: 'ghostgame' } })), capabilities: { mod_browse: true }, get: fake.get, files: anyFiles }));
      assert.match(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), /^Nexus game: ghostgame\s*Your choice$/);
      await yourCard(page, 'PPSA33333').getByRole('button', { name: 'Browse mods' }).click();
      await page.getByRole('heading', { name: 'All mods for Zeta' }).waitFor();
      assert.equal(fake.requests[0].game, 'ghostgame');
      await clean(page, api); await page.context().close();
      // No games list at all (signed out): the confirmed card still browses; Choose is off with its reason; the Nexus part says so.
      for (const [account, reason] of [[undefined, 'The Nexus games list has not loaded.'], [signedOutAccount(), 'Connect a Nexus account to choose a game.']]) {
        ({ page, api } = await yourFixture({ items: yourItems() }, { games: [], account }));
        assert.equal(await yourCard(page, 'PPSA33286').getByRole('button', { name: 'Browse mods' }).isEnabled(), true);
        const choose = yourCard(page, 'PPSA33333').getByRole('button', { name: 'Choose the Nexus game' });
        assert.equal(await choose.isDisabled(), true);
        assert.match(await yourCard(page, 'PPSA33333').innerText(), new RegExp(reason));
        await clean(page, api); await page.context().close();
      }
      // A host without Nexus browsing: nothing opens.
      ({ page, api } = await yourFixture({ items: yourItems() }, { capabilities: { nexus_catalog: false } }));
      assert.equal(await yourCard(page, 'PPSA33286').getByRole('button', { name: 'Browse mods' }).isDisabled(), true);
      assert.match(await yourCard(page, 'PPSA33286').innerText(), /Nexus browsing is unavailable in this build\./);
      await clean(page, api);
    });

    await test('your games: an inventory that is unavailable, partial, in error, empty or failed leaves the Nexus-only list exactly as it was', async () => {
      const running = { schema_version: 1, status: 'observed', complete: true, snapshot_id: '1'.repeat(32), source_scope: 'running_processes', reason: null, issues: [],
        items: [titleRow(1, 'PPSA99999', 'A running title')] };
      const variants = [['no provider', {}], ['unavailable', { titles: { status: 'unavailable' } }], ['partial', { titles: { status: 'partial', issues: [{ code: 'provider_observation_incomplete' }] } }],
        ['error status', { titles: { status: 'error' } }], ['observed and empty', { titles: { items: [] } }], ['a failed request', { titles: { fail: true } }],
        ['running processes', { legacyTitles: running }],
        ['v1 partial', { legacyTitles: { ...running, status: 'partial', complete: false, snapshot_id: null, source_scope: 'console_title_metadata', reason: 'provider_observation_incomplete', items: [] } }]];
      for (const [label, extra] of variants) {
        const { page, api } = await fixture(extra);
        await titlesSettled(page);
        assert.equal(await page.locator('#your-games').isHidden(), true, `${label}: no Your games`);
        assert.equal(await page.locator('#all-games-heading').isHidden(), true, `${label}: no All Nexus games heading`);
        assert.equal(await page.locator('[data-your-game]').count(), 0, label);
        assert.equal(await page.locator('#games-view h1').innerText(), 'Choose a game', label);
        assert.equal(await page.locator('#games-view .subheading').innerText(), 'Browse the latest mods listed on Nexus.', label);
        assert.deepEqual(await cards(page, 'game'), ['zeta', 'alpha', 'cafe', 'plain'], label);
        await clean(page, api); await page.context().close();
      }
    });

    await test('your games: text from the host is escaped, and a title with an unfit id, name or version, or a repeated id, is left out', async () => {
      const items = [titleRow(1, 'GOOD1', 'Good Game', '1.0'), titleRow(2, 'HOSTILE1', '<img src=x onerror=window.__pwned=1>', '<b>1</b>'),
        titleRow(3, 'CTRL1', 'Bad\u0007name', '1'), titleRow(4, 'LONG1', 'x'.repeat(257), '1'), titleRow(5, 'LONGV1', 'Fine name', 'v'.repeat(129)),
        titleRow(6, 'bad id', 'Spaced id', '1'), titleRow(7, 'GOOD1', 'Second of the same id', '2'), titleRow(8, 'PAD1', ' padded', '1'),
        titleRow(9, 'NOTSTRING1', 'x', '1')];
      items[8].display_name = { value: 5, status: 'observed', source: 'homebrew_dump' };
      const legacy = { schema_version: 1, status: 'observed', complete: true, snapshot_id: 'b'.repeat(32), source_scope: 'console_title_metadata', reason: null, issues: [], items };
      const { page, api } = await yourFixture(undefined, { legacyTitles: legacy });
      assert.deepEqual((await yourIds(page)).sort(), ['GOOD1', 'HOSTILE1']);
      const hostile = await yourCard(page, 'HOSTILE1').innerText();
      assert.match(hostile, /<img src=x onerror=window\.__pwned=1>/); assert.match(hostile, /Version <b>1<\/b>/);
      assert.equal(await page.locator('#your-games img, #your-games b').count(), 0);
      assert.match(await yourCard(page, 'GOOD1').innerText(), /Good Game[\s\S]*Version 1\.0/, 'the first of a repeated id stays');
      await clean(page, api);
    });

    await test('your games: a note says some entries could not be read only when the host reports it', async () => {
      const note = 'Some entries could not be read, so this list may be missing games.';
      let { page, api } = await yourFixture({ items: yourItems(), issues: [{ code: 'provider_observation_incomplete' }] });
      assert.equal(await page.locator('#your-games-incomplete').innerText(), note);
      assert.equal(await page.locator('#your-games-incomplete').isVisible(), true);
      assert.equal(await yourIds(page).then(ids => ids.length), 6, 'the readable titles are all listed');
      await clean(page, api); await page.context().close();
      ({ page, api } = await yourFixture({ items: yourItems() }));
      assert.equal(await page.locator('#your-games-incomplete').isHidden(), true);
      await clean(page, api);
    });

    await test('your games: the controller walks the cards, opens the chooser and a game, and Circle backs out of each', async () => {
      const { page, api } = await yourFixture({ items: yourItems() });
      await page.locator('[data-yg-choose="PPSA44444"]').focus();
      await press(page, RIGHT);
      const moved = await page.evaluate(() => document.activeElement.closest('[data-your-game]')?.dataset.yourGame);
      assert.ok(moved && moved !== 'PPSA44444', `Right reaches the next card (${moved})`);
      await press(page, R2);
      assert.equal(await page.evaluate(() => document.activeElement.closest('[data-your-game]')?.dataset.yourGame), 'PPSA11111', 'R2 goes to the last control of the list');
      await press(page, L2);
      assert.equal(await yourFocus(page), 'PPSA44444|choose', 'L2 goes to the first');
      // The chooser: Cross opens it on the filter; Down leaves the field for the results; Circle closes it onto the control that opened it.
      await page.locator('[data-yg-choose="PPSA33333"]').focus(); await press(page, CROSS);
      assert.equal(await yourFocus(page), 'PPSA33333|filter');
      await press(page, DOWN);
      assert.match(await yourFocus(page), /^PPSA33333\|pick:/);
      await press(page, CIRCLE);
      assert.equal(await page.locator('.your-game-chooser').count(), 0);
      assert.equal(await yourFocus(page), 'PPSA33333|choose');
      // Cross on a pick matches the title.
      await press(page, CROSS); await page.locator('[data-yg-pick="alpha"]').focus(); await press(page, CROSS);
      assert.match(await yourCard(page, 'PPSA33333').locator('.your-game-match').innerText(), /Nexus game: Alpha world/);
      // Cross on Browse mods opens the game; Circle comes back to the list.
      await page.locator('[data-yg-browse="PPSA11111"]').focus(); await press(page, CROSS);
      await page.getByRole('heading', { name: 'Latest mods for Zeta world' }).waitFor();
      await press(page, CIRCLE);
      await yourReady(page);
      assert.equal(await page.locator('#your-games').isVisible(), true);
      assert.equal(await page.locator('#games-view h1').innerText(), 'Choose a game');
      await clean(page, api);
    });

    await test('your games: Refresh games reads the inventory again and shows a title that appeared; Refresh targets stays on a console preview that pages', async () => {
      const { page, api } = await yourFixture({ items: yourItems().slice(0, 2) });
      assert.deepEqual(await yourIds(page), ['PPSA33286', 'PPSA11111']);
      const refresh = page.getByRole('button', { name: 'Refresh games' });
      assert.equal(await refresh.isEnabled(), true);
      api.titles = { items: yourItems() };
      await page.locator('#games-view').getByRole('button', { name: 'Refresh games' }).focus();
      await press(page, CROSS);
      await page.locator('[data-your-game="PPSA55555"]').waitFor();
      assert.equal((await yourIds(page)).length, 6);
      assert.ok(api.requests.includes('POST /api/detected-titles/pages/refresh'), 'the same re-read as Refresh targets');
      assert.equal(await yourFocus(page), 'refresh-games', 'focus is back on the button when the list returns');
      await clean(page, api); await page.context().close();
      // A console preview that advertises paging keeps Refresh targets; one without paging hides it as before.
      for (const [label, extra, hidden] of [['paging', { titles: { items: yourItems() } }, false], ['no paging', {}, true]]) {
        const fixtureOptions = { mode: 'console_preview', ...extra };
        const { page: preview, api: previewApi } = await fixture(extra.titles ? { ...fixtureOptions, games: yourNexusGames(), ready: yourReady } : fixtureOptions);
        await titlesSettled(preview);
        assert.equal(await preview.locator('#reload-titles').isHidden(), hidden, label);
        await clean(preview, previewApi); await preview.context().close();
      }
    });

    await test('your games: the cards and the open chooser keep the type floor and the safe area at 1920x1080 and 1280x720', async () => {
      for (const viewport of [FULL_HD, HD]) {
        const { page, api } = await yourFixture({ items: [...yourItems(), titleRow(7, 'PPSA77777', `Long name ${'X'.repeat(120)}`, '2.00')], issues: [{ code: 'provider_observation_incomplete' }] },
          { viewport, games: [...yourNexusGames(), ...fillerGames(20)] });
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, cards`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, cards`);
        await yourCard(page, 'PPSA33333').getByRole('button', { name: 'Choose the Nexus game' }).click();
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor, chooser`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout, chooser`);
        await clean(page, api); await page.context().close();
      }
    });

    // ---- Runtime profile on the Your games cards (docs/profiles/APP-INTEGRATION.md): the host's status, shown as the host sent it ----
    const profileRecord = (status, extra = {}) => ({ status, profile_id: null, generated_utc: null, confidence: null, unresolved: null, reason: null, ...extra });
    const madeRecord = (status, extra = {}) => profileRecord(status, { profile_id: 'PPSA33286/02.003.000/3b6cfad4', generated_utc: '2026-10-07T12:00:00Z', confidence: { high: 12, medium: 5, low: 3 }, unresolved: 4, ...extra });
    const FAILED_TEXT = {
      no_executable: 'This game has no executable ModBridge can read. Only game dumps can be profiled.',
      executable_encrypted: 'The game’s executable is encrypted. A profile needs the decrypted file.',
      no_version: 'This game has no usable version number to file a profile under.',
      storage_unavailable: 'Private storage for profiles is unavailable.',
      generator_unavailable: 'The profile generator could not be started.',
      generator_failed: 'The generator could not make a profile from this executable.',
      generator_timeout: 'The generator took too long and was stopped.',
      profile_unreadable: 'The saved profile could not be read.',
      profile_invalid: 'The saved profile is not a valid profile.',
      profile_mismatch: 'The saved profile belongs to another game or version.',
      status_inconsistent: 'The saved profile claims a status it cannot back up.',
      verified_profile_kept: 'This profile carries a console verification, so it was not replaced. Replacing it is a deliberate step outside this page.',
    };
    // A fake of the host's two profile routes. `statuses` maps "<id>|<version>" to a record or a function returning one (called at each GET);
    // `onGenerate(id, post)` may return { status, body }; the default is 202 and a generating record. Everything asked is logged in `seen`.
    const profileHost = (statuses, { onGenerate, capabilities = { runtime_profiles: true, profile_generate: true }, delay = 0 } = {}) => {
      const seen = { gets: [], posts: [] };
      return {
        seen, statuses, capabilities,
        get: async url => {
          const m = /^\/api\/titles\/([^/]+)\/profile$/.exec(url.pathname);
          if (!m) return undefined;
          seen.gets.push(url.pathname + url.search);
          if (delay) await new Promise(resolve => setTimeout(resolve, delay));
          const key = `${m[1]}|${url.searchParams.get('content_version')}`, value = statuses[key];
          const body = typeof value === 'function' ? value() : value ?? profileRecord('none');
          return body && body.__http ? { status: body.__http, body: body.body } : { body };
        },
        reply: post => {
          const m = /^\/api\/titles\/([^/]+)\/profile\/generate$/.exec(post.path);
          if (!m) return undefined;
          seen.posts.push({ id: m[1], body: post.body, csrf: post.headers['x-csrf-token'] });
          return onGenerate ? onGenerate(m[1], post) : { status: 202, body: profileRecord('generating') };
        },
      };
    };
    const profileGames = () => [
      titleRow(1, 'PPSA33286', 'SILENT HILL: Townfall', '02.003.000'),
      titleRow(2, 'PPSA00002', 'Second dump', '1.00'),
      titleRow(3, 'PPSA00003', 'Third dump', '1.00'),
    ];
    // More titles than one inventory page holds (8) are sent the way a host without paging sends them: one v1 record.
    const legacyInventory = items => ({ schema_version: 1, status: 'observed', complete: true, snapshot_id: 'b'.repeat(32), source_scope: 'console_title_metadata', reason: null, issues: [], items });
    const profileFixture = (host, items = profileGames(), extra = {}) => (items.length > 8
      ? yourFixture(undefined, { legacyTitles: legacyInventory(items), capabilities: host.capabilities, get: host.get, reply: host.reply, ...extra })
      : yourFixture({ items }, { capabilities: host.capabilities, get: host.get, reply: host.reply, ...extra }));
    const profileLine = (page, id) => yourCard(page, id).locator('.your-game-profile');
    const profileButton = (page, id) => yourCard(page, id).locator(`[data-yg-generate="${id}"]`);
    const profileLineText = async (page, id) => (await profileLine(page, id).innerText()).replace(/\s+/g, ' ');
    const profileSettled = page => page.waitForFunction(() => document.querySelectorAll('[data-yg-profile]').length > 0 && ![...document.querySelectorAll('[data-yg-profile]')].some(el => el.dataset.profileStatus === 'loading'));

    await test('runtime profile: each status is shown as the host sent it, with the wording of the catalog, and verified only when the host says so', async () => {
      const items = [1, 2, 3, 4, 5, 6, 7].map(n => titleRow(n, `PPSA0000${n}`, `Game ${n}`, '1.0'));
      const host = profileHost({
        'PPSA00001|1.0': profileRecord('none'),
        'PPSA00002|1.0': madeRecord('generated'),
        'PPSA00003|1.0': madeRecord('console-verified'),
        'PPSA00004|1.0': madeRecord('stale', { reason: 'digest_mismatch' }),
        'PPSA00005|1.0': madeRecord('stale', { reason: 'executable_unavailable' }),
        'PPSA00006|1.0': profileRecord('generating'),
        'PPSA00007|1.0': profileRecord('failed', { reason: 'executable_encrypted' }),
      });
      const { page, api } = await profileFixture(host, items);
      await profileSettled(page);
      const facts = '2026-10-07 · 12 high, 5 medium, 3 low confidence · 4 unresolved';
      assert.equal(await profileLineText(page, 'PPSA00001'), 'Runtime profile: None yet.');
      assert.equal(await profileLineText(page, 'PPSA00002'), `Runtime profile: Generated ${facts}. Made by reading the game’s executable; not tested on the console.`);
      assert.equal(await profileLineText(page, 'PPSA00003'), `Runtime profile: Console-verified for this build · ${facts}.`);
      assert.equal(await profileLineText(page, 'PPSA00004'), `Runtime profile: Stale: Verified for a different build; regenerate. ${facts}.`);
      assert.equal(await profileLineText(page, 'PPSA00005'), `Runtime profile: Stale: Verified earlier, but this game’s executable cannot be read here to check it. ${facts}.`);
      assert.equal(await profileLineText(page, 'PPSA00006'), 'Runtime profile: Generating… this can take several minutes.');
      assert.equal(await profileLineText(page, 'PPSA00007'), `Runtime profile: Failed: ${FAILED_TEXT.executable_encrypted}`);
      const kinds = await page.$$eval('[data-yg-profile]', els => Object.fromEntries(els.map(el => [el.dataset.ygProfile, [el.dataset.profileStatus, el.getAttribute('aria-busy')]])));
      assert.deepEqual(kinds, { PPSA00001: ['none', 'false'], PPSA00002: ['generated', 'false'], PPSA00003: ['console-verified', 'false'], PPSA00004: ['stale', 'false'],
        PPSA00005: ['stale', 'false'], PPSA00006: ['generating', 'true'], PPSA00007: ['failed', 'false'] });
      // "Console-verified" is on exactly the one card whose record the host sent that way; a generated profile never reads as tested.
      const all = await page.locator('#your-games').innerText();
      assert.equal((all.match(/Console-verified/g) || []).length, 1);
      assert.doesNotMatch(await profileLine(page, 'PPSA00002').innerText(), /verified|tested on PS5|compatible/i);
      assert.doesNotMatch(all, /\b(supports?|supported|moddable|works)\b/i);
      // One request per game, with the version the inventory showed.
      assert.deepEqual(host.seen.gets.sort(), items.map((_, n) => `/api/titles/PPSA0000${n + 1}/profile?content_version=1.0`));
      assert.deepEqual(host.seen.posts, []);
      await clean(page, api);
    });

    await test('runtime profile: every reason code of a failure reads its catalog sentence, and nothing the host wrote', async () => {
      const reasons = Object.keys(FAILED_TEXT);
      const items = reasons.map((_, n) => titleRow(n + 1, `PPSA1${String(n).padStart(4, '0')}`, `Fail ${n}`, '1.0'));
      const statuses = Object.fromEntries(reasons.map((reason, n) => [`PPSA1${String(n).padStart(4, '0')}|1.0`, profileRecord('failed', { reason })]));
      const { page, api } = await profileFixture(profileHost(statuses), items);
      await profileSettled(page);
      for (const [n, reason] of reasons.entries()) assert.equal(await profileLineText(page, `PPSA1${String(n).padStart(4, '0')}`), `Runtime profile: Failed: ${FAILED_TEXT[reason]}`, reason);
      await clean(page, api);
    });

    await test('runtime profile: a reply the page cannot verify is never shown as a status (fail closed)', async () => {
      const bad = {
        'unknown status': profileRecord('verified'),
        'an extra key': { ...profileRecord('none'), note: 'x' },
        'a missing key': (({ reason, ...rest }) => rest)(profileRecord('none')),
        'verified with no facts': profileRecord('console-verified'),
        'verified with a reason': madeRecord('console-verified', { reason: 'digest_mismatch' }),
        'generated with a reason': madeRecord('generated', { reason: 'profile_invalid' }),
        'none with facts': madeRecord('none'),
        'generating with a reason': profileRecord('generating', { reason: 'generator_failed' }),
        'failed with no reason': profileRecord('failed'),
        'failed with a reason outside the catalog': profileRecord('failed', { reason: 'disk_on_fire <b>x</b>' }),
        'failed with a stale reason': profileRecord('failed', { reason: 'digest_mismatch' }),
        'stale with no reason': madeRecord('stale'),
        'stale with a failure reason': madeRecord('stale', { reason: 'generator_failed' }),
        'a hostile profile id': madeRecord('generated', { profile_id: '<img src=x onerror=window.__pwned=1>' }),
        'a date that is not one': madeRecord('generated', { generated_utc: 'yesterday' }),
        'counts that are not counts': madeRecord('generated', { confidence: { high: -1, medium: 1.5, low: '3' } }),
        'counts with an extra level': madeRecord('generated', { confidence: { high: 1, medium: 1, low: 1, certain: 9 } }),
        'unresolved as text': madeRecord('generated', { unresolved: '4' }),
        'a non-object': ['none'],
        'an http error': { __http: 500, body: { error: { code: 'operation_failed', message: 'host text <b>no</b>' } } },
      };
      const names = Object.keys(bad);
      const items = names.map((_, n) => titleRow(n + 1, `PPSA2${String(n).padStart(4, '0')}`, `Bad ${n}`, '1.0'));
      const statuses = Object.fromEntries(names.map((name, n) => [`PPSA2${String(n).padStart(4, '0')}|1.0`, bad[name]]));
      const { page, api } = await profileFixture(profileHost(statuses), items);
      await profileSettled(page);
      for (const [n, name] of names.entries()) {
        const id = `PPSA2${String(n).padStart(4, '0')}`;
        assert.equal(await profileLineText(page, id), 'Runtime profile: Could not be checked. Refresh games to try again.', name);
        assert.equal(await profileLine(page, id).getAttribute('data-profile-status'), 'error', name);
      }
      const text = await page.locator('#your-games').innerText();
      assert.doesNotMatch(text, /Console-verified|Generated|Stale|disk_on_fire|host text|certain/);
      assert.equal(await page.locator('#your-games img, #your-games b').count(), 0);
      await clean(page, api);
    });

    await test('runtime profile: without exactly runtime_profiles true the card is as it was (no line, no button, no request)', async () => {
      for (const capabilities of [undefined, { profile_generate: true }, { runtime_profiles: 'yes', profile_generate: true }, { runtime_profiles: false }, { runtime_profiles: 1 }]) {
        const host = profileHost({ 'PPSA33286|02.003.000': madeRecord('console-verified') }, { capabilities: capabilities || {} });
        const { page, api } = await profileFixture(host);
        assert.equal(await page.locator('.your-game-profile, [data-yg-generate]').count(), 0, JSON.stringify(capabilities));
        assert.deepEqual(await yourButtons(page, 'PPSA33286'), ['Browse mods']);
        assert.deepEqual(host.seen.gets, []);
        assert.doesNotMatch(await page.locator('#your-games').innerText(), /profile/i);
        await clean(page, api); await page.context().close();
      }
    });

    await test('runtime profile: a host that reads profiles but cannot make them shows the status and a button that is off, with the reason', async () => {
      for (const capabilities of [{ runtime_profiles: true }, { runtime_profiles: true, profile_generate: false }, { runtime_profiles: true, profile_generate: 'yes' }]) {
        const host = profileHost({ 'PPSA33286|02.003.000': madeRecord('generated') }, { capabilities });
        const { page, api } = await profileFixture(host);
        await profileSettled(page);
        assert.match(await profileLineText(page, 'PPSA33286'), /^Runtime profile: Generated 2026-10-07/);
        const button = profileButton(page, 'PPSA33286');
        assert.equal(await button.isDisabled(), true);
        assert.equal(await button.innerText(), 'Regenerate');
        assert.equal(await yourCard(page, 'PPSA33286').locator('[data-yg-profile-reason]').innerText(), 'This host cannot generate profiles.');
        await button.click({ force: true }).catch(() => {});
        assert.deepEqual(host.seen.posts, []);
        await clean(page, api); await page.context().close();
      }
    });

    await test('runtime profile: the button reads Generate profile or Regenerate, and is off with its reason for a game that is not a dump or has no usable version', async () => {
      const items = [
        titleRow(1, 'PPSA00001', 'No profile yet', '1.0'), titleRow(2, 'PPSA00002', 'Has profile', '1.0'), titleRow(3, 'PPSA00003', 'Verified', '1.0'),
        titleRow(4, 'PPSA00004', 'Stale one', '1.0'), titleRow(5, 'PPSA00005', 'Failed one', '1.0'),
        titleRow(6, 'PPSA00006', 'Installed only', '1.0', 'user_metadata_cache'), titleRow(7, 'PPSA00007', 'No version', null),
        titleRow(8, 'PPSA00008', 'Odd version', '1 0/x'),
      ];
      const host = profileHost({
        'PPSA00002|1.0': madeRecord('generated'), 'PPSA00003|1.0': madeRecord('console-verified'), 'PPSA00004|1.0': madeRecord('stale', { reason: 'executable_mismatch' }),
        'PPSA00005|1.0': profileRecord('failed', { reason: 'generator_failed' }),
      });
      const { page, api } = await profileFixture(host, items);
      await profileSettled(page);
      const label = async id => (await profileButton(page, id).innerText()).trim();
      assert.deepEqual([await label('PPSA00001'), await label('PPSA00002'), await label('PPSA00003'), await label('PPSA00004'), await label('PPSA00005')],
        ['Generate profile', 'Regenerate', 'Regenerate', 'Regenerate', 'Generate profile']);
      for (const id of ['PPSA00001', 'PPSA00002', 'PPSA00003', 'PPSA00004', 'PPSA00005']) {
        assert.equal(await profileButton(page, id).isEnabled(), true, id);
        assert.equal(await yourCard(page, id).locator('[data-yg-profile-reason]').count(), 0, id);
      }
      // Not a dump: off for good, with the reason, and the status still shown.
      assert.equal(await profileButton(page, 'PPSA00006').isDisabled(), true);
      assert.equal(await yourCard(page, 'PPSA00006').locator('[data-yg-profile-reason]').innerText(), 'Only a game dump can be profiled.');
      assert.equal(await profileLineText(page, 'PPSA00006'), 'Runtime profile: None yet.');
      // No version or one the host could not file a profile under: off, the reason, and no request for its status.
      for (const id of ['PPSA00007', 'PPSA00008']) {
        assert.equal(await profileButton(page, id).isDisabled(), true, id);
        assert.equal(await yourCard(page, id).locator('[data-yg-profile-reason]').innerText(), FAILED_TEXT.no_version, id);
        assert.equal(await profileLineText(page, id), 'Runtime profile: Not available without a usable version number.', id);
      }
      assert.ok(!host.seen.gets.some(path => /PPSA0000[78]/.test(path)), 'no request for a game with no usable version');
      assert.ok(host.seen.gets.some(path => path === '/api/titles/PPSA00006/profile?content_version=1.0'), 'an installed title still shows its status');
      assert.equal(await yourCard(page, 'PPSA00001').locator('[data-yg-generate]').getAttribute('aria-label'), 'Generate runtime profile for No profile yet');
      assert.equal(await yourCard(page, 'PPSA00002').locator('[data-yg-generate]').getAttribute('aria-label'), 'Regenerate runtime profile for Has profile');
      await page.locator('[data-yg-generate="PPSA00006"]').click({ force: true, timeout: 500 }).catch(() => {});
      assert.deepEqual(host.seen.posts, [], 'a button that is off sends nothing');
      await clean(page, api);
    });

    await test('runtime profile: while a status is being fetched the line says so (aria-busy) and the button waits', async () => {
      const host = profileHost({ 'PPSA33286|02.003.000': madeRecord('generated') }, { delay: 700 });
      const { page, api } = await profileFixture(host, profileGames().slice(0, 1));
      assert.equal(await profileLineText(page, 'PPSA33286'), 'Runtime profile: Checking…');
      assert.equal(await profileLine(page, 'PPSA33286').getAttribute('aria-busy'), 'true');
      assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), 'true');
      await profileButton(page, 'PPSA33286').click({ force: true });
      assert.deepEqual(host.seen.posts, [], 'a press while the status is unknown sends nothing');
      await page.waitForFunction(() => document.querySelector('[data-yg-profile]')?.dataset.profileStatus === 'generated');
      assert.equal(await profileLine(page, 'PPSA33286').getAttribute('aria-busy'), 'false');
      assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), null);
      await clean(page, api);
    });

    await test('runtime profile: Generate profile sends one empty POST, shows Generating, polls, and settles on what the host says', async () => {
      let polls = 0;
      const statuses = { 'PPSA33286|02.003.000': profileRecord('none') };
      const host = profileHost(statuses);
      const { page, api } = await profileFixture(host);
      await profileSettled(page);
      assert.equal(await profileLineText(page, 'PPSA33286'), 'Runtime profile: None yet.');
      statuses['PPSA33286|02.003.000'] = () => (polls++ < 2 ? profileRecord('generating') : madeRecord('generated'));
      await profileButton(page, 'PPSA33286').click();
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'generating');
      assert.deepEqual(host.seen.posts, [{ id: 'PPSA33286', body: {}, csrf: 'browse-csrf' }]);
      assert.equal(await profileLine(page, 'PPSA33286').getAttribute('aria-busy'), 'true');
      assert.equal(await profileButton(page, 'PPSA33286').innerText(), 'Generating…');
      assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), 'true');
      assert.equal(await yourFocus(page), 'PPSA33286|generate', 'focus stays on the button through the redraws');
      assert.match(await page.locator('#announcer').innerText(), /^Generating a runtime profile for SILENT HILL: Townfall\.$/);
      // The other games cannot start one at the same time: the host takes one at a time.
      assert.equal(await profileButton(page, 'PPSA00002').getAttribute('aria-disabled'), 'true');
      assert.equal(await yourCard(page, 'PPSA00002').locator('[data-yg-profile-reason]').innerText(), 'Another profile is being generated.');
      await profileButton(page, 'PPSA00002').click({ force: true }); await profileButton(page, 'PPSA33286').click({ force: true });
      assert.equal(host.seen.posts.length, 1, 'a press on a button that is waiting sends nothing');
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'generated', null, { timeout: 12000 });
      assert.match(await profileLineText(page, 'PPSA33286'), /^Runtime profile: Generated 2026-10-07 · 12 high, 5 medium, 3 low confidence · 4 unresolved\./);
      assert.equal(await profileButton(page, 'PPSA33286').innerText(), 'Regenerate');
      assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), null);
      assert.equal(await yourCard(page, 'PPSA00002').locator('[data-yg-profile-reason]').count(), 0);
      assert.equal(await yourFocus(page), 'PPSA33286|generate');
      assert.equal(await page.locator('#announcer').innerText(), 'SILENT HILL: Townfall: runtime profile generated.');
      assert.equal(host.seen.posts.length, 1);
      const polled = host.seen.gets.filter(path => path === '/api/titles/PPSA33286/profile?content_version=02.003.000');
      assert.ok(polled.length >= 4, `polled while generating (${polled.length} reads)`);
      await clean(page, api);
    });

    await test('runtime profile: a start the host refuses, a failure it reports and a reply the page cannot verify each read their catalog text, and Generate can be pressed again', async () => {
      const cases = [
        ['busy', () => ({ status: 409, body: { error: { code: 'busy', message: 'host text <b>x</b>' } } }), 'Runtime profile: Could not start: ModBridge is busy with another operation. Wait a moment and try again.'],
        ['not listed', () => ({ status: 404, body: { error: { code: 'not_found', message: 'x' } } }), 'Runtime profile: Could not start: That item no longer exists. Refresh the list.'],
        ['no code', () => ({ status: 500, body: {} }), 'Runtime profile: Could not start: Request failed (500)'],
        ['a failed record', () => ({ status: 200, body: profileRecord('failed', { reason: 'executable_encrypted' }) }), `Runtime profile: Failed: ${FAILED_TEXT.executable_encrypted}`],
        ['an unverifiable record', () => ({ status: 200, body: { status: 'console-verified' } }), 'Runtime profile: Could not start: The host sent a reply this page could not verify.'],
      ];
      for (const [name, reply, text] of cases) {
        const host = profileHost({}, { onGenerate: reply });
        const { page, api } = await profileFixture(host, profileGames().slice(0, 1));
        await profileSettled(page);
        await profileButton(page, 'PPSA33286').click();
        await page.waitForFunction(expected => document.querySelector('[data-yg-profile="PPSA33286"]')?.textContent.replace(/\s+/g, ' ') === expected, text);
        assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), null, name);
        assert.equal(await profileButton(page, 'PPSA33286').innerText(), 'Generate profile', name);
        assert.equal(await profileLine(page, 'PPSA33286').getAttribute('aria-busy'), 'false', name);
        assert.equal(await page.locator('#your-games b').count(), 0, name);
        assert.equal(await yourFocus(page), 'PPSA33286|generate', name);
        await profileButton(page, 'PPSA33286').click();
        for (let n = 0; n < 40 && host.seen.posts.length < 2; n++) await page.waitForTimeout(50);
        assert.equal(host.seen.posts.length, 2, `${name}: pressed again`);
        await clean(page, api); await page.context().close();
      }
    });

    await test('runtime profile: a generation already running on the host is picked up on load and followed to its end without a POST', async () => {
      let polls = 0;
      const host = profileHost({ 'PPSA33286|02.003.000': () => (polls++ < 1 ? profileRecord('generating') : profileRecord('failed', { reason: 'generator_timeout' })) });
      const { page, api } = await profileFixture(host);
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'generating');
      assert.equal(await profileButton(page, 'PPSA33286').getAttribute('aria-disabled'), 'true');
      assert.equal(await profileButton(page, 'PPSA00002').getAttribute('aria-disabled'), 'true', 'the others wait');
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'failed', null, { timeout: 8000 });
      assert.equal(await profileLineText(page, 'PPSA33286'), `Runtime profile: Failed: ${FAILED_TEXT.generator_timeout}`);
      assert.equal(await profileButton(page, 'PPSA00002').getAttribute('aria-disabled'), null);
      assert.deepEqual(host.seen.posts, []);
      await clean(page, api);
    });

    await test('runtime profile: Refresh games reads every status again, and only what the host says now is shown', async () => {
      const host = profileHost({ 'PPSA33286|02.003.000': madeRecord('console-verified') });
      const { page, api } = await profileFixture(host);
      await profileSettled(page);
      assert.match(await profileLineText(page, 'PPSA33286'), /^Runtime profile: Console-verified/);
      host.statuses['PPSA33286|02.003.000'] = madeRecord('stale', { reason: 'executable_mismatch' });
      await page.locator('#games-view').getByRole('button', { name: 'Refresh games' }).click();
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'stale');
      assert.doesNotMatch(await page.locator('#your-games').innerText(), /Console-verified/);
      assert.equal(host.seen.gets.filter(path => path.includes('PPSA33286')).length, 2);
      await clean(page, api);
    });

    await test('runtime profile: the controller reaches Generate profile with the four directions and Cross presses it', async () => {
      const host = profileHost({ 'PPSA33286|02.003.000': profileRecord('none') });
      const { page, api } = await profileFixture(host);
      await profileSettled(page);
      const result = await unreachable(page, 'games');
      assert.ok(result.total >= 8, `${result.total} stops`); assert.deepEqual(result.missing, [], 'every stop is reachable'); assert.deepEqual(result.noRing, []);
      await page.locator('[data-yg-browse="PPSA33286"]').focus();
      // The two buttons wrap onto two rows in a 4-column card at 1920 wide, so Down reaches the second and Up comes back.
      await press(page, DOWN);
      assert.equal(await yourFocus(page), 'PPSA33286|generate', 'Down from Browse mods reaches Generate profile');
      await press(page, UP);
      assert.equal(await yourFocus(page), 'PPSA33286|browse');
      await page.locator('[data-yg-generate="PPSA33286"]').focus(); await press(page, CROSS);
      await page.waitForFunction(() => document.querySelector('[data-yg-profile="PPSA33286"]')?.dataset.profileStatus === 'generating');
      assert.equal(host.seen.posts.length, 1);
      assert.equal(await yourFocus(page), 'PPSA33286|generate');
      await clean(page, api);
    });

    await test('runtime profile: the lines and buttons keep the type floor, the safe area and no overlap at 1920x1080 and 1280x720', async () => {
      const items = [1, 2, 3, 4, 5].map(n => titleRow(n, `PPSA0000${n}`, n === 1 ? `Long name ${'X'.repeat(100)}` : `Game ${n}`, '1.0'));
      for (const viewport of [FULL_HD, HD]) {
        const host = profileHost({
          'PPSA00001|1.0': madeRecord('stale', { reason: 'executable_unavailable' }), 'PPSA00002|1.0': madeRecord('generated'),
          'PPSA00003|1.0': profileRecord('failed', { reason: 'executable_encrypted' }), 'PPSA00004|1.0': profileRecord('generating'),
          'PPSA00005|1.0': madeRecord('console-verified', { confidence: { high: 123456, medium: 234567, low: 345678 }, unresolved: 456789 }),
        });
        const { page, api } = await profileFixture(host, items, { viewport });
        await profileSettled(page);
        assert.deepEqual(await smallText(page), [], `${viewport.width}: type floor`); assert.deepEqual(await layoutProblems(page), [], `${viewport.width}: layout`);
        await clean(page, api); await page.context().close();
      }
    });

  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
  console.log(`browse UI: ${passed} passed, ${failed} failed`);
  if (failed) process.exitCode = 1;
}

main().catch(error => { console.error(error); process.exitCode = 1; });
