const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

const webRoot = path.join(__dirname, '..', 'web');
const fixtures = {
  capabilities: { mode: 'desktop_preparation', csrf_token: 'fixture-csrf-12345678', capabilities: { nexus_catalog: true, account_status: true, nexus_auth_start: true, detected_titles: false, jobs: true, installed_records: true, download_analyze: true, prepared_translation: false, deploy: false } },
  account: { status: 'connected', live_service: 'verified', account: { display_name: 'Fixture Modder', is_premium: false }, console_translation: 'unverified' },
  games: { items: [{ domain_name: 'stellarblade', id: 1, name: 'Stellar Blade' }, { domain_name: 'eldenring', id: 2, name: 'Elden Ring' }] },
  mods: { items: [{ mod_id: 42, name: 'Readable UI', author: 'Fixture Author', summary: 'Owned test record.', version: '1.2', source_url: 'https://www.nexusmods.com/stellarblade/mods/42' }] },
  files: { items: [{ file_id: 7, name: 'Main file', file_name: 'readable-ui.zip', version: '1.2', size_kb: 12, description: 'Fixture archive' }] },
};

async function controllerPress(page, index) {
  await page.evaluate(index => {
    if (!window.fixturePad) {
      window.fixturePad = { mapping: 'standard', buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })), axes: [0, 0] };
      Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.fixturePad] });
    }
    window.fixturePad.buttons[index].pressed = true; window.fixturePad.buttons[index].value = 1;
  }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  await page.evaluate(index => { if (window.fixturePad) { window.fixturePad.buttons[index].pressed = false; window.fixturePad.buttons[index].value = 0; } }, index);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function controllerReach(page, selector) {
  const graph = new Map(), get = key => { if (!graph.has(key)) graph.set(key, {}); return graph.get(key); };
  const observe = () => page.evaluate(selector => {
    const all = [...document.querySelectorAll('button,input,a[href],[tabindex]')], active = document.activeElement;
    const target = [...document.querySelectorAll(selector)].find(el => el.getClientRects().length && !el.disabled && !el.closest('[hidden]'));
    const point = el => { const r = el?.getBoundingClientRect(); return r ? { x: r.x + r.width / 2, y: r.y + r.height / 2 } : { x: 0, y: 0 }; };
    return { exists: !!target, matched: active === target, key: active.id || String(all.indexOf(active)), active: point(active), target: point(target) };
  }, selector);
  for (let move = 0; move < 320; move++) {
    const before = await observe(); assert(before.exists, `Visible controller target missing: ${selector}`); if (before.matched) return;
    const dx = before.target.x - before.active.x, dy = before.target.y - before.active.y;
    const order = Math.abs(dy) > Math.abs(dx) ? [dy >= 0 ? 13 : 12, dx >= 0 ? 15 : 14] : [dx >= 0 ? 15 : 14, dy >= 0 ? 13 : 12];
    let edge = [...new Set([...order, 13, 15, 12, 14])].find(value => !Object.hasOwn(get(before.key), value));
    if (edge === undefined) {
      const queue = [{ key: before.key, first: undefined }], seen = new Set([before.key]);
      while (queue.length && edge === undefined) {
        const node = queue.shift(); if (Object.keys(get(node.key)).length < 4) { edge = node.first; break; }
        for (const [direction, next] of Object.entries(get(node.key))) if (!seen.has(next)) { seen.add(next); queue.push({ key: next, first: node.first ?? Number(direction) }); }
      }
    }
    assert.notEqual(edge, undefined, `Controller cannot reach ${selector}`);
    await controllerPress(page, edge); get(before.key)[edge] = (await observe()).key;
  }
  assert.fail(`Controller reach exceeded its bound: ${selector}`);
}

