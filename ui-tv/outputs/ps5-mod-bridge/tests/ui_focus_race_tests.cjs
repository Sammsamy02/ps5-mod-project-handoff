const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const {chromium} = require('playwright');
const web = path.join(__dirname, '..', 'web');
async function reach(page, selector, press) {
 const graph=new Map(),get=key=>{if(!graph.has(key))graph.set(key,{});return graph.get(key);};
 const observe=()=>page.evaluate(selector=>{const all=[...document.querySelectorAll('button,input,a[href],[tabindex]')],active=document.activeElement,target=[...document.querySelectorAll(selector)].find(el=>el.getClientRects().length&&!el.disabled&&!el.closest('[hidden]'));const p=el=>{const r=el?.getBoundingClientRect();return r?{x:r.x+r.width/2,y:r.y+r.height/2}:{x:0,y:0};};return{exists:!!target,matched:active===target,key:active.id||String(all.indexOf(active)),active:p(active),target:p(target)};},selector);
 for(let move=0;move<240;move++){
  const before=await observe();assert(before.exists,`Controller target missing: ${selector}`);if(before.matched)return;
  const dx=before.target.x-before.active.x,dy=before.target.y-before.active.y,order=Math.abs(dy)>Math.abs(dx)?[dy>=0?13:12,dx>=0?15:14]:[dx>=0?15:14,dy>=0?13:12];
  let edge=[...new Set([...order,13,15,12,14])].find(value=>!Object.hasOwn(get(before.key),value));
  if(edge===undefined){const queue=[{key:before.key,first:undefined}],seen=new Set([before.key]);while(queue.length&&edge===undefined){const node=queue.shift();if(Object.keys(get(node.key)).length<4){edge=node.first;break;}for(const[direction,next]of Object.entries(get(node.key)))if(!seen.has(next)){seen.add(next);queue.push({key:next,first:node.first??Number(direction)});}}}
  assert.notEqual(edge,undefined,`Controller cannot reach ${selector}`);await press(edge);get(before.key)[edge]=(await observe()).key;
 }
 assert.fail(`Controller reach exceeded its bound: ${selector}`);
}
async function main() {
 const server = http.createServer((req,res) => { const file=path.join(web, req.url==='/'?'index.html':req.url.slice(1)); if(!file.startsWith(web+path.sep)||!fs.existsSync(file)) return res.writeHead(404).end(); res.writeHead(200,{'Content-Type':file.endsWith('.html')?'text/html':file.endsWith('.css')?'text/css':'text/javascript'}).end(fs.readFileSync(file)); });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const origin=`http://127.0.0.1:${server.address().port}`;
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROMIUM_EXECUTABLE_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',args:['--no-sandbox']});
 let failures=0;
 try { for(const phase of ['mods','files']) {
  const ctx=await browser.newContext({viewport:{width:1920,height:1080}});
  let release, requested;
  const gate=new Promise(resolve=>release=resolve), started=new Promise(resolve=>requested=resolve);
  await ctx.addInitScript(()=>{window.pad={mapping:'standard',buttons:Array.from({length:17},()=>({pressed:false})),axes:[0,0]}; Object.defineProperty(navigator,'getGamepads',{value:()=>[window.pad]});});
  await ctx.route('**/*',async route=>{
   const u=new URL(route.request().url()); if(u.origin!==origin)return route.abort();
   const send=data=>route.fulfill({contentType:'application/json',body:JSON.stringify(data)});
   if(!u.pathname.startsWith('/api/'))return route.continue();
   if(u.pathname==='/api/capabilities')return send({csrf_token:'race-fixture',capabilities:{nexus_catalog:true,nexus_auth_start:true}});
   if(u.pathname==='/api/account')return send({status:'disconnected'});
   if(u.pathname==='/api/games')return send({items:[{name:'Game one',domain_name:'world'}]});
   if(u.pathname==='/api/games/world/mods'){if(phase==='mods'){requested();await gate;}return send({items:[{mod_id:1,name:'Readable menus'}]});}
   if(u.pathname==='/api/games/world/mods/1/files'){requested();await gate;return send({items:[{file_id:1,name:'Main file'}]});}
   return send({items:[],status:'unavailable'});
  });
  const page=await ctx.newPage();page.setDefaultTimeout(4000);await page.goto(origin);await page.locator('[data-card="game:world"]').waitFor();
  const press=async index=>{await page.evaluate(i=>{window.pad.buttons[i].pressed=true;},index);await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));await page.evaluate(i=>{window.pad.buttons[i].pressed=false;},index);await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));};
  try {
   await reach(page,'[data-card="game:world"]',press); assert.equal(await page.evaluate(()=>document.activeElement.dataset.card),'game:world');await press(0);
   if(phase==='files'){await page.locator('[data-card="mod:1"]').waitFor();await press(0);}
   await started;await page.waitForTimeout(80);
   if(phase==='mods'){await press(15);assert.equal(await page.evaluate(()=>document.activeElement.id),'catalog-sort');}
   else {await press(12);await press(15);assert.equal(await page.evaluate(()=>document.activeElement.id),'tab-activity');}
   const owned=await page.evaluate(()=>document.activeElement.id);release();
   await page.locator(phase==='mods'?'[data-card="mod:1"]':'[data-file-card="1"]').waitFor();await page.waitForTimeout(80);
   assert.equal(await page.evaluate(()=>document.activeElement.id),owned,`${phase} response stole intervening controller focus`);
   console.log('PASS delayed '+phase+' respects intervening controller focus');
  } catch(e){failures++;console.error('FAIL '+phase+': '+e.message);release();}
  await ctx.close();
 }} finally {await browser.close();await new Promise(resolve=>server.close(resolve));}
 assert.equal(failures,0);
}
main().catch(e=>{console.error(e.message);process.exitCode=1;});
