const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');
const web = path.join(__dirname, '../web');
const good = () => ({ok:true,result:{mods:['downloaded','prepared','installed'].map((state,i)=>({download_id:`dl-${i}`,title_id:'PPSA12345',state,source:{name:`Owned ${state} <img src=x onerror=alert(1)>`},...(state==='prepared'?{blockers:[{code:'blocked',message:'<script>unsafe</script>'}]}:{})})),games:[{title_id:'PPSA12345',installed:{mods:['p-1'],generation:1,load_list_sha256:'a'.repeat(64)},runtime:{state:'mods-started',pid:1234,detail:'<b>literal</b>'}}]}});
(async()=>{
 const server=http.createServer((req,res)=>{const name=req.url==='/'?'index.html':req.url.slice(1); if(!['index.html','style.css','app.js'].includes(name))return res.writeHead(404).end();res.setHeader('Content-Type',name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':'text/html');res.end(fs.readFileSync(path.join(web,name)));});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin=`http://127.0.0.1:${server.address().port}`;
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROMIUM_EXECUTABLE_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',args:['--no-sandbox']});let passed=0;
 async function fixture(options={}){const context=await browser.newContext();const api={cap:true,contract:'modforge-service/1',body:good(),gets:[],posts:[],outbound:[],errors:[],...options};
 await context.route('**/*',route=>new URL(route.request().url()).origin===origin?route.continue():(api.outbound.push(route.request().url()),route.abort()));
 await context.route(`${origin}/api/**`,route=>{const req=route.request(),url=new URL(req.url());api[req.method()==='GET'?'gets':'posts'].push(url.pathname);const reply=(body,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
 if(url.pathname==='/api/capabilities')return reply({capabilities:{mod_service_records:api.cap,installed_records:api.legacy||false}});
 if(url.pathname==='/api/mod-service')return reply({ok:true,result:{contract:api.contract}});
 if(url.pathname==='/api/mod-service/records')return api.disconnect?route.abort():reply(api.body);
 if(url.pathname==='/api/installed')return reply({items:[{name:'Legacy confirmed',sha256:'a'.repeat(64),transaction_confirmed:true}]});
 return reply({items:[]});});const page=await context.newPage();page.on('pageerror',e=>api.errors.push(e.message));await page.goto(origin);await page.locator('#tab-installed').click();await page.waitForTimeout(150);return {page,api,context};}
 async function test(name,fn){await fn();passed++;console.log(`PASS ${name}`);}
 try{
 await test('capability off performs no service requests',async()=>{const {api,context}=await fixture({cap:false});assert(!api.gets.some(x=>x.startsWith('/api/mod-service')));await context.close();});
 await test('legacy confirmed records remain supported',async()=>{const {page,api,context}=await fixture({cap:false,legacy:true});assert.match(await page.locator('#installed-content').innerText(),/Legacy confirmed/);assert(api.gets.includes('/api/installed'));await context.close();});
 await test('wrong contract never requests or accepts records',async()=>{const {page,api,context}=await fixture({contract:'modforge-service/2'});assert(!api.gets.includes('/api/mod-service/records'));assert.match(await page.locator('#installed-content').innerText(),/Unsupported.*contract/);await context.close();});
 await test('mixed states render distinctly and all source strings escape',async()=>{const {page,api,context}=await fixture();const text=await page.locator('#installed-content').innerText();for(const state of ['Downloaded','Prepared','Installed'])assert(text.includes(state));assert(text.includes('<img src=x onerror=alert(1)>'));assert.equal(await page.locator('#installed-content img,#installed-content script,#installed-content b').count(),0);assert.deepEqual(api.posts,[]);assert.deepEqual(api.outbound,[]);assert.deepEqual(api.errors,[]);await context.close();});
 await test('installed and mods-started with pid do not claim running',async()=>{const {page,context}=await fixture();const text=await page.locator('#installed-content').innerText();assert.match(text,/Runtime report: mods-started/);assert.match(text,/Live game process unverified/);assert(!/\brunning\b/i.test(text));await context.close();});
 for(const [name,mutate] of [['unknown mod state',body=>body.result.mods[0].state='running'],['unknown runtime state',body=>body.result.games[0].runtime.state='unknown'],['malformed records',body=>body.result.mods[0].source=null],['malformed envelope',body=>body.ok=false]])await test(`${name} refuses records and clears stale status`,async()=>{const {page,api,context}=await fixture();mutate(api.body);await page.locator('#tab-games').click();await page.locator('#tab-installed').click();await page.getByText('Service records unavailable',{exact:true}).waitFor();assert(!await page.locator('#installed-content').getByText('Installed',{exact:true}).count());await context.close();});
 await test('disconnect clears stale records and error data escapes',async()=>{const {page,api,context}=await fixture();api.body={ok:false,error:{message:'<img src=x onerror=alert(1)>'}};await page.locator('#tab-games').click();await page.locator('#tab-installed').click();await page.getByText('Service records unavailable',{exact:true}).waitFor();assert.equal(await page.locator('#installed-content img').count(),0);api.disconnect=true;await page.locator('#tab-games').click();await page.locator('#tab-installed').click();await page.waitForTimeout(150);assert(!/Owned installed/.test(await page.locator('#installed-content').innerText()));await context.close();});
 console.log(`${passed} service records UI tests passed`);
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(error=>{console.error(error);process.exitCode=1;});
