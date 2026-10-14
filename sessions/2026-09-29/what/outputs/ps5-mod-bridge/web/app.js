(() => {
  'use strict';
  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const state = {
    csrf: '', capabilities: {}, account: null, view: 'games', games: [], mods: [], files: [], jobs: [], installed: [],
    game: null, mod: null, file: null, requests: { games: 0, mods: 0, files: 0, jobs: 0, installed: 0, titles: 0, lookup: 0 },
    titles: null, titlesLoading: false, titlesController: null, targetRef: null, lookupLoading: false,
    busy: false, jobsLoading: false, pendingIntent: null, modalReturnFocus: null, toastTimer: null, gamepadTimer: null,
  };
  const navKey = 'modbridge.navigation.v1';
  const cleanRecord = (value) => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
  const safeText = (value, fallback = 'Details unavailable') => typeof value === 'string' && value.trim() ? value : fallback;
  const esc = (value) => String(value).replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[ch]);
  const idValue = (value) => {
    if (typeof value === 'number') return Number.isSafeInteger(value) && value > 0 ? String(value) : '';
    if (typeof value !== 'string' || !/^[1-9][0-9]{0,18}$/.test(value)) return '';
    const maxSafe = String(Number.MAX_SAFE_INTEGER);
    return value.length < maxSafe.length || (value.length === maxSafe.length && value <= maxSafe) ? value : '';
  };
  const domainValue = value => typeof value === 'string' && /^[a-z0-9][a-z0-9_-]{0,79}$/.test(value) ? value : '';
  const opaqueValue = value => typeof value === 'string' && /^[a-f0-9]{32}$/.test(value) ? value : '';
  const targetReference = value => {
    const record = cleanRecord(value);
    return Object.keys(record).length === 2 && opaqueValue(record.snapshot_id) && opaqueValue(record.title_key)
      ? { snapshot_id: record.snapshot_id, title_key: record.title_key } : null;
  };
  const sameTarget = (left, right) => left?.snapshot_id === right?.snapshot_id && left?.title_key === right?.title_key;
  const endpoint = async (path, options = {}) => {
    const headers = new Headers(options.headers || {});
    if (options.body !== undefined) headers.set('Content-Type', 'application/json');
    if (options.method && options.method !== 'GET') headers.set('X-CSRF-Token', state.csrf);
    const response = await fetch(path, { credentials: 'same-origin', ...options, headers });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      const error = new Error(safeText(data?.error?.message, `Request failed (${response.status})`));
      error.status = response.status;
      error.code = typeof data?.error?.code === 'string' && /^[a-z_]{1,64}$/.test(data.error.code) ? data.error.code : 'request_failed';
      if (Object.keys(cleanRecord(data)).length === 2 && typeof data.code === 'string' && typeof data.error === 'string' && Object.hasOwn(titleMessages, data.code)) {
        error.code = data.code; error.message = titleMessages[data.code];
      }
      throw error;
    }
    return data;
  };
  const saveNavigation = () => {
    const selection = { domain: state.game?.domain_name || '', mod: state.mod?.mod_id || '', file: state.file?.file_id || '' };
    if (state.targetRef) selection.target_ref = state.targetRef;
    try { sessionStorage.setItem(navKey, JSON.stringify(selection)); } catch (_) {}
  };
  const restoreNavigation = () => {
    try {
      const saved = cleanRecord(JSON.parse(sessionStorage.getItem(navKey) || '{}'));
      return { domain: domainValue(saved.domain), mod: idValue(saved.mod), file: idValue(saved.file), target_ref: targetReference(saved.target_ref) };
    } catch (_) { return { domain: '', mod: '', file: '' }; }
  };
  const showToast = (message) => {
    const toast = $('#toast'); toast.textContent = message; toast.hidden = false;
    clearTimeout(state.toastTimer); state.toastTimer = setTimeout(() => { toast.hidden = true; }, 3600);
  };
  const showError = (message) => {
    const banner = $('#status-banner'); banner.textContent = message; banner.hidden = false;
  };
  const clearError = () => { $('#status-banner').hidden = true; };
  // Adapted from the frozen Python/JS canonical-page experiment. V2 issues
  // now use the producer's closed six-code allowlist; no page is selectable.
  const titlePageCodec = (() => {
    const WIRE_LIMIT = 8191;
    const MAX_VALUES_AND_KEYS = 2048;
    const MAX_DEPTH = 8;
    const INVALID = 'invalid_title_page';
    const LIMIT = 'title_page_resource_limit';
    const TOKEN = /^[a-f0-9]{32}$/;
    const TITLE_ID = /^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$/;
    const SOURCE = /^[A-Za-z0-9][A-Za-z0-9_.:-]{0,63}$/;
    const SCOPE = /^[a-z0-9][a-z0-9_.-]{0,47}$/;
    const ISSUE_CODE = /^[a-z][a-z0-9_]{0,63}$/;
    const MAX_SIGNED63 = '9223372036854775807';
    const TERMINAL_REASONS = new Set([
      'platform_provider_unavailable', 'provider_error', 'invalid_provider_capture',
      'inventory_resource_limit', 'source_changed', 'provider_observation_incomplete',
    ]);

    function fail(reason = INVALID) { throw reason; }
    function object(value, keys) {
      if (value === null || typeof value !== 'object' || Array.isArray(value) ||
          Object.getPrototypeOf(value) !== Object.prototype) fail();
      const actual = Object.keys(value);
      if (actual.length !== keys.length || keys.some(key => !Object.hasOwn(value, key))) fail();
    }
    function token(value) { return typeof value === 'string' && TOKEN.test(value); }
    function counter(value, lower, upper) {
      if (!Number.isSafeInteger(value) || Object.is(value, -0) || value < lower || value > upper) fail();
    }

    function treeBound(root) {
      const stack = [{value: root, depth: 1}];
      let count = 0;
      while (stack.length) {
        const {value, depth} = stack.pop();
        if (depth > MAX_DEPTH || ++count > MAX_VALUES_AND_KEYS) fail(LIMIT);
        if (value === null || typeof value !== 'object') continue;
        const keys = Object.keys(value);
        if (!Array.isArray(value)) count += keys.length;
        if (count + stack.length + keys.length > MAX_VALUES_AND_KEYS) fail(LIMIT);
        for (const key of keys) stack.push({value: value[key], depth: depth + 1});
      }
    }

    // Python str.strip() edge-space set, not JavaScript trim() (notably U+0085/U+FEFF).
    function edgeSpace(code) {
      return (code >= 9 && code <= 13) || (code >= 0x1c && code <= 0x20) ||
        code === 0x85 || code === 0xa0 || code === 0x1680 ||
        (code >= 0x2000 && code <= 0x200a) || code === 0x2028 || code === 0x2029 ||
        code === 0x202f || code === 0x205f || code === 0x3000;
    }
    function text(value, byteLimit, pattern = null) {
      if (typeof value !== 'string' || value.length === 0 ||
          edgeSpace(value.charCodeAt(0)) || edgeSpace(value.charCodeAt(value.length - 1))) fail();
      let bytes = 0;
      for (let index = 0; index < value.length; index += 1) {
        const code = value.charCodeAt(index);
        if (code < 32 || code === 127) fail();
        if (code >= 0xd800 && code <= 0xdbff) {
          const low = value.charCodeAt(++index);
          if (!(low >= 0xdc00 && low <= 0xdfff)) fail();
          bytes += 4;
        } else if (code >= 0xdc00 && code <= 0xdfff) fail();
        else bytes += code < 0x80 ? 1 : code < 0x800 ? 2 : 3;
        if (bytes > byteLimit) fail();
      }
      if (pattern !== null && !pattern.test(value)) fail();
    }
    function metadata(value, required, byteLimit, pattern = null) {
      object(value, ['value', 'status', 'source']);
      if (value.status === 'observed') {
        text(value.value, byteLimit, pattern);
        text(value.source, 64, SOURCE);
      } else if (required || !['unavailable', 'error'].includes(value.status) ||
                 value.value !== null || value.source !== null) fail();
    }
    function fingerprint(value) {
      if (value === null) return;
      object(value, ['algorithm', 'value', 'confidence', 'source', 'provenance']);
      if (value.algorithm !== 'sha256' || typeof value.value !== 'string' ||
          !/^[a-f0-9]{64}$/.test(value.value) || value.confidence !== 'exact_bytes') fail();
      text(value.source, 64, SOURCE);
      const provenance = value.provenance;
      object(provenance, ['method', 'byte_length', 'complete', 'artifact_kind']);
      const length = provenance.byte_length;
      if (provenance.method !== 'sha256_exact_bytes' || provenance.complete !== true ||
          !['executable', 'content'].includes(provenance.artifact_kind) ||
          typeof length !== 'string' || !/^[1-9][0-9]{0,18}$/.test(length) ||
          (length.length === 19 && length > MAX_SIGNED63)) fail();
    }
    function validatePage(value) {
      treeBound(value);
      object(value, ['schema_version', 'kind', 'snapshot', 'page', 'items']);
      if (value.schema_version !== 2 || value.kind !== 'title_inventory_page') fail();
      const snapshot = value.snapshot;
      object(snapshot, ['status', 'complete', 'snapshot_id', 'source_scope', 'reason', 'issues']);
      text(snapshot.source_scope, 48, SCOPE);
      if (!Array.isArray(snapshot.issues) || snapshot.issues.length > 32) fail(LIMIT);
      for (const issue of snapshot.issues) {
        object(issue, ['code']);
        text(issue.code, 64, ISSUE_CODE);
        if (!TERMINAL_REASONS.has(issue.code)) fail();
      }
      if (!Array.isArray(value.items) || value.items.length > 8) fail(LIMIT);
      if (snapshot.status !== 'observed') {
        if (!['partial', 'unavailable', 'error'].includes(snapshot.status) ||
            snapshot.complete !== false || snapshot.snapshot_id !== null ||
            !TERMINAL_REASONS.has(snapshot.reason) || value.page !== null || value.items.length !== 0) fail();
        return;
      }
      if (snapshot.complete !== true || !token(snapshot.snapshot_id) || snapshot.reason !== null) fail();
      const page = value.page;
      object(page, ['index', 'page_count', 'item_offset', 'total_count', 'next_cursor']);
      counter(page.page_count, 1, 128);
      counter(page.index, 0, page.page_count - 1);
      counter(page.item_offset, 0, 128);
      counter(page.total_count, 0, 128);
      if (page.index !== 0 && snapshot.issues.length !== 0) fail();
      if (page.total_count === 0) {
        if (page.page_count !== 1 || page.index !== 0 || page.item_offset !== 0 ||
            value.items.length !== 0 || page.next_cursor !== null) fail();
      } else {
        const end = page.item_offset + value.items.length;
        const last = page.index + 1 === page.page_count;
        if (value.items.length === 0 || page.page_count > page.total_count ||
            (page.index === 0 && page.item_offset !== 0) ||
            page.item_offset < page.index || page.item_offset > page.index * 8 ||
            end > page.total_count ||
            (last ? page.next_cursor !== null || end !== page.total_count :
              !token(page.next_cursor) || end >= page.total_count)) fail();
        const remainingPages = page.page_count - page.index - 1;
        const remainingItems = page.total_count - end;
        if (remainingItems < remainingPages || remainingItems > remainingPages * 8) fail();
      }
      const keys = new Set();
      for (const record of value.items) {
        object(record, ['title_key', 'title_id', 'display_name', 'build_version', 'build_fingerprint']);
        if (!token(record.title_key) || keys.has(record.title_key)) fail();
        keys.add(record.title_key);
        metadata(record.title_id, true, 64, TITLE_ID);
        metadata(record.display_name, false, 256);
        metadata(record.build_version, false, 128);
        fingerprint(record.build_fingerprint);
      }
    }

    function asciiString(value) {
      let out = '"';
      for (let index = 0; index < value.length; index += 1) {
        const code = value.charCodeAt(index);
        if (code === 34) out += '\\"';
        else if (code === 92) out += '\\\\';
        else if (code === 8) out += '\\b';
        else if (code === 9) out += '\\t';
        else if (code === 10) out += '\\n';
        else if (code === 12) out += '\\f';
        else if (code === 13) out += '\\r';
        else if (code >= 32 && code <= 126) out += value[index];
        else out += '\\u' + code.toString(16).padStart(4, '0');
      }
      return out + '"';
    }
    function encode(value) {
      if (value === null) return 'null';
      if (typeof value === 'string') return asciiString(value);
      if (typeof value === 'boolean') return value ? 'true' : 'false';
      if (typeof value === 'number') return String(value);
      if (Array.isArray(value)) return '[' + value.map(encode).join(',') + ']';
      return '{' + Object.keys(value).sort().map(key => asciiString(key) + ':' + encode(value[key])).join(',') + '}';
    }
    function canonicalAsciiTitlePage(value) {
      try {
        validatePage(value);
        const encoded = encode(value);
        if (encoded.length > WIRE_LIMIT) fail(LIMIT);
        return encoded;
      } catch (reason) { throw new TypeError(reason === LIMIT ? LIMIT : INVALID); }
    }
    function normalizeTitlePage(bytes) {
      try {
        if (!(bytes instanceof Uint8Array) || bytes.length === 0) fail();
        if (bytes.length > WIRE_LIMIT) fail(LIMIT);
        let raw = '';
        for (const byte of bytes) {
          if (byte > 127) fail(); // canonical ASCII is a strict UTF-8 subset
          raw += String.fromCharCode(byte);
        }
        const page = JSON.parse(raw);
        validatePage(page);
        const canonical = encode(page);
        if (canonical.length > WIRE_LIMIT) fail(LIMIT);
        if (canonical !== raw) fail(); // includes duplicate/escaped keys and alternate forms
        return {ok: true, reason: null, page, selectable: false};
      } catch (reason) {
        return {ok: false, reason: reason === LIMIT ? LIMIT : INVALID, page: null, selectable: false};
      }
    }
    return {normalizeTitlePage, encode};
  })();
  const titleMessages = Object.freeze({
    inventory_page_stale: 'The title inventory changed.',
    inventory_refresh_busy: 'A title inventory refresh is already in progress.',
    inventory_capture_in_progress: 'The title inventory is still being captured.',
    invalid_input: 'The title inventory request could not be verified.',
    session_required: 'The local app session has expired.',
    invalid_title_page: 'The title inventory response could not be verified.',
    title_page_resource_limit: 'The title inventory exceeds the response limits.',
    title_page_timeout: 'The title inventory request timed out.',
    title_page_cancelled: 'The title inventory request was cancelled.',
    request_failed: 'The title inventory request failed.',
  });
  const titleError = (code, status = 0) => {
    const error = new Error(titleMessages[code] || titleMessages.request_failed);
    error.code = Object.hasOwn(titleMessages, code) ? code : 'request_failed';
    error.status = status;
    return error;
  };
  const titleGuard = ledger => {
    if (ledger.controller.signal.aborted) throw titleError('title_page_cancelled');
    if (performance.now() >= ledger.deadline) throw titleError('title_page_timeout');
  };
  const waitForTitle = (promise, signal) => new Promise((resolve, reject) => {
    const abort = () => reject(titleError('title_page_cancelled'));
    if (signal.aborted) { reject(titleError('title_page_cancelled')); return; }
    signal.addEventListener('abort', abort, { once: true });
    Promise.resolve(promise).then(resolve, reject).finally(() => signal.removeEventListener('abort', abort));
  });
  const titleAsciiObject = bytes => {
    let raw = '';
    for (const byte of bytes) { if (byte > 127) throw titleError('invalid_title_page'); raw += String.fromCharCode(byte); }
    try { return { raw, value: JSON.parse(raw) }; }
    catch (_) { throw titleError('invalid_title_page'); }
  };
  const titleExact = (value, fields) => {
    if (!value || typeof value !== 'object' || Array.isArray(value) || Object.keys(value).length !== fields.length || fields.some(field => !Object.hasOwn(value, field))) throw titleError('invalid_title_page');
  };
  const readTitleResponse = async (path, ledger, refresh = false) => {
    titleGuard(ledger);
    if (++ledger.requests > 129) throw titleError('title_page_resource_limit');
    const controller = new AbortController(), abort = () => controller.abort();
    const remaining = ledger.deadline - performance.now();
    let timedOut = false, finished = false, reader = null;
    const timer = setTimeout(() => { timedOut = true; controller.abort(); }, Math.min(5000, remaining));
    ledger.controller.signal.addEventListener('abort', abort, { once: true });
    try {
      const headers = new Headers({ Accept: 'application/json' });
      if (refresh) { headers.set('Content-Type', 'application/json'); headers.set('X-CSRF-Token', state.csrf); }
      const response = await waitForTitle(fetch(path, { credentials: 'same-origin', cache: 'no-store', signal: controller.signal, headers, ...(refresh ? { method: 'POST', body: '{}' } : {}) }), controller.signal);
      titleGuard(ledger);
      if (!response.body || typeof response.body.getReader !== 'function') throw titleError('invalid_title_page');
      reader = response.body.getReader();
      const buffer = new Uint8Array(8191);
      let size = 0;
      while (true) {
        titleGuard(ledger);
        const chunk = await waitForTitle(reader.read(), controller.signal);
        titleGuard(ledger);
        if (chunk.done) break;
        if (!(chunk.value instanceof Uint8Array)) throw titleError('invalid_title_page');
        ledger.bytes += chunk.value.byteLength;
        if (ledger.bytes > 1056639 || size + chunk.value.byteLength > buffer.length) throw titleError('title_page_resource_limit');
        buffer.set(chunk.value, size); size += chunk.value.byteLength;
      }
      const bytes = buffer.slice(0, size);
      if (!response.ok) {
        const { raw, value } = titleAsciiObject(bytes);
        titleExact(value, ['code', 'error']);
        if (typeof value.code !== 'string' || !['inventory_page_stale', 'inventory_refresh_busy', 'inventory_capture_in_progress', 'invalid_input', 'session_required'].includes(value.code) || typeof value.error !== 'string' || value.error.length > 256 || titlePageCodec.encode(value) !== raw) throw titleError('invalid_title_page');
        throw titleError(value.code, response.status);
      }
      finished = true;
      return bytes;
    } catch (error) {
      if (ledger.controller.signal.aborted) throw titleError('title_page_cancelled');
      if (timedOut) throw titleError('title_page_timeout');
      throw Object.hasOwn(titleMessages, error?.code) ? error : titleError('request_failed');
    } finally {
      clearTimeout(timer); ledger.controller.signal.removeEventListener('abort', abort);
      if (!finished) { controller.abort(); if (reader) reader.cancel().catch(() => {}); }
      if (reader) { try { reader.releaseLock(); } catch (_) {} }
    }
  };
  const collectTitlePages = async (controller, refresh) => {
    const ledger = { controller, requests: 0, bytes: 0, deadline: performance.now() + 15000 };
    const items = [], keys = new Set(), cursors = new Set();
    let header = null, pageCount = 0, total = 0, stagedBytes = 0;
    let path = refresh ? '/api/detected-titles/pages/refresh' : '/api/detected-titles/pages';
    for (let index = 0; index < 128; index++) {
      const bytes = await readTitleResponse(path, ledger, refresh && index === 0);
      const normalized = titlePageCodec.normalizeTitlePage(bytes);
      if (!normalized.ok) throw titleError(normalized.reason);
      const value = normalized.page;
      if (value.snapshot.status !== 'observed') {
        if (index !== 0) throw titleError('invalid_title_page');
        return { schema_version: 2, ...value.snapshot, items: [] };
      }
      const page = value.page;
      if (index === 0) {
        header = value.snapshot; pageCount = page.page_count; total = page.total_count;
        stagedBytes = titlePageCodec.encode({ schema_version: 2, ...header, items: [] }).length;
      }
      if (page.index !== index || page.item_offset !== items.length || page.page_count !== pageCount || page.total_count !== total || ['status', 'complete', 'snapshot_id', 'source_scope', 'reason'].some(field => value.snapshot[field] !== header[field])) throw titleError('invalid_title_page');
      for (const item of value.items) {
        if (keys.has(item.title_key)) throw titleError('invalid_title_page');
        stagedBytes += titlePageCodec.encode(item).length + (items.length ? 1 : 0);
        if (stagedBytes > 524288) throw titleError('title_page_resource_limit');
        keys.add(item.title_key); items.push(item);
      }
      if (page.next_cursor !== null) {
        if (cursors.has(page.next_cursor)) throw titleError('invalid_title_page');
        cursors.add(page.next_cursor);
        path = `/api/detected-titles/pages/${page.next_cursor}`;
        continue;
      }
      if (index + 1 !== pageCount || items.length !== total) throw titleError('invalid_title_page');
      const currentBytes = await readTitleResponse(`/api/detected-titles/pages/current/${header.snapshot_id}`, ledger);
      const { raw, value: current } = titleAsciiObject(currentBytes);
      titleExact(current, ['schema_version', 'kind', 'snapshot_id', 'current']);
      if (current.schema_version !== 2 || current.kind !== 'title_inventory_current' || current.current !== true || current.snapshot_id !== header.snapshot_id || titlePageCodec.encode(current) !== raw) throw titleError('invalid_title_page');
      titleGuard(ledger);
      return { schema_version: 2, ...header, items };
    }
    throw titleError('title_page_resource_limit');
  };
  const normalizedInventory = data => {
    const record = cleanRecord(data), items = apiItems(record);
    const validKeys = new Set();
    const invalidKey = items.some(item => {
      if (!opaqueValue(item?.title_key) || validKeys.has(item.title_key)) return true;
      validKeys.add(item.title_key); return false;
    });
    if (record.schema_version !== 1 || !['observed', 'partial', 'unavailable', 'error'].includes(record.status) || typeof record.complete !== 'boolean' || items.length > 128 || invalidKey) return { status: 'error', complete: false, snapshot_id: null, source_scope: 'unavailable', items: [] };
    if (record.status === 'observed' && (record.complete !== true || !opaqueValue(record.snapshot_id))) return { status: 'error', complete: false, snapshot_id: null, source_scope: 'unavailable', items: [] };
    return { ...record, items };
  };
  const selectableTitles = () => state.titles?.status === 'observed' && state.titles.complete === true && !!opaqueValue(state.titles.snapshot_id);
  const observedText = (field, fallback) => field?.status === 'observed' && typeof field.value === 'string' && field.value.length <= 256 ? safeText(field.value, fallback) : fallback;
  const selectedTitle = () => selectableTitles() && state.targetRef?.snapshot_id === state.titles.snapshot_id ? state.titles.items.find(item => item.title_key === state.targetRef.title_key) : null;
  const renderTitles = () => {
    const inventory = state.titles, current = selectedTitle();
    const root = $('#title-list'), focusedKey = document.activeElement?.dataset.titleKey;
    const running = inventory?.source_scope === 'running_processes';
    $('#title-status').textContent = state.titlesLoading ? 'Checking console title inventory…' : !inventory || inventory.status === 'unavailable' ? 'Console title inventory unavailable. Nexus browsing remains analysis only.' : inventory.status === 'partial' ? 'Partial title observation · selection unavailable until a complete snapshot.' : inventory.status === 'error' ? 'Console title inventory could not be verified.' : inventory.items.length ? `${running ? 'Running titles' : 'Observed titles'} · choose a target for analysis.` : `No ${running ? 'running' : 'observed'} titles in this complete snapshot.`;
    root.innerHTML = (inventory?.items || []).map(item => {
      const title = observedText(item.title_id, 'Title ID unavailable');
      const label = observedText(item.display_name, 'Display name unavailable');
      const version = observedText(item.build_version, 'Version unavailable');
      const measured = item.build_fingerprint?.confidence === 'exact_bytes' && item.build_fingerprint?.algorithm === 'sha256' && /^[a-f0-9]{64}$/.test(item.build_fingerprint?.value || '');
      return `<button class="title-choice" data-title-key="${esc(item.title_key)}" aria-pressed="${current?.title_key === item.title_key}" ${selectableTitles() && !state.titlesLoading && !state.busy ? '' : 'disabled'}><strong>${esc(title)}</strong><span>${esc(label)}</span><small>${esc(version)} · ${measured ? 'Build fingerprint measured · compatibility unverified' : 'Build unknown · compatibility unverified'}</small></button>`;
    }).join('');
    $$('[data-title-key]', root).forEach(button => button.addEventListener('click', () => chooseTitle(button.dataset.titleKey)));
    $('#selected-target').textContent = current ? `Analysis target · ${observedText(current.title_id, 'Title ID unavailable')} · ${observedText(current.display_name, 'Name unavailable')}` : 'No console target · analysis only';
    $('#clear-target').hidden = !current;
    $('#clear-target').disabled = state.busy || state.titlesLoading;
    $('#reload-titles').disabled = state.titlesLoading || state.busy;
    if (focusedKey) $$('[data-title-key]', root).find(button => button.dataset.titleKey === focusedKey)?.focus();
  };
  const chooseTitle = key => {
    if (!selectableTitles() || state.titlesLoading || state.busy || !state.titles.items.some(item => item.title_key === key)) return;
    state.targetRef = { snapshot_id: state.titles.snapshot_id, title_key: key };
    state.pendingIntent = null; saveNavigation(); renderTitles();
  };
  const clearTarget = () => { state.targetRef = null; state.pendingIntent = null; saveNavigation(); renderTitles(); };
  const loadTitles = async (savedRef, refresh = false) => {
    const reference = targetReference(savedRef || state.targetRef);
    state.titlesController?.abort();
    const request = ++state.requests.titles;
    const controller = new AbortController(); state.titlesController = controller;
    state.titlesLoading = true; state.targetRef = null; state.pendingIntent = null; renderTitles(); renderGames();
    try {
      const data = state.capabilities.title_inventory_paging === true ? await collectTitlePages(controller, refresh) :
        normalizedInventory(await endpoint(refresh ? '/api/detected-titles/refresh' : '/api/detected-titles', refresh ? { method: 'POST', body: '{}', signal: controller.signal } : { signal: controller.signal }));
      if (request !== state.requests.titles) return;
      state.titles = data;
      state.targetRef = selectableTitles() && reference?.snapshot_id === state.titles.snapshot_id && state.titles.items.some(item => item.title_key === reference.title_key) ? targetReference(reference) : null;
      if (!sameTarget(state.targetRef, reference)) state.pendingIntent = null;
      saveNavigation();
    } catch (error) {
      if (request !== state.requests.titles) return;
      state.titles = { status: state.capabilities.title_inventory_paging === true ? 'error' : 'unavailable', complete: false, snapshot_id: null, items: [] };
      state.targetRef = null; state.pendingIntent = null; saveNavigation();
      if (state.capabilities.title_inventory_paging === true) showError(`${titleMessages[error?.code] || titleMessages.request_failed} Refresh targets to try again.`);
    } finally { if (request === state.requests.titles) { state.titlesLoading = false; state.titlesController = null; renderTitles(); renderGames(); } }
  };
  const refreshTitles = async () => { clearTarget(); await loadTitles(null, true); };
  const lookupIdentity = () => {
    const entered = $('#lookup-url').value.trim();
    if (!entered) {
      const domain = domainValue($('#lookup-domain').value.trim()), modId = idValue($('#lookup-mod-id').value.trim());
      return domain && modId ? { domain, modId } : null;
    }
    if (entered.length > 2048 || /[\\\u0000-\u0020]/.test(entered)) return null;
    // Match the original path before URL normalization can conceal dot segments
    // or encoded separators. Only the identity is sent to our fixed local route.
    const match = /^https:\/\/www\.nexusmods\.com(?::443)?\/([a-z0-9][a-z0-9_-]{0,79})\/mods\/([1-9][0-9]{0,18})\/?(?:\?([^#]*))?(?:#[^\r\n]*)?$/.exec(entered);
    if (!match || !idValue(match[2])) return null;
    const query = new URLSearchParams(match[3] || '');
    const seen = new Set();
    for (const [key, value] of query) {
      if (seen.has(key)) return null;
      seen.add(key);
      if (key === 'tab' && ['files', 'description'].includes(value)) continue;
      if (key === 'file_id' && idValue(value) && query.get('tab') === 'files') continue;
      return null;
    }
    return { domain: match[1], modId: match[2] };
  };
  const lookupMod = async event => {
    event?.preventDefault(); clearError();
    if (state.capabilities.nexus_catalog !== true) { showError('Nexus mod lookup is unavailable in this runtime.'); return; }
    const identity = lookupIdentity();
    if (!identity) { showError('Enter a valid official Nexus mod URL, or a safe Nexus domain and positive mod ID.'); return; }
    const request = ++state.requests.lookup;
    ++state.requests.mods; ++state.requests.files;
    state.lookupLoading = true; $('#lookup-status').textContent = 'Looking up mod…';
    try {
      const data = await endpoint(`/api/games/${identity.domain}/mods/${identity.modId}`);
      if (request !== state.requests.lookup) return;
      const mod = cleanRecord(data?.item);
      if (idValue(mod.mod_id) !== identity.modId || mod.game_domain !== identity.domain) throw new Error('Nexus returned a different mod identity.');
      state.game = state.games.find(item => item.domain_name === identity.domain) || { domain_name: identity.domain, name: identity.domain };
      state.mods = [mod]; state.mod = null; state.file = null; state.files = []; state.pendingIntent = null;
      await chooseMod(mod.mod_id, true);
    } catch (error) { if (request === state.requests.lookup) showError(error.message); }
    finally { if (request === state.requests.lookup) { state.lookupLoading = false; $('#lookup-status').textContent = ''; } }
  };
  const setView = (view, moveFocus = false) => {
    if (!['games', 'activity', 'installed'].includes(view)) return;
    state.view = view;
    for (const tab of $$('.tab')) {
      const active = tab.dataset.view === view;
      tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); tab.tabIndex = active ? 0 : -1;
    }
    for (const name of ['games', 'activity', 'installed']) $(`#${name}-view`).hidden = name !== view;
    saveNavigation(); render();
    if (moveFocus) $(`#tab-${view}`).focus();
  };
  const renderAccount = () => {
    const box = $('#account');
    const verified = state.account?.status === 'connected' && state.account?.live_service === 'verified' && state.account?.account;
    box.classList.toggle('connected', !!verified);
    if (verified) {
      const person = state.account.account;
      box.innerHTML = `<span class="status-dot" aria-hidden="true"></span><span>Nexus · ${esc(safeText(person.display_name, 'Connected'))}</span><button class="account-action" id="disconnect-account">Disconnect</button>`;
      $('#disconnect-account').addEventListener('click', disconnectAccount);
    } else {
      const canSignIn = state.capabilities.nexus_auth_start === true;
      box.innerHTML = `<span class="status-dot" aria-hidden="true"></span><span>${state.account?.status === 'error' ? 'Nexus status unavailable' : 'Nexus account not connected'}</span>${canSignIn ? '<button class="account-action" id="sign-in">Sign in</button>' : ''}`;
      $('#sign-in')?.addEventListener('click', signIn);
    }
  };
  const card = ({ title, description, meta, cls = '', action, details }) => `<button class="card ${cls}" ${details.type !== 'game' && !idValue(details.id) ? 'disabled' : ''} data-card="${esc(details.type)}:${esc(details.id)}" data-details="${esc(JSON.stringify(details))}"><span class="card-arrow" aria-hidden="true">›</span><p class="card-title">${esc(title)}</p><p class="card-meta">${esc(description || '')}</p>${meta ? `<span class="card-grid-meta">${meta}</span>` : ''}</button>`;
  const bindCards = (root) => {
    $$('.card[data-details]', root).forEach(el => {
      const details = JSON.parse(el.dataset.details || '{}');
      el.addEventListener('click', () => details.type === 'game' ? chooseGame(details.id) : details.type === 'mod' ? chooseMod(details.id) : details.type === 'file' ? chooseFile(details.id) : openDetails(details, el));
      el.addEventListener('contextmenu', event => { event.preventDefault(); openDetails(details, el); });
      el.addEventListener('keydown', event => { if (event.key === 'ContextMenu' || event.key === 'BrowserBack') { event.preventDefault(); openDetails(details, el); } });
    });
  };
  const breadcrumb = () => `<div class="breadcrumbs"><button data-breadcrumb="games">Games</button>${state.game ? `<span>›</span><button data-breadcrumb="mods">${esc(state.game.name)}</button>` : ''}${state.mod ? `<span>›</span><span>${esc(state.mod.name)}</span>` : ''}</div>`;
  const bindBreadcrumbs = (root) => $$('.breadcrumbs button', root).forEach(button => button.addEventListener('click', () => button.dataset.breadcrumb === 'games' ? resetSelection() : backToMods()));
  const renderGames = () => {
    const root = $('#games-content');
    if (!state.game) {
      $('#games-view h1').textContent = 'Choose a game';
      $('#games-view .subheading').textContent = 'Browse the latest mods listed on Nexus.';
      root.innerHTML = state.games.length ? state.games.map(game => card({ title: safeText(game.name), description: `Nexus domain · ${safeText(game.domain_name)}`, meta: '<span class="pill">Nexus catalog</span>', details: { type: 'game', id: game.domain_name, name: game.name, domain_name: game.domain_name } })).join('') : '<div class="empty-state"><h2>No games available</h2><p>Connect a verified Nexus account to browse its catalog.</p></div>';
      bindCards(root); return;
    }
    const heading = state.mod ? `Files for ${safeText(state.mod.name)}` : `Latest mods for ${safeText(state.game.name)}`;
    $('#games-view h1').textContent = heading;
    $('#games-view .subheading').textContent = state.mod ? 'Choose a file to review its details.' : 'Latest mods from Nexus · this is not a search result.';
    root.innerHTML = `${breadcrumb()}<h2 class="content-title">${state.mod ? 'Available files' : 'Latest mods from Nexus'}</h2>${state.mod ? `<p class="content-note">Compatibility has not been measured. Downloads are kept for analysis.</p>` : ''}${state.mod ? (state.files.length ? state.files.map(file => { const safeId = idValue(file.file_id); const available = state.capabilities.download_analyze === true && state.capabilities.jobs === true && !!safeId; const enabled = available && !state.titlesLoading && !state.busy; const label = !safeId ? 'File ID unavailable' : available ? 'Download for analysis' : 'Download analysis unavailable'; return `<article class="card file-card" data-file-card="${esc(safeId || 'unavailable')}"><span class="file-symbol" aria-hidden="true">ZIP</span><div class="file-main"><p class="card-title">${esc(safeText(file.name || file.file_name))}</p><p class="card-meta">${esc(safeText(file.version, 'Version unavailable'))} · ${Number.isFinite(Number(file.size_kb)) ? `${esc(file.size_kb)} KB` : 'Size unavailable'}</p></div><button class="icon-button details-file" aria-label="Details for ${esc(safeText(file.name || file.file_name))}" data-file-details="${esc(safeId)}">i</button><button class="action-button download-file" data-file-action="${esc(safeId)}" ${enabled ? '' : 'disabled'}>${label}</button></article>`; }).join('') : '<div class="empty-state"><h2>No files available</h2><p>Nexus did not return any files for this mod.</p></div>') : (state.mods.length ? state.mods.map(mod => { const safeId = idValue(mod.mod_id); return card({ title: safeText(mod.name), description: `By ${safeText(mod.author, 'Author unavailable')} · ${safeText(mod.version, 'Version unavailable')}`, meta: safeId ? '<span class="pill">Compatibility unresolved</span>' : '<span class="pill">Browser-safe request unavailable</span>', details: { type: 'mod', id: safeId, name: mod.name, author: mod.author, summary: mod.summary, version: mod.version, source_url: mod.source_url } }); }).join('') : '<div class="empty-state"><h2>No recent mods available</h2><p>Nexus has no latest mod records for this game right now.</p></div>')}`;
    bindBreadcrumbs(root); bindCards(root);
    $$('[data-file-action]', root).forEach(btn => btn.addEventListener('click', () => { const file = state.files.find(item => String(item.file_id) === btn.dataset.fileAction); if (file) chooseFile(file.file_id, btn); }));
    $$('[data-file-details]', root).forEach(btn => btn.addEventListener('click', () => { const file = state.files.find(item => String(item.file_id) === btn.dataset.fileDetails); if (file) openDetails({ type: 'file', id: file.file_id, name: file.name || file.file_name, file_name: file.file_name, version: file.version, size_kb: file.size_kb, description: file.description }, btn); }));
  };
  const renderActivity = () => {
    const root = $('#activity-content');
    const focused = document.activeElement;
    const focusedJobId = focused?.closest('[data-job-id]')?.dataset.jobId || '';
    const focusedKey = focused?.dataset.focusKey || '';
    if (!state.jobs.length) { root.innerHTML = '<div class="empty-state"><span class="empty-icon" aria-hidden="true">↻</span><h2>No activity yet</h2><p>Choose a Nexus file and request a download for analysis.</p></div>'; return; }
    root.innerHTML = state.jobs.map(job => {
      const stateLabel = ({ queued: 'Queued', downloading: 'Downloading', analyzing: 'Analyzing archive', analyzed: 'Analysis complete', needs_nexus_authorization: 'Nexus authorization needed', failed: 'Could not complete', interrupted: 'Interrupted' })[job.status] || 'Status unavailable';
      const bytes = Number(job.bytes_downloaded);
      const hash = typeof job.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(job.sha256) ? `<div>Verified archive hash: ${esc(job.sha256)}</div>` : '';
      const sourceUrl = job.status === 'needs_nexus_authorization' ? nexusFilePage(job) : '';
      const authorization = job.status === 'needs_nexus_authorization' ? sourceUrl ? `<a class="secondary-button source-link" href="${esc(sourceUrl)}" target="_blank" rel="noopener noreferrer" data-focus-key="nexus-source">Open Nexus file page</a>` : 'The Nexus file page is unavailable for this request.' : '';
      const report = validatedAnalysis(job.analysis_report);
      const compatibility = report ? `Compatibility ${report.compatibility_state}` : 'Compatibility unverified · unresolved';
      const analysis = report ? `<div>${report.operation_state === 'supported' ? 'Read-only archive inventory complete' : 'Read-only archive inventory unverified'} · desktop preparation</div><ul class="analysis-blockers">${report.reason_codes.map(code => `<li>${esc(analysisReasons[code])}</li>`).join('')}</ul><div>Installation unavailable · nothing was installed.</div>` : '';
      return `<article class="card job-card" data-job-id="${esc(safeText(job.id, ''))}"><div><p class="card-title">${esc(safeText(job.label, `${safeText(job.game, 'Game')} · mod ${safeText(job.mod_id)} · file ${safeText(job.file_id)}`))}</p><div class="job-state"><span class="pill">${esc(stateLabel)}</span><span>${esc(compatibility)}</span></div></div><div class="job-state">${Number.isFinite(bytes) ? `${esc(bytes)} bytes` : ''}</div><div class="job-details">${analysis || (job.status === 'analyzed' ? 'Archive analyzed. Target compatibility remains unresolved; nothing was installed.' : job.status === 'needs_nexus_authorization' ? 'Authorize this download on the official Nexus file page.' : job.status === 'failed' ? esc(safeText(job.error?.message, 'Please try again.')) : job.status === 'interrupted' ? 'The app stopped before this job completed.' : 'This request downloads to private quarantine for nonexecuting analysis.')}${authorization}${hash}</div></article>`;
    }).join('');
    if (focusedJobId && focusedKey) {
      const replacement = $$('[data-job-id]', root).find(card => card.dataset.jobId === focusedJobId);
      if (replacement) $$('[data-focus-key]', replacement).find(element => element.dataset.focusKey === focusedKey)?.focus();
    }
  };
  const renderInstalled = () => {
    const root = $('#installed-content');
    if (!state.installed.length) { root.className = 'empty-state'; root.innerHTML = '<span class="empty-icon" aria-hidden="true">✓</span><h2>No confirmed installations</h2><p>Gameplay status is unverified. ModBridge has not installed anything.</p>'; return; }
    root.className = 'stack'; root.innerHTML = state.installed.map(item => `<article class="card"><p class="card-title">${esc(safeText(item.name))}</p><p class="card-meta">Confirmed transaction · ${esc(safeText(item.sha256, 'Hash unavailable'))}</p></article>`).join('');
  };
  const analysisReasons = Object.freeze({
    unknown_target: 'Target build is unknown.', unbound_package_requirement: 'Package requirement is not bound to this target.',
    inventory_unresolved: 'Archive inventory remains unresolved.', inventory_incomplete: 'Archive inventory is incomplete.',
    inventory_issue: 'Archive inventory has unresolved issues.', input_provenance_unstable: 'Source bytes could not be shown to remain stable.',
    missing_package_identity: 'Package identity is unavailable.', missing_source_hash: 'Source hash is unavailable.',
    missing_requirements: 'Package requirements are unavailable.', missing_backend: 'Required backend is unavailable.',
    ambiguous_backend: 'Backend selection is ambiguous.', version_conflict: 'Required versions conflict.',
    unsupported_dependency_constraint: 'Dependency constraints are unsupported.', missing_dependency: 'A required dependency is unavailable.',
    installation_conflict: 'Package installation requirements conflict.', unknown_dependency_type: 'Dependency type is unknown.',
    ordering_cycle: 'Dependency ordering contains a cycle.', dependency_cycle: 'Dependencies contain a cycle.',
    unsupported_reference_cycle: 'A reference cycle is unsupported.', dependency_unresolved: 'A dependency remains unresolved.',
    platform_acceptance_missing: 'PS5 asset and runtime acceptance has not been measured.',
    windows_runtime_unavailable: 'Required Windows runtime is unavailable.', managed_runtime_unavailable: 'Required managed runtime is unavailable.',
    analysis_detail_unavailable: 'Further analysis details are unavailable.',
  });
  const validatedAnalysis = value => {
    const record = cleanRecord(value), reasons = record.reason_codes, install = cleanRecord(record.install_authority);
    if (record.schema_version !== 1 || record.operation !== 'archive_inventory' || record.execution_context !== 'desktop_preparation' || !['supported', 'unverified'].includes(record.operation_state) || !['unverified', 'unsupported'].includes(record.compatibility_state) || !Array.isArray(reasons) || reasons.length < 1 || reasons.length > 32 || reasons.some(code => typeof code !== 'string' || !Object.hasOwn(analysisReasons, code)) || reasons.some((code, index) => index > 0 && code <= reasons[index - 1]) || !reasons.includes('platform_acceptance_missing') || install.state !== 'unsupported' || install.reason !== 'install_operation_unavailable' || !Array.isArray(install.scope) || install.scope.length) return null;
    const unsupported = reasons.includes('windows_runtime_unavailable') || reasons.includes('managed_runtime_unavailable');
    return (record.compatibility_state === 'unsupported') === unsupported ? record : null;
  };
  const render = () => { renderAccount(); renderGames(); renderTitles(); renderActivity(); renderInstalled(); };
  const apiItems = data => Array.isArray(data?.items) ? data.items : [];
  const nexusFilePage = job => {
    if (typeof job?.game !== 'string' || !/^[a-z0-9][a-z0-9_-]{0,79}$/.test(job.game)) return '';
    const modId = idValue(job.mod_id), fileId = idValue(job.file_id);
    return modId && fileId ? `https://www.nexusmods.com/${job.game}/mods/${modId}?tab=files&file_id=${fileId}` : '';
  };
  const loadGames = async () => {
    if (state.capabilities.nexus_catalog !== true) { state.games = []; renderGames(); return; }
    const request = ++state.requests.games;
    try { const data = await endpoint('/api/games'); if (request !== state.requests.games) return; state.games = apiItems(data); renderGames(); }
    catch (error) { if (request === state.requests.games) { state.games = []; showError(error.message); renderGames(); } }
  };
  const chooseGame = async domain => {
    const game = state.games.find(item => item.domain_name === domain); if (!game || !domainValue(domain) || state.capabilities.nexus_catalog !== true) return;
    cancelLookup(); state.pendingIntent = null;
    state.game = game; state.mod = null; state.file = null; state.mods = []; state.files = []; saveNavigation();
    const request = ++state.requests.mods; state.requests.files++;
    renderGames();
    try { const data = await endpoint(`/api/games/${encodeURIComponent(game.domain_name)}/mods`); if (request !== state.requests.mods || state.game?.domain_name !== game.domain_name) return; state.mods = apiItems(data); renderGames(); }
    catch (error) { if (request === state.requests.mods) { showError(error.message); renderGames(); } }
  };
  const chooseMod = async (id, fromLookup = false) => {
    const mod = state.mods.find(item => String(item.mod_id) === String(id)); if (!mod || !state.game) return;
    if (!fromLookup) cancelLookup();
    state.pendingIntent = null;
    state.mod = mod; state.file = null; state.files = []; saveNavigation();
    const request = ++state.requests.files; const game = state.game.domain_name;
    renderGames();
    try { const data = await endpoint(`/api/games/${encodeURIComponent(game)}/mods/${encodeURIComponent(mod.mod_id)}/files`); if (request !== state.requests.files || state.game?.domain_name !== game || String(state.mod?.mod_id) !== String(mod.mod_id)) return; state.files = apiItems(data); renderGames(); }
    catch (error) { if (request === state.requests.files) { showError(error.message); renderGames(); } }
  };
  const chooseFile = async (id, trigger) => {
    const file = state.files.find(item => idValue(item.file_id) && idValue(item.file_id) === idValue(id));
    if (!file || state.capabilities.download_analyze !== true || state.capabilities.jobs !== true || !state.game || !state.mod || state.busy || state.titlesLoading || (state.targetRef && !selectedTitle())) return;
    state.file = file; saveNavigation();
    renderGames();
    if (!idValue(state.mod.mod_id) || !idValue(file.file_id)) return;
    if (!state.pendingIntent || state.pendingIntent.game !== state.game.domain_name || state.pendingIntent.mod_id !== idValue(state.mod.mod_id) || state.pendingIntent.file_id !== idValue(file.file_id) || !sameTarget(state.pendingIntent.target_ref, state.targetRef)) {
      state.pendingIntent = { game: state.game.domain_name, mod_id: idValue(state.mod.mod_id), file_id: idValue(file.file_id), idempotency_key: makeIntentKey() };
      if (state.targetRef) state.pendingIntent.target_ref = { ...state.targetRef };
    }
    await submitIntent(trigger);
  };
  const makeIntentKey = () => `mb_${Array.from(crypto.getRandomValues(new Uint8Array(18)), n => n.toString(16).padStart(2, '0')).join('')}`;
  const submitIntent = async trigger => {
    const intent = state.pendingIntent; if (!intent || state.busy || state.titlesLoading || (intent.target_ref && (!sameTarget(intent.target_ref, state.targetRef) || !selectedTitle()))) return;
    state.busy = true; renderGames(); renderTitles();
    const button = trigger?.isConnected ? trigger : $$('[data-file-action]').find(el => el.dataset.fileAction === intent.file_id);
    if (button) { button.disabled = true; button.textContent = 'Requesting…'; }
    try {
      const body = { kind: 'download_analyze', game: intent.game, mod_id: Number(intent.mod_id), file_id: Number(intent.file_id), idempotency_key: intent.idempotency_key };
      if (intent.target_ref) body.target_ref = { ...intent.target_ref };
      const data = await endpoint('/api/jobs', { method: 'POST', body: JSON.stringify(body) });
      state.pendingIntent = null; showToast(data.status === 'queued' ? 'Analysis request queued.' : 'Request recorded.'); await loadJobs();
    } catch (error) {
      if (error.status === 409 && error.code === 'stale_target_ref') {
        clearTarget(); await loadTitles(null, true);
        showError('The console target changed. Select a target again before making another analysis request.');
      } else { showError(`${error.message} Retry to reuse this request safely.`); }
    }
    finally { state.busy = false; renderGames(); renderTitles(); }
  };
  const loadAccount = async () => {
    if (state.capabilities.account_status !== true) { state.account = { status: 'unavailable' }; renderAccount(); return; }
    try { state.account = await endpoint('/api/account'); }
    catch (_) { state.account = { status: 'error' }; }
    renderAccount();
  };
  const signIn = async () => {
    try {
      const result = await endpoint('/api/auth/start', { method: 'POST', body: '{}' });
      const url = new URL(result.authorization_url);
      if (url.protocol !== 'https:' || url.hostname !== 'users.nexusmods.com') throw new Error('Nexus sign-in could not be started safely.');
      window.location.assign(url.href);
    } catch (error) { showError(error.message); }
  };
  const disconnectAccount = async () => {
    if (state.busy) return;
    state.busy = true;
    try { await endpoint('/api/auth/disconnect', { method: 'POST', body: '{}' }); state.account = { status: 'disconnected' }; renderAccount(); showToast('Nexus account disconnected.'); }
    catch (error) { showError(error.message); }
    finally { state.busy = false; }
  };
  const loadJobs = async () => {
    if (state.capabilities.jobs !== true) { state.jobs = []; renderActivity(); return; }
    if (state.jobsLoading) return;
    state.jobsLoading = true;
    const request = ++state.requests.jobs;
    try {
      const data = await endpoint('/api/jobs');
      if (request === state.requests.jobs) {
        const nextJobs = apiItems(data);
        const stableJson = value => JSON.stringify(value, (_key, entry) => entry && typeof entry === 'object' && !Array.isArray(entry) ? Object.fromEntries(Object.entries(entry).sort(([left], [right]) => left.localeCompare(right))) : entry);
        if (stableJson(nextJobs) !== stableJson(state.jobs)) { state.jobs = nextJobs; renderActivity(); }
      }
    } catch (error) { if (request === state.requests.jobs) showError(error.message); }
    finally { state.jobsLoading = false; }
  };
  const loadInstalled = async () => {
    if (state.capabilities.installed_records !== true) { state.installed = []; renderInstalled(); return; }
    const request = ++state.requests.installed;
    try { const data = await endpoint('/api/installed'); if (request !== state.requests.installed) return; state.installed = apiItems(data).filter(item => item?.transaction_confirmed === true && typeof item.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(item.sha256)); renderInstalled(); }
    catch (error) { if (request === state.requests.installed) showError(error.message); }
  };
  const cancelLookup = () => { ++state.requests.lookup; state.lookupLoading = false; $('#lookup-status').textContent = ''; };
  const resetSelection = () => { cancelLookup(); state.pendingIntent = null; state.game = null; state.mod = null; state.file = null; state.mods = []; state.files = []; ++state.requests.mods; ++state.requests.files; saveNavigation(); renderGames(); };
  const backToMods = () => { cancelLookup(); state.pendingIntent = null; state.mod = null; state.file = null; state.files = []; ++state.requests.files; saveNavigation(); renderGames(); };
  const openDetails = (item, trigger) => {
    state.modalReturnFocus = trigger || document.activeElement;
    $('#details-title').textContent = safeText(item.name || item.title, 'Details');
    const fields = item.type === 'game' ? [['Nexus domain', item.domain_name], ['Catalog ID', item.id]] : item.type === 'mod' ? [['Author', item.author], ['Version', item.version], ['Summary', item.summary], ['Nexus page', item.source_url]] : [['File name', item.file_name], ['Version', item.version], ['Size', Number.isFinite(Number(item.size_kb)) ? `${item.size_kb} KB` : 'Unavailable'], ['Description', item.description]];
    $('#details-content').innerHTML = fields.filter(([, value]) => value !== undefined && value !== null && String(value).trim() !== '').map(([label, value]) => `<div class="detail-row"><div class="detail-label">${esc(label)}</div><div class="detail-value">${esc(value)}</div></div>`).join('') || '<p class="detail-value">No additional details available.</p>';
    $('#details-dialog').showModal(); $('[data-close-dialog]', $('#details-dialog')).focus();
  };
  const closeDetails = () => { const dialog = $('#details-dialog'); if (dialog.open) dialog.close(); if (state.modalReturnFocus?.isConnected) state.modalReturnFocus.focus(); else $(`#tab-${state.view}`).focus(); state.modalReturnFocus = null; };
  const handleBack = () => {
    if ($('#details-dialog').open) { closeDetails(); return; }
    if (state.view !== 'games') { setView('games', true); return; }
    if (state.mod) { backToMods(); return; }
    if (state.game) { resetSelection(); return; }
  };
  const moveFocus = delta => {
    if ($('#details-dialog').open) {
      const items = $$('button:not(:disabled),a[href],[tabindex="0"]', $('#details-dialog')).filter(el => el.getClientRects().length && !el.closest('[hidden]'));
      const index = items.indexOf(document.activeElement); items[(index + delta + items.length) % items.length]?.focus(); return;
    }
    const items = $$('.topbar a[href],.topbar button:not(:disabled),.tabs button:not(:disabled),#' + state.view + '-view button:not(:disabled),#' + state.view + '-view a[href],#' + state.view + '-view input:not(:disabled)')
      .filter(el => el.getClientRects().length && !el.closest('[hidden]'));
    const index = items.indexOf(document.activeElement);
    items[(index + delta + items.length) % items.length]?.focus();
  };
  const handleKey = event => {
    if ((event.target.matches('input,textarea,select') || event.target.isContentEditable) && event.key !== 'Escape' && event.key !== 'BrowserBack') return;
    if (event.key === 'Escape' || event.key === 'Backspace' || event.key === 'BrowserBack') { event.preventDefault(); handleBack(); return; }
    if (event.key === 'ArrowRight' || event.key === 'ArrowDown') { event.preventDefault(); moveFocus(1); return; }
    if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') { event.preventDefault(); moveFocus(-1); return; }
    if (event.key === 'F6' && state.view === 'games') { event.preventDefault(); const details = document.activeElement?.dataset?.details; if (details) openDetails(JSON.parse(details), document.activeElement); }
  };
  const bindTabs = () => {
    $$('.tab').forEach((tab, index, tabs) => {
      tab.addEventListener('click', () => activateView(tab.dataset.view));
      tab.addEventListener('keydown', event => {
        if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') { event.preventDefault(); event.stopPropagation(); tabs[(index + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length].focus(); }
      });
    });
    $('#details-dialog').addEventListener('click', event => { if (event.target.matches('[data-close-dialog]') || event.target === $('#details-dialog')) closeDetails(); });
    $('#lookup-form').addEventListener('submit', lookupMod);
    $('#reload-titles').addEventListener('click', refreshTitles);
    $('#clear-target').addEventListener('click', clearTarget);
    $('#lookup-url').addEventListener('input', () => { if ($('#lookup-url').value) { $('#lookup-domain').value = ''; $('#lookup-mod-id').value = ''; } });
    for (const input of [$('#lookup-domain'), $('#lookup-mod-id')]) input.addEventListener('input', () => { if (input.value) $('#lookup-url').value = ''; });
  };
  const activateView = (view, move = false) => { setView(view, move); if (view === 'activity') loadJobs(); if (view === 'installed') loadInstalled(); };
  const gamepadTick = () => {
    const pads = navigator.getGamepads?.();
    const pad = pads ? [...pads].find(candidate => candidate && candidate.mapping === 'standard') : null;
    if (pad) {
      const pressed = (index) => !!pad.buttons[index]?.pressed;
      if (pressed(0) && !gamepadTick.lastCross) document.activeElement?.click?.();
      if (pressed(1) && !gamepadTick.lastCircle) handleBack();
      if (pressed(9) && !gamepadTick.lastOptions) {
        const target = document.activeElement; const details = target?.dataset?.details;
        if (details) openDetails(JSON.parse(details), target);
        else { const fileButton = target?.closest('[data-file-card]')?.querySelector('.details-file'); fileButton?.click(); }
      }
      if (!$('#details-dialog').open && pressed(4) && !gamepadTick.lastL1) activateView(['games', 'activity', 'installed'][(['games', 'activity', 'installed'].indexOf(state.view) + 2) % 3], true);
      if (!$('#details-dialog').open && pressed(5) && !gamepadTick.lastR1) activateView(['games', 'activity', 'installed'][(['games', 'activity', 'installed'].indexOf(state.view) + 1) % 3], true);
      for (const [name, index] of [['lastCross', 0], ['lastCircle', 1], ['lastOptions', 9], ['lastL1', 4], ['lastR1', 5]]) gamepadTick[name] = pressed(index);
      const up = pressed(12) || pad.axes[1] < -.6, down = pressed(13) || pad.axes[1] > .6, left = pressed(14) || pad.axes[0] < -.6, right = pressed(15) || pad.axes[0] > .6;
      const now = Date.now();
      if (up && now - (gamepadTick.lastNav || 0) > 220) { moveFocus(-1); gamepadTick.lastNav = now; }
      else if (down && now - (gamepadTick.lastNav || 0) > 220) { moveFocus(1); gamepadTick.lastNav = now; }
      else if (left && now - (gamepadTick.lastNav || 0) > 220) { moveFocus(-1); gamepadTick.lastNav = now; }
      else if (right && now - (gamepadTick.lastNav || 0) > 220) { moveFocus(1); gamepadTick.lastNav = now; }
    } else { gamepadTick.lastCross = gamepadTick.lastCircle = gamepadTick.lastOptions = gamepadTick.lastL1 = gamepadTick.lastR1 = false; }
    state.gamepadTimer = requestAnimationFrame(gamepadTick);
  };
  const start = async () => {
    bindTabs(); document.addEventListener('keydown', handleKey);
    try {
      const capabilities = await endpoint('/api/capabilities'); state.capabilities = cleanRecord(capabilities.capabilities); state.csrf = safeText(capabilities.csrf_token, '');
      $('.mode-label').textContent = capabilities.mode === 'console_preview' ? 'CONSOLE PREVIEW' : capabilities.mode === 'desktop_preparation' || capabilities.mode === undefined ? 'DESKTOP PREPARATION' : 'RUNTIME UNVERIFIED';
      if (capabilities.mode === 'console_preview') $('.brand').setAttribute('href', '/app');
      $('#lookup-submit').disabled = state.capabilities.nexus_catalog !== true;
      $('#reload-titles').hidden = capabilities.mode === 'console_preview';
      $('#footer-capabilities').textContent = state.capabilities.deploy === true ? 'Deployment capability reported' : 'Install and deploy unavailable';
      if (new URLSearchParams(location.search).get('account') === 'connected') history.replaceState(null, '', location.pathname);
      const nav = restoreNavigation();
      const titles = loadTitles(nav.target_ref);
      // The host serializes identity/refresh and provider requests. Complete
      // account verification before catalog requests compete for that lock.
      await loadAccount();
      await Promise.all([loadGames(), loadJobs(), loadInstalled(), titles]);
      if (nav.domain) {
        const game = state.games.find(item => item.domain_name === nav.domain);
        if (game) await chooseGame(game.domain_name);
        if (nav.mod && state.capabilities.nexus_catalog === true) {
          const mod = state.mods.find(item => idValue(item.mod_id) === nav.mod);
          if (mod) await chooseMod(mod.mod_id);
          else { $('#lookup-domain').value = nav.domain; $('#lookup-mod-id').value = nav.mod; await lookupMod(); }
          if (nav.file) state.file = state.files.find(item => idValue(item.file_id) === nav.file) || null;
        }
      }
      render();
      setInterval(() => { if (!document.hidden) loadJobs(); }, 2500);
      state.gamepadTimer = requestAnimationFrame(gamepadTick);
    } catch (error) { showError(error.message); }
  };
  document.addEventListener('DOMContentLoaded', start);
})();
