'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require('playwright');

// Production assets, owned loopback fixtures only. Every external request aborts.
const webRoot = path.join(__dirname, '..', 'web');
const token = n => n.toString(16).padStart(32, '0');
const observed = value => ({value, status: 'observed', source: 'owned_fixture'});
const reasonCodes = ['platform_provider_unavailable','provider_error','invalid_provider_capture',
  'inventory_resource_limit','source_changed','provider_observation_incomplete'];
const row = n => ({title_key: token(n + 1), title_id: observed(`Title_${n}`),
  display_name: observed(`星 Café 🎮 / \\ "${n}"`), build_version: observed('1.0'),
  build_fingerprint: {algorithm:'sha256', value:'a'.repeat(64), confidence:'exact_bytes',
    source:'owned_fixture', provenance:{method:'sha256_exact_bytes',
      byte_length:'9223372036854775807', complete:true, artifact_kind:'content'}}});
const pagesFor = (count = 25, batch = 8, generation = 'a'.repeat(32)) => {
  const pages = [], total = Math.max(1, Math.ceil(count / batch));
  for (let index = 0; index < total; index++) {
    const start = index * batch;
    pages.push({schema_version:2, kind:'title_inventory_page',
      snapshot:{status:'observed', complete:true, snapshot_id:generation,
        source_scope:'fixture.metadata', reason:null, issues:index ? [] : [{code:'provider_observation_incomplete'}]},
      page:{index, page_count:total, item_offset:start, total_count:count,
        next_cursor:index + 1 < total ? token(1000 + index) : null},
      items:Array.from({length:Math.min(batch, count - start)}, (_, n) => row(start + n))});
  }
  return pages;
};
// Independent fixture serializer: native JSON.stringify plus ASCII escaping.
const canonical = value => JSON.stringify(value, (_key, entry) => entry &&
  typeof entry === 'object' && !Array.isArray(entry) ?
  Object.fromEntries(Object.entries(entry).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)) : entry)
  .replace(/[\u007f-\uffff]/g, ch => '\\u' + ch.charCodeAt(0).toString(16).padStart(4, '0'));
const clone = value => JSON.parse(JSON.stringify(value));
const legacy = {schema_version:1,status:'observed',complete:true,snapshot_id:'b'.repeat(32),
  source_scope:'running_processes',reason:null,issues:[],items:[row(0)]};
const unavailable = {schema_version:1,status:'unavailable',complete:false,snapshot_id:null,
  source_scope:'none',reason:'platform_provider_unavailable',issues:[],items:[]};
function gate() { let release; const promise = new Promise(resolve => { release = resolve; }); return {promise,release}; }
async function until(condition, timeout = 2500) {
  const deadline = Date.now() + timeout;
  while (!condition()) { if (Date.now() >= deadline) throw new Error('Owned fixture condition timed out'); await new Promise(resolve => setTimeout(resolve, 10)); }
}

