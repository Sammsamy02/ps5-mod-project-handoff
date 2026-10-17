// Development-only real browser check; never runs the PS5 ELF or contacts a console.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {chromium} = require('playwright');

(async () => {
  const port = await new Promise((resolve, reject) => {
    const socket = net.createServer();
    socket.once('error', reject);
    socket.listen(0, '127.0.0.1', () => {
      const found = socket.address().port;
      socket.close(() => resolve(found));
    });
  });
  const binary = process.env.MB_DESKTOP_BINARY || path.resolve(__dirname, '../build/drbg/desktop_fixture/modbridge-native-host-desktop');
  const child = spawn(binary, ['--port', String(port), '--lifetime', '15'], {stdio: ['ignore', 'ignore', 'pipe']});
  const exited = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('exit', (code, signal) => resolve({code, signal}));
  });
  let browser;
  try {
    const url = `http://127.0.0.1:${port}`;
    for (let attempt = 0; ; ++attempt) {
      try {
        const response = await fetch(url + '/version', {signal: AbortSignal.timeout(500)});
        assert.equal(response.status, 200);
        assert.equal((await response.json()).origin, 'desktop_fixture');
        break;
      } catch (error) {
        if (attempt === 29) throw error;
        await new Promise(resolve => setTimeout(resolve, 50));
      }
    }
    const launch = {headless: true};
    const chrome = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
    else if (fs.existsSync(chrome)) launch.executablePath = chrome;
    browser = await chromium.launch(launch);
    const page = await browser.newPage({viewport: {width: 1920, height: 1080}});
    const errors = [];
    page.on('pageerror', error => errors.push(String(error)));
    await page.goto(url, {waitUntil: 'networkidle'});
    await page.waitForFunction(() => {
      try { return JSON.parse(document.querySelector('#status').textContent).origin === 'desktop_fixture'; }
      catch { return false; }
    });
    const report = JSON.parse(await page.locator('#status').textContent());
    assert.equal(report.phase, 'main_http_ready');
    assert.equal(report.installation_supported, false);
    assert.deepEqual(report.observations.entropy, {
      status: 'unavailable', reason: 'desktop_fixture', api: 'sysctl(KERN_ARND)',
      scope: 'api_result_only', requested_bytes: 32, call_count: 0,
      return_code: null, errno: null, returned_length_bytes: null, elapsed_ms: null,
      hard_deadline_enforced: false,
    });
    assert.deepEqual(report.observations.drbg, {
      status: 'unavailable', reason: 'desktop_fixture', scope: 'owned_startup_checkpoint',
      mbedtls_version: null, config_id: null, requested_generation_bytes: 32,
      add_source_rc: null, seed_rc: null, generate_rc: null, completed_length_bytes: null,
      source_callback_count: 0, source_os_call_count: 0, total_api_call_count: 0,
      api_call_limit: 4, elapsed_ms: null, hard_deadline_enforced: false, source_failure: null,
    });
    assert.equal(report.core.status, 'passed');
    assert.equal(report.core.installation_supported, false);
    assert.equal(Object.values(report.core.checks).filter(Boolean).length, 7);
    assert.deepEqual(report.core, await (await fetch(url + '/core')).json());
    // Owned synthetic wire reply exercises exact display beyond JavaScript's
    // safe integer range; it is not native execution or API-return evidence.
    const oversized = structuredClone(report);
    oversized.observations.entropy = {
      ...oversized.observations.entropy, status: 'failed', reason: 'oversized_result',
      call_count: 1, return_code: 0, errno: 0,
      returned_length_bytes: 'EXACT_SIZE_MAX_DECIMAL', elapsed_ms: 0,
    };
    const wire = JSON.stringify(oversized).replace('"EXACT_SIZE_MAX_DECIMAL"', '18446744073709551615');
    await page.route(url + '/report', route => route.fulfill({status: 200, contentType: 'application/json', body: wire}));
    await page.reload({waitUntil: 'networkidle'});
    await page.waitForFunction(() => document.querySelector('#status').textContent.includes('oversized_result'));
    const displayed = await page.locator('#status').textContent();
    assert.match(displayed, /18446744073709551615/);
    assert.doesNotMatch(displayed, /18446744073709552000/);
    await page.unroute(url + '/report');
    await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'stop');
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => {
      try { return JSON.parse(document.querySelector('#status').textContent).status === 'shutdown_requested'; }
      catch { return false; }
    });
    assert.deepEqual(await exited, {code: 0, signal: null});
    assert.deepEqual(errors, []);
    console.log('native-host browser fixture passed (CSP, report, keyboard shutdown, actual desktop exit; no device execution)');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null && child.signalCode === null) {
      child.kill('SIGTERM');
      await exited.catch(() => {});
    }
  }
})().catch(error => {console.error(error); process.exitCode = 1;});
