(function (root, factory) {
  const value = factory();
  if (typeof module === 'object' && module.exports) module.exports = value;
  else root.ModBridgeConfig = value;
})(typeof globalThis === 'object' ? globalThis : this, function () {
  'use strict';
  const part = value => encodeURIComponent(String(value));
  const routes = Object.freeze({
    capabilities: '/api/capabilities', account: '/api/account', games: '/api/games',
    jobs: '/api/jobs', downloads: '/api/downloads', installed: '/api/installed',
    authStart: '/api/auth/start', authDisconnect: '/api/auth/disconnect',
    detectedTitles: '/api/detected-titles', detectedTitlesRefresh: '/api/detected-titles/refresh',
    titlePages: '/api/detected-titles/pages', titlePagesRefresh: '/api/detected-titles/pages/refresh',
    titlePage: cursor => `/api/detected-titles/pages/${part(cursor)}`,
    titleCurrent: snapshot => `/api/detected-titles/pages/current/${part(snapshot)}`,
    gameMods: game => `/api/games/${part(game)}/mods`,
    mod: (game, mod) => `/api/games/${part(game)}/mods/${part(mod)}`,
    modFiles: (game, mod) => `/api/games/${part(game)}/mods/${part(mod)}/files`,
    job: id => `/api/jobs/${part(id)}`,
    modService: '/api/mod-service', modServiceRecords: '/api/mod-service/records',
  });
  return Object.freeze({
    routes,
    installer: Object.freeze({ mode: 'mock', paths: Object.freeze({ plan: '/api/installer/plan', apply: '/api/installer/apply', restore: '/api/installer/restore', status: '/api/installer/status' }) }),
    nexus: Object.freeze({ authorizationOrigin: 'https://users.nexusmods.com',
      modPage: (game, mod) => `https://www.nexusmods.com/${part(game)}/mods/${part(mod)}`,
      filePage: (game, mod, file) => `https://www.nexusmods.com/${part(game)}/mods/${part(mod)}?tab=files&file_id=${part(file)}` }),
  });
});
