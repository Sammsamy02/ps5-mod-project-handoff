/* Installer presentation only. All real writes go through the four-call service. */
(function (global) {
  'use strict';
  const esc = value => String(value ?? '').replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]));
  const technicalText = /(?:0x[0-9a-f]{4,}|(?:^|[\s"'(])(?:\/[^\s"'()]|[A-Z]:\\)|\b[A-Z0-9_-]{32,}\b|\b(?:PPSA|CUSA)\d{4,}\b|\b(?:sha256|kernel|syscall)\b)/i;
  const plain = (value, fallback) => typeof value === 'string' && value.trim() && value.length <= 500 && !technicalText.test(value) ? value : fallback;
  const copy = {
    offline: ['No internet connection.', 'Reconnect to the internet, then try again.'],
    login_expired: ['Your Nexus login expired.', 'Sign in to Nexus again, then try again.'],
    download_corrupted: ['The download is damaged.', 'Download the mod again, then try again.'],
    unsupported_mod_type: ['This mod type is not supported for this game yet.', 'This game does not yet have a way to load this kind of mod. Choose another mod, or check back after support is added.'],
    install_refused: ['The install was refused.', 'Review the warning in More info, then try again.'],
    game_running: ['Close the game first.', 'Return to your PS5 home screen, close the game, then try again.'],
    console_busy: ['The console is busy.', 'Wait for the other task to finish, then try again.'],
    unavailable: ['The installer is not available yet.', 'Try again when the installer service is connected.'],
    unknown: ['The installer could not finish this step.', 'Check More info, then try again.']
  };
  let state;
  let panel;
  let dialog;
  let dialogReturn;
  let selectedRestore;
  let requestVersion = 0;
  let publishedBusy;
  let queuedGame;
  let hasQueuedGame = false;
  const seenSetup = new Set();
  const key = value => esc(value);
  const button = (label, action, extra = '', secondary = false) => `<button type="button" class="${secondary ? 'secondary-button' : 'action-button'}" data-installer-action="${action}" ${extra.includes('data-installer-key=') ? '' : `data-installer-key="${action}"`} ${extra}>${esc(label)}</button>`;
  const isRunning = status => status?.game_running === true;
  const unavailable = status => !status?.capability_file;
  const blocked = () => state.busy || !state.status || unavailable(state.status) || isRunning(state.status);
  function savedSetup(title) { if (seenSetup.has(title)) return true; try { return localStorage.getItem(`modbridge-ui-setup:${title}`) === 'seen'; } catch (_) { return false; } }
  function markSetup(title) { seenSetup.add(title); try { localStorage.setItem(`modbridge-ui-setup:${title}`, 'seen'); } catch (_) {} }

  function errorState(error, retry) {
    const aliases = { no_internet: 'offline', nexus_expired: 'login_expired', corrupt_download: 'download_corrupted', unsupported_type: 'unsupported_mod_type', plan_drift: 'install_refused', unknown_plan: 'install_refused', nexus_login_expired: 'login_expired', corrupted_download: 'download_corrupted', unsupported: 'unsupported_mod_type', busy: 'console_busy', refused: 'install_refused', game_is_running: 'game_running', network_error: 'offline' };
    const code = aliases[error?.code] || error?.code;
    state.error = { code: Object.hasOwn(copy, code) ? code : 'unknown', detail: error, retry };
    state.phase = 'error'; state.busy = false; render('retry');
  }
  function details() {
    dialogReturn = document.activeElement;
    const original = state.error?.detail;
    const content = original ? { ...original, code: original.code, message: original.message } : { game: state.game, archive: state.archive, plan: state.plan, report: state.result?.report, status: state.status };
    dialog.querySelector('[data-installer-details]').textContent = JSON.stringify(content, null, 2);
    dialog.showModal(); dialog.querySelector('[data-installer-dialog-close]').focus();
  }
  function closeDetails() { if (dialog?.open) dialog.close(); if (dialogReturn?.isConnected) dialogReturn.focus(); dialogReturn = null; }
  function init(options = {}) {
    if (state?.busy) return false;
    panel = document.getElementById('installer-panel');
    if (!panel) return false;
    if (!options.service && !global.ModBridgeInstaller) { panel.innerHTML = '<p class="content-note">The installer is not available yet. Try again after reconnecting.</p>'; return false; }
    requestVersion += 1;
    let service = options.service, controls = options.controls;
    // Injected adapters are real unless their owner explicitly marks them as
    // mock. An internal adapter always follows the config actually used by it.
    const mock = options.service ? options.mock === true : global.ModBridgeConfig?.installer?.mode !== 'real';
    let games = options.games || [];
    if (!service) {
      let session;
      if (mock && global.ModBridgeInstallerMock) {
        // A clearly named fictional preview is independent of every real game.
        if (!games.length) games = [{ game_title_id: 'DEMO_GAME_1', name: 'Sample game', capability_file: { supported_mod_types: ['textures'] } }];
        session = global.ModBridgeInstallerMock.create({ games, archives: options.archives || [{ archive_path: 'mock:sample-textures', name: 'Sample textures', mod_type: 'textures', changes: [{ action: 'add', description: 'Add the selected texture mod' }], warnings: ['Preview only. No game or mod files will be changed.'], decisions: [{ id: 'keep_settings', question: 'Keep your existing settings?', default: true }] }] });
        controls = session.controls;
      }
      service = global.ModBridgeInstaller.create({ config: global.ModBridgeConfig, mock: session?.adapter, csrf: options.csrf });
    }
    state = { service, controls, mock, games, knownGames: new Set(games.map(game => game.game_title_id)), game: null, archive: null, status: null, phase: 'game', plan: null, answers: {}, busy: false, error: null, result: null, setupMessage: false, operation: null, refreshFailed: false };
    publishedBusy = undefined;
    queuedGame = undefined; hasQueuedGame = false;
    selectedRestore = null;
    panel.addEventListener('click', onClick);
    dialog = document.getElementById('installer-info-dialog');
    if (!dialog) {
      dialog = document.createElement('dialog'); dialog.id = 'installer-info-dialog'; dialog.setAttribute('aria-labelledby', 'installer-info-title');
      dialog.innerHTML = '<div class="dialog-head"><h2 id="installer-info-title">More info</h2></div><pre data-installer-details data-controller-scroll tabindex="0"></pre><div class="dialog-actions"><button type="button" class="secondary-button" data-installer-dialog-close>Back</button></div>';
      document.body.appendChild(dialog);
      dialog.addEventListener('cancel', event => { event.preventDefault(); closeDetails(); });
      dialog.querySelector('[data-installer-dialog-close]').addEventListener('click', closeDetails);
    }
    render(); return true;
  }
  async function selectGame(game) {
    if (!state) init();
    if (!state) return;
    if (state.busy) return; // Keep the in-flight operation attached to its original game.
    if (state.phase === 'result') { queuedGame = game ? { ...game } : null; hasQueuedGame = true; return; }
    const version = ++requestVersion;
    state.game = game && typeof game.game_title_id === 'string' && game.game_title_id.trim() ? { ...game } : null;
    state.plan = null; state.error = null; state.result = null; state.phase = 'game'; state.refreshFailed = false; selectedRestore = null;
    if (!state.game) { state.status = null; state.archive = null; render(); return; }
    if (state.archive?.game_title_id !== state.game.game_title_id) state.archive = null;
    state.busy = true; state.status = null; render();
    try {
      if (state.mock && state.controls?.registerGame && !state.knownGames.has(state.game.game_title_id)) {
        await state.controls.registerGame({ game_title_id: state.game.game_title_id, name: state.game.name, capability_file: state.game.capability_file || null });
        state.knownGames.add(state.game.game_title_id);
      }
      const status = await state.service.status(state.game.game_title_id);
      if (version !== requestVersion) return;
      state.status = status; state.busy = false;
      state.setupMessage = !savedSetup(state.game.game_title_id) && (status?.setup?.state === 'needed' || status?.setup?.complete === false);
      if (state.setupMessage) markSetup(state.game.game_title_id);
      render();
    } catch (error) { if (version === requestVersion) errorState(error, 'status'); }
  }
  function offerArchive(archive) {
    if (!state || state.busy || state.phase === 'result') return false;
    if (!state.game || archive?.game_title_id !== state.game.game_title_id || typeof archive.archive_path !== 'string' || !archive.archive_path) { if (!state.game) render(); return false; }
    const keepStatusError = state.phase === 'error' && !state.status;
    state.archive = { ...archive }; state.plan = null;
    if (!keepStatusError) { state.error = null; state.phase = 'game'; }
    render(); return true;
  }
  async function plan(refresh = false) {
    if (state.busy || !state.game || !state.archive || (!refresh && blocked())) return;
    state.busy = true; state.error = null; state.phase = 'planning'; render();
    try {
      const status = await state.service.status(state.game.game_title_id);
      state.status = status;
      if (isRunning(status)) throw { code: 'game_running', nothing_changed: true };
      if (unavailable(status)) throw { code: 'unavailable', nothing_changed: true };
      state.plan = await state.service.plan(state.game.game_title_id, state.archive.archive_path);
      if (!state.plan?.plan_id || !state.plan?.plan_sha256 || !Array.isArray(state.plan.changes) || !Array.isArray(state.plan.warnings) || !Array.isArray(state.plan.decisions)) throw { code: 'unavailable', message: 'The installer returned an incomplete plan.' };
      state.answers = Object.fromEntries(state.plan.decisions.map(decision => [decision.id, decision.default === true]));
      state.phase = 'plan'; state.busy = false; render(state.plan.decisions.length ? `answer-${state.plan.decisions[0].id}-true` : 'apply');
    } catch (error) { errorState(error, 'plan'); }
  }
  function watchProgress() {
    let stopped = false, timer;
    const title = state.game.game_title_id;
    async function poll() {
      try {
        const status = await state.service.status(title);
        if (!stopped && state.busy && state.phase === 'progress' && state.game?.game_title_id === title) { state.status = status; state.progressUnavailable = false; render(); }
      } catch (_) { if (!stopped) { state.progressUnavailable = true; render(); } }
      if (!stopped) timer = setTimeout(poll, 1000);
    }
    timer = setTimeout(poll, 1000);
    return () => { stopped = true; clearTimeout(timer); state.progressUnavailable = false; };
  }
  async function apply() {
    if (state.busy || state.phase !== 'plan' || !state.plan) return;
    const accepted = { id: state.plan.plan_id, hash: state.plan.plan_sha256, answers: { ...state.answers } };
    state.busy = true; state.phase = 'progress'; state.operation = 'apply'; render();
    const stopProgress = watchProgress();
    try {
      const status = await state.service.status(state.game.game_title_id); state.status = status;
      if (isRunning(status)) throw { code: 'game_running', nothing_changed: true };
      if (unavailable(status)) throw { code: 'unavailable', nothing_changed: true };
      const result = await state.service.apply(accepted.id, accepted.hash, accepted.answers);
      if (result?.ok !== true) throw { code: result?.report?.code || 'install_refused', report: result?.report, nothing_changed: result?.report?.nothing_changed === true };
      state.result = { ...result, action: 'apply' }; state.phase = 'result'; stopProgress();
      try { state.status = await state.service.status(state.game.game_title_id); } catch (_) { state.status = null; state.refreshFailed = true; }
      state.busy = false;
      render('done');
    } catch (error) { errorState(error, 'plan'); }
    finally { stopProgress(); }
  }
  async function restore() {
    if (state.busy || state.phase !== 'restore' || !selectedRestore) return;
    const installId = selectedRestore.install_id;
    state.busy = true; state.phase = 'progress'; state.operation = 'restore'; render();
    const stopProgress = watchProgress();
    try {
      const status = await state.service.status(state.game.game_title_id); state.status = status;
      if (isRunning(status)) throw { code: 'game_running', nothing_changed: true };
      const result = await state.service.restore(installId);
      if (result?.ok !== true) throw { code: result?.report?.code || 'install_refused', report: result?.report, nothing_changed: result?.report?.nothing_changed === true };
      state.result = { ...result, action: 'restore' }; state.phase = 'result'; selectedRestore = null; stopProgress();
      try { state.status = await state.service.status(state.game.game_title_id); } catch (_) { state.status = null; state.refreshFailed = true; }
      state.busy = false;
      render('done');
    } catch (error) { errorState(error, 'restore'); }
    finally { stopProgress(); }
  }
  function cancelPlan() {
    state.plan = null; state.error = null; selectedRestore = null; state.phase = 'game';
    if (hasQueuedGame) { const game = queuedGame; queuedGame = undefined; hasQueuedGame = false; selectGame(game); return; }
    render(state.archive ? 'plan' : undefined);
  }
  function handleBack() {
    if (dialog?.open) { closeDetails(); return true; }
    if (!panel?.getClientRects().length || panel.closest('[hidden]')) return false;
    if (state && !state.busy && ['plan', 'restore', 'error', 'result'].includes(state.phase)) { cancelPlan(); return true; }
    return false;
  }
  async function mockManage(kind, id, direction) {
    if (state.busy || !state.mock || !state.controls || blocked()) return;
    const mods = state.status.installed_mods || [];
    const item = mods.find(mod => mod.install_id === id);
    if (!item) return;
    try {
      state.busy = true;
      render();
      if (kind === 'toggle') await state.controls.setEnabled(state.game.game_title_id, id, item.enabled !== true);
      else {
        const ids = mods.map(mod => mod.install_id), from = ids.indexOf(id), to = from + (direction === 'up' ? -1 : 1);
        if (to < 0 || to >= ids.length) { state.busy = false; render(); return; }
        [ids[from], ids[to]] = [ids[to], ids[from]]; await state.controls.reorder(state.game.game_title_id, ids);
      }
      state.status = await state.service.status(state.game.game_title_id); state.busy = false; render(`${kind}-${id}${direction ? `-${direction}` : ''}`);
    } catch (error) { errorState(error, 'status'); }
  }
  function onClick(event) {
    const target = event.target.closest('button'); if (!target || target.disabled || !panel.contains(target)) return;
    if (target.dataset.installerAnswer && !state.busy && state.phase === 'plan') { state.answers[target.dataset.installerAnswer] = target.dataset.value === 'true'; render(target.dataset.installerKey); return; }
    if (target.dataset.installerGame) {
      const game = state.games.find(item => item.game_title_id === target.dataset.installerGame);
      selectGame(game).then(() => offerArchive({ game_title_id: game.game_title_id, archive_path: 'mock:sample-textures', label: 'Sample textures' })); return;
    }
    if (target.dataset.installerToggle) { mockManage('toggle', target.dataset.installerToggle); return; }
    if (target.dataset.installerMove) { mockManage('move', target.dataset.installerMove, target.dataset.direction); return; }
    if (target.dataset.installerRestore && !blocked()) { selectedRestore = state.status.installed_mods.find(item => item.install_id === target.dataset.installerRestore); state.phase = 'restore'; render('restore-confirm'); return; }
    switch (target.dataset.installerAction) {
      case 'plan': plan(); break;
      case 'apply': apply(); break;
      case 'cancel': case 'done': cancelPlan(); break;
      case 'restore-confirm': restore(); break;
      case 'more': details(); break;
      case 'refresh': selectGame(state.game); break;
      case 'retry': { const retry = state.error?.retry; state.error = null; if (retry === 'status') selectGame(state.game); else if (retry === 'restore') { state.phase = 'restore'; render('restore-confirm'); } else { state.phase = 'game'; state.busy = false; plan(true); } break; }
      case 'browse': global.dispatchEvent(new CustomEvent('modbridge:browse-request', { detail: { game_title_id: state.game?.game_title_id } })); break;
    }
  }
  function installedCards() {
    const mods = state.status?.installed_mods || [];
    const sessions = Array.isArray(state.status?.last_session) ? state.status.last_session : [];
    const management = state.mock && !!state.controls;
    return `<h3>Installed mods${state.mock ? ' · preview' : ''}</h3><p class="content-note">Load order runs from top to bottom.</p>${mods.length ? mods.map((mod, index) => {
      const session = sessions.find(item => item.install_id === mod.install_id);
      const worked = session?.result === 'worked' || session?.worked === true;
      const failed = session?.result === 'did_not_work' || session?.worked === false;
      return `<article class="card installer-mod"><div><h4 class="card-title">${index + 1}. ${esc(plain(mod.name, 'Installed mod'))}</h4><p>${mod.enabled === true ? 'On' : mod.enabled === false ? 'Off' : 'On/off unavailable'}${mod.update_available ? ' · Update available' : ''}</p><p>Last play session: ${worked ? 'Worked' : failed ? "Didn't work" : 'No result yet'}${failed ? ` — ${esc(plain(session.reason, 'The game could not use this mod.'))}` : worked ? ' — The game reported this mod working.' : ''}</p></div><div class="installer-actions">${button(mod.enabled === true ? 'Turn off' : mod.enabled === false ? 'Turn on' : 'On/off unavailable', 'toggle', `data-installer-toggle="${key(mod.install_id)}" data-installer-key="toggle-${key(mod.install_id)}" ${!management || typeof mod.enabled !== 'boolean' || blocked() ? 'disabled' : ''}`, true)}${button('Move up', 'move-up', `data-installer-move="${key(mod.install_id)}" data-direction="up" data-installer-key="move-${key(mod.install_id)}-up" ${!management || blocked() || index === 0 ? 'disabled' : ''}`, true)}${button('Move down', 'move-down', `data-installer-move="${key(mod.install_id)}" data-direction="down" data-installer-key="move-${key(mod.install_id)}-down" ${!management || blocked() || index === mods.length - 1 ? 'disabled' : ''}`, true)}${button('Restore', 'restore', `data-installer-restore="${key(mod.install_id)}" data-installer-key="restore-${key(mod.install_id)}" ${blocked() ? 'disabled' : ''}`, true)}</div></article>`;
    }).join('') : '<p class="content-note">No installed mods for this game yet.</p>'}${!management ? '<p class="content-note">Turning mods on or off and changing load order are not available yet. The installer service needs to support these controls.</p>' : '<p class="content-note">On/off and order controls are preview only. They do not change any game.</p>'}${state.status?.conflicts?.length || mods.some(item => item.conflicts?.length) ? '<p class="installer-warning" role="note">These two mods change the same files — the one lower in the list wins.</p>' : ''}`;
  }
  function render(focusKey) {
    if (!panel || !state) return;
    const focused = panel.contains(document.activeElement) ? document.activeElement?.dataset.installerKey : null;
    const banner = state.mock ? '<p class="installer-preview content-note" role="note"><strong>Preview mode.</strong> This is a practice install. No games or mod files are changed.</p>' : '<p class="content-note">Keep modded games offline.</p>';
    let body = '';
    if (!state.game) {
      body = `<h3>Choose a game</h3><p class="content-note">Choose the game on your console before reviewing an install.</p>${state.mock ? state.games.map(game => button(`${plain(game.name, 'Sample game')} · preview only`, 'choose-game', `data-installer-game="${key(game.game_title_id)}"`)).join('') : ''}`;
    } else if (state.phase === 'planning' || state.phase === 'progress') {
      const progress = state.status?.progress;
      const minutes = Number(progress?.estimated_minutes);
      const message = plain(progress?.message, state.phase === 'planning' ? 'Checking what this mod will change.' : state.operation === 'restore' ? 'Restoring your previous setup.' : 'Installing your mod.');
      body = `<div data-installer-state="${state.phase === 'planning' ? 'planning' : 'progress'}" role="status" aria-live="polite" tabindex="0" data-controller-scroll><h3>${state.phase === 'planning' ? 'Making your plan' : state.operation === 'restore' ? 'Restoring' : 'Installing'}</h3><p>${esc(message)}${Number.isFinite(minutes) && minutes > 0 ? ` About ${Math.ceil(minutes)} minutes.` : ''}</p><p>You can leave this screen. Your progress will be here when you return.</p>${state.progressUnavailable ? '<p>Progress updates are temporarily unavailable. The install is still being checked.</p>' : ''}<progress ${Number.isFinite(progress?.percent) ? `value="${Math.max(0, Math.min(100, progress.percent))}" max="100"` : ''} aria-label="${state.phase === 'planning' ? 'Checking mod' : 'Install progress'}"></progress></div>`;
    } else if (state.phase === 'plan') {
      body = `<div data-installer-state="plan"><p class="eyebrow">STEP 1 · PLAN</p><h3>Review your install</h3><p>Review these changes before you confirm. Nothing has been changed.</p><ul>${state.plan.changes.map(change => `<li>${esc(plain(typeof change === 'string' ? change : change?.description, 'The installer will update this mod.'))}</li>`).join('') || '<li>No changes are needed.</li>'}</ul>${state.plan.warnings.length ? `<h4>Before you continue</h4><ul>${state.plan.warnings.map(warning => `<li>${esc(plain(typeof warning === 'string' ? warning : warning?.message, 'This mod needs your attention. Open More info for details.'))}</li>`).join('')}</ul>` : ''}${state.plan.decisions.map(decision => `<fieldset><legend>${esc(plain(decision.question, 'Allow this change?'))}</legend><div class="installer-actions">${[true, false].map(value => `<button type="button" class="secondary-button" data-installer-answer="${key(decision.id)}" data-value="${value}" data-installer-key="answer-${key(decision.id)}-${value}" aria-pressed="${state.answers[decision.id] === value}">${value ? 'Yes' : 'No'}${state.answers[decision.id] === value ? ' · selected' : ''}</button>`).join('')}</div></fieldset>`).join('')}<div class="installer-actions">${button('Confirm and install', 'apply')}${button('Cancel', 'cancel', '', true)}${button('More info', 'more', '', true)}</div></div>`;
    } else if (state.phase === 'restore') {
      body = `<div data-installer-state="restore"><h3>Restore your previous setup?</h3><p>This removes ${esc(plain(selectedRestore?.name, 'this mod'))} and asks the installer to restore the changes it made.</p><div class="installer-actions">${button('Confirm restore', 'restore-confirm')}${button('Cancel', 'cancel', '', true)}</div></div>`;
    } else if (state.phase === 'error') {
      const message = copy[state.error.code];
      body = `<div data-installer-state="error" role="alert"><h3>${esc(message[0])}</h3><p>${esc(message[1])}</p>${state.error.detail?.nothing_changed === true ? '<p>Nothing was changed.</p>' : ''}<div class="installer-actions">${button('Try again', 'retry')}${button('Back', 'cancel', '', true)}${button('More info', 'more', '', true)}</div></div>`;
    } else if (state.phase === 'result') {
      body = `<div data-installer-state="result" role="status"><p class="eyebrow">RESULT</p><h3>${state.mock ? 'Preview complete' : state.result.action === 'restore' ? 'Your previous setup is restored' : 'Your mod is installed'}</h3><p>${state.mock ? 'The practice run finished. No game or mod files were changed.' : esc(plain(state.result.report?.summary, state.result.action === 'restore' ? 'The installer restored this mod’s changes.' : 'Keep the game offline when you play with mods.'))}</p><div class="installer-actions">${state.refreshFailed ? '<p>The install finished, but the mod list could not be refreshed. Refresh this game before starting another install.</p>' : ''}${button('Back to game', 'done')}${button('More info', 'more', '', true)}</div></div>`;
    } else {
      const status = state.status;
      const selfTest = status?.self_test;
      const testPassed = selfTest?.state === 'passed' || selfTest?.ok === true;
      const testFailed = selfTest?.state === 'failed' || selfTest?.ok === false;
      body = `<div data-installer-state="game"><h3>${esc(plain(state.game.name, 'Selected game'))}</h3>${state.busy ? '<div class="installer-skeleton" role="status">Checking this game’s mod support…</div>' : state.refreshFailed ? `<p>The mod list could not be refreshed. Check this game again before starting another install.</p>${button('Refresh this game', 'refresh')}` : `${state.setupMessage ? '<p class="content-note" role="status">Setting up this game for mods — one time only.</p>' : ''}${unavailable(status) ? '<p class="installer-warning">Mods are not available yet for this game. The game’s mod support information is missing.</p>' : ''}${isRunning(status) ? '<p class="installer-warning">Close the game first. Then try your install again.</p>' : ''}<p>Game self-test: ${testPassed ? 'Passed' : testFailed ? 'Needs attention' : 'Not checked yet'}${selfTest?.message ? ` — ${esc(plain(selfTest.message, 'Open More info for details.'))}` : ''}</p>${state.archive ? `<div class="installer-install-choice"><h4>${esc(plain(state.archive.label, 'Selected mod'))}</h4><p>First review the plan. Then confirm to install.</p>${button('Review install', 'plan', blocked() ? 'disabled' : '')}</div>` : '<p class="content-note">Choose a downloaded mod to review its install. Downloads need to be provided by the installer service.</p>'}${installedCards()}<div class="installer-actions">${button('Browse mods', 'browse', '', true)}${button('More info', 'more', '', true)}</div>`}</div>`;
    }
    panel.innerHTML = `<div class="installer-section"><h2>Game mods</h2>${banner}${body}</div>`;
    const busy = state.busy === true;
    if (publishedBusy !== busy) {
      publishedBusy = busy;
      global.dispatchEvent(new CustomEvent('modbridge:installer-busy', { detail: { busy } }));
    }
    const restoreFocus = focusKey || focused;
    if (restoreFocus) [...panel.querySelectorAll('[data-installer-key]')].find(element => element.dataset.installerKey === restoreFocus && !element.disabled)?.focus();
  }
  global.ModBridgeInstallerUI = Object.freeze({ init, selectGame, offerArchive, handleBack, render });
})(window);
