'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '..');
const evidence = process.env.UI_EVIDENCE_ROOT ? path.join(path.resolve(process.env.UI_EVIDENCE_ROOT), 'screenshots') : path.join(root, 'docs/ui/step-02/screenshots');
const results = [];
async function press(page, selector) {
  for (let i = 0; i < 100; i++) {
    if (await page.evaluate(s => document.activeElement?.matches(s), selector)) {
      await page.keyboard.press('Enter'); return;
    }
    await page.keyboard.press('Tab');
  }
  assert.fail(`Keyboard/controller-focusable action unreachable: ${selector}`);
}
async function main() {
  fs.mkdirSync(evidence, { recursive: true });
  const options = { headless: true };
  const chrome = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  if (fs.existsSync(chrome)) options.executablePath = chrome;
  const browser = await chromium.launch(options);
  async function fixture({ error = '', statusError = '', decisions = [{ id: 'keep', question: 'Keep your settings?', default: true }], running = false, missing = false, slow = false } = {}) {
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    await page.route('**/*', route => route.abort());
    await page.setContent('<!doctype html><html lang="en"><body><div class="shell"><header class="topbar"><span class="brand">ModBridge · Install preview</span></header><main id="main"><section id="installer-panel"></section><button id="outside" class="secondary-button">Other screen</button></main></div></body></html>');
    await page.addStyleTag({ path: path.join(root, 'web/style.css') });
    await page.evaluate(options => {
      const item = { install_id: 'installed-one', name: 'Soft lights', enabled: true, load_order: 1, update_available: true };
      const second = { install_id: 'installed-two', name: 'Warm lights', enabled: true, load_order: 2 };
      window.fixture = { calls: [], error: options.error, statusError: options.statusError, running: options.running, resolve: null, progress: null, installed: [item, second] };
      const fail = (code) => { throw { code, message: '/private/game 0xdeadbeef raw detail', nothing_changed: true }; };
      window.service = {
        async status(title) { window.fixture.calls.push(['status', title]); if (window.fixture.statusError) fail(window.fixture.statusError); if (window.fixture.delayFinal && window.fixture.calls.some(x => x[0] === 'apply')) await new Promise(resolve => { window.fixture.resolveFinal = resolve; }); return { mock: true, game_running: window.fixture.running, progress: window.fixture.progress, capability_file: options.missing ? null : { supported_mod_types: ['textures'] }, installed_mods: window.fixture.installed, setup: { complete: false }, conflicts: [{ mod_ids: ['installed-one', 'installed-two'] }], last_session: [{ install_id: 'installed-one', worked: true }, { install_id: 'installed-two', worked: false, reason: 'The game could not use this mod.' }], self_test: { ok: true, message: 'The game is ready for supported mods.' } }; },
        async plan(title, archive) { window.fixture.calls.push(['plan', title, archive]); if (window.fixture.error) fail(window.fixture.error); return { plan_id: 'plan-owned', plan_sha256: 'a'.repeat(64), changes: [{ description: 'Add brighter room lighting', action: 'add' }], warnings: ['This mod changes the same files as another mod.'], decisions: options.decisions }; },
        async apply(id, hash, answers) { window.fixture.calls.push(['apply', id, hash, answers]); if (window.fixture.error) fail(window.fixture.error); if (options.slow) await new Promise(resolve => { window.fixture.resolve = resolve; }); if (window.fixture.failFinal) window.fixture.statusError = 'offline'; return window.fixture.reply || { ok: true, install_id: 'new-install', report: { summary: 'Your mod is ready.' } }; },
        async restore(id) { window.fixture.calls.push(['restore', id]); if (window.fixture.error) fail(window.fixture.error); return { ok: true, report: { summary: 'The previous setup was restored.' } }; }
      };
      window.controls = { setEnabled(title, id, enabled) { window.fixture.calls.push(['enabled', title, id, enabled]); window.fixture.installed.find(x => x.install_id === id).enabled = enabled; }, reorder(title, ids) { window.fixture.calls.push(['reorder', title, ids]); window.fixture.installed = ids.map(id => window.fixture.installed.find(x => x.install_id === id)); }, setSetup() {} };
    }, { error, statusError, decisions, running, missing, slow });
    const module = path.join(root, 'web/installer-ui.js');
    if (fs.existsSync(module)) await page.addScriptTag({ path: module });
    await page.evaluate(async () => {
      if (window.ModBridgeInstallerUI) { window.ModBridgeInstallerUI.init({ service: window.service, controls: window.controls, mock: true }); await window.ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); window.ModBridgeInstallerUI.offerArchive({ game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/archive.zip', label: 'Brighter rooms' }); }
    });
    return page;
  }
  async function test(name, run) {
    if (process.env.UI_INSTALLER_FILTER && !name.includes(process.env.UI_INSTALLER_FILTER)) return;
    try { await run(); results.push({ name, ok: true }); console.log(`PASS ${name}`); }
    catch (error) { results.push({ name, ok: false, error: error.message }); console.log(`FAIL ${name}: ${error.message}`); }
  }
  const text = page => page.locator('#installer-panel').innerText();
  await test('Plan is reviewable and cannot apply before explicit confirmation', async () => {
    const page = await fixture();
    assert.match(await text(page), /Review install/);
    await press(page, '[data-installer-action="plan"]');
    await page.locator('[data-installer-action="apply"]').waitFor();
    assert.match(await text(page), /Add brighter room lighting/);
    assert.match(await text(page), /Keep your settings/);
    assert.equal(await page.evaluate(() => fixture.calls.filter(x => x[0] === 'apply').length), 0);
    await page.screenshot({ path: path.join(evidence, 'plan-1080.png') });
    await press(page, '[data-installer-answer="keep"][data-value="false"]');
    await press(page, '[data-installer-action="apply"]');
    await page.locator('[data-installer-state="result"]').waitFor();
    assert.deepEqual(await page.evaluate(() => fixture.calls.find(x => x[0] === 'apply')), ['apply', 'plan-owned', 'a'.repeat(64), { keep: false }]);
    await page.screenshot({ path: path.join(evidence, 'result-1080.png') }); await page.close();
  });
  await test('Cancel and back discard the plan without applying', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="cancel"]').waitFor();
    await press(page, '[data-installer-action="cancel"]');
    assert.equal(await page.evaluate(() => fixture.calls.some(x => x[0] === 'apply')), false);
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    assert.equal(await page.evaluate(() => ModBridgeInstallerUI.handleBack()), true);
    assert.equal(await page.locator('[data-installer-action="apply"]').count(), 0); await page.close();
  });
  await test('A running game blocks planning and APPLY rechecks the game', async () => {
    const page = await fixture({ running: true }); assert.match(await text(page), /Close the game first/);
    assert.equal(await page.locator('[data-installer-action="plan"]:enabled').count(), 0);
    await page.evaluate(async () => { fixture.running = false; await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); });
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.running = true; }); await press(page, '[data-installer-action="apply"]');
    await page.locator('[data-installer-state="error"]').waitFor(); assert.match(await text(page), /Close the game first/);
    assert.equal(await page.evaluate(() => fixture.calls.some(x => x[0] === 'apply')), false); await page.close();
  });
  await test('Progress survives leaving, duplicate activation makes one APPLY', async () => {
    const page = await fixture({ slow: true }); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="progress"]').waitFor();
    await page.keyboard.press('Enter'); await page.keyboard.press('Enter');
    assert.match(await text(page), /leave this screen/);
    await page.screenshot({ path: path.join(evidence, 'progress-1080.png') });
    await page.evaluate(() => { document.querySelector('#installer-panel').hidden = true; });
    await page.evaluate(() => fixture.resolve()); await page.waitForFunction(() => document.querySelector('[data-installer-state="result"]'));
    await page.evaluate(() => { document.querySelector('#installer-panel').hidden = false; });
    assert.equal(await page.evaluate(() => fixture.calls.filter(x => x[0] === 'apply').length), 1); await page.close();
  });
  await test('Long steps show actual service progress and stop polling after completion', async () => {
    const page = await fixture({ slow: true }); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.waitForFunction(() => fixture.resolve !== null);
    await page.evaluate(() => { fixture.progress = { stage: 'converting', message: 'Converting textures.', estimated_minutes: 10, percent: 30, can_leave: true }; });
    await page.waitForFunction(() => document.querySelector('#installer-panel').textContent.includes('About 10 minutes.'), null, { timeout: 2500 });
    assert.equal(await page.locator('progress').getAttribute('value'), '30');
    await page.screenshot({ path: path.join(evidence, 'converting-1080.png') });
    await page.evaluate(() => fixture.resolve()); await page.locator('[data-installer-state="result"]').waitFor();
    const before = await page.evaluate(() => fixture.calls.length); await page.waitForTimeout(1200);
    assert.equal(await page.evaluate(() => fixture.calls.length), before); await page.close();
  });
  await test('APPLY failure shows no success and retry requires a new plan', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.error = 'console_busy'; }); await press(page, '[data-installer-action="apply"]');
    await page.locator('[data-installer-state="error"]').waitFor(); assert.equal(await page.locator('[data-installer-state="result"]').count(), 0);
    await page.evaluate(() => { fixture.error = ''; }); await press(page, '[data-installer-action="retry"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    assert.equal(await page.evaluate(() => fixture.calls.filter(x => x[0] === 'apply').length), 1); await page.close();
  });
  await test('A refusal without unchanged evidence never promises nothing changed', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.reply = { ok: false, report: { code: 'install_refused', message: 'The installer refused this request.' } }; });
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="error"]').waitFor();
    assert.doesNotMatch(await text(page), /Nothing was changed/); assert.equal(await page.locator('[data-installer-state="result"]').count(), 0); await page.close();
  });
  await test('A failed APPLY report preserves its specific game-running reason', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.reply = { ok: false, report: { code: 'game_running', nothing_changed: true } }; });
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="error"]').waitFor();
    assert.match(await text(page), /Close the game first/); await page.close();
  });
  await test('One-time setup copy is calm and not shown on later visits', async () => {
    const page = await fixture(); assert.match(await text(page), /Setting up this game for mods — one time only/);
    await page.screenshot({ path: path.join(evidence, 'setup-1080.png') });
    await page.evaluate(async () => { await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); });
    assert.doesNotMatch(await text(page), /Setting up this game for mods — one time only/); await page.close();
  });
  await test('RESTORE requires a clear confirmation and uses install identity only', async () => {
    const page = await fixture(); await press(page, '[data-installer-restore="installed-one"]');
    assert.equal(await page.evaluate(() => fixture.calls.some(x => x[0] === 'restore')), false);
    await page.locator('[data-installer-action="restore-confirm"]').waitFor(); await page.screenshot({ path: path.join(evidence, 'restore-1080.png') });
    await press(page, '[data-installer-action="restore-confirm"]'); await page.locator('[data-installer-state="result"]').waitFor();
    assert.deepEqual(await page.evaluate(() => fixture.calls.find(x => x[0] === 'restore')), ['restore', 'installed-one']); await page.close();
  });
  const errors = { no_internet: /No internet/, nexus_expired: /Nexus login expired/, corrupt_download: /download.*damaged|download.*corrupted/i, unsupported_type: /not supported/, install_refused: /Nothing was changed/, game_running: /Close the game first/, console_busy: /console is busy/i };
  for (const [code, pattern] of Object.entries(errors)) await test(`${code}: plain error, useful next action, technical details hidden`, async () => {
    const page = await fixture({ error: code }); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-state="error"]').waitFor();
    const copy = await text(page); assert.match(copy, pattern); assert.doesNotMatch(copy, /0xdeadbeef|\/private\/game/);
    assert.equal(await page.locator('[data-installer-action="retry"]').count(), 1);
    await page.screenshot({ path: path.join(evidence, `error-${code}-1080.png`) });
    await press(page, '[data-installer-action="more"]'); assert.match(await page.locator('#installer-info-dialog').innerText(), /0xdeadbeef/);
    await page.keyboard.press('Escape'); assert.equal(await page.evaluate(() => document.activeElement?.matches('[data-installer-action="more"]')), true);
    await page.evaluate(() => { fixture.error = ''; }); await press(page, '[data-installer-action="retry"]');
    await page.locator('[data-installer-action="apply"]').waitFor(); await page.close();
  });
  await test('Missing capability file makes the game unavailable for installs', async () => {
    const page = await fixture({ missing: true }); assert.match(await text(page), /not available yet/i); assert.equal(await page.locator('[data-installer-action="plan"]:enabled').count(), 0); await page.close();
  });
  await test('A status connection error survives offering an archive and can recover', async () => {
    const page = await fixture({ statusError: 'offline' }); assert.match(await text(page), /No internet/);
    await page.evaluate(() => { fixture.statusError = ''; }); await press(page, '[data-installer-action="retry"]');
    await page.locator('[data-installer-action="plan"]').waitFor(); assert.match(await text(page), /Brighter rooms/); await page.close();
  });
  await test('Back on another screen leaves the installer plan intact', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { document.querySelector('#installer-panel').hidden = true; });
    assert.equal(await page.evaluate(() => ModBridgeInstallerUI.handleBack()), false);
    await page.evaluate(() => { document.querySelector('#installer-panel').hidden = false; });
    assert.equal(await page.locator('[data-installer-action="apply"]').count(), 1); await page.close();
  });
  await test('Per-game preview shows order, conflict winner, updates, sessions and self-test', async () => {
    const page = await fixture(); const copy = await text(page);
    for (const phrase of [/Preview/, /Update available/, /lower in the list wins/, /Worked/, /Didn't work/, /self-test/i]) assert.match(copy, phrase);
    await page.screenshot({ path: path.join(evidence, 'game-1080.png') });
    await press(page, '[data-installer-toggle="installed-one"]'); assert.equal(await page.evaluate(() => fixture.installed[0].enabled), false);
    await press(page, '[data-installer-move="installed-two"][data-direction="up"]'); assert.equal(await page.evaluate(() => fixture.installed[0].install_id), 'installed-two'); await page.close();
  });
  await test('Plain-string planned changes retain their meaning', async () => {
    const page = await fixture(); await page.evaluate(() => { const original = service.plan; service.plan = async (...args) => ({ ...(await original(...args)), changes: ['Add soft lighting without changing your settings'] }); });
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    assert.match(await text(page), /Add soft lighting without changing your settings/); await page.close();
  });
  await test('Missing optional enabled status does not claim the mod is off', async () => {
    const page = await fixture(); await page.evaluate(async () => { delete fixture.installed[0].enabled; await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); });
    const card = page.locator('.installer-mod').first(); assert.match(await card.innerText(), /On\/off unavailable/);
    assert.equal(await card.locator('[data-installer-toggle]:enabled').count(), 0); await page.close();
  });
  await test('Technical identities and oversized copy stay behind More info', async () => {
    const page = await fixture();
    await page.evaluate(async () => {
      window.diagnostic = { hash: 'a'.repeat(64), token: 'token0123456789abcdefghijklmnopqr', id: 'CUSA12345', long: 'A detailed explanation for a technical reviewer. '.repeat(20) };
      fixture.installed[0].name = diagnostic.hash;
      const status = service.status; service.status = async title => { const value = await status(title); value.self_test.message = diagnostic.id; value.last_session[1].reason = diagnostic.long; return value; };
      const plan = service.plan; service.plan = async (...args) => ({ ...(await plan(...args)), changes: [diagnostic.hash, diagnostic.token, diagnostic.id, 'Add softer lighting'] });
      fixture.reply = { ok: true, install_id: 'new-install', report: { summary: diagnostic.id } };
      ModBridgeInstallerUI.init({ service, mock: false }); await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: diagnostic.hash });
      ModBridgeInstallerUI.offerArchive({ game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/archive.zip', label: diagnostic.token });
    });
    const diagnostic = await page.evaluate(() => window.diagnostic);
    for (const value of Object.values(diagnostic)) assert.equal((await text(page)).includes(value), false);
    await press(page, '[data-installer-action="more"]');
    for (const value of Object.values(diagnostic)) assert.equal((await page.locator('#installer-info-dialog').innerText()).includes(value), true);
    await page.keyboard.press('Escape'); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    assert.match(await text(page), /Add softer lighting/);
    for (const value of [diagnostic.hash, diagnostic.token, diagnostic.id]) assert.equal((await text(page)).includes(value), false);
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    assert.equal((await text(page)).includes(diagnostic.id), false); await press(page, '[data-installer-action="more"]');
    assert.match(await page.locator('#installer-info-dialog').innerText(), /CUSA12345/); await page.close();
  });
  await test('Unicode game names with standalone slash separators remain readable', async () => {
    const page = await fixture(); const name = '星 Café 🎮 / \\ "0"';
    await page.evaluate(async name => ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name }), name);
    assert.equal((await text(page)).includes(name), true);
    const screenshotFolder = path.join(root, 'docs/ui/step-03/screenshots'); fs.mkdirSync(screenshotFolder, { recursive: true });
    await page.screenshot({ path: path.join(screenshotFolder, 'installer-unicode-name-1080.png') });
    for (const technical of ['"/data/game"', '"C:\\Games\\Owned"', '12345678-abcd-1234-abcd-123456789abc', 'token0123456789abcdefghijklmnopqr']) {
      await page.evaluate(async name => ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name }), technical);
      assert.equal((await text(page)).includes(technical), false);
      await press(page, '[data-installer-action="more"]');
      assert.equal(await page.evaluate(() => JSON.parse(document.querySelector('[data-installer-details]').textContent).game.name), technical);
      await page.keyboard.press('Escape');
    }
    await page.close();
  });
  await test('No real management calls are invented', async () => {
    const page = await fixture(); await page.evaluate(async () => { ModBridgeInstallerUI.init({ service, mock: false }); await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); });
    assert.equal(await page.locator('[data-installer-toggle]:enabled,[data-installer-move]:enabled').count(), 0); assert.match(await text(page), /not available yet/i); await page.close();
  });
  await test('An injected service needs explicit mock marking before preview claims', async () => {
    const page = await fixture(); await page.evaluate(async () => { ModBridgeInstallerUI.init({ service }); await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); });
    assert.doesNotMatch(await text(page), /practice install|No games or mod files are changed/); await page.close();
  });
  await test('Busy coordination publishes initial state and only boolean transitions', async () => {
    const page = await fixture({ slow: true });
    await page.evaluate(async () => {
      window.busyEvents = [];
      window.addEventListener('modbridge:installer-busy', event => busyEvents.push(event.detail));
      ModBridgeInstallerUI.init({ service, controls, mock: true });
      ModBridgeInstallerUI.render(); ModBridgeInstallerUI.render();
      await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' });
      ModBridgeInstallerUI.offerArchive({ game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/archive.zip', label: 'Brighter rooms' });
    });
    assert.deepEqual(await page.evaluate(() => busyEvents), [{ busy: false }, { busy: true }, { busy: false }]);
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.waitForFunction(() => fixture.resolve !== null);
    await page.evaluate(() => { ModBridgeInstallerUI.render(); ModBridgeInstallerUI.render(); });
    assert.deepEqual(await page.evaluate(() => busyEvents), [{ busy: false }, { busy: true }, { busy: false }, { busy: true }, { busy: false }, { busy: true }]);
    await page.evaluate(() => fixture.resolve()); await page.locator('[data-installer-state="result"]').waitFor();
    assert.deepEqual(await page.evaluate(() => busyEvents), [{ busy: false }, { busy: true }, { busy: false }, { busy: true }, { busy: false }, { busy: true }, { busy: false }]);
    await page.close();
  });
  await test('Optional running-state metadata is not required by the real four-call contract', async () => {
    const page = await fixture(); await page.evaluate(async () => { const original = service.status; service.status = async title => { const reply = await original(title); delete reply.game_running; return reply; }; ModBridgeInstallerUI.init({ service, mock: false }); await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' }); ModBridgeInstallerUI.offerArchive({ game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/archive.zip', label: 'Brighter rooms' }); });
    assert.equal(await page.locator('[data-installer-action="plan"]:enabled').count(), 1); await page.close();
  });
  await test('Final status refresh cannot be replaced by a new UI session', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.delayFinal = true; }); await press(page, '[data-installer-action="apply"]'); await page.waitForFunction(() => !!fixture.resolveFinal);
    assert.equal(await page.evaluate(() => ModBridgeInstallerUI.init({ service, mock: true })), false);
    await page.evaluate(() => { fixture.delayFinal = false; fixture.resolveFinal(); }); await page.locator('[data-installer-state="result"]').waitFor(); await page.close();
  });
  for (const next of [null, { game_title_id: 'NEXT_OWNED_GAME', name: 'Next game' }]) await test(`Confirmed result survives a queued ${next ? 'different game' : 'target clear'} until acknowledgement`, async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    const before = await page.evaluate(() => fixture.calls.length);
    await page.evaluate(async game => ModBridgeInstallerUI.selectGame(game), next);
    assert.equal(await page.locator('[data-installer-state="result"]').count(), 1);
    assert.equal(await page.evaluate(() => fixture.calls.length), before);
    if (next) {
      assert.equal(await page.evaluate(() => ModBridgeInstallerUI.handleBack()), true);
      await page.waitForFunction(() => document.querySelector('#installer-panel').textContent.includes('Next game'));
      assert.deepEqual(await page.evaluate(() => fixture.calls.at(-1)), ['status', 'NEXT_OWNED_GAME']);
    } else {
      await press(page, '[data-installer-action="done"]'); assert.match(await text(page), /Choose a game/);
      assert.equal(await page.evaluate(() => fixture.calls.length), before);
    }
    await page.close();
  });
  await test('An offered archive cannot replace a confirmed result before acknowledgement', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    const archive = { game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/next.zip', label: 'Next mod' };
    assert.equal(await page.evaluate(value => ModBridgeInstallerUI.offerArchive(value), archive), false);
    assert.equal(await page.locator('[data-installer-state="result"]').count(), 1);
    await press(page, '[data-installer-action="done"]');
    assert.equal(await page.evaluate(value => ModBridgeInstallerUI.offerArchive(value), archive), true);
    assert.match(await text(page), /Next mod/); await page.close();
  });
  await test('Failed final refresh keeps the result and clears stale installed records', async () => {
    const page = await fixture(); await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { fixture.failFinal = true; }); await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    assert.match(await text(page), /could not.*refresh|could not.*update/i); await press(page, '[data-installer-action="done"]');
    assert.doesNotMatch(await text(page), /Soft lights/); await page.evaluate(() => { fixture.statusError = ''; }); await press(page, '[data-installer-action="refresh"]');
    await page.locator('[data-installer-toggle="installed-one"]').waitFor(); await page.close();
  });
  await test('Missing target never infers a game from an archive', async () => {
    const page = await fixture(); await page.evaluate(() => { ModBridgeInstallerUI.selectGame(null); ModBridgeInstallerUI.offerArchive({ archive_path: '/untrusted/without/target', label: 'Archive' }); });
    assert.match(await text(page), /Choose a game/); assert.equal(await page.locator('[data-installer-action="plan"]').count(), 0); await page.close();
  });
  await test('The real mock/service modules complete the default preview', async () => {
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    await page.route('**/*', route => route.request().url() === 'http://127.0.0.1:8787/' ? route.fulfill({ contentType: 'text/html', body: '<!doctype html><section id="installer-panel"></section>' }) : route.abort());
    await page.goto('http://127.0.0.1:8787/');
    await page.addStyleTag({ path: path.join(root, 'web/style.css') });
    assert.equal(await page.evaluate(() => typeof window.crypto.subtle.digest), 'function');
    for (const file of ['config.js', 'installer-service.js', 'installer-mock.js', 'installer-ui.js']) await page.addScriptTag({ path: path.join(root, 'web', file) });
    await page.evaluate(() => ModBridgeInstallerUI.init());
    await press(page, '[data-installer-game]'); await page.locator('[data-installer-action="plan"]').waitFor();
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    assert.match(await text(page), /No game or mod files were changed/);
    await press(page, '[data-installer-action="done"]');
    await page.evaluate(async () => ModBridgeInstallerUI.selectGame({ game_title_id: 'OBSERVED_UNKNOWN_FIXTURE', name: 'Observed game' }));
    assert.match(await text(page), /mod support information is missing/); await page.close();
  });
  await test('Configured real service reads the latest CSRF callback for each POST', async () => {
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    const posts = [];
    await page.route('**/*', route => {
      const request = route.request(), url = new URL(request.url());
      const json = value => route.fulfill({ contentType: 'application/json', body: JSON.stringify(value) });
      if (url.origin !== 'http://127.0.0.1:8787') return route.abort();
      if (url.pathname === '/') return route.fulfill({ contentType: 'text/html', body: '<!doctype html><section id="installer-panel"></section>' });
      if (request.method() === 'POST') posts.push({ path: url.pathname, token: request.headers()['x-csrf-token'] });
      if (url.pathname === '/api/installer/status') return json({ installed_mods: [], capability_file: { supported_mod_types: ['textures'] }, game_running: false });
      if (url.pathname === '/api/installer/plan') return json({ plan_id: 'owned-real-plan', plan_sha256: 'a'.repeat(64), changes: ['Add the selected mod'], warnings: [], decisions: [] });
      if (url.pathname === '/api/installer/apply') return json({ ok: true, install_id: 'owned-real-install', report: { summary: 'Your mod is ready.' } });
      return route.abort();
    });
    await page.goto('http://127.0.0.1:8787/');
    for (const file of ['config.js', 'installer-service.js', 'installer-ui.js']) await page.addScriptTag({ path: path.join(root, 'web', file) });
    await page.evaluate(async () => {
      window.ModBridgeConfig = { ...ModBridgeConfig, installer: { ...ModBridgeConfig.installer, mode: 'real' } };
      window.fixtureCsrf = 'initial-token'; ModBridgeInstallerUI.init({ csrf: () => fixtureCsrf });
      await ModBridgeInstallerUI.selectGame({ game_title_id: 'OWNED_FIXTURE_GAME', name: 'Sample adventure' });
      ModBridgeInstallerUI.offerArchive({ game_title_id: 'OWNED_FIXTURE_GAME', archive_path: '/private/owned/archive.zip', label: 'Brighter rooms' });
      window.fixtureCsrf = 'current-plan-token';
    });
    await press(page, '[data-installer-action="plan"]'); await page.locator('[data-installer-action="apply"]').waitFor();
    await page.evaluate(() => { window.fixtureCsrf = 'current-apply-token'; });
    await press(page, '[data-installer-action="apply"]'); await page.locator('[data-installer-state="result"]').waitFor();
    assert.deepEqual(posts, [{ path: '/api/installer/plan', token: 'current-plan-token' }, { path: '/api/installer/apply', token: 'current-apply-token' }]);
    assert.doesNotMatch(await text(page), /Preview mode/); await page.close();
  });
  await browser.close(); fs.writeFileSync(path.join(evidence, '..', 'test-results.json'), JSON.stringify({ results, passed: results.filter(x => x.ok).length, failed: results.filter(x => !x.ok).length, navigation: 'Keyboard Tab and Enter exercise focusable controls; full Gamepad navigation is tested separately by the app controller suite.', console_calls: 0, external_requests: 0 }, null, 2));
  process.exitCode = results.some(x => !x.ok) ? 1 : 0;
}
main().catch(error => { console.error(error); process.exitCode = 1; });