async function main() {
  const server = http.createServer((req, res) => {
    const name = req.url === '/' ? 'index.html' : req.url.slice(1);
    if (!['index.html','app.js','style.css'].includes(name)) return res.writeHead(404).end();
    res.writeHead(200, {'Content-Type':name.endsWith('.html') ? 'text/html' : name.endsWith('.css') ? 'text/css' : 'text/javascript'});
    res.end(fs.readFileSync(path.join(webRoot,name)));
  });
  await new Promise(resolve => server.listen(0,'127.0.0.1',resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const launch = {headless:true,args:['--no-sandbox']};
  if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch(launch);
  let passed = 0, failed = 0;
  const contexts = [];
  async function fixture(options = {}) {
    const context = await browser.newContext({viewport:{width:1440,height:1080}}); contexts.push(context);
    const api = {pages:pagesFor(), gets:[], posts:[], errors:[], outbound:[],
      confirmation:gate(),holdCurrent:false,paging:true,...options};
    await context.addInitScript(({streamMode}) => {
      window.fixtureClockOffset = 0;
      const actualNow = performance.now.bind(performance);
      Object.defineProperty(performance,'now',{value:() => actualNow() + window.fixtureClockOffset});
      window.fixtureAborts = [];
      const actualFetch = window.fetch.bind(window);
      window.fetch = async (...args) => {
        args[1]?.signal?.addEventListener('abort',() => window.fixtureAborts.push(String(args[0])),{once:true});
        const response = await actualFetch(...args);
        if (streamMode && new URL(String(args[0]),location.href).pathname === '/api/detected-titles/pages') {
          // Wrap only fixed owned fixture bytes, never a real provider response.
          const bytes = new Uint8Array(await response.arrayBuffer());
          let first = true;
          return new Response(new ReadableStream({
            pull(controller) {
              if (first) { first = false; controller.enqueue(bytes.subarray(0,bytes.length - 2)); window.fixturePartialStream = true; }
              else if (streamMode === 'error') controller.error(new Error('Owned late stream failure'));
            },
            cancel() { window.fixtureStreamCancelled = true; },
          }),{status:response.status,headers:response.headers});
        }
        return response;
      };
    },{streamMode:options.streamMode});
    if (options.savedRef) await context.addInitScript(ref => sessionStorage.setItem('modbridge.navigation.v1',JSON.stringify({target_ref:ref})),options.savedRef);
    await context.route('**/*', route => {
      const url = new URL(route.request().url());
      if (url.origin !== origin) { api.outbound.push(url.href); return route.abort(); }
      return route.continue();
    });
    await context.route(`${origin}/api/**`, async route => {
      const req = route.request(), url = new URL(req.url());
      const json = (value,status = 200) => route.fulfill({status,contentType:'application/json',body:JSON.stringify(value)});
      const bytes = (value,status = 200) => route.fulfill({status,contentType:'application/json',body:typeof value === 'string' ? value : canonical(value)});
      if (req.method() === 'GET') api.gets.push(url.pathname);
      else api.posts.push({path:url.pathname,body:req.postData(),headers:req.headers()});
      if (url.pathname === '/api/capabilities') return json({mode:api.preview ? 'console_preview' : 'desktop_preparation',csrf_token:'paging-fixture-csrf',
        capabilities:{title_inventory_paging:api.paging,detected_titles:!api.preview,nexus_catalog:!!api.catalog,
          account_status:false,jobs:!!api.catalog,installed_records:false,download_analyze:!!api.catalog,deploy:false}});
      if (url.pathname === '/api/detected-titles') return json(api.preview ? unavailable : legacy);
      if (url.pathname === '/api/detected-titles/refresh') return json(legacy);
      if (url.pathname === '/api/detected-titles/pages/refresh') {
        if (api.refreshedPages) api.pages = api.refreshedPages;
        return bytes(api.pages[0]);
      }
      if (url.pathname === '/api/detected-titles/pages') return bytes(api.rawFirst ?? api.pages[0]);
      if (/^\/api\/detected-titles\/pages\/current\/[a-f0-9]{32}$/.test(url.pathname)) {
        if (api.holdCurrent) { await api.confirmation.promise; api.holdCurrent = false; }
        const value = api.current ?? {schema_version:2,kind:'title_inventory_current',snapshot_id:api.pages[0].snapshot.snapshot_id,current:true};
        try { return await bytes(value,api.currentStatus ?? 200); } catch (_) { return; }
      }
      const continuation = /^\/api\/detected-titles\/pages\/([a-f0-9]{32})$/.exec(url.pathname);
      if (continuation) {
        const index = api.pages.findIndex(p => p.page?.next_cursor === continuation[1]) + 1;
        if (api.abortContinuation) return route.abort();
        if (api.continuationStatus) return bytes({code:'inventory_page_stale',error:'Title inventory page is stale.'},api.continuationStatus);
        if (api.beforeContinuation) await api.beforeContinuation();
        return bytes(api.rawContinuation ?? api.pages[index]);
      }
      if (url.pathname === '/api/games') return json({items:[{domain_name:'generic_world',id:81,name:'Generic catalog'}]});
      if (url.pathname === '/api/jobs' && req.method() === 'GET') return json({items:[]});
      if (url.pathname === '/api/jobs' && req.method() === 'POST') return json({id:'e'.repeat(32),status:'queued'},202);
      if (url.pathname === '/api/games/generic_world/mods/81') return json({item:{mod_id:81,game_domain:'generic_world',name:'Local fixture mod'}});
      if (url.pathname === '/api/games/generic_world/mods/81/files') return json({items:[{file_id:9,name:'Owned file',file_name:'owned.zip',version:'1.0',size_kb:1}]});
      return json({error:{code:'not_found',message:'Owned route unavailable.'}},404);
    });
    const page = await context.newPage(); page.setDefaultTimeout(2500);
    if (options.clock) await page.clock.install();
    page.on('pageerror', error => api.errors.push(error.message));
    await page.goto(origin);
    await page.getByRole('heading',{name:'Console target',exact:true}).waitFor();
    return {page,api,context};
  }
  async function test(name, action) {
    const before = contexts.length;
    try { await action(); passed++; console.log(`PASS ${name}`); }
    catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
    finally { for (const context of contexts.slice(before)) await context.close(); }
  }
  async function complete(page,count) {
    await page.waitForFunction(expected => document.querySelectorAll('[data-title-key]:not(:disabled)').length === expected,count);
  }
  async function noCommit(page,api) {
    await page.waitForFunction(() => !document.querySelector('#title-status').textContent.includes('Checking'));
    assert.equal(await page.locator('[data-title-key]:not(:disabled)').count(),0);
    assert.match(await page.locator('#selected-target').innerText(),/No console target/);
    assert.equal(await page.locator('[data-title-key]').count(),0);
    assert.deepEqual(api.outbound,[]); assert.deepEqual(api.errors,[]);
    assert.equal(api.posts.filter(p => p.path.endsWith('/refresh')).length,0);
  }
  try {
    await test('25 generic Unicode titles commit only after all pages and current confirmation',async () => {
      const savedRef = {snapshot_id:'a'.repeat(32),title_key:token(1)};
      const {page,api} = await fixture({holdCurrent:true,savedRef});
      await until(() => api.gets.some(p => p.includes('/pages/current/')));
      assert.equal(await page.locator('[data-title-key]').count(),0);
      assert.match(await page.locator('#selected-target').innerText(),/No console target/);
      api.confirmation.release(); await complete(page,25);
      assert.match(await page.locator('#selected-target').innerText(),/Title_0/);
      assert.match(await page.locator('#title-list').innerText(),/星 Café 🎮/);
      assert.equal(api.gets.filter(p => p.startsWith('/api/detected-titles/')).length,5);
      assert.equal(api.gets.includes('/api/detected-titles'),false);
      assert.deepEqual(api.outbound,[]); assert.deepEqual(api.errors,[]);
    });
    await test('refresh stages old rows read-only and blocks target-bearing submission',async () => {
      const {page,api} = await fixture({catalog:true,pages:pagesFor(1)}); await complete(page,1);
      await page.locator('[data-title-key]').click();
      await page.getByLabel('Nexus domain',{exact:true}).fill('generic_world');
      await page.getByLabel('Mod ID',{exact:true}).fill('81');
      await page.getByRole('button',{name:'Find mod',exact:true}).click();
      const action = page.getByRole('button',{name:'Download for analysis',exact:true}); await action.waitFor();
      api.holdCurrent = true;
      await page.locator('#reload-titles').click();
      await until(() => api.posts.some(p => p.path === '/api/detected-titles/pages/refresh'));
      assert.equal(await page.locator('[data-title-key]:not(:disabled)').count(),0);
      assert.equal(await action.isDisabled(),true);
      await action.evaluate(button => button.dispatchEvent(new Event('click',{bubbles:true})));
      assert.equal(api.posts.some(p => p.path === '/api/jobs'),false);
      api.confirmation.release(); await complete(page,1);
      assert.deepEqual(JSON.parse(api.posts[0].body),{});
      assert.equal(api.posts[0].headers['x-csrf-token'],'paging-fixture-csrf');
    });
    const mutations = [
      ['cross-page duplicate key',ps => {ps[1].items[0].title_key = ps[0].items[0].title_key;}],
      ['changed generation',ps => {ps[1].snapshot.snapshot_id = 'b'.repeat(32);}],
      ['changed source scope',ps => {ps[1].snapshot.source_scope = 'another.scope';}],
      ['wrong offset',ps => {ps[1].page.item_offset = 7;}],
      ['changed total',ps => {ps[1].page.total_count = 18;}],
      ['cursor loop',ps => {ps[1].page.next_cursor = ps[0].page.next_cursor;}],
      ['early terminal cursor',ps => {ps[0].page.next_cursor = null;}],
      ['unknown issue code',ps => {ps[0].snapshot.issues = [{code:'fixture_private_code'}];}],
      ['malformed late text',ps => {ps[1].items[7].display_name.value = 'late\x00bad';}],
      ['wrong kind',ps => {ps[0].kind = 'title_inventory_current';}],
    ];
    for (const [name,mutate] of mutations) await test(`${name} discards whole staged capture`,async () => {
      const pages = pagesFor(17); mutate(pages);
      const {page,api} = await fixture({pages}); await until(() => api.gets.includes('/api/detected-titles/pages'));
      await noCommit(page,api); assert.equal(api.gets.some(p => p.includes('/pages/current/')),false);
    });
    await test('identical duplicate object key rejects before any publication',async () => {
      const rawFirst = canonical(pagesFor(1)[0]).replace('"schema_version":2','"schema_version":2,"schema_version":2');
      const {page,api} = await fixture({pages:pagesFor(1),rawFirst}); await until(() => api.gets.includes('/api/detected-titles/pages'));
      await noCommit(page,api);
    });
    await test('oversize body cancels bounded read without publication or retry',async () => {
      const {page,api} = await fixture({rawFirst:' '.repeat(8192)}); await until(() => api.gets.includes('/api/detected-titles/pages'));
      await noCommit(page,api); assert.equal(api.gets.filter(p => p.includes('/detected-titles/')).length,1);
    });
    await test('late body stream failure discards partial bytes without publication',async () => {
      const {page,api} = await fixture({streamMode:'error',pages:pagesFor(1)});
      await page.waitForFunction(() => window.fixturePartialStream === true);
      await noCommit(page,api); assert.equal(api.gets.filter(p => p.includes('/detected-titles/')).length,1);
      assert.doesNotMatch(await page.locator('#status-banner').innerText(),/Owned late stream failure/);
    });
    await test('per-request deadline cancels a stalled partial stream',async () => {
      const {page,api} = await fixture({streamMode:'stall',pages:pagesFor(1),clock:true});
      await page.waitForFunction(() => window.fixturePartialStream === true);
      assert.equal(await page.locator('[data-title-key]').count(),0);
      await page.clock.fastForward(5001);
      await noCommit(page,api);
      await page.waitForFunction(() => window.fixtureStreamCancelled === true);
      assert.match(await page.locator('#status-banner').innerText(),/timed out/i);
      assert.equal(api.gets.filter(p => p.includes('/detected-titles/')).length,1);
    });
    await test('canonical stale 409 and late network failure do not auto refresh',async () => {
      for (const options of [{continuationStatus:409},{abortContinuation:true}]) {
        const {page,api} = await fixture(options); await until(() => api.gets.filter(p => p.includes('/detected-titles/')).length >= 2);
        await noCommit(page,api); assert.equal(api.gets.filter(p => p.includes('/detected-titles/')).length,2);
      }
    });
    await test('final current mismatch or duplicate confirmation rejects all rows',async () => {
      for (const current of [
        {schema_version:2,kind:'title_inventory_current',snapshot_id:'b'.repeat(32),current:true},
        '{"current":true,"current":true,"kind":"title_inventory_current","schema_version":2,"snapshot_id":"'+'a'.repeat(32)+'"}',
      ]) {
        const {page,api} = await fixture({current}); await until(() => api.gets.some(p => p.includes('/pages/current/')));
        await noCommit(page,api);
      }
    });
    await test('aggregate deadline expires before final commit',async () => {
      const {page,api} = await fixture({holdCurrent:true}); await until(() => api.gets.some(p => p.includes('/pages/current/')));
      await page.evaluate(() => {window.fixtureClockOffset = 16000;}); api.confirmation.release();
      await noCommit(page,api);
    });
    await test('new epoch aborts old confirmation and late old response cannot overwrite',async () => {
      const {page,api} = await fixture({holdCurrent:true,pages:pagesFor(1),refreshedPages:pagesFor(2,8,'b'.repeat(32))});
      await until(() => api.gets.some(p => p.includes('/pages/current/')));
      // A new internal refresh can occur during a stale-job recovery; force the
      // bound handler while its physical button is disabled to cover that seam.
      api.holdCurrent = false;
      await page.locator('#reload-titles').evaluate(button => button.dispatchEvent(new Event('click',{bubbles:true})));
      await complete(page,2); api.confirmation.release(); await page.waitForTimeout(50);
      assert.equal(await page.locator('[data-title-key]').count(),2);
      assert(await page.evaluate(() => window.fixtureAborts.some(p => p.includes('/pages/current/'))));
      assert.deepEqual(api.outbound,[]); assert.deepEqual(api.errors,[]);
    });
    await test('128 one-item pages use exactly 129 attempts including confirmation',async () => {
      const {page,api} = await fixture({pages:pagesFor(128,1)}); await complete(page,128);
      assert.equal(api.gets.filter(p => p.includes('/detected-titles/')).length,129);
      assert.deepEqual(api.outbound,[]); assert.deepEqual(api.errors,[]);
    });
    await test('complete empty and terminal unavailable have no selectable prefix',async () => {
      const empty = await fixture({pages:pagesFor(0)}); await until(() => empty.api.gets.some(p => p.includes('/pages/current/')));
      await empty.page.waitForFunction(() => document.querySelector('#title-status').textContent.startsWith('No observed'));
      assert.equal(await empty.page.locator('[data-title-key]').count(),0);
      const terminal = pagesFor(0)[0]; terminal.snapshot = {...terminal.snapshot,status:'unavailable',complete:false,snapshot_id:null,reason:reasonCodes[0],issues:[]}; terminal.page = null;
      const result = await fixture({pages:[terminal]}); await until(() => result.api.gets.includes('/api/detected-titles/pages'));
      await noCommit(result.page,result.api); assert.equal(result.api.gets.some(p => p.includes('/pages/current/')),false);
    });
    await test('v1 fallback and native unavailable preview never request v2 routes',async () => {
      const old = await fixture({paging:false}); await complete(old.page,1);
      assert(old.api.gets.includes('/api/detected-titles'));
      assert.equal(old.api.gets.some(p => p.includes('/pages')),false);
      const preview = await fixture({paging:false,preview:true}); await until(() => preview.api.gets.includes('/api/detected-titles'));
      await preview.page.locator('#title-status').getByText(/unavailable/i).waitFor();
      assert.equal(preview.api.gets.some(p => /\/pages|\/api\/(?:account|games|jobs|installed)/.test(p)),false);
      assert.equal(await preview.page.locator('#reload-titles').isHidden(),true);
      assert.deepEqual(preview.api.outbound,[]); assert.deepEqual(preview.api.errors,[]);
    });
  } finally { await browser.close(); await new Promise(resolve => server.close(resolve)); }
  console.log(JSON.stringify({passed,failed})); if (failed) process.exitCode = 1;
}
main().catch(error => {console.error(error);process.exitCode=1;});
