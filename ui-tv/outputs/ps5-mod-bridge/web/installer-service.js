(function (root, factory) {
  const value = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = value;
  else root.ModBridgeInstaller = value;
})(typeof globalThis === 'object' ? globalThis : this, function (root) {
  'use strict';
  const messages = Object.freeze({
    no_internet: 'No internet connection. Check your connection, then try again.',
    nexus_expired: 'Your Nexus sign-in expired. Sign in again to continue.',
    corrupt_download: 'The download is damaged. Download it again, then retry.',
    unsupported_type: 'This kind of mod is not available for this game yet. Choose another mod.',
    game_running: 'Close the game first, then try again.',
    console_busy: 'The console is busy. Wait a moment, then try again.',
    install_refused: 'The installation was refused. Review the plan, then try again.',
    plan_drift: 'The plan changed. Review a new plan before installing.',
    unknown_plan: 'This plan is no longer available. Review a new plan.',
    unknown_install: 'This installation is no longer available. Refresh this game.',
    invalid_answers: 'Answer each question with Yes or No before continuing.',
    intent_conflict: 'This request already used different answers. Review a new plan.',
    invalid_input: 'This request is incomplete. Go back and choose the game and mod again.',
    unavailable: 'This game or download is not available yet. Choose another item.',
    invalid_reply: 'The installer could not confirm its reply. Refresh the game before continuing.',
    configuration: 'The installer is not connected yet. Try again when it is available.',
    invalid_mock_state: 'The saved preview could not be read. Reset the preview to continue.',
    operation_failed: 'The installer could not complete the request. Refresh the game, then try again.',
  });
  class InstallerError extends Error {
    constructor(code, nothingChanged = false) {
      const key = Object.hasOwn(messages, code) ? code : 'operation_failed';
      super(messages[key]); this.name = 'InstallerError'; this.code = key;
      // Transport failure alone never proves whether the service changed anything.
      if (nothingChanged) this.nothing_changed = true;
    }
  }
  const record = value => !!value && typeof value === 'object' && !Array.isArray(value);
  const text = value => typeof value === 'string' && value.length > 0 && value.length <= 4096 && !/[\u0000-\u001f]/.test(value);
  const hash = value => typeof value === 'string' && /^[a-f0-9]{64}$/.test(value);
  const answersValid = value => record(value) && Object.keys(value).length <= 128 && Object.entries(value).every(([key, answer]) => text(key) && typeof answer === 'boolean');
  function validateCall(method, args) {
    const valid = method === 'plan' ? args.length === 2 && args.every(text) : method === 'apply' ? args.length === 3 && text(args[0]) && hash(args[1]) && answersValid(args[2]) : args.length === 1 && text(args[0]);
    if (!valid) throw new InstallerError(method === 'apply' && args.length === 3 && !answersValid(args[2]) ? 'invalid_answers' : 'invalid_input');
  }
  function validateReply(method, value) {
    let valid = record(value);
    const description = (item, field) => text(item) || (record(item) && (!Object.hasOwn(item, field) || text(item[field])));
    const installed = item => record(item) && text(item.install_id) && (!Object.hasOwn(item, 'name') || text(item.name)) && (!Object.hasOwn(item, 'enabled') || typeof item.enabled === 'boolean');
    if (method === 'plan') valid = valid && text(value.plan_id) && hash(value.plan_sha256) && Array.isArray(value.changes) && value.changes.every(item => description(item, 'description')) && Array.isArray(value.warnings) && value.warnings.every(item => description(item, 'message')) && Array.isArray(value.decisions) && value.decisions.every(item => record(item) && text(item.id) && text(item.question) && typeof item.default === 'boolean') && new Set(value.decisions.map(item => item.id)).size === value.decisions.length;
    else if (method === 'status') valid = valid && Array.isArray(value.installed_mods) && value.installed_mods.every(installed) && new Set(value.installed_mods.map(item => item.install_id)).size === value.installed_mods.length && (value.capability_file === null || (record(value.capability_file) && (!Object.hasOwn(value.capability_file, 'supported_mod_types') || (Array.isArray(value.capability_file.supported_mod_types) && value.capability_file.supported_mod_types.every(text)))));
    else valid = valid && typeof value.ok === 'boolean' && Object.hasOwn(value, 'report') && (typeof value.report === 'string' || record(value.report)) && (method !== 'apply' || !value.ok || text(value.install_id));
    if (!valid) throw new InstallerError('invalid_reply');
    // An empty capability object carries no support information. Other
    // nonempty capability schemas remain the service team's responsibility.
    if (method === 'status' && value.capability_file && Object.keys(value.capability_file).length === 0) return { ...value, capability_file: null };
    if (value.ok === false) {
      const code = record(value.report) && Object.hasOwn(messages, value.report.code) ? value.report.code : 'install_refused';
      return { ...value, report: { ...(record(value.report) ? value.report : {}), code, message: messages[code] } };
    }
    return value;
  }
  function sameOriginPath(value) {
    if (typeof value !== 'string' || !/^\/(?!\/)/.test(value) || /[\\\u0000-\u001f\u007f#]/.test(value)) return false;
    try {
      const origin = /^https?:\/\//.test(root.location?.origin || '') ? root.location.origin : 'https://modbridge.invalid';
      return new URL(value, origin).origin === new URL(origin).origin;
    } catch (_) { return false; }
  }
  function create(options = {}) {
    const config = options.config || root.ModBridgeConfig;
    const setting = config?.installer;
    if (!setting || !['mock', 'real'].includes(setting.mode)) throw new InstallerError('configuration');
    const methods = ['plan', 'apply', 'restore', 'status'];
    if (setting.mode === 'mock') {
      if (!options.mock || methods.some(name => typeof options.mock[name] !== 'function')) throw new InstallerError('configuration');
      return Object.freeze(Object.fromEntries(methods.map(method => [method, async (...args) => { validateCall(method, args); return validateReply(method, await options.mock[method](...args)); }])));
    }
    const fetch = options.fetch || root.fetch?.bind(root);
    if (!fetch || !setting.paths || methods.some(name => !sameOriginPath(setting.paths[name]))) throw new InstallerError('configuration');
    const invoke = async (method, args) => {
      validateCall(method, args);
      const body = method === 'plan' ? { game_title_id: args[0], archive_path: args[1] } : method === 'apply' ? { plan_id: args[0], plan_sha256: args[1], answers: args[2] } : { install_id: args[0] };
      const token = typeof options.csrf === 'function' ? options.csrf() : options.csrf || '';
      const headers = { Accept: 'application/json' };
      let url = setting.paths[method];
      const request = { method: method === 'status' ? 'GET' : 'POST', credentials: 'same-origin', cache: 'no-store', headers };
      if (method === 'status') url += `${url.includes('?') ? '&' : '?'}game_title_id=${encodeURIComponent(args[0])}`;
      else { headers['Content-Type'] = 'application/json'; headers['X-CSRF-Token'] = token; request.body = JSON.stringify(body); }
      let response;
      try { response = await fetch(url, request); } catch (_) { throw new InstallerError('no_internet'); }
      let data;
      try { data = await response.json(); } catch (_) { throw new InstallerError('invalid_reply'); }
      if (!response.ok) throw new InstallerError(data?.error?.code);
      return validateReply(method, data);
    };
    return Object.freeze(Object.fromEntries(methods.map(method => [method, (...args) => invoke(method, args)])));
  }
  return Object.freeze({ create, InstallerError, messages, validateCall });
});
