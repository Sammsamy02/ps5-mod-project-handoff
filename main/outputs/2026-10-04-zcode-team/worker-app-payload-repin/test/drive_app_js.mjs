// drive_app_js.mjs — Level-2 UI harness (worker-app-ui-flows).
// Loads the EXACT app.js bytes the real server serves, runs them in Node with
// a minimal DOM stub, and drives the Installed-tab flows end to end against
// the live server: real fetch with the real session cookie + CSRF + Origin.
// HONEST LIMIT: the DOM is a stub (no layout/visual rendering); this proves
// the real UI code paths and their real HTTP effects, not pixels.
import { writeFileSync } from 'node:fs';

const PORT = 62283, ORIGIN = `http://127.0.0.1:${PORT}`;
let cookie = null;
const checks = [];
const ok = (name, cond, detail = '') => {
  checks.push({ name, ok: !!cond, detail: String(detail).slice(0, 300) });
  console.log(`  ${cond ? 'ok  ' : 'FAIL'} ${name}${cond ? '' : ' :: ' + detail}`);
};
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
async function settle(needle, rootSel = '#installed-content', timeout = 9000) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeout) {
    if (globalThis.document.querySelector(rootSel).innerHTML.includes(needle)) return true;
    await sleep(60);
  }
  return false;
}

// --- fetch wrapper: injects the session cookie + Origin like a browser ------
const realFetch = globalThis.fetch;
globalThis.fetch = async (path, options = {}) => {
  const headers = new Headers(options.headers || {});
  if (cookie) headers.set('Cookie', 'mb_session=' + cookie);
  if ((options.method || 'GET') !== 'GET') headers.set('Origin', ORIGIN);
  const url = String(path).startsWith('http') ? String(path) : ORIGIN + path;
  const res = await realFetch(url, { ...options, headers });
  for (const c of (res.headers.getSetCookie?.() || [])) {
    const m = /mb_session=([A-Za-z0-9_-]{43})/.exec(c);
    if (m) cookie = m[1];
  }
  return res;
};