async function main() {
  const server = http.createServer((req, res) => {
    const file = path.join(webRoot, req.url === '/' ? 'index.html' : req.url.slice(1));
    if (!file.startsWith(webRoot) || !fs.existsSync(file)) { res.writeHead(404).end('missing UI asset'); return; }
    const type = file.endsWith('.html') ? 'text/html' : file.endsWith('.css') ? 'text/css' : 'text/javascript';
    res.writeHead(200, { 'Content-Type': type }); res.end(fs.readFileSync(file));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  let analysisAvailable = true;
  let unverifiedAccount = false;
  let unsafeIds = false;
  let fileRequests = 0;
  let delayStellarMods = false;
  let authOnly = false;
  let verifiedEmpty = false;
  let authStarts = 0;
  let disconnectStarts = 0;
  let needsNexusAuthorization = false;
  let changedReceipt = false;
  let invalidAuthorizationDomain = false;
  let sourceNavigation = '';
  let authNavigation = '';
  const outboundUrls = [];
  const launchOptions = { headless: true, args: ['--no-sandbox'] };
  if (process.env.CHROMIUM_EXECUTABLE_PATH) launchOptions.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launchOptions.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(launchOptions);
  try {
    const context = await browser.newContext({ viewport: { width: 1920, height: 1080 } });
    const browserErrors = [];
    context.on('page', page => page.on('pageerror', error => browserErrors.push(error.message)));
    const page = await context.newPage();
    page.setDefaultTimeout(3000);
    let submitted = null;
    await context.route('**/*', async route => { const url = new URL(route.request().url()); if (url.origin !== origin) { outboundUrls.push(url.href); return route.abort(); } return route.continue(); });
    await context.route('https://users.nexusmods.com/**', async route => { authNavigation = route.request().url(); outboundUrls.push(authNavigation); return route.fulfill({ contentType: 'text/html', body: '<title>Fixture official authorization</title>' }); });
    await context.route('https://www.nexusmods.com/**', async route => { sourceNavigation = route.request().url(); outboundUrls.push(sourceNavigation); return route.fulfill({ contentType: 'text/html', body: '<title>Fixture Nexus page</title>' }); });
    await context.route('**/api/**', async route => {
      const req = route.request();
      const url = new URL(req.url());
      if (url.hostname === 'users.nexusmods.com' && url.pathname === '/oauth/authorize') { authNavigation = url.href; return route.fulfill({ contentType: 'text/html', body: '<title>Fixture official authorization</title>' }); }
      const json = (body, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(body) });
      if (url.pathname === '/api/capabilities') return json({ ...fixtures.capabilities, capabilities: { ...fixtures.capabilities.capabilities, download_analyze: analysisAvailable } });
      if (url.pathname === '/api/account') return json(authOnly ? { status: 'disconnected' } : verifiedEmpty ? fixtures.account : unverifiedAccount ? { status: 'connected', live_service: 'unverified', account: { display_name: 'Do not show this', token: 'fixture-secret' } } : fixtures.account);
      if (url.pathname === '/api/games') return json(authOnly || verifiedEmpty ? { items: [] } : fixtures.games);
      if (url.pathname === '/api/games/stellarblade/mods') { if (delayStellarMods) await new Promise(resolve => setTimeout(resolve, 350)); return json(unsafeIds ? { items: [{ ...fixtures.mods.items[0], mod_id: 9223372036854775807 }] } : fixtures.mods); }
      if (url.pathname === '/api/games/eldenring/mods') return json({ items: [{ mod_id: 99, name: 'Elden Fixture Mod', author: 'Fixture Author', summary: 'Second game record.', version: '1.0' }] });
      if (url.pathname === '/api/games/stellarblade/mods/42/files' || url.pathname === '/api/games/stellarblade/mods/9223372036854775807/files') { fileRequests++; return json(unsafeIds ? { items: [{ ...fixtures.files.items[0], file_id: 9223372036854775807 }] } : fixtures.files); }
      if (url.pathname === '/api/detected-titles') return json({ items: [], status: 'unavailable' });
      if (url.pathname === '/api/installed') return json({ items: [] });
      if (url.pathname === '/api/jobs' && req.method() === 'GET') return json({ items: needsNexusAuthorization ? [{ id: 'b'.repeat(32), kind: 'download_analyze', game: invalidAuthorizationDomain ? 'evil.com/redirect' : 'stellarblade', mod_id: 42, file_id: 7, bytes_downloaded: changedReceipt ? 22 : 0, label: 'Readable UI · Main file', status: 'needs_nexus_authorization', source_url: 'https://evil.example/redirect' }] : [] });
      if (url.pathname === '/api/jobs' && req.method() === 'POST') {
        submitted = { body: req.postDataJSON(), headers: req.headers() };
        return json({ id: 'a'.repeat(32), kind: 'download_analyze', game: 'stellarblade', mod_id: 42, file_id: 7, status: 'queued', stage: 'requested', compatibility: 'unresolved' }, 202);
      }
      if (url.pathname === '/api/auth/start' && req.method() === 'POST') { authStarts++; return json({ authorization_url: 'https://users.nexusmods.com/oauth/authorize?fixture=1' }); }
      if (url.pathname === '/api/auth/disconnect' && req.method() === 'POST') { disconnectStarts++; return json({ status: 'disconnected' }); }
      return json({ error: { code: 'not_found', message: 'Unavailable' } }, 404);
    });

    await page.goto(origin);
    await page.getByRole('heading', { name: 'Choose a game' }).waitFor();
    if (process.env.UI_SCREENSHOT_PATH) await page.screenshot({ path: process.env.UI_SCREENSHOT_PATH, fullPage: true });
    await page.locator('[data-card="game:stellarblade"]').click();
    await page.getByRole('heading', { name: 'Latest mods from Nexus' }).waitFor();
    await page.getByRole('button', { name: /Readable UI/ }).click();
    await page.getByText('Available files').waitFor();
    assert.match(await page.locator('body').innerText(), /These downloads are checked only/);
    const detailsButton = page.getByRole('button', { name: 'More info about Main file' });
    await detailsButton.focus();
    await page.evaluate(() => { const buttons = Array.from({ length: 17 }, () => ({ pressed: false })); buttons[9].pressed = true; const pad = { mapping: 'standard', buttons, axes: [0, 0] }; Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [pad] }); window.fixtureGamepad = pad; });
    await page.waitForFunction(() => document.querySelector('#details-dialog').open);
    await page.evaluate(() => { window.fixtureGamepad.buttons[9].pressed = false; });
    assert.equal(await page.locator('#details-dialog').evaluate(dialog => dialog.open), true);
    await page.keyboard.press('Escape');
    assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'More info about Main file');
    assert.deepEqual(await page.evaluate(() => JSON.parse(sessionStorage.getItem('modbridge.navigation.v1'))), { domain: 'stellarblade', mod: 42, file: '' });
    await page.getByRole('button', { name: 'Download and check' }).click();

    await page.getByText(/Download requested/).waitFor();
    assert.equal(submitted.body.kind, 'download_analyze');
    assert.deepEqual([submitted.body.game, submitted.body.mod_id, submitted.body.file_id], ['stellarblade', 42, 7]);
    assert.match(submitted.body.idempotency_key, /^[a-zA-Z0-9_-]{8,80}$/);
    assert.equal(submitted.headers['x-csrf-token'], fixtures.capabilities.csrf_token);

    await page.getByRole('tab', { name: 'Installed' }).click();
    assert.match(await page.locator('body').innerText(), /No confirmed installations/);
    assert.match(await page.locator('body').innerText(), /Mods have not been tested in a game yet/);
    assert.equal(await page.getByRole('button', { name: /Install/ }).count(), 0);

    await page.getByRole('tab', { name: 'Games' }).click();
    await page.keyboard.press('Escape');
    await page.keyboard.press('Escape');
    // Catalog filtering adds focusable controls. Reach the actual game card
    // through keyboard traversal without assuming a fixed number of controls.
    await page.getByRole('tab', { name: 'Games' }).focus();
    for (let step = 0; step < 20; ++step) {
      if (await page.evaluate(() => document.activeElement?.dataset.card === 'game:stellarblade')) break;
      await page.keyboard.press('Tab');
    }
    assert.equal(await page.evaluate(() => document.activeElement?.dataset.card), 'game:stellarblade');
    await page.keyboard.press('Enter');
    await page.getByRole('heading', { name: 'Latest mods from Nexus' }).waitFor();

    analysisAvailable = false; unverifiedAccount = true;
    const secondPage = await context.newPage(); secondPage.setDefaultTimeout(3000);
    await secondPage.goto(origin); await secondPage.getByRole('heading', { name: 'Choose a game' }).waitFor();
    assert.doesNotMatch(await secondPage.locator('#account').innerText(), /Do not show this|fixture-secret/);
    await secondPage.locator('[data-card="game:stellarblade"]').click();
    await secondPage.getByRole('heading', { name: 'Latest mods from Nexus' }).waitFor();
    await secondPage.getByRole('button', { name: /Readable UI/ }).click();
    await secondPage.getByText('Available files').waitFor();
    const unavailable = secondPage.getByRole('button', { name: 'Downloads not available yet' });
    assert.equal(await unavailable.isDisabled(), true);

    const fileRequestsBeforeUnsafe = fileRequests;
    analysisAvailable = true; unverifiedAccount = false; unsafeIds = true;
    const thirdPage = await context.newPage(); thirdPage.setDefaultTimeout(3000);
    await thirdPage.goto(origin); await thirdPage.getByRole('heading', { name: 'Choose a game' }).waitFor();
    await thirdPage.locator('[data-card="game:stellarblade"]').click();
    await thirdPage.getByRole('heading', { name: 'Latest mods from Nexus' }).waitFor();
    const unsafeMod = thirdPage.getByRole('button', { name: /Readable UI/ });
    assert.equal(await unsafeMod.isDisabled(), true);
    assert.match(await unsafeMod.innerText(), /This mod cannot be opened here yet/);
    assert.equal(fileRequests, fileRequestsBeforeUnsafe);

    delayStellarMods = true;
    const stalePage = await context.newPage(); stalePage.setDefaultTimeout(3000);
    await stalePage.goto(origin); await stalePage.getByRole('heading', { name: 'Choose a game' }).waitFor();
    await stalePage.locator('[data-card="game:stellarblade"]').click();
    await stalePage.keyboard.press('Escape');
    await stalePage.locator('[data-card="game:eldenring"]').click();
    await stalePage.getByRole('heading', { name: 'Latest mods for Elden Ring' }).waitFor();
    await stalePage.getByRole('button', { name: /Elden Fixture Mod/ }).waitFor();
    await stalePage.waitForTimeout(400);
    assert.equal(await stalePage.locator('#games-view h1').innerText(), 'Latest mods for Elden Ring');
    assert.equal(await stalePage.getByRole('button', { name: /Elden Fixture Mod/ }).count(), 1);

    authOnly = true;
    const authPage = await context.newPage(); authPage.setDefaultTimeout(3000);
    await authPage.goto(origin); await authPage.getByRole('button', { name: 'Sign in' }).waitFor();
    await controllerReach(authPage, '#sign-in');
    assert.equal(await authPage.evaluate(() => document.activeElement?.id), 'sign-in');
    // Authorization leaves this document; do not wait for a release RAF in it.
    await authPage.evaluate(() => { window.fixturePad.buttons[0].pressed = true; window.fixturePad.buttons[0].value = 1; });
    await authPage.waitForFunction(() => location.hostname === 'users.nexusmods.com');
    assert.equal(authStarts, 1);
    assert.match(authNavigation, /^https:\/\/users\.nexusmods\.com\/oauth\/authorize\?fixture=1$/);

    authOnly = false; verifiedEmpty = true;
    const disconnectPage = await context.newPage(); disconnectPage.setDefaultTimeout(3000);
    await disconnectPage.goto(origin); await disconnectPage.getByRole('button', { name: 'Disconnect' }).waitFor();
    await controllerReach(disconnectPage, '#disconnect-account');
    assert.equal(await disconnectPage.evaluate(() => document.activeElement?.id), 'disconnect-account');
    await controllerPress(disconnectPage, 0);
    await disconnectPage.waitForFunction(() => document.querySelector('#account')?.innerText.includes('not connected'));
    assert.equal(disconnectStarts, 1);

    needsNexusAuthorization = true;
    const activityPage = await context.newPage(); activityPage.setDefaultTimeout(3000);
    await activityPage.goto(origin); await activityPage.getByRole('tab', { name: 'Activity' }).click();
    const openSource = activityPage.getByRole('link', { name: 'Open Nexus file page' });
    await openSource.waitFor();
    assert.equal(await openSource.getAttribute('href'), 'https://www.nexusmods.com/stellarblade/mods/42?tab=files&file_id=7');
    await openSource.focus();
    await activityPage.waitForTimeout(2700);
    assert.equal(await activityPage.evaluate(() => document.activeElement?.textContent.trim()), 'Open Nexus file page');
    changedReceipt = true;
    await activityPage.getByText('22 bytes').waitFor({ timeout: 4000 });
    assert.equal(await activityPage.evaluate(() => document.activeElement?.textContent.trim()), 'Open Nexus file page');
    await activityPage.evaluate(() => { const buttons = Array.from({ length: 17 }, () => ({ pressed: false })); buttons[0].pressed = true; const pad = { mapping: 'standard', buttons, axes: [0, 0] }; Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [pad] }); window.fixtureGamepad = pad; });
    const sourcePopupPromise = activityPage.waitForEvent('popup');
    const sourcePopup = await sourcePopupPromise; await sourcePopup.waitForFunction(() => location.hostname === 'www.nexusmods.com');
    assert.equal(sourceNavigation, 'https://www.nexusmods.com/stellarblade/mods/42?tab=files&file_id=7');
    invalidAuthorizationDomain = true;
    const invalidActivityPage = await context.newPage(); invalidActivityPage.setDefaultTimeout(3000);
    await invalidActivityPage.goto(origin); await invalidActivityPage.getByRole('tab', { name: 'Activity' }).click();
    await invalidActivityPage.getByText(/Nexus file page is not available/).waitFor();
    assert.equal(await invalidActivityPage.getByRole('link', { name: 'Open Nexus file page' }).count(), 0);
    assert.deepEqual(outboundUrls.filter(url => !url.startsWith('https://users.nexusmods.com/oauth/authorize?fixture=1') && !url.startsWith('https://www.nexusmods.com/stellarblade/mods/42?tab=files&file_id=7')), []);
    assert.deepEqual(browserErrors, []);
    console.log('ui tests passed');
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
