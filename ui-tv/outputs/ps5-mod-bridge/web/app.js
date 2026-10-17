(() => {
  'use strict';
  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const routes = window.ModBridgeConfig.routes;
  const state = {
    lastGame: null, catalogLoading: {}, catalogFailure: {}, jobsError: '',
    csrf: '', capabilities: {}, account: null, view: 'games', games: [], mods: [], files: [], jobs: [], installed: [],
    downloads: [], downloadsError: '', downloadsLoading: false,
    serviceRecords: null, serviceRecordsError: '', serviceRecordsLoading: false,
    game: null, mod: null, file: null, requests: { games: 0, mods: 0, files: 0, jobs: 0, installed: 0, titles: 0, lookup: 0 },
    titles: null, titlesLoading: false, titlesController: null, targetRef: null, lookupLoading: false,
    basket: [], basketEpoch: 0, filters: { games: '', mods: '' }, sort: { games: 0, mods: 0 },
    busy: false, installerBusy: false, jobsLoading: false, pendingIntent: null, modalReturnFocus: null, toastTimer: null, gamepadTimer: null,
  };
  const technicalText = /(?:\b0x[0-9a-f]{4,}\b|\b[A-Z0-9_-]{32,}\b|\b(?:PPSA|CUSA)\d+\b|(?:^|[\s"'(])(?:\/[^\s"'<>]+|[A-Z]:[\\/][^\s"'<>]+)|\b(?:kernel|syscall|sha256|quarantine|idempotency)\b)/i;
  const playerText = (value, fallback = 'Open More info for details.') => typeof value === 'string' && value.trim() && value.length <= 500 && !technicalText.test(value) ? value : fallback;
  const preferenceKey = 'modbridge.catalog.preferences.v1';
  const restorePreferences = () => {
    try {
      const raw = localStorage.getItem(preferenceKey);
      if (!raw || raw.length > 4096) return;
      const saved = JSON.parse(raw), filters = cleanRecord(saved.filters), sort = cleanRecord(saved.sort), game = saved.last_game;
      if (saved.version !== 1 || Object.keys(saved).sort().join(',') !== 'filters,last_game,sort,version' ||
          Object.keys(filters).sort().join(',') !== 'games,mods' || Object.keys(sort).sort().join(',') !== 'games,mods' ||
          !['games', 'mods'].every(kind => typeof filters[kind] === 'string' && filters[kind].length <= 160 && !/[\u0000-\u001f\u007f]/.test(filters[kind]) && [0, 1, 2].includes(sort[kind])) ||
          (game !== null && (Object.keys(cleanRecord(game)).sort().join(',') !== 'domain,name' || !domainValue(game.domain) || typeof game.name !== 'string' || game.name.length > 160))) return;
      state.filters = { ...filters }; state.sort = { ...sort }; state.lastGame = game ? { ...game } : null;
    } catch (_) { /* Browsing remains usable when preferences are unavailable. */ }
  };
  const savePreferences = () => {
    if (state.game && domainValue(state.game.domain_name)) state.lastGame = { domain: state.game.domain_name, name: playerText(state.game.name, 'your last game').slice(0, 160) };
    try { localStorage.setItem(preferenceKey, JSON.stringify({ version: 1, last_game: state.lastGame, filters: state.filters, sort: state.sort })); } catch (_) {}
  };
  const errorCopy = error => {
    const code = error?.code;
    const messages = {
      network: 'Could not connect to Nexus. Check your internet connection, then try again.',
      no_internet: 'No internet connection. Reconnect, then try again.',
      offline: 'No internet connection. Reconnect, then try again.',
      busy: 'The console is busy. Wait for the other task to finish, then try again.',
      session_required: 'Your app session expired. Reopen the app, then try again.',
      translation_unavailable: 'Mod checking is not available in this app yet. Choose another download or try again after support is added.',
      provider: 'Nexus could not finish the request. Try again later.',
      payload: 'Nexus sent a reply the app could not read. Try again, or choose another mod.',
      size_limit: 'The Nexus reply is too large for the app. Try another mod or a smaller download.',
      not_found: 'That item is no longer available here. Go back and choose another mod or download.',
      origin_refused: 'The app could not accept this connection. Reopen the app using its normal address, then try again.',
      intent_conflict: 'This download request changed. Clear the selection and choose the download again.',
      interrupted: 'The app stopped before this download finished. Open the mod and download it again.',
      redirect: 'The Nexus link could not be opened. Open the official mod page and try again.',
      invalid_input: 'This request could not be read. Check the Nexus mod link and try again.',
      unsupported_type: 'This game cannot load this kind of mod yet. Choose another mod or check back after support is added.',
      install_refused: 'The install was refused. Review More info before trying again.',
      wrong_mod_identity: 'Nexus returned a different mod. Check the mod link and try again.',
      timeout: 'Nexus is taking longer than expected. Try again.',
      reconnect_required: 'Your Nexus sign-in expired. Sign in again to continue.',
      credentials: 'Sign in to your own Nexus account to continue.',
      nexus_expired: 'Your Nexus sign-in expired. Sign in again to continue.',
      authorization_cancelled: 'Sign-in cancelled. You can sign in whenever you are ready.',
      authorization: 'Nexus could not finish sign-in. Start sign-in again.',
      download_authorization: 'This download needs approval on Nexus. Open its Nexus file page to continue.',
      rate_limit: 'Nexus needs a pause. Wait before trying again.',
      truncated: 'The download stopped before it finished. Download it again.',
      corrupt_download: 'The download is damaged. Download the mod again.',
      content_hidden: 'Nexus is hiding this mod with your account content settings. Check your settings on Nexus.',
      job_limit: 'The download queue is full. Finish your current downloads, then try again.',
      storage: 'The app could not save this download. Restart the app, then try again.',
      storage_unavailable: 'The app could not save this download. Restart the app, then try again.',
      destination: 'The app could not save this download. Restart the app, then try again.',
      stale_target_ref: 'The game list changed. Choose your game again.',
      game_running: 'Close the game first, then try again.',
      console_busy: 'The console is busy. Wait for the other task to finish, then try again.',
      configuration: 'Nexus sign-in is not available in this app yet. Try again after it is set up.',
      unsupported_service_contract: 'This version of the mod service does not work with the app yet. Update the app or service, then try again.',
      invalid_service_records: 'The saved mod records could not be read. Open Installed again to try again.',
      unsupported_mod_type: 'This game cannot load this kind of mod yet. Choose another mod or check back after support is added.'
    };
    if (Object.hasOwn(messages, code)) return messages[code];
    if (error instanceof TypeError) return 'Could not connect. Check your internet connection, then try again.';
    if (typeof error === 'string') return playerText(error, 'This step could not finish. Open More info, then try again.');
    return 'This step could not finish. Open More info, then try again.';
  };
  const skeleton = (kind, message) => `<div class="loading-state" data-catalog-loading="${kind}" role="status" aria-live="polite"><p>${esc(message)}</p><div class="loading-cards" aria-hidden="true"><span></span><span></span><span></span></div></div>`;
  const detailButton = (name, entries, attributes = '') => `<button class="secondary-button" ${attributes} data-more-info="${esc(JSON.stringify({ type: 'diagnostic', name, entries }))}">More info</button>`;
  const bindMoreInfo = root => $$('[data-more-info]', root).forEach(button => button.addEventListener('click', () => openDetails(JSON.parse(button.dataset.moreInfo), button)));
  const navKey = 'modbridge.navigation.v1';
  const selectionKey = 'modbridge.selection.v1';
  let selectionStorageReady = false;
  const cleanRecord = (value) => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
  const safeText = (value, fallback = 'Details unavailable') => typeof value === 'string' && value.trim() ? value : fallback;
  const esc = (value) => String(value).replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[ch]);
  // Focus belongs to the controller user even when a screen refresh replaces DOM.
  const visibleControl = el => el && el.matches('button,a[href],input,select,textarea,[tabindex]') && el.getClientRects().length && !el.closest('[hidden]') && !el.disabled && !el.closest('[inert]');
  const openDialog = () => document.querySelector('dialog[open]');
  const focusControl = el => {
    if (!visibleControl(el)) return false;
    el.focus({ preventScroll: true });
    el.scrollIntoView({ block: 'nearest', inline: 'nearest', behavior: 'instant' });
    return document.activeElement === el;
  };
  const focusContent = preferred => {
    const root = $('#' + state.view + '-view');
    const controls = $$('button,a[href],input,select,[tabindex="0"]', root).filter(visibleControl);
    const match = preferred && controls.find(el => el.dataset.card === preferred);
    return focusControl(match || controls.find(el => el.matches('#games-content .card,#games-content [data-file-details]')) || controls[0] || $('#tab-' + state.view));
  };
  const controlKey = el => {
    if (!el) return null;
    if (el.id) return ['id', el.id];
    for (const name of ['data-card', 'data-file-details', 'data-file-action', 'data-basket-add', 'data-basket-remove', 'data-breadcrumb', 'data-show-details', 'data-title-key', 'data-title-info', 'data-job-more', 'data-download-more', 'data-basket-info', 'data-record-more', 'data-error-action'])
      if (el.hasAttribute(name)) return [name, el.getAttribute(name)];
    return null;
  };
  let lastFocused = null, lastFocusKey = null, focusVersion = 0, inputVersion = 0, pendingTitleFocus = null;
  const watchFocus = () => {
    document.addEventListener('focusin', event => { focusVersion++; lastFocused = event.target; lastFocusKey = controlKey(event.target); });
    new MutationObserver(() => {
      if (!lastFocused || visibleControl(document.activeElement)) return;
      if (document.activeElement !== document.body && document.activeElement !== lastFocused) return;
      const scope = openDialog() || document;
      const replacement = lastFocusKey && $$('button,a[href],input,select,[tabindex="0"]', scope)
        .find(el => el.getAttribute(lastFocusKey[0]) === lastFocusKey[1] && visibleControl(el));
      if (replacement) focusControl(replacement);
      else if (openDialog()) focusControl($$('button,[tabindex="0"]', scope).find(visibleControl));
      else focusContent();
    }).observe(document.body, { childList: true, subtree: true, attributes: true, attributeFilter: ['hidden', 'disabled'] });
  };
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
    savePreferences();
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
  const showError = (error, retry) => {
    const banner = $('#status-banner');
    const code = typeof error?.code === 'string' && /^[a-z_]{1,64}$/.test(error.code) ? error.code : 'request_failed';
    if (['reconnect_required', 'credentials', 'nexus_expired'].includes(code)) {
      state.account = { status: 'error', live_service: 'unavailable', error: { code } };
      renderAccount(); renderBasket();
    }
    const entries = [['Error', code], ['Response', Number.isInteger(error?.status) ? error.status : 'No response']];
    if (code === 'unsupported_service_contract') entries.push(['Explanation', 'Unsupported mod service contract; status unverified. Expected modforge-service/1.']);
    if (code === 'invalid_service_records') entries.push(['Explanation', 'Malformed mod service records. The app has not accepted these records.']);
    if (code === 'wrong_mod_identity') entries.push(['Explanation', 'Nexus returned a different mod identity. The app did not open its files.']);
    banner.innerHTML = `<p>${esc(errorCopy(error))}</p><div class="error-actions">${typeof retry === 'function' ? '<button class="secondary-button" data-error-action="retry">Try again</button>' : ''}${detailButton('What happened', entries, 'data-error-action="more"')}</div>`;
    banner.hidden = false; bindMoreInfo(banner);
    $('[data-error-action="retry"]', banner)?.addEventListener('click', async event => {
      event.currentTarget.disabled = true; clearError(); await retry();
    });
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
    inventory_page_stale: 'The game list changed.',
    inventory_refresh_busy: 'A game list refresh is already in progress.',
    inventory_capture_in_progress: 'Your games are still being checked.',
    invalid_input: 'The game list request could not be verified.',
    session_required: 'The local app session has expired.',
    invalid_title_page: 'The game list response could not be verified.',
    title_page_resource_limit: 'The game list exceeds the response limits.',
    title_page_timeout: 'The game list request timed out.',
    title_page_cancelled: 'The game list request was cancelled.',
    request_failed: 'The game list request failed.',
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
    let path = refresh ? routes.titlePagesRefresh : routes.titlePages;
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
        path = routes.titlePage(page.next_cursor);
        continue;
      }
      if (index + 1 !== pageCount || items.length !== total) throw titleError('invalid_title_page');
      const currentBytes = await readTitleResponse(routes.titleCurrent(header.snapshot_id), ledger);
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
  let installerTargetKey = null;
  const syncInstallerTarget = () => {
    if (state.installerBusy) return;
    const game = selectedTitle(), titleId = observedText(game?.title_id, '');
    const nextKey = titleId ? `${state.targetRef.snapshot_id}:${state.targetRef.title_key}:${titleId}` : null;
    if (nextKey === installerTargetKey) return;
    installerTargetKey = nextKey;
    window.ModBridgeInstallerUI?.selectGame(nextKey ? { game_title_id: titleId, name: observedText(game.display_name, 'Selected game') } : null);
  };
  const renderTitles = () => {
    const inventory = state.titles, current = selectedTitle();
    const root = $('#title-list'), focusedKey = document.activeElement?.dataset.titleKey;
    const running = inventory?.source_scope === 'running_processes';
    $('#title-status').textContent = state.titlesLoading ? 'Checking your games…' : !inventory || inventory.status === 'unavailable' ? 'Games on this console are not available yet. You can still browse Nexus.' : inventory.status === 'partial' ? 'The game list is incomplete. Refresh it before choosing a game.' : inventory.status === 'error' ? 'The game list could not be checked. Refresh it to try again.' : inventory.items.length ? 'Choose the game you want to mod.' : running ? 'No open games found.' : 'No games found.';
    root.innerHTML = (inventory?.items || []).map(item => {
      const name = playerText(observedText(item.display_name, 'Unnamed game'), 'Unnamed game');
      const version = playerText(observedText(item.build_version, 'Version not checked'), 'Version not checked');
      const entries = [['Game name', observedText(item.display_name, 'Unavailable')], ['Game number', observedText(item.title_id, 'Unavailable')], ['Version', observedText(item.build_version, 'Unavailable')], ['Build check', JSON.stringify(item.build_fingerprint || {})]];
      return `<div class="title-entry"><button class="title-choice" data-title-key="${esc(item.title_key)}" aria-pressed="${current?.title_key === item.title_key}" ${selectableTitles() && !state.titlesLoading && !state.busy && !state.installerBusy ? '' : 'disabled'}><strong>${esc(name)}</strong><span>${esc(version)}</span><small>Mod support has not been checked yet.</small></button>${detailButton(name, entries, `data-title-info="${esc(item.title_key)}"`)}</div>`;
    }).join('');
    $$('[data-title-key]', root).forEach(button => button.addEventListener('click', () => chooseTitle(button.dataset.titleKey)));
    bindMoreInfo(root);
    $('#selected-target').textContent = current ? `Selected game · ${playerText(observedText(current.display_name, 'Selected game'), 'Selected game')}` : 'No game selected · browse and check downloads only';
    $('#clear-target').hidden = !current;
    $('#clear-target').disabled = state.busy || state.installerBusy || state.titlesLoading;
    $('#reload-titles').disabled = state.titlesLoading || state.busy || state.installerBusy;
    if (focusedKey) $$('[data-title-key]', root).find(button => button.dataset.titleKey === focusedKey)?.focus();
  };
  const chooseTitle = key => {
    if (!selectableTitles() || state.titlesLoading || state.busy || state.installerBusy || !state.titles.items.some(item => item.title_key === key)) return;
    const next = { snapshot_id: state.titles.snapshot_id, title_key: key };
    const restored = state.basket.filter(item => item.needsTargetSelection);
    if (!sameTarget(state.targetRef, next)) clearBasket();
    state.targetRef = next;
    // A restored target-specific request has no reusable target reference.
    // A fresh, explicit target choice creates a new request identity.
    if (restored.length) {
      const current = selectedTitle();
      state.basket = restored.map(item => ({ ...item, needsTargetSelection: false,
        intent: { ...item.intent, idempotency_key: makeIntentKey(), target_ref: { ...next } },
        target: playerText(observedText(current.display_name, 'Selected game'), 'Selected game'),
      }));
      saveBasket();
    }
    state.pendingIntent = null; saveNavigation(); renderTitles(); renderGames();
    syncInstallerTarget();
  };
  const clearTarget = (keepRestored = false) => { if (state.installerBusy) return; const focusKey = state.targetRef?.title_key; clearBasket(keepRestored === true); state.targetRef = null; state.pendingIntent = null; saveNavigation(); renderTitles(); renderGames(); syncInstallerTarget(); focusControl($$('[data-title-key]').find(el => el.dataset.titleKey === focusKey) || $('#reload-titles')); };
  const loadTitles = async (savedRef, refresh = false) => {
    const reference = targetReference(savedRef || state.targetRef);
    state.titlesController?.abort();
    const request = ++state.requests.titles;
    const controller = new AbortController(); state.titlesController = controller;
    clearBasket(true); state.titlesLoading = true; state.targetRef = null; state.pendingIntent = null; renderTitles(); renderGames(); syncInstallerTarget();
    try {
      const data = state.capabilities.title_inventory_paging === true ? await collectTitlePages(controller, refresh) :
        normalizedInventory(await endpoint(refresh ? routes.detectedTitlesRefresh : routes.detectedTitles, refresh ? { method: 'POST', body: '{}', signal: controller.signal } : { signal: controller.signal }));
      if (request !== state.requests.titles) return;
      state.titles = data;
      state.targetRef = selectableTitles() && reference?.snapshot_id === state.titles.snapshot_id && state.titles.items.some(item => item.title_key === reference.title_key) ? targetReference(reference) : null;
      if (!sameTarget(state.targetRef, reference)) state.pendingIntent = null;
      saveNavigation();
    } catch (error) {
      if (request !== state.requests.titles) return;
      state.titles = { status: state.capabilities.title_inventory_paging === true ? 'error' : 'unavailable', complete: false, snapshot_id: null, items: [] };
      state.targetRef = null; state.pendingIntent = null; saveNavigation();
      if (state.capabilities.title_inventory_paging === true) showError(`${titleMessages[error?.code] || titleMessages.request_failed} Refresh your games to try again.`);
    } finally { if (request === state.requests.titles) { state.titlesLoading = false; state.titlesController = null; renderTitles(); renderGames(); syncInstallerTarget(); } }
  };
  const refreshTitles = async () => { if (state.installerBusy) return; clearTarget(true); await loadTitles(null, true); };
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
    if (state.capabilities.nexus_catalog !== true) { showError('Finding a Nexus mod is not available in this app yet. Try again after Nexus is connected.'); return; }
    const identity = lookupIdentity();
    if (!identity) { showError('Enter an official Nexus mod link, or the game name and mod number from that link.'); return; }
    const request = ++state.requests.lookup;
    ++state.requests.mods; ++state.requests.files;
    state.catalogLoading.mods = false; state.catalogLoading.files = false;
    delete state.catalogFailure.mods; delete state.catalogFailure.files;
    state.lookupLoading = true; $('#lookup-status').textContent = 'Looking up mod…';
    try {
      const data = await endpoint(routes.mod(identity.domain, identity.modId));
      if (request !== state.requests.lookup) return;
      const mod = cleanRecord(data?.item);
      if (idValue(mod.mod_id) !== identity.modId || mod.game_domain !== identity.domain) throw Object.assign(new Error('Nexus returned a different mod identity.'), { code: 'wrong_mod_identity' });
      if (state.basket.some(item => item.intent.game !== identity.domain)) clearBasket();

      state.game = state.games.find(item => item.domain_name === identity.domain) || { domain_name: identity.domain, name: identity.domain };
      state.mods = [mod]; state.mod = null; state.file = null; state.files = []; state.pendingIntent = null;
      await chooseMod(mod.mod_id, true);
    } catch (error) { if (request === state.requests.lookup) showError(error, lookupMod); }
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
      box.innerHTML = `<span class="status-dot" aria-hidden="true"></span><span>Nexus · ${esc(playerText(person.display_name, 'Connected'))}</span><button class="account-action" id="disconnect-account">Disconnect</button>`;
      $('#disconnect-account').addEventListener('click', disconnectAccount);
    } else {
      const canSignIn = state.capabilities.nexus_auth_start === true;
      box.innerHTML = `<span class="status-dot" aria-hidden="true"></span><span>${['reconnect_required', 'credentials', 'nexus_expired'].includes(state.account?.error?.code) ? 'Nexus sign-in expired. Sign in again.' : state.account?.status === 'error' ? 'Nexus could not be checked. Try signing in again.' : canSignIn ? 'Nexus account not connected' : 'Nexus sign-in is not available in this app yet'}</span>${canSignIn ? '<button class="account-action" id="sign-in">Sign in</button>' : ''}`;
      $('#sign-in')?.addEventListener('click', signIn);
    }
  };
  const card = ({ title, description, meta, cls = '', action, details }) => `<article class="catalog-card"><button class="card ${cls}" ${details.type !== 'game' && !idValue(details.id) ? 'disabled' : ''} data-card="${esc(details.type)}:${esc(details.id)}" data-details="${esc(JSON.stringify(details))}"><span class="card-arrow" aria-hidden="true">›</span><p class="card-title">${esc(title)}</p><p class="card-meta">${esc(description || '')}</p>${meta ? `<span class="card-grid-meta">${meta}</span>` : ''}</button><button class="secondary-button card-info" data-show-details="${esc(details.type)}:${esc(details.id)}" data-details="${esc(JSON.stringify(details))}" aria-label="More info">More info</button></article>`;
  const bindCards = (root) => {
    $$('[data-show-details]', root).forEach(el => el.addEventListener('click', () => openDetails(JSON.parse(el.dataset.details), el)));
    $$('.card[data-details]', root).forEach(el => {
      const details = JSON.parse(el.dataset.details || '{}');
      el.addEventListener('click', () => details.type === 'game' ? chooseGame(details.id) : details.type === 'mod' ? chooseMod(details.id) : details.type === 'file' ? chooseFile(details.id) : openDetails(details, el));
      el.addEventListener('contextmenu', event => { event.preventDefault(); openDetails(details, el); });
      el.addEventListener('keydown', event => { if (event.key === 'ContextMenu') { event.preventDefault(); openDetails(details, el); } });
    });
  };
  const breadcrumb = () => `<div class="breadcrumbs"><button data-breadcrumb="games">Games</button>${state.game ? `<span>›</span><button data-breadcrumb="mods">${esc(playerText(state.game.name, 'Selected game'))}</button>` : ''}${state.mod ? `<span>›</span><span>${esc(playerText(state.mod.name, 'Selected mod'))}</span>` : ''}</div>`;
  const bindBreadcrumbs = (root) => $$('.breadcrumbs button', root).forEach(button => button.addEventListener('click', () => button.dataset.breadcrumb === 'games' ? resetSelection() : backToMods()));
  const verifiedAccount = () => state.account?.status === 'connected' && state.account?.live_service === 'verified' && !!state.account?.account;
  const selectionLabel = value => safeText(value).replace(/[\u0000-\u001f\u007f]/g, ' ').slice(0, 160);
  const savedSelection = item => ({
    game: item.intent.game, mod_id: item.intent.mod_id, file_id: item.intent.file_id,
    idempotency_key: item.intent.idempotency_key, name: selectionLabel(item.name), modName: selectionLabel(item.modName),
    needsTargetSelection: item.needsTargetSelection === true || !!item.intent.target_ref,
  });
  const validSavedSelection = row => {
    const fields = ['game', 'mod_id', 'file_id', 'idempotency_key', 'name', 'modName', 'needsTargetSelection'];
    return row && typeof row === 'object' && !Array.isArray(row) && Object.keys(row).length === fields.length &&
      fields.every(field => Object.hasOwn(row, field)) && !!domainValue(row.game) &&
      typeof row.mod_id === 'string' && !!idValue(row.mod_id) && typeof row.file_id === 'string' && !!idValue(row.file_id) &&
      typeof row.idempotency_key === 'string' && /^mb_[a-f0-9]{36}$/.test(row.idempotency_key) &&
      [row.name, row.modName].every(label => typeof label === 'string' && label.length > 0 && label.length <= 160 && !/[\u0000-\u001f\u007f]/.test(label)) &&
      typeof row.needsTargetSelection === 'boolean';
  };
  const saveBasket = () => {
    if (!selectionStorageReady) return;
    try {
      const items = state.basket.map(savedSelection);
      if (items.length && items.length <= 24 && items.every(validSavedSelection)) localStorage.setItem(selectionKey, JSON.stringify({ version: 1, items }));
      else localStorage.removeItem(selectionKey);
    } catch (_) {
      // If a replacement fails (for example quota exhaustion), remove the old
      // envelope so an already-admitted request is not restored from that copy.
      try { localStorage.removeItem(selectionKey); } catch (_) {}
    }
  };
  const restoreBasket = () => {
    try {
      const raw = localStorage.getItem(selectionKey);
      if (!raw) return;
      if (raw.length > 32768) throw new Error('Selection exceeds storage limit');
      const saved = JSON.parse(raw);
      if (!saved || typeof saved !== 'object' || Array.isArray(saved) || Object.keys(saved).length !== 2 ||
          saved.version !== 1 || !Array.isArray(saved.items) || saved.items.length > 24 || !saved.items.every(validSavedSelection)) throw new Error('Invalid saved selection');
      const keys = saved.items.map(row => basketKey(row.game, row.mod_id, row.file_id));
      if (new Set(keys).size !== keys.length || new Set(saved.items.map(row => row.idempotency_key)).size !== keys.length) throw new Error('Duplicate saved selection');
      state.basket = saved.items.map((row, index) => ({ key: keys[index],
        intent: { game: row.game, mod_id: row.mod_id, file_id: row.file_id, idempotency_key: row.idempotency_key },
        name: row.name, modName: row.modName, needsTargetSelection: row.needsTargetSelection,
        target: row.needsTargetSelection ? 'Choose your console game again before downloading.' : 'No game selected · check downloads only', error: '',
      }));
    } catch (_) { state.basket = []; }
    finally { selectionStorageReady = true; saveBasket(); }
  };
  const clearBasket = (keepRestored = false) => {
    state.basket = keepRestored ? state.basket.filter(item => item.needsTargetSelection) : []; state.pendingIntent = null; state.basketEpoch++;
    saveBasket(); renderBasket();
  };
  const catalogItems = (items, kind) => {
    const query = state.filters[kind].trim().toLocaleLowerCase();
    const filtered = items.filter(item => !query || [item.name, item.domain_name, item.author, item.summary].some(value => typeof value === 'string' && value.toLocaleLowerCase().includes(query)));
    return state.sort[kind] ? filtered.slice().sort((a, b) => safeText(a.name, '').localeCompare(safeText(b.name, '')) * (state.sort[kind] === 1 ? 1 : -1)) : filtered;
  };
  const renderCatalogTools = () => {
    let tools = $('#catalog-tools');
    if (!tools) {
      tools = document.createElement('div'); tools.id = 'catalog-tools'; tools.className = 'catalog-tools';
      tools.innerHTML = '<label class="catalog-search"><span id="catalog-search-label"></span><input id="catalog-search" type="search" maxlength="160" autocomplete="off" aria-labelledby="catalog-search-label"></label><button class="secondary-button catalog-sort" id="catalog-sort"></button><p class="catalog-count" id="catalog-count" aria-live="polite"></p>';
      $('#games-content').before(tools);
      $('#catalog-search').addEventListener('input', event => { state.filters[state.game ? 'mods' : 'games'] = event.target.value; savePreferences(); renderGames(); });
      $('#catalog-sort').addEventListener('click', () => { const kind = state.game ? 'mods' : 'games'; state.sort[kind] = (state.sort[kind] + 1) % 3; savePreferences(); renderGames(); });
    }
    tools.hidden = !!state.mod;
    const kind = state.game ? 'mods' : 'games', items = state[kind];
    let resume = $('#continue-game');
    const canResume = !state.game && state.lastGame && state.games.some(game => game.domain_name === state.lastGame.domain);
    if (!canResume) resume?.remove();
    else {
      if (!resume) { resume = document.createElement('button'); resume.id = 'continue-game'; resume.className = 'secondary-button'; $('.view-heading', $('#games-view')).appendChild(resume); resume.addEventListener('click', () => chooseGame(state.lastGame?.domain)); }
      resume.textContent = `Continue ${playerText(state.lastGame.name, 'your last game')}`;
    }
    const description = state.game ? 'loaded latest mods' : 'loaded games';
    $('#catalog-search-label').textContent = `Filter ${description}`;
    if ($('#catalog-search').value !== state.filters[kind]) $('#catalog-search').value = state.filters[kind];
    $('#catalog-sort').textContent = ['Sort: Nexus order', 'Sort: Name A–Z', 'Sort: Name Z–A'][state.sort[kind]];
    $('#catalog-count').textContent = `${catalogItems(items, kind).length} of ${items.length} ${description} · filters apply to this list only`;
  };
  const basketKey = (game, mod, file) => `${game}:${idValue(mod)}:${idValue(file)}`;
  const addToBasket = id => {
    const file = state.files.find(item => idValue(item.file_id) === idValue(id));
    if (!selectionStorageReady || !file || !state.game || !idValue(state.mod?.mod_id) || !idValue(id) || state.busy || state.titlesLoading ||
        !verifiedAccount() || state.capabilities.download_analyze !== true || state.capabilities.jobs !== true || (state.targetRef && !selectedTitle())) return;
    const key = basketKey(state.game.domain_name, state.mod.mod_id, id);
    if (state.basket.some(item => item.key === key)) return;
    if (state.basket.length >= 24) { showError('Choose up to 24 files in one selection.'); return; }
    const intent = { game: state.game.domain_name, mod_id: idValue(state.mod.mod_id), file_id: idValue(id), idempotency_key: makeIntentKey() };
    if (state.targetRef) intent.target_ref = { ...state.targetRef };
    const current = selectedTitle();
    state.basket.push({ key, intent, name: selectionLabel(file.name || file.file_name), modName: selectionLabel(state.mod.name), target: current ? playerText(observedText(current.display_name, 'Selected game'), 'Selected game') : 'No game selected · check downloads only', error: '' });
    saveBasket(); renderGames();
    $$('[data-file-details]').find(button => button.dataset.fileDetails === idValue(id))?.focus();
  };
  const renderBasket = () => {
    let root = $('#selection-basket');
    if (!root) {
      root = document.createElement('section'); root.id = 'selection-basket'; root.className = 'selection-basket';
      root.setAttribute('aria-labelledby', 'basket-heading'); $('#games-content').after(root);
    }
    root.hidden = state.basket.length === 0;
    if (!state.basket.length) { root.innerHTML = ''; return; }
    const canSubmit = selectionStorageReady && !state.basket.some(item => item.needsTargetSelection) && !state.busy && !state.titlesLoading && verifiedAccount() && state.capabilities.download_analyze === true && state.capabilities.jobs === true;
    root.innerHTML = `<div class="basket-heading"><h2 id="basket-heading">Selected files <span>(${state.basket.length}/24)</span></h2><p>Review your selected downloads and game before continuing.</p></div><div class="basket-list">${state.basket.map(item => `<article class="basket-item" data-basket-key="${esc(item.key)}"><div class="basket-file"><strong>${esc(playerText(item.name, 'Mod download'))}</strong><p>${esc(playerText(item.modName, 'Selected mod'))}</p><p class="basket-target">${esc(playerText(item.target, 'Choose your console game again before downloading.'))}</p>${item.error ? `<p class="basket-status" role="status">${esc(playerText(item.error, 'This download could not be requested. Try again.'))}</p>` : ''}${detailButton('Selected download', [['Nexus game', item.intent.game], ['Mod number', item.intent.mod_id], ['File number', item.intent.file_id], ['Error', item.errorCode || 'None']], `data-basket-info="${esc(item.key)}"`)}</div><button class="secondary-button" data-basket-remove="${esc(item.key)}" aria-label="Remove ${esc(playerText(item.name, 'this download'))}" ${state.busy ? 'disabled' : ''}>Remove</button></article>`).join('')}</div><p class="content-note">These downloads are checked only. Review an install separately in Installed.</p><div class="basket-actions"><button class="action-button" id="basket-submit" ${canSubmit ? '' : 'disabled'}>${state.busy ? 'Requesting…' : 'Download selected'}</button><button class="secondary-button" id="basket-clear" ${state.busy ? 'disabled' : ''}>Clear selection</button></div>`;
    bindMoreInfo(root);
    $$('[data-basket-remove]', root).forEach(button => button.addEventListener('click', () => { if (state.busy) return; state.basket = state.basket.filter(item => item.key !== button.dataset.basketRemove); state.pendingIntent = null; saveBasket(); renderGames(); if (!focusControl($('#basket-submit'))) focusContent(); }));
    $('#basket-clear').addEventListener('click', () => { if (!state.busy) { clearBasket(); renderGames(); $('#tab-games').focus(); } });
    $('#basket-submit').addEventListener('click', submitBasket);
  };
  const submitBasket = async () => {
    if (!selectionStorageReady || !state.basket.length || state.basket.some(item => item.needsTargetSelection) || state.busy || state.titlesLoading || !verifiedAccount() || state.capabilities.download_analyze !== true || state.capabilities.jobs !== true) return;
    if (state.basket.some(item => !sameTarget(item.intent.target_ref, state.targetRef) || (item.intent.target_ref && !selectedTitle()))) { clearBasket(); showError('The selected game changed. Select your downloads again.'); return; }
    const epoch = state.basketEpoch, items = state.basket.slice();
    state.busy = true; clearError(); renderGames(); renderTitles();
    try {
      for (const item of items) {
        if (epoch !== state.basketEpoch) break;
        const intent = item.intent;
        const body = { kind: 'download_analyze', game: intent.game, mod_id: Number(intent.mod_id), file_id: Number(intent.file_id), idempotency_key: intent.idempotency_key };
        if (intent.target_ref) body.target_ref = { ...intent.target_ref };
        try {
          await endpoint(routes.jobs, { method: 'POST', body: JSON.stringify(body) });
          if (epoch !== state.basketEpoch) break;
          state.basket = state.basket.filter(entry => entry !== item);
          if (state.pendingIntent === intent) state.pendingIntent = null;
          saveBasket(); renderBasket();
        } catch (error) {
          if (epoch !== state.basketEpoch) break;
          if (error.status === 409 && error.code === 'stale_target_ref') {
            clearTarget(); await loadTitles(null, true); showError('The game list changed. Choose your game and downloads again.');
          } else {
            item.errorCode = error.code;
            item.error = errorCopy(error);
            showError(error, submitBasket);
          }
          break;
        }
      }
      if (epoch === state.basketEpoch && !state.basket.length) showToast('Selected downloads requested.');
      await loadJobs();
    } finally { state.busy = false; renderGames(); renderTitles(); }
  };
  const renderGames = () => {
    const root = $('#games-content'), kind = state.mod ? 'files' : state.game ? 'mods' : 'games';
    renderCatalogTools(); renderBasket();
    $('#games-view h1').textContent = state.mod ? `Files for ${playerText(state.mod.name, 'this mod')}` : state.game ? `Latest mods for ${playerText(state.game.name, 'this game')}` : 'Choose a game';
    $('#games-view .subheading').textContent = state.mod ? 'Read about this mod, then choose its download.' : state.game ? 'Latest mods from Nexus. Filters search this loaded list only.' : 'Browse Nexus with your own account. Keep modded games offline.';
    if (state.catalogLoading[kind]) {
      root.innerHTML = (state.game ? breadcrumb() : '') + skeleton(kind, { games: 'Loading games…', mods: 'Loading mods…', files: 'Loading downloads…' }[kind]);
      bindBreadcrumbs(root); return;
    }
    if (state.catalogFailure[kind]) {
      const error = state.catalogFailure[kind];
      const retry = kind === 'games' ? loadGames : kind === 'mods' ? () => chooseGame(state.game.domain_name) : () => chooseMod(state.mod.mod_id);
      root.innerHTML = (state.game ? breadcrumb() : '') + `<section class="loading-state"><p>${esc(errorCopy(error))}</p><div class="error-actions"><button class="secondary-button" data-catalog-retry>Try again</button>${detailButton('List check', [['Error', error.code || 'request_failed'], ['Response', error.status || 'No response']])}</div></section>`;
      bindBreadcrumbs(root); bindMoreInfo(root);
      $('[data-catalog-retry]', root).addEventListener('click', () => { clearError(); retry(); });
      return;
    }
    const noMatches = '<div class="empty-state"><h2>No matches in this list</h2><p>Try fewer words, or clear the filter.</p><button class="secondary-button" id="clear-catalog-filter">Clear filter</button></div>';
    if (!state.game) {
      root.innerHTML = state.games.length ? catalogItems(state.games, 'games').map(game => card({ title: playerText(game.name, 'Game'), description: 'Browse mods from Nexus', meta: '<span class="pill">Nexus Mods</span>', details: { type: 'game', id: game.domain_name, name: game.name, domain_name: game.domain_name } })).join('') || noMatches : `<div class="empty-state"><h2>No games available</h2><p>${verifiedAccount() ? 'Nexus did not return any games right now. Try again later.' : 'Sign in to your own Nexus account, then try again.'}</p></div>`;
    } else if (!state.mod) {
      root.innerHTML = `${breadcrumb()}<h2 class="content-title">Latest mods from Nexus</h2>${state.mods.length ? catalogItems(state.mods, 'mods').map(mod => {
        const safeId = idValue(mod.mod_id);
        return card({ title: playerText(mod.name, 'Mod'), description: `By ${playerText(mod.author, 'an unknown author')} · ${playerText(mod.version, 'Version not listed')}`, meta: `<span class="pill">${safeId ? 'Not tested in this game yet' : 'This mod cannot be opened here yet'}</span>`, details: { type: 'mod', id: safeId, name: mod.name, author: mod.author, summary: mod.summary, description: mod.description, version: mod.version, source_url: mod.source_url } });
      }).join('') || noMatches : '<div class="empty-state"><h2>No recent mods available</h2><p>Nexus has no latest mods for this game right now. You can still open a mod using its official Nexus link below.</p></div>'}`;
    } else {
      const modPage = domainValue(state.game.domain_name) && idValue(state.mod.mod_id) ? window.ModBridgeConfig.nexus.modPage(state.game.domain_name, state.mod.mod_id) : '';
      const overview = `<section class="mod-overview"><h2>About this mod</h2><p>By ${esc(playerText(state.mod.author, 'an unknown author'))}</p><p>${esc(playerText(state.mod.summary || state.mod.description, 'Open More info or the Nexus page to read about this mod.'))}</p><p class="content-note">Images are not available in this app yet.</p><div class="error-actions">${modPage ? `<a class="secondary-button" href="${esc(modPage)}" target="_blank" rel="noopener noreferrer">Open mod on Nexus</a>` : ''}${detailButton(playerText(state.mod.name, 'Mod details'), [['Mod name', state.mod.name], ['Description', state.mod.description || state.mod.summary || 'Not provided'], ['Version', state.mod.version || 'Not provided']])}</div></section>`;
      root.innerHTML = `${breadcrumb()}${overview}<h2 class="content-title">Available files</h2><p class="content-note">These downloads are checked only. Review an install separately in Installed.</p>${state.files.length ? state.files.map(file => {
        const safeId = idValue(file.file_id), available = state.capabilities.download_analyze === true && state.capabilities.jobs === true && !!safeId;
        const enabled = selectionStorageReady && available && !state.titlesLoading && !state.busy;
        const selected = state.basket.some(item => item.key === basketKey(state.game.domain_name, state.mod.mod_id, safeId));
        const label = !safeId ? 'This download cannot be opened yet' : available ? 'Download and check' : 'Downloads not available yet';
        return `<article class="card file-card" data-file-card="${esc(safeId || 'unavailable')}"><span class="file-symbol" aria-hidden="true">↓</span><div class="file-main"><p class="card-title">${esc(playerText(file.name || file.file_name, 'Mod download'))}</p><p class="card-meta">${esc(playerText(file.version, 'Version not listed'))} · ${Number.isFinite(Number(file.size_kb)) && Number(file.size_kb) >= 0 ? `${esc(file.size_kb)} KB` : 'Size not listed'}</p></div><button class="secondary-button details-file" aria-label="More info about ${esc(playerText(file.name || file.file_name, 'this download'))}" data-file-details="${esc(safeId)}">More info</button><button class="secondary-button select-file" data-basket-add="${esc(safeId)}" aria-pressed="${selected}" ${enabled && verifiedAccount() && !selected ? '' : 'disabled'}>${selected ? 'Selected' : 'Add to selection'}</button><button class="action-button download-file" data-file-action="${esc(safeId)}" ${enabled ? '' : 'disabled'}>${label}</button></article>`;
      }).join('') : '<div class="empty-state"><h2>No files available</h2><p>Nexus did not return any downloads for this mod.</p></div>'}`;
    }
    bindBreadcrumbs(root); bindCards(root); bindMoreInfo(root);
    $('#clear-catalog-filter')?.addEventListener('click', () => { state.filters[state.game ? 'mods' : 'games'] = ''; savePreferences(); renderGames(); focusControl($('#catalog-search')); });
    $$('[data-basket-add]', root).forEach(button => button.addEventListener('click', () => addToBasket(button.dataset.basketAdd)));
    $$('[data-file-action]', root).forEach(button => button.addEventListener('click', () => { const file = state.files.find(item => String(item.file_id) === button.dataset.fileAction); if (file) chooseFile(file.file_id, button); }));
    $$('[data-file-details]', root).forEach(button => button.addEventListener('click', () => { const file = state.files.find(item => String(item.file_id) === button.dataset.fileDetails); if (file) openDetails({ type: 'file', id: file.file_id, name: file.name || file.file_name, file_name: file.file_name, version: file.version, size_kb: file.size_kb, description: file.description }, button); }));
  };
  const savedDownloadsMarkup = () => {
    if (state.capabilities.retained_downloads !== true) return '';
    return '<section aria-labelledby="saved-downloads-heading"><h2 class="content-title" id="saved-downloads-heading">Saved downloads</h2><p class="content-note">Original downloads saved for a later install review.</p>' +
      (state.downloadsError ? '<p role="status">Saved downloads unavailable. Try opening Activity again.</p>' : state.downloadsLoading && !state.downloads.length ? skeleton('downloads', 'Checking saved downloads…') : state.downloads.length ? state.downloads.map((item, index) => `<article class="card job-card" data-download-id="${esc(item.download_id)}"><div><p class="card-title">Saved download ${index + 1}</p><span class="pill">Downloaded</span></div><div class="job-details">Original download saved · ${esc(item.bytes)} bytes<div>Available for a later install review.</div>${detailButton('Saved download', [['Nexus game', item.game], ['Mod number', item.mod_id], ['File number', item.file_id], ['Archive hash', item.sha256]], `data-download-more="${esc(item.download_id)}"`)}</div></article>`).join('') : '<p>No saved downloads.</p>') + '</section>';
  };
  const loadDownloads = async () => {
    if (state.capabilities.retained_downloads !== true) { state.downloads = []; state.downloadsError = ''; return; }
    if (state.downloadsLoading) return;
    state.downloadsLoading = true; if (!state.downloads.length) renderActivity();
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    try {
      const data = await endpoint(routes.downloads, { signal: controller.signal, cache: 'no-store' });
      if (!data || !Array.isArray(data.items) || data.items.length > 1000) throw new Error('Invalid saved downloads.');
      const seen = new Set();
      const next = data.items.map(item => {
        if (!item || typeof item !== 'object' || Array.isArray(item) ||
            typeof item.download_id !== 'string' || !/^dl-[a-f0-9]{64}$/.test(item.download_id) || seen.has(item.download_id) ||
            item.state !== 'downloaded' || item.retained !== true || !domainValue(item.game) ||
            !idValue(item.mod_id) || !idValue(item.file_id) || typeof item.sha256 !== 'string' || !/^[a-f0-9]{64}$/.test(item.sha256) ||
            !Number.isSafeInteger(item.bytes) || item.bytes < 0) throw new Error('Invalid saved download.');
        seen.add(item.download_id);
        return { download_id: item.download_id, game: item.game, mod_id: idValue(item.mod_id), file_id: idValue(item.file_id), sha256: item.sha256, bytes: item.bytes };
      });
      if (JSON.stringify(next) !== JSON.stringify(state.downloads) || state.downloadsError) { state.downloads = next; state.downloadsError = ''; renderActivity(); }
    } catch (_) { state.downloads = []; state.downloadsError = 'unavailable'; renderActivity(); }
    finally { clearTimeout(timeout); state.downloadsLoading = false; if (!state.downloads.length || !state.jobs.length) renderActivity(); }
  };
  const renderActivity = () => {
    const root = $('#activity-content'), focused = document.activeElement;
    const focusedJobId = focused?.closest('[data-job-id]')?.dataset.jobId || '', focusedKey = focused?.dataset.focusKey || '';
    root.innerHTML = '<h2 class="content-title">Downloads and checks</h2>' + (!state.jobs.length ? state.jobsLoading ? skeleton('jobs', 'Checking your downloads…') : state.jobsError ? '<p>Downloads could not be checked. Open Activity again to try again.</p>' : '<div class="empty-state"><h3>No current downloads</h3><p>Choose a mod download to check it before installing.</p></div>' : state.jobs.map(job => {
      const stateLabel = ({ queued: 'Waiting', downloading: 'Downloading', analyzing: 'Checking download', analyzed: 'Download checked', needs_nexus_authorization: 'Needs your approval on Nexus', failed: 'Could not complete', interrupted: 'Interrupted' })[job.status] || 'Status not available';
      const retention = job.retention?.state === 'saved' && /^dl-[a-f0-9]{64}$/.test(job.retention.download_id || '') ? '<div>Original download saved.</div>' : job.retention?.state === 'failed' ? '<div>Download not saved. Try downloading it again.</div>' : '';
      const bytes = Number(job.bytes_downloaded), sourceUrl = job.status === 'needs_nexus_authorization' ? nexusFilePage(job) : '';
      const authorization = job.status === 'needs_nexus_authorization' ? `<p>This download needs approval on Nexus. Use your own account and choose Mod Manager Download on its file page.</p><p>Returning the approved download to ModBridge is not available yet.</p>${sourceUrl ? `<a class="secondary-button source-link" href="${esc(sourceUrl)}" target="_blank" rel="noopener noreferrer" data-focus-key="nexus-source">Open Nexus file page</a>` : '<p>Go back to the mod and choose the download again. Its Nexus file page is not available for this request.</p>'}` : '';
      const report = validatedAnalysis(job.analysis_report);
      const unsupported = report?.compatibility_state === 'unsupported';
      const explanation = unsupported ? 'This mod needs software that is not available in this game yet.' : 'Not tested in this game yet.';
      const message = job.status === 'analyzed' ? 'The download was checked. Nothing was installed.' : job.status === 'failed' ? errorCopy(job.error) : job.status === 'interrupted' ? 'The app stopped before this download finished. Open the mod and download it again.' : job.status === 'needs_nexus_authorization' ? '' : 'Your download is being checked before an install can be reviewed.';
      const entries = [['Name', job.label], ['Request', job.id], ['Nexus game', job.game], ['Mod number', job.mod_id], ['File number', job.file_id], ['Archive hash', job.sha256], ['Error', job.error?.code], ['Check details', report ? JSON.stringify({ ...report, explanations: report.reason_codes.map(code => analysisReasons[code]) }, null, 2) : 'No game compatibility evidence yet']];
      return `<article class="card job-card" data-job-id="${esc(safeText(job.id, ''))}"><div><p class="card-title">${esc(playerText(job.label, 'Mod download'))}</p><div class="job-state"><span class="pill">${esc(stateLabel)}</span><span>${esc(explanation)}</span></div></div><div class="job-state">${Number.isFinite(bytes) && bytes >= 0 ? `${esc(bytes)} bytes` : ''}</div><div class="job-details"><p>${esc(message)}</p>${authorization}${retention}${detailButton('Download details', entries, `data-job-more="${esc(safeText(job.id, ''))}" data-focus-key="job-more"`)}</div></article>`;
    }).join('')) + savedDownloadsMarkup();
    bindMoreInfo(root);
    if (focusedJobId && focusedKey) {
      const replacement = $$('[data-job-id]', root).find(card => card.dataset.jobId === focusedJobId);
      if (replacement) $$('[data-focus-key]', replacement).find(element => element.dataset.focusKey === focusedKey)?.focus();
    }
  };
  const renderInstalled = () => {
    const root = $('#installed-content');
    if (state.capabilities.mod_service_records === true) {
      root.className = 'stack'; const records = state.serviceRecords;
      if (!records) { root.innerHTML = state.serviceRecordsLoading ? skeleton('records', 'Checking saved mod records…') : '<div class="empty-state"><h2>Saved mod records unavailable</h2><p>Open Installed again to try again. The current game status has not been checked.</p></div>'; return; }
      const runtimeCopy = { 'not-started': 'Not started', 'activated': 'Mod support started', 'engine-ready': 'Game connection ready', 'mods-started': 'Mods started', 'failed': 'Needs attention', 'stopped': 'Stopped' };
      root.innerHTML = '<h2 class="content-title">Saved mod records</h2><p class="content-note">These are saved records. Check Game mods above for the selected game.</p>' + (records.mods.length ? records.mods.map(item => `<article class="card"><div><p class="card-title">${esc(playerText(item.source.name, 'Saved mod'))}</p><p class="card-meta"><span class="pill">${esc({ downloaded: 'Downloaded', prepared: 'Prepared', installed: 'Installed' }[item.state])}</span></p>${item.state === 'prepared' ? `<p>${item.blockers?.length ? 'This mod needs attention before it can be installed. Open More info.' : 'This mod has been prepared. Installation support has not been checked yet.'}</p>` : ''}${detailButton('Saved mod details', [['Mod name', item.source.name], ['Game number', item.title_id], ['Download record', item.download_id], ['Preparation details', JSON.stringify(item.blockers || [])]], `data-record-more="mod:${esc(item.download_id)}:${esc(item.title_id)}"`)}</div></article>`).join('') : '<p>No saved mod records.</p>') + records.games.map(game => `<article class="card"><div><p class="card-title">Game record</p><p>${esc(game.installed.mods.length)} installed mod(s)</p><p>Last report: ${esc(runtimeCopy[game.runtime.state] || 'Not checked')}. Current game status has not been checked.</p>${detailButton('Game record details', [['Game number', game.title_id], ['Installed list', JSON.stringify(game.installed)], ['Last runtime report', JSON.stringify(game.runtime)]], `data-record-more="game:${esc(game.title_id)}"`)}</div></article>`).join('');
      bindMoreInfo(root); return;
    }
    if (!state.installed.length) { root.className = 'empty-state'; root.innerHTML = '<span class="empty-icon" aria-hidden="true">✓</span><h2>No confirmed installations</h2><p>ModBridge has not installed anything. Mods have not been tested in a game yet.</p>'; return; }
    root.className = 'stack'; root.innerHTML = '<h2 class="content-title">Saved installations</h2>' + state.installed.map(item => `<article class="card"><div><p class="card-title">${esc(playerText(item.name, 'Installed mod'))}</p><p class="card-meta">Installation confirmed. Check Game mods above for play results.</p>${detailButton('Installation details', [['Mod name', item.name], ['Archive hash', item.sha256]], `data-record-more="confirmed:${esc(item.sha256)}"`)}</div></article>`).join(''); bindMoreInfo(root);
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
    if (record.schema_version !== 1 || record.operation !== 'archive_inventory' || !['desktop_preparation', 'ps5_native'].includes(record.execution_context) || !['supported', 'unverified'].includes(record.operation_state) || !['unverified', 'unsupported'].includes(record.compatibility_state) || !Array.isArray(reasons) || reasons.length < 1 || reasons.length > 32 || reasons.some(code => typeof code !== 'string' || !Object.hasOwn(analysisReasons, code)) || reasons.some((code, index) => index > 0 && code <= reasons[index - 1]) || !reasons.includes('platform_acceptance_missing') || install.state !== 'unsupported' || install.reason !== 'install_operation_unavailable' || !Array.isArray(install.scope) || install.scope.length) return null;
    const unsupported = reasons.includes('windows_runtime_unavailable') || reasons.includes('managed_runtime_unavailable');
    return (record.compatibility_state === 'unsupported') === unsupported ? record : null;
  };
  const render = () => { renderAccount(); renderGames(); renderTitles(); renderActivity(); renderInstalled(); };
  const apiItems = data => Array.isArray(data?.items) ? data.items : [];
  const nexusFilePage = job => {
    if (typeof job?.game !== 'string' || !/^[a-z0-9][a-z0-9_-]{0,79}$/.test(job.game)) return '';
    const modId = idValue(job.mod_id), fileId = idValue(job.file_id);
    return modId && fileId ? window.ModBridgeConfig.nexus.filePage(job.game, modId, fileId) : '';
  };
  const loadGames = async () => {
    if (state.capabilities.nexus_catalog !== true) { state.games = []; renderGames(); return; }
    const request = ++state.requests.games;
    state.catalogLoading.games = true; delete state.catalogFailure.games; renderGames();
    try {
      const data = await endpoint(routes.games);
      if (request !== state.requests.games) return;
      state.games = apiItems(data); state.catalogLoading.games = false; renderGames();
    } catch (error) {
      if (request !== state.requests.games) return;
      state.games = []; state.catalogLoading.games = false; state.catalogFailure.games = error;
      showError(error, loadGames); renderGames();
    }
  };
  const chooseGame = async domain => {
    const game = state.games.find(item => item.domain_name === domain);
    if (!game || !domainValue(domain) || state.capabilities.nexus_catalog !== true) return;
    cancelLookup(); state.pendingIntent = null; clearError();
    if (state.basket.some(item => item.intent.game !== domain)) clearBasket();
    state.game = game; state.mod = null; state.file = null; state.mods = []; state.files = []; saveNavigation();
    const request = ++state.requests.mods; state.requests.files++; state.catalogLoading.files = false; delete state.catalogFailure.files;
    state.catalogLoading.mods = true; delete state.catalogFailure.mods; renderGames();
    await Promise.resolve(); const focusEpoch = focusVersion;
    try {
      const data = await endpoint(routes.gameMods(game.domain_name));
      if (request !== state.requests.mods || state.game?.domain_name !== game.domain_name) return;
      const ownsFocus = focusVersion === focusEpoch;
      state.mods = apiItems(data); state.catalogLoading.mods = false; renderGames();
      if (ownsFocus && state.view === 'games') focusContent();
    } catch (error) {
      if (request !== state.requests.mods) return;
      state.catalogLoading.mods = false; state.catalogFailure.mods = error; showError(error, () => chooseGame(domain)); renderGames();
    }
  };
  const chooseMod = async (id, fromLookup = false) => {
    const mod = state.mods.find(item => String(item.mod_id) === String(id)); if (!mod || !state.game) return;
    if (!fromLookup) cancelLookup();
    state.pendingIntent = null; clearError();
    state.mod = mod; state.file = null; state.files = []; saveNavigation();
    const request = ++state.requests.files, game = state.game.domain_name;
    state.catalogLoading.files = true; delete state.catalogFailure.files; renderGames();
    await Promise.resolve(); const focusEpoch = focusVersion;
    try {
      const data = await endpoint(routes.modFiles(game, mod.mod_id));
      if (request !== state.requests.files || state.game?.domain_name !== game || String(state.mod?.mod_id) !== String(mod.mod_id)) return;
      const ownsFocus = focusVersion === focusEpoch;
      state.files = apiItems(data); state.catalogLoading.files = false; renderGames();
      if (ownsFocus && state.view === 'games') focusContent();
    } catch (error) {
      if (request !== state.requests.files) return;
      state.catalogLoading.files = false; state.catalogFailure.files = error; showError(error, () => chooseMod(id)); renderGames();
    }
  };
  const chooseFile = async (id, trigger) => {
    const file = state.files.find(item => idValue(item.file_id) && idValue(item.file_id) === idValue(id));
    if (!selectionStorageReady || !file || state.capabilities.download_analyze !== true || state.capabilities.jobs !== true || !state.game || !state.mod || state.busy || state.titlesLoading || (state.targetRef && !selectedTitle())) return;
    state.file = file; saveNavigation();
    renderGames();
    if (!idValue(state.mod.mod_id) || !idValue(file.file_id)) return;
    const key = basketKey(state.game.domain_name, state.mod.mod_id, file.file_id);
    let item = state.basket.find(entry => entry.key === key);
    if (item?.needsTargetSelection) { showError('Choose your console game again before downloading this saved selection.'); return; }
    if (item && !sameTarget(item.intent.target_ref, state.targetRef)) { showError('The selected game changed. Select your download again.'); return; }
    if (!item) {
      if (state.basket.length >= 24) { showError('Choose up to 24 files in one selection.'); return; }
      const intent = { game: state.game.domain_name, mod_id: idValue(state.mod.mod_id), file_id: idValue(file.file_id), idempotency_key: makeIntentKey() };
      if (state.targetRef) intent.target_ref = { ...state.targetRef };
      const current = selectedTitle();
      item = { key, intent, name: selectionLabel(file.name || file.file_name), modName: selectionLabel(state.mod.name),
        target: current ? playerText(observedText(current.display_name, 'Selected game'), 'Selected game') : 'No game selected · check downloads only', error: '' };
      state.basket.push(item);
    }
    state.pendingIntent = item.intent;
    saveBasket();
    await submitIntent(trigger);
  };
  const makeIntentKey = () => `mb_${Array.from(crypto.getRandomValues(new Uint8Array(18)), n => n.toString(16).padStart(2, '0')).join('')}`;
  const submitIntent = async trigger => {
    const intent = state.pendingIntent; if (!selectionStorageReady || !intent || state.busy || state.titlesLoading || (intent.target_ref && (!sameTarget(intent.target_ref, state.targetRef) || !selectedTitle()))) return;
    state.busy = true; renderGames(); renderTitles();
    const button = trigger?.isConnected ? trigger : $$('[data-file-action]').find(el => el.dataset.fileAction === intent.file_id);
    if (button) { button.disabled = true; button.textContent = 'Requesting…'; }
    try {
      const body = { kind: 'download_analyze', game: intent.game, mod_id: Number(intent.mod_id), file_id: Number(intent.file_id), idempotency_key: intent.idempotency_key };
      if (intent.target_ref) body.target_ref = { ...intent.target_ref };
      const data = await endpoint(routes.jobs, { method: 'POST', body: JSON.stringify(body) });
      state.basket = state.basket.filter(item => item.intent !== intent);
      if (state.pendingIntent === intent) state.pendingIntent = null;
      saveBasket(); showToast(data.status === 'queued' ? 'Download requested.' : 'Request recorded.'); await loadJobs();
    } catch (error) {
      if (error.status === 409 && error.code === 'stale_target_ref') {
        clearTarget(); await loadTitles(null, true);
        showError('The game list changed. Choose your game again before downloading.');
      } else { showError(error, () => submitIntent()); }
    }
    finally { state.busy = false; renderGames(); renderTitles(); }
  };
  const loadAccount = async () => {
    if (state.capabilities.account_status !== true) { state.account = { status: 'unavailable' }; renderAccount(); return; }
    try { state.account = await endpoint(routes.account); }
    catch (error) { state.account = { status: 'error', error: { code: error.code } }; }
    renderAccount();
  };
  const signIn = async () => {
    try {
      const result = await endpoint(routes.authStart, { method: 'POST', body: '{}' });
      const url = new URL(result.authorization_url);
      if (url.origin !== window.ModBridgeConfig.nexus.authorizationOrigin || url.username || url.password) throw Object.assign(new Error('Nexus sign-in could not be started.'), { code: 'authorization' });
      window.location.assign(url.href);
    } catch (error) { showError(error); }
  };
  const disconnectAccount = async () => {
    if (state.busy) return;
    state.busy = true;
    try { await endpoint(routes.authDisconnect, { method: 'POST', body: '{}' }); clearBasket(); state.pendingIntent = null; state.account = { status: 'disconnected' }; renderAccount(); showToast('Nexus account disconnected.'); }
    catch (error) { showError(error); }
    finally { state.busy = false; renderGames(); renderTitles(); }
  };
  const loadJobs = async () => {
    if (state.capabilities.jobs !== true) { state.jobs = []; renderActivity(); return; }
    if (state.jobsLoading) return;
    state.jobsLoading = true; if (!state.jobs.length) renderActivity();
    const request = ++state.requests.jobs;
    try {
      const data = await endpoint(routes.jobs);
      if (request === state.requests.jobs) {
        const nextJobs = apiItems(data);
        const stableJson = value => JSON.stringify(value, (_key, entry) => entry && typeof entry === 'object' && !Array.isArray(entry) ? Object.fromEntries(Object.entries(entry).sort(([left], [right]) => left.localeCompare(right))) : entry);
        if (stableJson(nextJobs) !== stableJson(state.jobs) || state.jobsError) { state.jobs = nextJobs; state.jobsError = ''; renderActivity(); }
      }
    } catch (error) { if (request === state.requests.jobs) { state.jobsError = errorCopy(error); showError(error, loadJobs); } }
    finally { state.jobsLoading = false; if (!state.jobs.length) renderActivity(); }
  };
  const serviceResult = data => {
    if (!data || data.ok !== true || !data.result || typeof data.result !== 'object' || Array.isArray(data.result)) throw Object.assign(new Error('Malformed service reply.'), { code: 'invalid_service_records' });
    return data.result;
  };
  const validateServiceRecords = data => {
    const value = serviceResult(data);
    const record = item => item && typeof item === 'object' && !Array.isArray(item);
    const text = item => typeof item === 'string' && !!item.trim();
    if (!Array.isArray(value.mods) || !Array.isArray(value.games) ||
        !value.mods.every(item => record(item) && text(item.download_id) && text(item.title_id) && ['downloaded', 'prepared', 'installed'].includes(item.state) && record(item.source) &&
          (item.prepared_id === undefined || text(item.prepared_id)) && (item.source.name === undefined || typeof item.source.name === 'string') &&
          (item.blockers === undefined || (Array.isArray(item.blockers) && item.blockers.every(blocker => record(blocker) && text(blocker.code) && typeof blocker.message === 'string')))) ||
        !value.games.every(item => record(item) && text(item.title_id) && record(item.installed) && Array.isArray(item.installed.mods) && item.installed.mods.every(text) &&
          Number.isSafeInteger(item.installed.generation) && item.installed.generation >= 0 && text(item.installed.load_list_sha256) && record(item.runtime) &&
          ['not-started', 'activated', 'engine-ready', 'mods-started', 'failed', 'stopped'].includes(item.runtime.state) &&
          (item.runtime.pid === undefined || (Number.isSafeInteger(item.runtime.pid) && item.runtime.pid >= 0)) &&
          (item.runtime.code === undefined || Number.isSafeInteger(item.runtime.code)) &&
          (item.runtime.session_ms === undefined || (Number.isSafeInteger(item.runtime.session_ms) && item.runtime.session_ms >= 0)) &&
          (item.runtime.detail === undefined || typeof item.runtime.detail === 'string'))) throw new Error('Malformed service records; status unverified.');
    return value;
  };
  const loadInstalled = async () => {
    if (state.serviceRecordsLoading) return;
    const request = ++state.requests.installed;
    if (state.capabilities.mod_service_records === true) {
      state.serviceRecordsLoading = true; if (!state.serviceRecords) renderInstalled();
      try {
        const description = serviceResult(await endpoint(routes.modService));
        if (description.contract !== 'modforge-service/1') throw Object.assign(new Error('Unsupported mod service contract; status unverified.'), { code: 'unsupported_service_contract' });
        const records = validateServiceRecords(await endpoint(routes.modServiceRecords));
        if (request !== state.requests.installed) return;
        if (JSON.stringify(records) !== JSON.stringify(state.serviceRecords) || state.serviceRecordsError) { state.serviceRecords = records; state.serviceRecordsError = ''; renderInstalled(); }
      } catch (error) {
        if (request === state.requests.installed) { state.serviceRecords = null; state.serviceRecordsError = 'Saved mod records could not be checked.'; showError(error, loadInstalled); }
      } finally { state.serviceRecordsLoading = false; if (!state.serviceRecords) renderInstalled(); }
      return;
    }
    if (state.capabilities.installed_records !== true) { state.installed = []; renderInstalled(); return; }
    try { const data = await endpoint(routes.installed); if (request !== state.requests.installed) return; state.installed = apiItems(data).filter(item => item?.transaction_confirmed === true && typeof item.sha256 === 'string' && /^[a-f0-9]{64}$/i.test(item.sha256)); renderInstalled(); }
    catch (error) { if (request === state.requests.installed) { state.installed = []; renderInstalled(); showError(error, loadInstalled); } }
  };
  const cancelLookup = () => { ++state.requests.lookup; state.lookupLoading = false; $('#lookup-status').textContent = ''; };
  const resetSelection = () => { const parent = state.game && 'game:' + state.game.domain_name; cancelLookup(); state.catalogLoading.mods = false; state.catalogLoading.files = false; delete state.catalogFailure.mods; delete state.catalogFailure.files; state.pendingIntent = null; state.game = null; state.mod = null; state.file = null; state.mods = []; state.files = []; ++state.requests.mods; ++state.requests.files; saveNavigation(); renderGames(); focusContent(parent); };
  const backToMods = () => { const parent = state.mod && 'mod:' + state.mod.mod_id; cancelLookup(); state.catalogLoading.mods = false; state.catalogLoading.files = false; delete state.catalogFailure.mods; delete state.catalogFailure.files; state.pendingIntent = null; state.mod = null; state.file = null; state.files = []; ++state.requests.files; saveNavigation(); renderGames(); focusContent(parent); };
  const openDetails = (item, trigger) => {
    inputVersion++;
    state.modalReturnFocus = trigger || document.activeElement;
    $('#details-title').textContent = playerText(item.name || item.title, 'More info');
    const fields = item.type === 'diagnostic' ? item.entries : item.type === 'game' ? [['Nexus domain', item.domain_name], ['Catalog ID', item.id]] : item.type === 'mod' ? [['Author', item.author], ['Version', item.version], ['Summary', item.summary], ['Description', item.description], ['Nexus page', item.source_url]] : [['File name', item.file_name], ['Version', item.version], ['Size', Number.isFinite(Number(item.size_kb)) ? `${item.size_kb} KB` : 'Unavailable'], ['Description', item.description]];
    const originalName = item.name || item.title;
    if (typeof originalName === 'string' && playerText(originalName, '') !== originalName) fields.unshift(['Name', originalName]);
    $('#details-content').innerHTML = fields.filter(([, value]) => value !== undefined && value !== null && String(value).trim() !== '').map(([label, value]) => `<div class="detail-row"><div class="detail-label">${esc(label)}</div><div class="detail-value">${esc(value)}</div></div>`).join('') || '<p class="detail-value">No additional details available.</p>';
    $('#details-content').scrollTop = 0;
    $('#details-dialog').showModal(); focusControl($('[data-close-dialog]', $('#details-dialog')));
  };
  const closeDetails = () => {
    const dialog = $('#details-dialog'); if (dialog.open) dialog.close();
    const key = controlKey(state.modalReturnFocus);
    const replacement = key && $$('button,a[href],input,select,[tabindex="0"]').find(el => el.getAttribute(key[0]) === key[1] && visibleControl(el));
    if (!focusControl(state.modalReturnFocus?.isConnected ? state.modalReturnFocus : replacement)) focusControl($(`#tab-${state.view}`));
    state.modalReturnFocus = null;
  };
  const handleBack = () => {
    inputVersion++;
    if ($('#details-dialog').open) { closeDetails(); return; }
    if (window.ModBridgeInstallerUI?.handleBack()) return;
    const dialog = openDialog();
    if (dialog) { const event = new Event('cancel', { cancelable: true }); if (dialog.dispatchEvent(event)) dialog.close(); return; }
    if (state.view !== 'games') { setView('games', true); return; }
    if (state.mod) { backToMods(); return; }
    if (state.game) { resetSelection(); return; }
  };
  const moveFocus = direction => {
    inputVersion++;
    const dialog = openDialog();
    if (dialog && document.activeElement?.matches('[data-controller-scroll]') && ['up', 'down'].includes(direction)) {
      const region = document.activeElement, old = region.scrollTop;
      region.scrollTop += (direction === 'down' ? 1 : -1) * Math.max(80, region.clientHeight * .65);
      if (region.scrollTop !== old) return;
    }
    const scope = dialog || document;
    const items = $$('button:not(:disabled),a[href],input:not(:disabled),select:not(:disabled),[tabindex="0"]', scope)
      .filter(el => visibleControl(el) && !el.matches('#main') && (dialog || el.closest('.topbar,.tabs,#status-banner,#' + state.view + '-view')));
    const current = document.activeElement;
    if (!items.includes(current)) { focusControl(items.find(el => el.id === 'tab-' + state.view) || items[0]); return; }
    const box = current.getBoundingClientRect(), horizontal = direction === 'left' || direction === 'right';
    if (horizontal && current.closest('.topbar')) {
      const row = items.filter(el => el.closest('.topbar'));
      focusControl(row[row.indexOf(current) + (direction === 'right' ? 1 : -1)]);
      return;
    }
    const sign = direction === 'left' || direction === 'up' ? -1 : 1;
    const cx = (box.left + box.right) / 2, cy = (box.top + box.bottom) / 2;
    let best = null, bestScore = Infinity;
    for (const el of items) {
      if (el === current) continue;
      const r = el.getBoundingClientRect(), dx = (r.left + r.right) / 2 - cx, dy = (r.top + r.bottom) / 2 - cy;
      const primary = (horizontal ? dx : dy) * sign;
      if (primary < 2) continue;
      const overlap = horizontal ? Math.min(box.bottom, r.bottom) - Math.max(box.top, r.top) : Math.min(box.right, r.right) - Math.max(box.left, r.left);
      const secondary = Math.abs(horizontal ? dy : dx);
      // Broad rows and their own panel heading overlap even when their centres differ.
      const panel = current.closest('.selection-panel');
      const localOverlap = panel && panel.contains(el) && overlap > 0;
      const score = primary + secondary * (localOverlap ? .1 : 2) + (overlap > 0 ? 0 : 500 * Math.max(1, innerWidth / 1920));
      if (score < bestScore) { best = el; bestScore = score; }
    }
    if (best) focusControl(best);
  };
  const handleKey = event => {
    inputVersion++;
    if ((event.target.matches('input,textarea,select') || event.target.isContentEditable) && event.key !== 'Escape' && event.key !== 'BrowserBack') return;
    if (event.key === 'Escape' || event.key === 'Backspace' || event.key === 'BrowserBack') { event.preventDefault(); handleBack(); return; }
    if (event.key === 'ArrowRight' || event.key === 'ArrowDown') { event.preventDefault(); moveFocus(event.key === 'ArrowRight' ? 'right' : 'down'); return; }
    if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') { event.preventDefault(); moveFocus(event.key === 'ArrowLeft' ? 'left' : 'up'); return; }
    if (event.key === 'F6' && state.view === 'games') { event.preventDefault(); const details = document.activeElement?.dataset?.details; if (details) openDetails(JSON.parse(details), document.activeElement); }
  };
  const bindTabs = () => {
    $$('.tab').forEach((tab, index, tabs) => {
      tab.addEventListener('click', () => activateView(tab.dataset.view));
      tab.addEventListener('keydown', event => {
        if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') { inputVersion++; event.preventDefault(); event.stopPropagation(); tabs[(index + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length].focus(); }
      });
    });
    $('#details-dialog').addEventListener('click', event => { if (event.target.matches('[data-close-dialog]') || event.target === $('#details-dialog')) closeDetails(); });
    $('#lookup-form').addEventListener('submit', lookupMod);
    $('#reload-titles').addEventListener('click', refreshTitles);
    $('#clear-target').addEventListener('click', clearTarget);
    $('#lookup-url').addEventListener('input', () => { if ($('#lookup-url').value) { $('#lookup-domain').value = ''; $('#lookup-mod-id').value = ''; } });
    for (const input of [$('#lookup-domain'), $('#lookup-mod-id')]) input.addEventListener('input', () => { if (input.value) $('#lookup-url').value = ''; });
  };
  const activateView = (view, move = false) => { inputVersion++; setView(view, move); if (view === 'activity') { loadJobs(); loadDownloads(); } if (view === 'installed') loadInstalled(); };
  const gamepadTick = () => {
    let pads; try { pads = navigator.getGamepads?.(); } catch (_) { pads = []; }
    const pad = pads ? [...pads].find(candidate => candidate && candidate.mapping === 'standard') : null;
    if (pad) {
      const pressed = (index) => !!pad.buttons[index]?.pressed;
      if (pad.buttons.some(button => button?.pressed) || pad.axes.some(axis => Math.abs(axis) > .6)) document.documentElement.dataset.input = 'controller';
      if (pressed(0) && !gamepadTick.lastCross) { inputVersion++; if (!visibleControl(document.activeElement)) focusContent(); else if (!window.ModBridgeKeyboard?.open(document.activeElement)) document.activeElement?.click?.(); }
      if (pressed(1) && !gamepadTick.lastCircle) handleBack();
      if (pressed(9) && !gamepadTick.lastOptions) {
        const target = document.activeElement; const details = target?.dataset?.details;
        if (details) openDetails(JSON.parse(details), target);
        else { const fileButton = target?.closest('[data-file-card]')?.querySelector('.details-file'); fileButton?.click(); }
      }
      if (!openDialog() && pressed(4) && !gamepadTick.lastL1) activateView(['games', 'activity', 'installed'][(['games', 'activity', 'installed'].indexOf(state.view) + 2) % 3], true);
      if (!openDialog() && pressed(5) && !gamepadTick.lastR1) activateView(['games', 'activity', 'installed'][(['games', 'activity', 'installed'].indexOf(state.view) + 1) % 3], true);
      for (const [name, index] of [['lastCross', 0], ['lastCircle', 1], ['lastOptions', 9], ['lastL1', 4], ['lastR1', 5]]) gamepadTick[name] = pressed(index);
      const up = pressed(12) || pad.axes[1] < -.6, down = pressed(13) || pad.axes[1] > .6, left = pressed(14) || pad.axes[0] < -.6, right = pressed(15) || pad.axes[0] > .6;
      const now = Date.now(), direction = up ? 'up' : down ? 'down' : left ? 'left' : right ? 'right' : '';
      if (direction && (direction !== gamepadTick.direction || now - (gamepadTick.lastNav || 0) > 240)) {
        moveFocus(direction); gamepadTick.lastNav = now;
      }
      gamepadTick.direction = direction;
    } else { gamepadTick.lastCross = gamepadTick.lastCircle = gamepadTick.lastOptions = gamepadTick.lastL1 = gamepadTick.lastR1 = false; }
    state.gamepadTimer = requestAnimationFrame(gamepadTick);
  };
  const start = async () => {
    restorePreferences(); bindTabs(); watchFocus(); document.addEventListener('keydown', handleKey);
    document.addEventListener('pointerdown', () => { inputVersion++; document.documentElement.dataset.input = 'pointer'; });
    focusControl($('#tab-games'));
    window.addEventListener('modbridge:installer-busy', event => {
      const busy = event.detail?.busy === true;
      if (busy && !state.installerBusy && document.activeElement?.dataset.titleKey) pendingTitleFocus = { key: document.activeElement.dataset.titleKey, version: inputVersion };
      state.installerBusy = busy;
      renderTitles();
      if (!busy) {
        if (pendingTitleFocus?.version === inputVersion) focusControl($$('[data-title-key]').find(el => el.dataset.titleKey === pendingTitleFocus.key));
        pendingTitleFocus = null;
        syncInstallerTarget();
      }
    });
    window.ModBridgeInstallerUI?.init({ csrf: () => state.csrf });
    window.addEventListener('modbridge:browse-request', () => { resetSelection(); activateView('games', true); });
    state.gamepadTimer = requestAnimationFrame(gamepadTick);
    try {
      const capabilities = await endpoint(routes.capabilities); state.capabilities = cleanRecord(capabilities.capabilities); state.csrf = safeText(capabilities.csrf_token, '');
      $('.mode-label').textContent = capabilities.mode === 'console_preview' ? 'CONSOLE PREVIEW' : 'PREVIEW';
      if (capabilities.mode === 'console_preview') $('.brand').setAttribute('href', '/app');
      $('#lookup-submit').disabled = state.capabilities.nexus_catalog !== true;
      $('#reload-titles').hidden = capabilities.mode === 'console_preview';
      $('#footer-capabilities').textContent = window.ModBridgeConfig.installer.mode === 'mock' ? 'Practice installs only' : 'Mod support depends on the selected game';
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
      restoreBasket(); render();
      setInterval(() => { if (!document.hidden) { loadJobs(); if (state.view === 'activity') loadDownloads(); } }, 2500);
      setInterval(() => { if (!document.hidden && state.view === 'installed' && state.capabilities.mod_service_records === true) loadInstalled(); }, 5000);
    } catch (error) { showError(error); }
  };
  document.addEventListener('DOMContentLoaded', start);
})();