// --- minimal DOM stub ---------------------------------------------------------
class El {
  constructor(tag) {
    this.tagName = (tag || 'div').toUpperCase(); this.children = []; this.parentNode = null;
    this.attributes = {}; this.dataset = {}; this.listeners = {};
    this._innerHTML = ''; this.hidden = false; this.textContent = ''; this.value = '';
    this.disabled = false; this._class = ''; this.id = '';
    this.classList = {
      toggle: (c, force) => {
        const has = this.classList.contains(c);
        const want = force === undefined ? !has : force;
        if (want && !has) this._class = (this._class + ' ' + c).trim();
        if (!want) this._class = this._class.split(/\s+/).filter(x => x && x !== c).join(' ');
      },
      add: (c) => this.classList.toggle(c, true),
      remove: (c) => this.classList.toggle(c, false),
      contains: (c) => this._class.split(/\s+/).includes(c),
    };
  }
  get className() { return this._class; } set className(v) { this._class = String(v); }
  get isConnected() { let n = this; while (n.parentNode) n = n.parentNode; return n === docRoot; }
  setAttribute(k, v) {
    this.attributes[k] = String(v);
    if (k === 'id') this.id = String(v);
    if (k.startsWith('data-')) this.dataset[k.slice(5).replace(/-([a-z])/g, (_, c) => c.toUpperCase())] = String(v);
  }
  getAttribute(k) { return this.attributes[k] ?? null; }
  appendChild(c) { c.parentNode = this; this.children.push(c); return c; }
  before() {} after() {}
  addEventListener(t, fn) { (this.listeners[t] ||= []).push(fn); }
  focus() { docRoot.activeElement = this; }
  dispatch(t, ev = {}) { for (const fn of [...(this.listeners[t] || [])]) fn({ preventDefault() {}, target: this, ...ev }); }
  click() { this.dispatch('click'); }
  submit() { this.dispatch('submit'); }
  set innerHTML(html) {
    this._innerHTML = String(html); this.children = [];
    for (const el of parseHTML(this._innerHTML)) { el.parentNode = this; this.children.push(el); }
  }
  get innerHTML() { return this._innerHTML; }
  _all() { const out = []; for (const c of this.children) { out.push(c); out.push(...c._all()); } return out; }
  _match(compound) {
    const parts = compound.replace(/:not\([^)]*\)/g, '').match(/([#.][\w-]+|\[[^\]]+\]|[a-zA-Z][\w-]*)/g) || [];
    return parts.every(p => {
      if (p.startsWith('#')) return this.id === p.slice(1);
      if (p.startsWith('.')) return this.classList.contains(p.slice(1));
      if (p.startsWith('[')) {
        const m = /\[([\w-]+)(?:="([^"]*)")?\]/.exec(p);
        if (!m) return false;
        const val = this.attributes[m[1]];
        return m[2] === undefined ? val !== undefined : val === m[2];
      }
      return this.tagName === p.toUpperCase();
    });
  }
  querySelector(sel) { return this.querySelectorAll(sel)[0] || null; }
  querySelectorAll(sel) {
    const last = sel.trim().split(/\s+/).pop();
    return this._all().filter(el => el._match(last));
  }
}
const VOID = new Set(['input', 'br', 'img', 'hr', 'meta', 'link']);
function parseHTML(html) {
  const out = []; const stack = [];
  const re = /<\/?([a-zA-Z][\w-]*)((?:\s+[\w-]+(?:="[^"]*")?)*)\s*[^>]*?(\/?)>|([^<]+)/g;
  let m;
  while ((m = re.exec(html))) {
    if (m[4] !== undefined) continue;
    if (m[0].startsWith('</')) { stack.pop(); continue; }
    const el = new El(m[1]);
    const are = /([\w-]+)(?:="([^"]*)")?/g; let a;
    while ((a = are.exec(m[2] || ''))) el.setAttribute(a[1], a[2] ?? '');
    const parent = stack[stack.length - 1] || null;
    (parent ? parent.children : out).push(el);
    el.parentNode = parent || docRoot;
    if (m[3] !== '/' && !VOID.has(m[1].toLowerCase())) stack.push(el);
  }
  return out;
}
const docRoot = new El('div');
docRoot.activeElement = { dataset: {}, focus() {}, closest: () => null };
const registry = new Map();
function dom(sel) {
  const key = sel.trim().split(/\s+/).pop();
  if (registry.has(key)) return registry.get(key);
  const el = new El('div');
  const idm = /^#([\w-]+)$/.exec(key), clm = /^\.([\w-]+)$/.exec(key);
  if (idm) el.setAttribute('id', idm[1]);
  if (clm) el.className = clm[1];
  registry.set(key, el); return el;
}
const tabsAll = [];
for (const v of ['games', 'activity', 'installed']) {
  const t = new El('button'); t.className = 'tab'; t.setAttribute('data-view', v);
  t.setAttribute('id', 'tab-' + v); registry.set('#tab-' + v, t); tabsAll.push(t);
}
docRoot.children.push(...tabsAll);
docRoot.querySelector = (sel) => dom(sel);
docRoot.querySelectorAll = (sel) => {
  const last = sel.trim().split(/\s+/).pop();
  if (last === '.tab') return tabsAll;
  const found = docRoot._all().filter(el => el._match(last));
  return found.length ? found : (registry.has(last) ? [registry.get(last)] : []);
};
docRoot.createElement = (t) => new El(t);
const docListeners = {};
docRoot.addEventListener = (t, fn) => { (docListeners[t] ||= []).push(fn); };
const store = () => { const m = new Map(); return { getItem: k => m.has(k) ? m.get(k) : null, setItem: (k, v) => m.set(k, String(v)), removeItem: k => m.delete(k) }; };
globalThis.document = docRoot;
globalThis.sessionStorage = store(); globalThis.localStorage = store();
globalThis.location = { search: '', pathname: '/' };
globalThis.history = { replaceState() {} };
Object.defineProperty(globalThis, 'navigator', { value: { getGamepads: () => [] }, configurable: true });
globalThis.setInterval = () => 0;
globalThis.requestAnimationFrame = () => 0;

// --- load the REAL served app.js and run it ----------------------------------
const appRes = await realFetch(`${ORIGIN}/app.js`);
const appCode = await appRes.text();
ok('served app.js fetched', appRes.ok && appCode.includes('renderInstalled'), String(appCode.length));
new Function(appCode)();
for (const fn of [...(docListeners.DOMContentLoaded || [])]) await fn(); // runs start()

// --- flow 1: installed list renders real rows --------------------------------
ok('capabilities session issued', !!cookie, cookie || 'no cookie');
const tabInstalled = tabsAll.find(t => t.dataset.view === 'installed');
tabInstalled.click();
ok('installed tab renders install panel', await settle('install-heading'), docRoot.querySelector('#installed-content').innerHTML.slice(0, 150));
ok('installed list renders real seeded row', await settle('data-mod-toggle'), 'no rows');
let html = docRoot.querySelector('#installed-content').innerHTML;
ok('row shows name + confirmed transaction + sha256', html.includes('PPSA33286') && html.includes('Confirmed transaction') && /[0-9a-f]{16}/i.test(html), html.slice(0, 220));
ok('row shows enabled state + files + bytes meta', html.includes('Enabled in load list') && /file\(s\)/.test(html) && /bytes/.test(html));
ok('row has Disable + Uninstall controls', html.includes('>Disable</button>') && html.includes('>Uninstall</button>'), html.slice(0, 400));
ok('install form has title/id/path fields', ['install-title', 'install-mod-id', 'install-path'].every(id => html.includes(id)));

// --- flow 2: disable -> enable round trip ------------------------------------
let before = await (await fetch(`${ORIGIN}/api/installed?title=PPSA33286`)).json();
const rowId = before.items.find(i => i.transaction_confirmed).id;
docRoot.querySelector('#installed-content').querySelectorAll('[data-mod-toggle]')[0].click();
const disableOk = await settle('>Enable</button>');
ok('disable flow: UI shows Enable (disabled state)', disableOk && !docRoot.querySelector('#installed-content').innerHTML.includes('>Disable</button>'));
let mid = await (await fetch(`${ORIGIN}/api/installed?title=PPSA33286`)).json();
ok('disable flow: server enabled=false after UI click', mid.items.find(i => i.id === rowId)?.enabled === false, JSON.stringify(mid.items.find(i => i.id === rowId)));
docRoot.querySelector('#installed-content').querySelectorAll('[data-mod-toggle]')[0].click();
ok('enable flow: UI shows Disable again', await settle('>Disable</button>'));
let after = await (await fetch(`${ORIGIN}/api/installed?title=PPSA33286`)).json();
ok('enable flow: server enabled=true (round trip)', after.items.find(i => i.id === rowId)?.enabled === true);

// --- flow 3: uninstall with two-step confirm ---------------------------------
const armBtn = docRoot.querySelector('#installed-content').querySelectorAll('[data-mod-remove]').find(b => b.dataset.modRemove === rowId);
armBtn.click();
const armed = await settle('Confirm uninstall');
ok('uninstall arms with Confirm uninstall (single row)', armed && (docRoot.querySelector('#installed-content').innerHTML.match(/Confirm uninstall/g) || []).length === 1);
armBtn.click(); // second click on the same row confirms
ok('uninstall confirm: row leaves the tab (UI filter)',
  await settle('No confirmed installations') || !docRoot.querySelector('#installed-content').innerHTML.includes(`data-mod-remove="${rowId}"`));
const post = await (await fetch(`${ORIGIN}/api/installed?title=PPSA33286`)).json();
const prow = post.items.find(i => i.id === rowId);
ok('uninstall confirm: server no longer transaction_confirmed',
  !!prow && prow.transaction_confirmed === false && prow.state !== 'installed', JSON.stringify(prow));

// --- flow 4: install from archive via the form -------------------------------
dom('#install-title').value = 'PPSA33286';
dom('#install-mod-id').value = 'ui-flow-harness';
dom('#install-path').value = '/tmp/mb-ui-flows-fix/intake/ui-flow-mod.zip';
dom('#install-form').submit();
ok('install flow: busy/progress state rendered',
  docRoot.querySelector('#installed-content').innerHTML.includes('Installing') || dom('#install-status').textContent.length > 0, dom('#install-status').textContent);
ok('install flow: new row appears with controls', await settle('data-mod-toggle="ui-flow-harness"'));
const fin = await (await fetch(`${ORIGIN}/api/installed?title=PPSA33286`)).json();
const frow = fin.items.find(i => i.id === 'ui-flow-harness');
ok('install flow: server confirms new install (sha256 present)',
  frow?.transaction_confirmed === true && /^[0-9a-f]{64}$/.test(frow?.sha256 || ''), JSON.stringify(frow));

const pass = checks.filter(c => c.ok).length, fail = checks.length - pass;
console.log(`== HARNESS PASS=${pass} FAIL=${fail} ==`);
writeFileSync(new URL('../receipts/ui-flow-harness.json', import.meta.url).pathname,
  JSON.stringify({ worker: 'worker-app-payload-repin', date: '2026-10-04',
    mode: 'real served app.js bytes executed in a Node DOM stub against the live server (real session cookie + CSRF + Origin admission, real modmgr driver + installer binary, fixture tree)',
    pass, fail, checks, console_executed: false, provider_contact: false }, null, 1) + '\n');
process.exit(fail ? 1 : 0);
