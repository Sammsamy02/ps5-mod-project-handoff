'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {chromium} = require('playwright');
const bounded = promise => new Promise((resolve, reject) => {
  const timer = setTimeout(() => reject(new Error('generation confirmation was not requested')), 6000);
  promise.then(value => { clearTimeout(timer); resolve(value); }, error => { clearTimeout(timer); reject(error); });
});

// The app's CSP forbids 'unsafe-eval', and page.waitForFunction evaluates its predicate through eval,
// so wait by polling page.evaluate (a function call over CDP, which the CSP does not govern).
async function until(page, predicate, description, timeout = 6000) {
  const deadline = Date.now() + timeout;
  for (;;) {
    if (await page.evaluate(predicate)) return;
    if (Date.now() >= deadline) throw new Error(`timed out waiting for ${description}`);
    await new Promise(resolve => setTimeout(resolve, 25));
  }
}

async function main() {
  const child = spawn(process.env.PYTHON || 'python3', [path.join(__dirname, 'serve_title_paging_fixture.py')],
    {stdio: ['ignore', 'pipe', 'pipe']});
  const stopped = new Promise(resolve => child.once('exit', resolve));
  const captures = [];
  let browser, release, input = '', diagnostic = '', readyResolve, readyReject;
  const ready = new Promise((resolve, reject) => { readyResolve = resolve; readyReject = reject; });
  child.stderr.on('data', bytes => { diagnostic += bytes.toString(); });
  child.stdout.on('data', bytes => {
    input += bytes.toString();
    while (input.includes('\n')) {
      const index = input.indexOf('\n'), line = input.slice(0, index); input = input.slice(index + 1);
      try {
        const record = JSON.parse(line);
        if (record.evidence) readyResolve(record);
        if (record.event === 'owned_capture') captures.push(record.count);
      } catch (error) { readyReject(error); }
    }
  });
  child.once('error', readyReject);
  child.once('exit', code => readyReject(new Error(`Owned fixture exited ${code}: ${diagnostic}`)));
  const startup = setTimeout(() => readyReject(new Error('Owned fixture startup timed out')), 10000);
  try {
    const fixture = await ready;
    clearTimeout(startup);
    assert.equal(fixture.evidence, 'owned_generic_title_fixture');
    const launch = {headless: true};
    if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
    else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'))
      launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch(launch);
    const page = await browser.newPage({viewport: {width: 1920, height: 1080}});
    page.setDefaultTimeout(6000);
    const failures = [], requests = [];
    page.on('pageerror', error => failures.push(error.message));
    let entered;
    let confirmation = new Promise(resolve => { entered = resolve; });
    let held = new Promise(resolve => { release = resolve; });
    await page.route('**/*', async route => {
      const url = new URL(route.request().url());
      assert.equal(url.origin, fixture.origin, 'unexpected remote traffic');
      requests.push(url.pathname);
      if (url.pathname.startsWith('/api/detected-titles/pages/current/')) {
        entered();
        await held;
      }
      await route.continue();
    });
    await page.goto(fixture.origin);
    await bounded(confirmation);
    assert.equal(await page.locator('#title-list [data-title-key]:not([disabled])').count(), 0,
      'no selectable prefix before generation confirmation');
    assert.match(await page.locator('#title-status').textContent(), /Checking/);
    release();
    await until(page, () => document.querySelectorAll('#title-list [data-title-key]:not([disabled])').length === 128,
      '128 selectable generic titles');
    assert.equal(await page.locator('#title-list [data-title-key]').count(), 128);
    assert.equal(requests.filter(item => /^\/api\/detected-titles\/pages\/[a-f0-9]{32}$/.test(item)).length, 15);
    assert.deepEqual(captures, [1]);
    const last = page.locator('#title-list [data-title-key]').last();
    assert.match(await last.textContent(), /GENERIC_127/);
    assert.match(await last.textContent(), /Build fingerprint measured/);
    await last.click();
    assert.match(await page.locator('#selected-target').textContent(), /GENERIC_127/);

    confirmation = new Promise(resolve => { entered = resolve; });
    held = new Promise(resolve => { release = resolve; });
    await page.locator('#reload-titles').click();
    await bounded(confirmation);
    assert.equal(await page.locator('#title-list [data-title-key]:not([disabled])').count(), 0);
    const refreshed = await page.evaluate(async () => {
      const caps = await (await fetch('/api/capabilities')).json();
      const response = await fetch('/api/detected-titles/pages/refresh', {
        method: 'POST', headers: {'Content-Type': 'application/json', 'X-CSRF-Token': caps.csrf_token}, body: '{}'});
      return response.status;
    });
    assert.equal(refreshed, 200);
    release();
    await until(page, () => !document.querySelector('#reload-titles').disabled, 'title reload to re-enable');
    assert.equal(await page.locator('#title-list [data-title-key]:not([disabled])').count(), 0,
      'refresh racing final confirmation cannot commit a selectable list');
    assert.match(await page.locator('#selected-target').textContent(), /No console target/);
    assert.deepEqual(captures, [1, 2, 3]);
    assert.deepEqual(failures, []);
    console.log('actual-host paging browser passed: 128 generic titles, canonical wide length, atomic commit, stale confirmation; no remote/device execution');
  } finally {
    clearTimeout(startup);
    if (release) release();
    if (browser) await browser.close();
    child.kill('SIGTERM');
    await stopped;
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
