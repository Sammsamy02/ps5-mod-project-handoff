const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { chromium } = require('playwright');

async function main() {
  const child = spawn(process.env.PYTHON || 'python3', [path.join(__dirname, 'serve_ui_fixture.py')], { stdio: ['ignore', 'pipe', 'pipe'], env: { ...process.env, FIXTURE_ACCOUNT_DELAY: '3' } });
  let browser;
  const stopped = new Promise(resolve => child.once('exit', resolve));
  let diagnostic = '';
  child.stderr.on('data', bytes => { diagnostic += bytes.toString(); });
  try {
    const fixture = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Owned host did not start')), 10000);
      let input = '';
      child.stdout.on('data', bytes => {
        input += bytes.toString();
        if (input.includes('\n')) {
          clearTimeout(timer);
          try { resolve(JSON.parse(input.split('\n')[0])); } catch (error) { reject(error); }
        }
      });
      child.once('error', error => { clearTimeout(timer); reject(error); });
      child.once('exit', code => { clearTimeout(timer); reject(new Error(`Owned host exited ${code}: ${diagnostic}`)); });
    });
    assert.equal(fixture.evidence, 'owned_desktop_fixture');
    const launch = { headless: true };
    if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
    else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch(launch);
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    page.setDefaultTimeout(6000);
    const failures = [];
    page.on('pageerror', error => failures.push(error.message));
    await page.route('**/*', route => {
      if (new URL(route.request().url()).origin !== fixture.origin) return route.abort();
      return route.continue();
    });
    await page.goto(fixture.origin);
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    assert.match(await page.locator('body').innerText(), /Compatibility unverified/);
    assert.match(await page.locator('body').innerText(), /Target build is unknown/);
    assert.match(await page.locator('body').innerText(), /Read-only archive inventory complete/);
    assert.match(await page.locator('body').innerText(), /Verified archive hash: [a-f0-9]{64}/);
    const records = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.equal(records.length, 1);
    assert.equal(records[0].status, 'analyzed');
    assert.equal(records[0].compatibility, 'unresolved');
    assert.equal(records[0].analysis_report.compatibility_state, 'unverified');
    assert.equal(records[0].analysis_report.install_authority.state, 'unsupported');
    assert.equal(records[0].stage, 'analyzed');
    assert.ok(records[0].bytes_downloaded > 0);
    assert.ok(!JSON.stringify(records).includes('secret=private'));
    assert.ok(!('inventory_path' in records[0]));
    await page.reload();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor();
    fs.mkdirSync(path.join(__dirname, '..', 'build'), { recursive: true });
    await page.screenshot({ path: path.join(__dirname, '..', 'build', 'ui-integration.png'), fullPage: true });
    assert.deepEqual(failures, []);
    console.log('actual-host UI integration passed (owned fixture + C++ analysis; no provider/device execution)');
  } finally {
    if (browser) await browser.close();
    child.kill('SIGTERM');
    await stopped;
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
