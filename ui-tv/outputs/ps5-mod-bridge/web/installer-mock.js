(function (root, factory) {
  const common = typeof module === 'object' && module.exports;
  const value = factory(root, common ? require('./installer-service.js') : root.ModBridgeInstaller, common ? require('node:crypto').webcrypto : root.crypto);
  if (common) module.exports = value;
  else root.ModBridgeInstallerMock = value;
})(typeof globalThis === 'object' ? globalThis : this, function (root, Service, defaultCrypto) {
  'use strict';
  const scenarios = Object.freeze(['no_internet', 'nexus_expired', 'corrupt_download', 'unsupported_type', 'game_running', 'console_busy', 'install_refused', 'plan_drift']);
  const storageKey = 'modbridge.installer.mock.v1';
  const clone = value => JSON.parse(JSON.stringify(value));
  const record = value => !!value && typeof value === 'object' && !Array.isArray(value);
  const text = value => typeof value === 'string' && value.length > 0 && value.length <= 4096 && !/[\u0000-\u001f]/.test(value);
  const fail = code => { throw new Service.InstallerError(code, true); };
  const canonical = value => JSON.stringify(value, (_key, item) => record(item) ? Object.fromEntries(Object.entries(item).sort(([a], [b]) => a.localeCompare(b))) : item);
  const shaConstants = new Uint32Array([
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  ]);
  async function sha256(input, crypto = defaultCrypto) {
    if (typeof input !== 'string' || typeof TextEncoder !== 'function') fail('configuration');
    const bytes = new TextEncoder().encode(input);
    if (crypto?.subtle) return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), byte => byte.toString(16).padStart(2, '0')).join('');
    // Ordinary HTTP origins may lack SubtleCrypto. This implements the same
    // SHA-256 padding, message schedule and compression, only for preview data.
    const length = Math.ceil((bytes.length + 9) / 64) * 64;
    const padded = new Uint8Array(length); padded.set(bytes); padded[bytes.length] = 0x80;
    const view = new DataView(padded.buffer);
    view.setUint32(length - 8, Math.floor(bytes.length / 0x20000000)); view.setUint32(length - 4, (bytes.length * 8) >>> 0);
    const state = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
    const words = new Int32Array(64), rotate = (value, bits) => (value >>> bits) | (value << (32 - bits));
    for (let block = 0; block < length; block += 64) {
      for (let index = 0; index < 16; index++) words[index] = view.getInt32(block + index * 4);
      for (let index = 16; index < 64; index++) {
        const before = words[index - 15], recent = words[index - 2];
        const s0 = rotate(before, 7) ^ rotate(before, 18) ^ (before >>> 3), s1 = rotate(recent, 17) ^ rotate(recent, 19) ^ (recent >>> 10);
        words[index] = (words[index - 16] + s0 + words[index - 7] + s1) | 0;
      }
      let [a, b, c, d, e, f, g, h] = state;
      for (let index = 0; index < 64; index++) {
        const s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25), choice = (e & f) ^ (~e & g);
        const first = (h + s1 + choice + shaConstants[index] + words[index]) | 0;
        const s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22), majority = (a & b) ^ (a & c) ^ (b & c), second = (s0 + majority) | 0;
        h = g; g = f; f = e; e = (d + first) | 0; d = c; c = b; b = a; a = (first + second) | 0;
      }
      const round = [a, b, c, d, e, f, g, h];
      for (let index = 0; index < 8; index++) state[index] = (state[index] + round[index]) >>> 0;
    }
    return Array.from(state, word => word.toString(16).padStart(8, '0')).join('');
  }
  function normalizeGame(value) {
    if (!record(value) || !text(value.game_title_id) || (value.capability_file != null && (!record(value.capability_file) || !Array.isArray(value.capability_file.supported_mod_types) || !value.capability_file.supported_mod_types.every(text)))) fail('invalid_input');
    const mods = value.installed_mods || [];
    if (!Array.isArray(mods) || mods.some(item => !record(item) || !text(item.install_id) || !text(item.name) || !text(item.mod_type) || typeof item.enabled !== 'boolean')) fail('invalid_input');
    if (new Set(mods.map(item => item.install_id)).size !== mods.length) fail('invalid_input');
    const conflicts = value.conflicts || [];
    if (!Array.isArray(conflicts) || conflicts.some(item => !record(item) || !Array.isArray(item.mod_ids) || item.mod_ids.length < 2 || !item.mod_ids.every(text))) fail('invalid_input');
    return { game_title_id: value.game_title_id, name: text(value.name) ? value.name : 'Game', capability_file: clone(value.capability_file || null), installed_mods: mods.map((item, index) => ({ ...clone(item), load_order: index })), game_running: value.game_running === true,
      conflicts: clone(conflicts), last_session: clone(value.last_session || []), self_test: clone(value.self_test || { state: 'not_run', message: 'No self-test result yet.' }), setup: clone(value.setup || { state: 'needed', message: 'Setting up this game for mods — one time only' }), progress: clone(value.progress || null), revision: Number.isSafeInteger(value.revision) && value.revision >= 0 ? value.revision : 0 };
  }
  function normalizeArchive(value, revision = 0) {
    if (!record(value) || !text(value.archive_path) || !text(value.name) || !text(value.mod_type)) fail('invalid_input');
    const decisions = value.decisions || [{ id: 'keep_settings', question: 'Keep your existing settings?', default: true }];
    if (!Array.isArray(decisions) || decisions.some(item => !record(item) || !text(item.id) || !text(item.question) || typeof item.default !== 'boolean') || new Set(decisions.map(item => item.id)).size !== decisions.length) fail('invalid_input');
    const changes = value.changes || [{ action: 'add', description: `Add ${value.name}` }], warnings = value.warnings || [];
    if (!Array.isArray(changes) || !Array.isArray(warnings) || !warnings.every(text)) fail('invalid_input');
    return { archive_path: value.archive_path, name: value.name, mod_type: value.mod_type, changes: clone(changes), warnings: clone(warnings), decisions: clone(decisions), revision };
  }
  function create(options = {}) {
    const crypto = options.crypto || defaultCrypto;
    if (typeof TextEncoder !== 'function') fail('configuration');
    const games = new Map(), archives = new Map(), plans = new Map();
    let serial = 0, scenario = null;
    function putGame(value) { const normalized = normalizeGame(value); games.set(normalized.game_title_id, normalized); }
    function putArchive(value) { const normalized = normalizeArchive(value, (archives.get(value.archive_path)?.revision || 0) + 1); archives.set(normalized.archive_path, normalized); }
    for (const value of options.games || []) putGame(value);
    for (const value of options.archives || []) putArchive(value);
    if (options.storage) {
      try {
        const saved = options.storage.getItem(storageKey);
        if (saved) {
          if (typeof saved !== 'string' || saved.length > 1048576) fail('invalid_mock_state');
          const data = JSON.parse(saved);
          if (!record(data) || Object.keys(data).sort().join(',') !== 'archives,games,serial,version' || data.version !== 1 || !Array.isArray(data.games) || !Array.isArray(data.archives) || !Number.isSafeInteger(data.serial) || data.serial < 0) fail('invalid_mock_state');
          if (new Set(data.games.map(item => item?.game_title_id)).size !== data.games.length || new Set(data.archives.map(item => item?.archive_path)).size !== data.archives.length) fail('invalid_mock_state');
          games.clear(); archives.clear();
          for (const value of data.games) putGame(value);
          for (const value of data.archives) putArchive(value);
          serial = data.serial;
        }
      } catch (_) { fail('invalid_mock_state'); }
    }
    const persist = () => { if (options.storage) { try { options.storage.setItem(storageKey, JSON.stringify({ version: 1, serial, games: [...games.values()], archives: [...archives.values()] })); } catch (_) { fail('invalid_mock_state'); } } };
    function mutate(action) {
      const previous = { games: clone([...games.entries()]), archives: clone([...archives.entries()]), serial };
      try { const result = action(); persist(); return result; }
      catch (error) {
        games.clear(); archives.clear();
        for (const [key, value] of previous.games) games.set(key, value);
        for (const [key, value] of previous.archives) archives.set(key, value);
        serial = previous.serial; throw error;
      }
    }
    const digest = value => sha256(canonical(value), crypto);
    const getGame = id => games.get(id) || fail('unavailable');
    const getArchive = id => archives.get(id) || fail('unavailable');
    const supported = (game, archive) => { if (!game.capability_file?.supported_mod_types.includes(archive.mod_type)) fail('unsupported_type'); };
    function check(method, game) {
      if (scenario === 'no_internet' || scenario === 'console_busy') fail(scenario);
      if (method !== 'status' && (scenario === 'game_running' || game?.game_running)) fail('game_running');
      if (method === 'plan' && ['nexus_expired', 'corrupt_download', 'unsupported_type'].includes(scenario)) fail(scenario);
      if (method === 'apply' && ['install_refused', 'plan_drift'].includes(scenario)) fail(scenario);
    }
    const adapter = {
      async plan(...args) {
        Service.validateCall('plan', args); const game = getGame(args[0]), archive = getArchive(args[1]); check('plan', game); supported(game, archive);
        const plan_id = `preview-plan-${++serial}`;
        const value = { plan_id, changes: clone(archive.changes), warnings: clone(archive.warnings), decisions: clone(archive.decisions) };
        const content = { ...value, game_title_id: game.game_title_id, game_revision: game.revision, capability_file: clone(game.capability_file), archive: clone(archive) };
        const plan_sha256 = await digest(content);
        persist(); plans.set(plan_id, { value: { ...value, plan_sha256 }, content, result: null, answerHash: null });
        return clone({ ...value, plan_sha256 });
      },
      async apply(...args) {
        Service.validateCall('apply', args);
        const entry = plans.get(args[0]); if (!entry) fail('unknown_plan');
        if (args[1] !== entry.value.plan_sha256 || await digest(entry.content) !== args[1]) fail('plan_drift');
        const keys = Object.keys(args[2]).sort(), expected = entry.value.decisions.map(item => item.id).sort();
        if (canonical(keys) !== canonical(expected)) fail('invalid_answers');
        const answerHash = await digest({ plan_sha256: args[1], answers: args[2] });
        if (entry.result) { if (entry.answerHash !== answerHash) fail('intent_conflict'); return clone(entry.result); }
        const game = getGame(entry.content.game_title_id), archive = getArchive(entry.content.archive.archive_path); check('apply', game); supported(game, archive);
        if (game.revision !== entry.content.game_revision || archive.revision !== entry.content.archive.revision) fail('plan_drift');
        entry.result = mutate(() => {
          let install_id;
          do { install_id = `preview-install-${++serial}`; }
          while ([...games.values()].some(item => item.installed_mods.some(mod => mod.install_id === install_id)));
          game.installed_mods.push({ install_id, name: archive.name, mod_type: archive.mod_type, enabled: true, load_order: game.installed_mods.length }); game.revision++;
          return { ok: true, install_id, report: { message: 'Preview installation complete.', mock: true, nothing_changed_on_console: true } };
        });
        entry.answerHash = answerHash;
        return clone(entry.result);
      },
      async restore(...args) {
        Service.validateCall('restore', args);
        const game = [...games.values()].find(item => item.installed_mods.some(mod => mod.install_id === args[0]));
        if (!game) fail('unknown_install'); check('restore', game);
        return mutate(() => {
          game.installed_mods = game.installed_mods.filter(item => item.install_id !== args[0]).map((item, index) => ({ ...item, load_order: index })); game.revision++;
          return { ok: true, report: { message: 'Preview restoration complete.', mock: true, nothing_changed_on_console: true } };
        });
      },
      async status(...args) {
        Service.validateCall('status', args); const game = getGame(args[0]); check('status', game);
        const value = clone(game); delete value.revision; return { ...value, mock: true };
      },
    };
    function setScenario(value) { if (value !== null && !scenarios.includes(value)) fail('invalid_input'); scenario = value; }
    function setFixture(id, field, value) { mutate(() => { const game = getGame(id); game[field] = clone(value); }); }
    const controls = Object.freeze({
      setScenario,
      registerGame(value) { mutate(() => { const old = games.get(value.game_title_id); putGame({ ...value, revision: (old?.revision || 0) + 1 }); }); },
      registerArchive(value) { mutate(() => putArchive(value)); },
      setGameRunning(id, running) { if (typeof running !== 'boolean') fail('invalid_input'); setFixture(id, 'game_running', running); },
      setEnabled(id, installId, enabled) { mutate(() => { const game = getGame(id), mod = game.installed_mods.find(item => item.install_id === installId); if (!mod || typeof enabled !== 'boolean') fail('invalid_input'); mod.enabled = enabled; game.revision++; }); },
      reorder(id, ids) { mutate(() => { const game = getGame(id); if (!Array.isArray(ids) || ids.length !== game.installed_mods.length || new Set(ids).size !== ids.length || ids.some(key => !game.installed_mods.some(item => item.install_id === key))) fail('invalid_input'); game.installed_mods = ids.map((key, index) => ({ ...game.installed_mods.find(item => item.install_id === key), load_order: index })); game.revision++; }); },
      setSetup(id, value) { if (!record(value) || !['needed', 'setting_up', 'ready', 'failed'].includes(value.state) || !text(value.message)) fail('invalid_input'); setFixture(id, 'setup', value); },
      setProgress(id, value) { if (value !== null && (!record(value) || !text(value.stage) || !text(value.message) || !Number.isFinite(value.estimated_minutes) || value.estimated_minutes < 0 || !Number.isFinite(value.percent) || value.percent < 0 || value.percent > 100 || typeof value.can_leave !== 'boolean')) fail('invalid_input'); setFixture(id, 'progress', value); },
    });
    if (options.scenario != null) setScenario(options.scenario);
    return Object.freeze({ adapter: Object.freeze(adapter), controls });
  }
  return Object.freeze({ create, scenarios, storageKey, sha256 });
});
