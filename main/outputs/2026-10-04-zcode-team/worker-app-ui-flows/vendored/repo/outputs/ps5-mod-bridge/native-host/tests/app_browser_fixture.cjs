// Owned same-origin browser check for the built native app preview.
// It targets only the local desktop fixture; no console or provider calls.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const net = require('node:net');
const {spawn} = require('node:child_process');
const {chromium} = require('playwright');

(async () => {
  const binary = process.argv[2];
  assert.ok(binary, 'pass the fresh desktop fixture binary path');
  const port = await new Promise((resolve, reject) => {
    const socket = net.createServer();
    socket.once('error', reject);
    socket.listen(0, '127.0.0.1', () => {
      const found = socket.address().port;
      socket.close(() => resolve(found));
    });
  });
  const child = spawn(binary, ['--port', String(port), '--lifetime', '20'],
                      {stdio: ['ignore', 'ignore', 'pipe']});
  const exited = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('exit', (code, signal) => resolve({code, signal}));
  });
  const origin = 'http://127.0.0.1:' + port;
  const requests = [];
  const blocked = [];
  const errors = [];
  let browser;
  try {
    for (let attempt = 0; ; ++attempt) {
      try {
        const response = await fetch(origin + '/version', {signal: AbortSignal.timeout(500)});
        assert.equal(response.status, 200);
        assert.equal((await response.json()).origin, 'desktop_fixture');
        break;
      } catch (error) {
        if (attempt === 39) throw error;
        await new Promise(resolve => setTimeout(resolve, 50));
      }
    }
    const launch = {headless: true, args: ['--disable-background-networking', '--disable-component-update']};
    const configured = process.env.CHROMIUM_EXECUTABLE_PATH;
    const chrome = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    if (configured) launch.executablePath = configured;
    else if (fs.existsSync(chrome)) launch.executablePath = chrome;
    browser = await chromium.launch(launch);
    const page = await browser.newPage({viewport: {width: 1440, height: 960}});
    await page.route('**/*', route => {
      const url = new URL(route.request().url());
      if (url.origin !== origin) {
        blocked.push(url.href);
        return route.abort();
      }
      requests.push(url.pathname);
      return route.continue();
    });
    page.on('pageerror', error => errors.push(String(error)));
    page.on('console', message => {
      if (message.type() === 'error') errors.push(message.text());
    });
    const response = await page.goto(origin + '/app', {waitUntil: 'networkidle'});
    assert.equal(response.status(), 200);
    await page.waitForFunction(() =>
      document.querySelector('#title-status')?.textContent.includes('Console title inventory unavailable'));
    assert.equal(await page.locator('.mode-label').textContent(), 'CONSOLE PREVIEW');
    assert.equal(await page.locator('#selected-target').textContent(), 'No console target · analysis only');
    assert.equal(await page.locator('#lookup-submit').isDisabled(), true);
    assert.equal(await page.locator('#reload-titles').isHidden(), true);
    assert.equal((await page.locator('#account').innerText()).includes('not connected'), true);
    assert.equal(await page.locator('#footer-capabilities').textContent(), 'Install and deploy unavailable');
    assert.deepEqual(blocked, []);
    const requestSet = [...new Set(requests)].sort();
    assert.deepEqual(requestSet, [
      '/api/capabilities', '/api/detected-titles', '/app', '/app.js', '/style.css',
    ]);
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({
      result: 'passed',
      route: '/app',
      mode: 'console_preview',
      browser_requests: requestSet,
      external_requests_blocked: blocked.length,
      page_errors: errors.length,
      console_execution: 'not tested; desktop fixture only',
    }));
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null && child.signalCode === null) {
      child.kill('SIGTERM');
      await exited.catch(() => {});
    }
  }
})().catch(error => {console.error(error); process.exitCode = 1;});
