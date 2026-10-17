const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { chromium } = require('playwright');

async function main() {
  await mainFlow();
  await installFlow('free', async (page, fixture) => {
    // A free account: the job waits for a website link, and the real host resumes it from the pasted one.
    // The same account record the real host returns names the plan, and the page points free accounts at that path.
    await page.locator('#account .account-tier').waitFor({ timeout: 15000 });
    assert.equal(await page.locator('#account .account-tier').innerText(), 'Free');
    assert.match(await page.locator('#account .account-hint').innerText(), /“Slow download” button and a pasted nxm:\/\/ link/);
    assert.equal((await page.evaluate(async () => (await (await fetch('/api/account')).json()).account.is_premium)), false);
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Nexus authorization needed', { exact: true }).waitFor({ timeout: 15000 });
    const waiting = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.equal(waiting.length, 1);
    assert.equal(waiting[0].status, 'needs_nexus_authorization');
    assert.ok(!('idempotency_key' in waiting[0]));
    const secret = 'integration-grant-fixture';
    const link = `nxm://ownedgame/mods/${waiting[0].mod_id}/files/${waiting[0].file_id}?key=${secret}&expires=${Math.floor(Date.now() / 1000) + 3600}&user_id=42`;
    // A link for another file is stopped by the page; one the page cannot judge is refused by the host.
    await page.getByLabel('Nexus download link').fill(link.replace(`/files/${waiting[0].file_id}`, '/files/999'));
    await page.getByRole('button', { name: 'Continue download' }).click();
    await page.getByText(/different file/).waitFor();
    await page.getByLabel('Nexus download link').fill(link);
    await page.getByRole('button', { name: 'Continue download' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    const done = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.equal(done.length, 1);
    assert.equal(done[0].id, waiting[0].id);
    assert.equal(done[0].status, 'analyzed');
    assert.ok(!JSON.stringify(done).includes(secret) && !JSON.stringify(done).includes('nxm://'));
    assert.equal(await page.getByLabel('Nexus download link').count(), 0);
    // The same job refuses a second paste: it is no longer waiting.
    const again = await page.evaluate(async (id) => {
      const capabilities = await (await fetch('/api/capabilities')).json();
      const response = await fetch(`/api/jobs/${id}/resume`, { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': capabilities.csrf_token }, body: JSON.stringify({ nxm: 'nxm://ownedgame/mods/1/files/2?key=x&expires=9999999999&user_id=42' }) });
      return { status: response.status, body: await response.json() };
    }, waiting[0].id);
    assert.equal(again.status, 409);
    assert.equal(again.body.error.code, 'auth_refused');
  });
  await installFlow('flaky', async page => {
    // A failed download shows catalog text and Request again makes a new job that completes.
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Could not complete', { exact: true }).waitFor({ timeout: 15000 });
    assert.match(await page.locator('#activity-content').innerText(), /Nexus could not be reached\. Try again\./);
    const first = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.deepEqual(first.map(job => job.status), ['failed']);
    await page.getByRole('button', { name: 'Request again' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    const jobs = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.deepEqual(jobs.map(job => job.status), ['failed', 'analyzed']);
    assert.notEqual(jobs[0].id, jobs[1].id);
    assert.deepEqual([jobs[1].game, jobs[1].mod_id, jobs[1].file_id], [jobs[0].game, jobs[0].mod_id, jobs[0].file_id]);
    assert.ok(jobs.every(job => !('idempotency_key' in job)));
    assert.equal(await page.locator(`[data-request-again="${jobs[0].id}"]`).innerText(), 'Requested again');
  });
  await installFlow('lua', async page => {
    // A Lua mod analyzed by the real core and host: the record the host writes passes the page's own
    // validator and renders the PS5 Lua runtime section; the page claims neither support nor install.
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    const [job] = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.deepEqual(job.runtime_coverage, { status: 'blocked', reason: null, runtime: 'ps5-ue4ss-port', console_wired: false, missing: ['RegisterLoadMapPreHook'], partial: ['ExecuteInGameThread'], missing_modules: [], partial_modules: [], dynamic_require: false });
    const section = page.locator('.runtime-coverage');
    assert.equal(await section.count(), 1);
    const text = await section.innerText();
    assert.match(text, /PS5 Lua runtime/);
    assert.match(text, /Missing functions/);
    assert.match(text, /RegisterLoadMapPreHook/);
    assert.match(text, /ExecuteInGameThread/);
    assert.match(text, /The PS5 Lua runtime is not running on the console yet, so this mod cannot run there today\./);
    assert.match(text, /Checked by reading the mod's script, not by running it\./);
    assert.doesNotMatch(await page.locator('body').innerText(), /Ready to install|PS5 compatible|will work/i);
    assert.equal(await page.getByRole('button', { name: /^Install/ }).count(), 0);
  });
  await installFlow('update', async page => {
    // The real host answers GET /api/jobs/{id}/update from the fixture's files.json links: file 2 links to file 3,
    // which links to nothing. The page's own validator must accept the real reply shape.
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    const [first] = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.equal(first.file_id, 2);
    const reply = await page.evaluate(async id => { const response = await fetch(`/api/jobs/${id}/update`); return { status: response.status, body: await response.json() }; }, first.id);
    assert.equal(reply.status, 200);
    assert.deepEqual(Object.keys(reply.body).sort(), ['latest', 'status', 'steps']);
    assert.equal(reply.body.status, 'newer');
    assert.equal(reply.body.steps, 1);
    assert.deepEqual(Object.keys(reply.body.latest).sort(), ['category_name', 'file_id', 'name', 'uploaded_timestamp', 'version']);
    assert.equal(reply.body.latest.file_id, 3);
    // Nothing is shown until the button is pressed, and the page asked for nothing before that.
    const card = page.locator(`[data-job-id="${first.id}"]`);
    assert.equal(await card.locator('.update-line').count(), 0);
    await card.getByRole('button', { name: 'Check for newer file' }).click();
    const line = card.locator('.update-line');
    await line.waitFor();
    // The name is hostile markup from the fixture: shown as text, never parsed.
    assert.equal(await line.innerText(), 'Newer file linked by the author: Owned file <b>update</b> (version 2.0)');
    assert.equal(await card.locator('.update-result b').count(), 0);
    assert.match(await card.locator('.update-meta').innerText(), /^Update · Uploaded Mar \d+, 2024$/);
    assert.equal(await card.getByRole('link', { name: 'Open newer file on Nexus' }).getAttribute('href'), 'https://www.nexusmods.com/ownedgame/mods/1?tab=files&file_id=3');
    assert.doesNotMatch(await card.innerText(), /up to date|latest|current version/i);
    // The newer file becomes a new job through the real host: same game and mod, file 3.
    await card.getByRole('button', { name: 'Download newer file for analysis' }).click();
    await page.getByText('Analysis requested for the newer file.').waitFor();
    // The host's content security policy forbids waitForFunction's string evaluation, so poll with evaluate.
    for (let n = 0; n < 300; n++) {
      if (await page.evaluate(() => document.querySelectorAll('[data-job-id]').length === 2 && [...document.querySelectorAll('[data-job-id] .job-pill')].every(pill => pill.querySelector('.pill-label')?.textContent === 'Analysis complete'))) break;
      await page.waitForTimeout(50);
    }
    const jobs = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.deepEqual(jobs.map(job => [job.game, job.mod_id, job.file_id, job.status]), [['ownedgame', 1, 2, 'analyzed'], ['ownedgame', 1, 3, 'analyzed']]);
    assert.notEqual(jobs[0].id, jobs[1].id);
    assert.ok(jobs.every(job => !('idempotency_key' in job)));
    // The old job's check is kept; the new one (file 3) has nothing linked after it.
    assert.match(await card.locator('.update-line').innerText(), /^Newer file linked by the author:/);
    const second = page.locator(`[data-job-id="${jobs[1].id}"]`);
    await second.getByRole('button', { name: 'Check for newer file' }).click();
    await second.locator('.update-line').waitFor();
    assert.equal(await second.locator('.update-line').innerText(), 'No newer file linked by the author.');
    assert.doesNotMatch(await second.innerText(), /up to date|latest|current version/i);
    assert.equal(await second.getByRole('button', { name: /Download newer/ }).count(), 0);
    // Reading the verdict changed nothing.
    assert.deepEqual((await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items)).map(job => job.status), ['analyzed', 'analyzed']);
  });
  await installFlow('update-fails', async page => {
    // A provider failure behind the check is shown as a failed check with the catalog text, never as a verdict.
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    const [job] = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    const raw = await page.evaluate(async id => { const response = await fetch(`/api/jobs/${id}/update`); return { status: response.status, body: await response.json() }; }, job.id);
    assert.ok(raw.status >= 400 && typeof raw.body.error.code === 'string', 'the host refuses the check');
    const card = page.locator(`[data-job-id="${job.id}"]`);
    await card.getByRole('button', { name: 'Check for newer file' }).click();
    const line = card.locator('.update-line');
    await line.waitFor();
    assert.equal(await line.getAttribute('data-update-status'), 'failed');
    assert.match(await line.innerText(), /^Could not check for updates\. Nexus asked ModBridge to slow down\. Wait a minute and try again\.$/);
    assert.equal(await card.getByRole('button', { name: /Download newer/ }).count(), 0);
    assert.equal(await page.locator('#status-banner').isHidden(), true);
  });
  // ---- Mod requirements: the real host route, validator and details sheet against the fixture's fake Nexus client ----
  const introSentence = 'The author lists these on Nexus. ModBridge has not checked whether any of them are installed or compatible.';
  const rateLimitSentence = 'Nexus asked ModBridge to slow down. Wait a minute and try again.';
  // Opens the sheet of the fixture's one mod with Options/F6 on its card, as a controller does, and waits for the section to settle.
  const openRequirements = async page => {
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    await page.locator('[data-card="mod:1"]').focus();
    await page.keyboard.press('F6');
    await page.locator('#details-dialog[open]').waitFor();
    const section = page.locator('#details-content [data-requirements]');
    await page.locator('#details-content [data-requirements-status]:not([data-requirements-status="loading"])').waitFor({ timeout: 15000 });
    return section;
  };
  // Nothing in a requirements section may read as a verdict about installation. The one allowed sentence says "installed or compatible" only to
  // say none of them were checked, so it is taken out before looking.
  const assertNoVerdict = (text, label) => assert.doesNotMatch(text.replace(introSentence, ''), /\b(installed|met|satisfied|satisfies|missing|ok|okay|passed|ready|compatible)\b|[✓✔]/i, `${label}: reads as a verdict`);
  const requirementRequests = page => { const seen = []; page.on('request', request => { if (/\/api\/games\/[^/]+\/mods\/[0-9]+\/requirements$/.test(new URL(request.url()).pathname)) seen.push(new URL(request.url()).pathname); }); return seen; };
  await installFlow('requirements', async page => {
    // The real host serves the route from the fixture's decoded record; the page's fail-closed validator accepts it and draws every row as "Not checked".
    const asked = requirementRequests(page);
    assert.equal((await page.evaluate(async () => (await (await fetch('/api/capabilities')).json()).capabilities.mod_requirements)), true);
    const wire = await page.evaluate(async () => { const response = await fetch('/api/games/ownedgame/mods/1/requirements'); return { status: response.status, body: await response.json() }; });
    assert.equal(wire.status, 200);
    assert.deepEqual(Object.keys(wire.body).sort(), ['dlc_requirements', 'discovery_complete', 'file_version_constraints_provided', 'game', 'installation_supported', 'mod_id', 'nexus_requirements'].sort());
    assert.deepEqual([wire.body.game, wire.body.mod_id, wire.body.discovery_complete, wire.body.file_version_constraints_provided, wire.body.installation_supported], ['ownedgame', 1, true, false, false]);
    assert.deepEqual(wire.body.nexus_requirements.map(row => [row.external_requirement, row.name, row.url]),
      [[false, 'Fixture Framework', 'https://www.nexusmods.com/ownedgame/mods/5'], [true, 'Fixture Script Runtime', 'https://example.com/fixture-runtime']]);
    assert.deepEqual(wire.body.dlc_requirements.map(row => row.name), ['Fixture Expansion']);
    assert.ok([...wire.body.nexus_requirements, ...wire.body.dlc_requirements].every(row => row.satisfaction === 'unresolved'));
    asked.length = 0;
    const section = await openRequirements(page);
    assert.deepEqual(asked, ['/api/games/ownedgame/mods/1/requirements'], 'opening the sheet asks the host once');
    assert.equal(await page.locator('#details-content .detail-label', { hasText: 'Requirements listed by the author' }).count(), 1);
    assert.equal(await section.locator('.req-title').innerText(), 'Requirements listed on Nexus');
    assert.equal(await section.locator('[data-requirements-status]').getAttribute('data-requirements-status'), 'listed');
    assert.equal(await section.locator('[data-requirements-status]').innerText(), introSentence);
    assert.deepEqual(await section.locator('.req-name').allInnerTexts(), ['Fixture Framework', 'Fixture Script Runtime', 'DLC: Fixture Expansion']);
    assert.deepEqual(await section.locator('.req-kind').allInnerTexts(), ['Nexus mod', 'Not on Nexus', 'DLC']);
    assert.deepEqual(await section.locator('.req-state').allInnerTexts(), ['Not checked', 'Not checked', 'Not checked']);
    assert.deepEqual(await section.locator('.req-row').evaluateAll(rows => rows.map(row => row.querySelector('.req-notes')?.textContent ?? null)), ['Install it before this mod.', null, 'Story pack']);
    const links = await section.locator('.req-link').evaluateAll(els => els.map(el => [el.closest('.req-row').querySelector('.req-name').textContent, el.textContent.replace(/\s+/g, ' ').trim(), el.getAttribute('href'), el.target, el.rel]));
    assert.deepEqual(links, [['Fixture Framework', 'Open on Nexus', 'https://www.nexusmods.com/ownedgame/mods/5', '_blank', 'noopener noreferrer'],
      ['Fixture Script Runtime', 'Open external link example.com', 'https://example.com/fixture-runtime', '_blank', 'noopener noreferrer']], 'no link for the DLC row');
    // Information only: nothing here acts on a requirement, says anything is installed or met, or raises an error.
    assert.equal(await section.locator('button').count(), 0);
    assertNoVerdict(await section.innerText(), 'requirements section');
    for (const row of await section.locator('.req-row').allInnerTexts()) assertNoVerdict(row, 'row');
    assert.equal(await page.locator('#announcer').innerText(), 'Requirements listed by the author: 3 listed on Nexus. None are checked.');
    assert.equal(await page.locator('#status-banner').isHidden(), true);
    // Closing and reopening the sheet is answered from the page's own copy.
    await page.keyboard.press('Escape');
    await page.locator('[data-card="mod:1"]').focus();
    await page.keyboard.press('F6');
    await page.locator('#details-content .req-row').first().waitFor();
    assert.equal(asked.length, 1);
    assert.equal(await page.locator('#details-content .req-row').count(), 3);
  });
  await installFlow('requirements-fails', async page => {
    // Nexus is rate limiting: the section reads the catalog text for the code with Retry, never an empty list or a verdict, and Retry asks again.
    const asked = requirementRequests(page);
    const section = await openRequirements(page);
    assert.equal(await section.locator('.req-title').innerText(), 'Could not load requirements');
    assert.equal(await section.locator('[data-requirements-status]').getAttribute('data-requirements-status'), 'failed');
    assert.equal(await section.locator('[data-requirements-status]').innerText(), rateLimitSentence);
    assert.equal(await section.getByRole('button', { name: 'Retry' }).count(), 1);
    assert.equal(await section.locator('.req-row, .req-state, .req-link').count(), 0);
    assert.doesNotMatch(await section.innerText(), /No requirements listed|Not checked/);
    assertNoVerdict(await section.innerText(), 'failed section');
    assert.equal(await page.locator('#announcer').innerText(), 'Could not load requirements.');
    assert.equal(await page.locator('#status-banner').isHidden(), true, 'the optional section raises no page-wide error');
    assert.equal(asked.length, 1, 'opening the sheet asks the host once');
    const raw = await page.evaluate(async () => { const response = await fetch('/api/games/ownedgame/mods/1/requirements'); return { status: response.status, body: await response.json() }; });
    assert.ok(raw.status >= 400 && raw.body.error.code === 'rate_limit', 'the host refuses with the provider code');
    asked.length = 0;
    // The host does not cache a failure: Retry asks it again (and here it fails again).
    await section.getByRole('button', { name: 'Retry' }).click();
    await page.locator('#details-content [data-requirements-status="loading"]').waitFor();
    await page.locator('#details-content [data-requirements-status="failed"]').waitFor();
    assert.equal(asked.length, 1, 'Retry asks the host once');
    assert.equal(await section.locator('[data-requirements-status]').innerText(), rateLimitSentence);
    assert.equal(await section.getByRole('button', { name: 'Retry' }).count(), 1);
  });
  await installFlow('requirements-flaky', async page => {
    // The first answer fails and the second is a list: Retry replaces the error with the rows, which still read "Not checked".
    const asked = requirementRequests(page);
    const section = await openRequirements(page);
    assert.equal(await section.locator('[data-requirements-status]').getAttribute('data-requirements-status'), 'failed');
    assert.equal(await section.locator('[data-requirements-status]').innerText(), rateLimitSentence);
    await section.getByRole('button', { name: 'Retry' }).click();
    await page.locator('#details-content [data-requirements-status="listed"]').waitFor();
    assert.equal(asked.length, 2);
    assert.equal(await section.locator('.req-row').count(), 3);
    assert.deepEqual(await section.locator('.req-state').allInnerTexts(), ['Not checked', 'Not checked', 'Not checked']);
    assert.equal(await section.getByRole('button', { name: 'Retry' }).count(), 0);
    assert.doesNotMatch(await section.innerText(), /Could not load requirements/);
    assertNoVerdict(await section.innerText(), 'recovered section');
  });
  // ---- Nexus search: the real host route, validator and results view against the fixture's fake Nexus client ----
  const searchRequests = page => { const seen = []; page.on('request', request => { const url = new URL(request.url()); if (/\/api\/games\/[^/]+\/mods\/search$/.test(url.pathname)) seen.push(url.search); }); return seen; };
  const searchFor = async (page, text) => { await page.locator('#nexus-search-input').fill(text); await page.locator('#nexus-search-input').press('Enter'); };
  const openOwnedGame = async page => { await page.getByRole('button', { name: 'Owned Game Fixture' }).click(); await page.locator('[data-card="mod:1"]').waitFor(); };
  const openGameOnly = page => page.getByRole('button', { name: 'Owned Game Fixture' }).click();
  const subtitle = page => page.locator('#games-view .subheading').innerText();
  const searchState = (page, status) => page.locator(`#games-content [data-search-status="${status}"]`);
  const pageLabel = page => page.locator('[data-search-pager="top"] .search-page').innerText();
  await installFlow('search', async page => {
    const asked = searchRequests(page);
    assert.equal((await page.evaluate(async () => (await (await fetch('/api/capabilities')).json()).capabilities.mod_search)), true);
    // The host answers with exactly the four documented keys, each item the ten-field latest-mods record, and judges the query before Nexus is asked.
    const wire = await page.evaluate(async () => { const response = await fetch('/api/games/ownedgame/mods/search?q=paged&page=2'); return { status: response.status, body: await response.json() }; });
    assert.equal(wire.status, 200);
    assert.deepEqual(Object.keys(wire.body).sort(), ['items', 'page', 'pages', 'total']);
    assert.deepEqual([wire.body.total, wire.body.page, wire.body.pages], [45, 2, 3]);
    assert.deepEqual(wire.body.items.map(item => item.mod_id), [201, 202]);
    assert.deepEqual(Object.keys(wire.body.items[0]).sort(), ['author', 'contains_adult_content', 'description', 'game_domain', 'game_id', 'mod_id', 'name', 'source_url', 'summary', 'version']);
    for (const query of ['q=a', 'q=%20%20%20', 'page=1', 'q=ab&page=0', 'q=ab&page=51', 'q=ab&page=1x', 'q=ab&x=1', 'q=ab&q=cd', 'q=ab%09cd']) {
      const refused = await page.evaluate(async query => { const response = await fetch(`/api/games/ownedgame/mods/search?${query}`); return { status: response.status, body: await response.json() }; }, query);
      assert.equal(refused.status, 400, query); assert.equal(refused.body.error.code, 'invalid_input', query);
    }
    asked.length = 0;
    await openOwnedGame(page);
    assert.equal(await page.locator('#nexus-search').isVisible(), true, 'the host advertises mod_search and a game is chosen');
    // A search by Enter: the page sends the trimmed, collapsed text and page 1; the real host's reply passes the page's validator.
    await searchFor(page, '  owned   archive ');
    await page.locator('#games-content h2.content-title', { hasText: 'Search results from Nexus' }).waitFor();
    assert.deepEqual(asked, ['?q=owned%20archive&page=1']);
    assert.equal(await page.locator('#games-view h1').innerText(), 'Search results for Owned Game Fixture');
    assert.equal(await subtitle(page), 'Results come from a Nexus search for owned archive. 2 mods matched.');
    // The same mod cards as the latest list, names as text: the fixture's second result is hostile-looking markup.
    assert.deepEqual(await page.locator('[data-card^="mod:"] .card-title').allInnerTexts(), ['Owned Archive Fixture', 'Hostile <b>Owned</b> result']);
    assert.equal(await page.locator('#games-content b').count(), 0);
    assert.equal(await page.locator('#catalog-tools').isHidden(), true, 'the local filter and sort are off while results show');
    assert.equal(await page.locator('#announcer').innerText(), 'Search results from Nexus: 2 mods matched, page 1 of 1.');
    assert.doesNotMatch(await page.locator('#games-content').innerText(), /compatible|installed|safe/i, 'a result is a listing, nothing more');
    // Paging through the real host: three pages of two listed mods each (45 matched), never beyond the last.
    await searchFor(page, 'paged');
    await page.locator('[data-card="mod:101"]').waitFor();
    assert.equal(await subtitle(page), 'Results come from a Nexus search for paged. 45 mods matched.');
    assert.equal(await pageLabel(page), 'Page 1 of 3');
    const nextTop = page.locator('[data-search-pager="top"] [data-search-page="next"]'), prevTop = page.locator('[data-search-pager="top"] [data-search-page="prev"]');
    assert.equal(await prevTop.isDisabled(), true);
    await nextTop.click(); await page.locator('[data-card="mod:201"]').waitFor();
    assert.equal(await pageLabel(page), 'Page 2 of 3');
    await nextTop.click(); await page.locator('[data-card="mod:301"]').waitFor();
    assert.equal(await pageLabel(page), 'Page 3 of 3');
    assert.equal(await nextTop.isDisabled(), true, 'no Next past the last page');
    assert.equal(await page.locator('[data-search-pager="bottom"] [data-search-page="next"]').isDisabled(), true);
    await prevTop.click(); await page.locator('[data-card="mod:201"]').waitFor();
    assert.equal(await pageLabel(page), 'Page 2 of 3');
    assert.deepEqual(asked.slice(1), ['?q=paged&page=1', '?q=paged&page=2', '?q=paged&page=3', '?q=paged&page=2']);
    // A result opens its files through the real host, and returning shows the results again without asking Nexus.
    await searchFor(page, 'archive');
    await page.locator('[data-card="mod:1"]').waitFor();
    const before = asked.length;
    await page.locator('[data-card="mod:1"]').click();
    await page.locator('[data-file-card="2"]').waitFor();
    assert.equal(await page.locator('#games-view h1').innerText(), 'Files for Owned Archive Fixture');
    assert.equal(await page.locator('#nexus-search').isHidden(), true);
    await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
    await page.locator('[data-card="mod:2"]').waitFor();
    assert.equal(await subtitle(page), 'Results come from a Nexus search for archive. 2 mods matched.');
    assert.equal(asked.length, before, 'going back asks Nexus nothing');
    // No match and nothing listable are successful answers with their own words, and no Retry.
    await searchFor(page, 'empty words');
    await searchState(page, 'empty').waitFor();
    assert.equal(await searchState(page, 'empty').locator('h2').innerText(), 'No mods matched');
    assert.equal(await searchState(page, 'empty').locator('p').innerText(), 'Nexus found no mods whose name matches empty words for this game. Try fewer or different words.');
    assert.equal(await page.locator('[data-search-retry]').count(), 0);
    assert.equal(await page.locator('#announcer').innerText(), 'No mods matched.');
    await searchFor(page, 'hidden things');
    await searchState(page, 'none-listed').waitFor();
    assert.equal(await searchState(page, 'none-listed').locator('h2').innerText(), 'No mods to show on this page');
    assert.equal(await searchState(page, 'none-listed').locator('p').innerText(), 'Nexus matched 5 mods, but none can be listed here (hidden by your content settings or not published).');
    assert.equal(await page.locator('[data-search-retry]').count(), 0);
    assert.doesNotMatch(await page.locator('#games-content').innerText(), /No mods matched/);
    // Back to all mods (the host advertises mod_browse, so All mods is the list the search started from): the loaded page returns without
    // another browse or latest-mods request, and the local filter, which belongs to the Newest added list, stays off.
    const listRequests = []; page.on('request', request => { if (/\/api\/games\/[^/]+\/mods(\/browse)?$/.test(new URL(request.url()).pathname)) listRequests.push(request.url()); });
    const searches = asked.length;
    await page.getByRole('button', { name: 'Back to all mods' }).first().click();
    await page.getByRole('heading', { name: 'All mods for Owned Game Fixture' }).waitFor();
    assert.deepEqual(await page.locator('[data-card^="mod:"]').evaluateAll(els => els.map(el => el.dataset.card)), ['mod:1', 'mod:1012']);
    assert.equal(await page.locator('#catalog-tools').isHidden(), true);
    assert.equal(await page.locator('#nexus-search-input').inputValue(), '');
    assert.deepEqual(listRequests, []);
    assert.equal(asked.length, searches);
    assert.equal(await page.locator('#status-banner').isHidden(), true);
  });
  await installFlow('search-fails', async page => {
    // Nexus is rate limiting: the page reads the catalog text with Retry, never "No mods matched", and Retry asks the real host again.
    const asked = searchRequests(page);
    await openOwnedGame(page);
    await searchFor(page, 'owned archive');
    await searchState(page, 'failed').waitFor();
    assert.equal(await searchState(page, 'failed').locator('h2').innerText(), 'Could not search Nexus');
    assert.equal(await searchState(page, 'failed').locator('p').innerText(), rateLimitSentence);
    assert.equal(await page.getByRole('button', { name: 'Retry' }).count(), 1);
    assert.equal(await page.locator('#games-content [data-card^="mod:"]').count(), 0);
    assert.doesNotMatch(await page.locator('body').innerText(), /No mods matched|mods matched/);
    assert.equal(await page.locator('#announcer').innerText(), 'Could not search Nexus.');
    assert.equal(await page.locator('#status-banner').isHidden(), true, 'the search raises no page-wide error');
    const raw = await page.evaluate(async () => { const response = await fetch('/api/games/ownedgame/mods/search?q=owned%20archive&page=1'); return { status: response.status, body: await response.json() }; });
    assert.ok(raw.status >= 400 && raw.body.error.code === 'rate_limit', 'the host refuses with the provider code');
    asked.length = 0;
    await page.getByRole('button', { name: 'Retry' }).click();
    await page.locator('#games-content [data-search-status="loading"]').waitFor();
    await searchState(page, 'failed').waitFor();
    assert.deepEqual(asked, ['?q=owned%20archive&page=1'], 'Retry asks once, for the same text and page');
    assert.equal(await searchState(page, 'failed').locator('p').innerText(), rateLimitSentence);
    // Leaving the search returns to All mods.
    await page.getByRole('button', { name: 'Back to all mods' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Owned Game Fixture');
  });
  await installFlow('search-flaky', async page => {
    // The first answer fails and the second is a list: Retry replaces the failure with the results (the host does not cache a failure).
    await openOwnedGame(page);
    await searchFor(page, 'owned archive');
    await searchState(page, 'failed').waitFor();
    await page.getByRole('button', { name: 'Retry' }).click();
    await page.locator('#games-content h2.content-title', { hasText: 'Search results from Nexus' }).waitFor();
    assert.equal(await subtitle(page), 'Results come from a Nexus search for owned archive. 2 mods matched.');
    assert.deepEqual(await page.locator('[data-card^="mod:"] .card-title').allInnerTexts(), ['Owned Archive Fixture', 'Hostile <b>Owned</b> result']);
    assert.equal(await page.locator('[data-search-status="failed"]').count(), 0);
    assert.doesNotMatch(await page.locator('#games-content').innerText(), /Could not search Nexus/);
  });
  // ---- Browse all mods: the real host route, validator and view against the fixture's fake Nexus client ----
  const browseRequests = page => { const seen = []; page.on('request', request => { const url = new URL(request.url()); if (/\/api\/games\/[^/]+\/mods\/browse$/.test(url.pathname)) seen.push(url.search); }); return seen; };
  const latestListRequests = page => { const seen = []; page.on('request', request => { const url = new URL(request.url()); if (/\/api\/games\/[^/]+\/mods$/.test(url.pathname)) seen.push(url.search); }); return seen; };
  const cardIds = page => page.locator('[data-card^="mod:"]').evaluateAll(els => els.map(el => el.dataset.card));
  const browseLabel = page => page.locator('[data-browse-pager="top"] .search-page').innerText();
  const browseStatus = (page, status) => page.locator(`#games-content [data-browse-status="${status}"]`);
  const pressedSort = page => page.locator('[data-browse-sort][aria-pressed="true"]').innerText();
  const focusKey = page => page.evaluate(() => document.activeElement?.dataset?.browseFocus || document.activeElement?.id || '');
  const browseWire = (page, query) => page.evaluate(async query => { const response = await fetch(`/api/games/ownedgame/mods/browse${query}`); return { status: response.status, body: await response.json() }; }, query);
  await installFlow('browse', async page => {
    const asked = browseRequests(page), latest = latestListRequests(page);
    assert.equal((await page.evaluate(async () => (await (await fetch('/api/capabilities')).json()).capabilities.mod_browse)), true);
    // The host answers with exactly the five documented keys, each item the ten-field latest-mods record, and judges the query before Nexus is asked.
    const wire = await browseWire(page, '?sort=endorsements&page=2');
    assert.equal(wire.status, 200);
    assert.deepEqual(Object.keys(wire.body).sort(), ['items', 'page', 'pages', 'sort', 'total']);
    assert.deepEqual([wire.body.total, wire.body.page, wire.body.pages, wire.body.sort], [45, 2, 3, 'endorsements']);
    assert.deepEqual(wire.body.items.map(item => item.mod_id), [2021, 2022]);
    assert.deepEqual(Object.keys(wire.body.items[0]).sort(), ['author', 'contains_adult_content', 'description', 'game_domain', 'game_id', 'mod_id', 'name', 'source_url', 'summary', 'version']);
    const bare = await browseWire(page, '');
    assert.deepEqual([bare.status, bare.body.sort, bare.body.page], [200, 'downloads', 1], 'both fields are optional');
    for (const query of ['?sort=relevance', '?sort=', '?sort=Name', '?page=0', '?page=51', '?page=1x', '?q=ab', '?sort=name&x=1', '?sort=name&sort=name', '?sort=name%00']) {
      const refused = await browseWire(page, query);
      assert.equal(refused.status, 400, query); assert.equal(refused.body.error.code, 'invalid_input', query);
    }
    asked.length = 0; latest.length = 0;
    // Picking a game shows All mods, most downloaded first, and does not ask for the latest list.
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    assert.deepEqual(asked, ['?sort=downloads&page=1']);
    assert.deepEqual(latest, [], 'the newest list is asked for only when it is chosen');
    assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Owned Game Fixture');
    assert.equal(await subtitle(page), 'Results come from Nexus, sorted most downloaded first. Nexus counts 45 mods for this game.');
    assert.deepEqual(await cardIds(page), ['mod:1', 'mod:1012']);
    assert.deepEqual(await page.locator('[data-browse-view]').allInnerTexts(), ['All mods', 'Newest added']);
    assert.deepEqual(await page.locator('[data-browse-sort]').allInnerTexts(), ['Most downloaded', 'Most endorsed', 'Recently updated', 'Name A–Z']);
    assert.equal(await pressedSort(page), 'Most downloaded');
    assert.equal(await page.locator('[data-browse-view][aria-pressed="true"]').innerText(), 'All mods');
    assert.equal(await browseLabel(page), 'Page 1 of 3');
    assert.equal(await page.locator('#announcer').innerText(), 'All mods from Nexus, most downloaded first: page 1 of 3.');
    assert.equal(await page.locator('#catalog-tools').isHidden(), true, 'the local filter belongs to the newest list');
    assert.doesNotMatch(await page.locator('#games-content').innerText(), /compatible|installed|safe|works/i, 'a listing says nothing about a mod working');
    // A sort change asks for page 1 of that sort; focus stays on the pressed sort button.
    await page.getByRole('button', { name: 'Most endorsed' }).click();
    await page.locator('[data-card="mod:2011"]').waitFor();
    assert.deepEqual(asked, ['?sort=downloads&page=1', '?sort=endorsements&page=1']);
    assert.equal(await pressedSort(page), 'Most endorsed');
    assert.equal(await subtitle(page), 'Results come from Nexus, sorted most endorsed first. Nexus counts 45 mods for this game.');
    assert.equal(await page.locator('#announcer').innerText(), 'All mods from Nexus, most endorsed first: page 1 of 3.');
    assert.equal(await focusKey(page), 'sort-endorsements');
    // Paging: three pages, never past the last, and the sort holds across pages.
    const nextTop = page.locator('[data-browse-pager="top"] [data-browse-page="next"]'), prevTop = page.locator('[data-browse-pager="top"] [data-browse-page="prev"]');
    assert.equal(await prevTop.isDisabled(), true);
    await nextTop.click(); await page.locator('[data-card="mod:2021"]').waitFor();
    assert.equal(await browseLabel(page), 'Page 2 of 3');
    await nextTop.click(); await page.locator('[data-card="mod:2031"]').waitFor();
    assert.equal(await browseLabel(page), 'Page 3 of 3');
    assert.equal(await nextTop.isDisabled(), true, 'no Next past the last page');
    assert.equal(await page.locator('[data-browse-pager="bottom"] [data-browse-page="next"]').isDisabled(), true);
    await prevTop.click(); await page.locator('[data-card="mod:2021"]').waitFor();
    assert.deepEqual(asked.slice(2), ['?sort=endorsements&page=2', '?sort=endorsements&page=3', '?sort=endorsements&page=2']);
    // Changing the sort from page 2 starts at page 1 of the new sort; a hostile-looking name is text.
    await page.getByRole('button', { name: 'Name A–Z' }).click();
    await page.locator('[data-card="mod:4012"]').waitFor();
    assert.equal(asked.at(-1), '?sort=name&page=1');
    assert.equal(await browseLabel(page), 'Page 1 of 3');
    assert.equal(await page.locator('[data-card="mod:4012"] .card-title').innerText(), 'Hostile <b>Owned</b> A-Z');
    assert.equal(await page.locator('#games-content b').count(), 0);
    // A mod opens its files like any other card; going back shows the same page of the same sort without asking again.
    const before = asked.length;
    await page.locator('[data-card="mod:4011"]').click();
    await page.locator('[data-file-card="2"]').waitFor();
    assert.equal(await page.locator('#games-view h1').innerText(), 'Files for Name A-Z page 1 mod 1');
    await page.locator('.breadcrumbs [data-breadcrumb="mods"]').click();
    await page.locator('[data-card="mod:4011"]').waitFor();
    assert.equal(await page.locator('#games-view h1').innerText(), 'All mods for Owned Game Fixture');
    assert.equal(await pressedSort(page), 'Name A–Z');
    assert.equal(asked.length, before, 'going back asks Nexus nothing');
    // Newest added is the REST latest list, asked for the first time it is chosen and kept while the game stays; the local filter is back.
    await page.getByRole('button', { name: 'Newest added' }).click();
    await page.getByRole('heading', { name: 'Newest added mods for Owned Game Fixture' }).waitFor();
    await page.locator('[data-card="mod:1"]').waitFor();
    assert.deepEqual(latest, [''], 'one latest-mods request');
    assert.deepEqual(await cardIds(page), ['mod:1']);
    assert.equal(await subtitle(page), 'Newest added mods from Nexus · this is not a search result.');
    assert.equal(await page.locator('#catalog-search-label').innerText(), 'Filter loaded newest mods');
    assert.equal(await page.locator('#catalog-tools').isVisible(), true);
    assert.equal(await page.locator('[data-browse-sort]').count(), 0, 'the sorts belong to All mods');
    assert.equal(await page.locator('[data-browse-view][aria-pressed="true"]').innerText(), 'Newest added');
    await page.getByRole('button', { name: 'All mods' }).click();
    await page.locator('[data-card="mod:4011"]').waitFor();
    await page.getByRole('button', { name: 'Newest added' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    assert.deepEqual(latest, [''], 'the newest list is kept');
    assert.equal(asked.length, before, 'switching views asks Nexus nothing');
    // Search from either view comes back to the view it started from.
    await searchFor(page, 'owned archive');
    await page.locator('#games-content h2.content-title', { hasText: 'Search results from Nexus' }).waitFor();
    await page.getByRole('button', { name: 'Back to newest mods' }).first().click();
    await page.getByRole('heading', { name: 'Newest added mods for Owned Game Fixture' }).waitFor();
    await page.getByRole('button', { name: 'All mods' }).click();
    await searchFor(page, 'owned archive');
    await page.locator('#games-content h2.content-title', { hasText: 'Search results from Nexus' }).waitFor();
    await page.getByRole('button', { name: 'Back to all mods' }).first().click();
    await page.getByRole('heading', { name: 'All mods for Owned Game Fixture' }).waitFor();
    assert.equal(await pressedSort(page), 'Name A–Z');
    assert.equal(asked.length, before, 'search and back asked the browse route nothing');
    assert.equal(await page.locator('#status-banner').isHidden(), true);
  });
  await installFlow('browse-fails', async page => {
    // Nexus is rate limiting: the page reads the catalog text with Retry, never "No mods found", and the other view still works.
    const asked = browseRequests(page);
    await openGameOnly(page);
    await browseStatus(page, 'failed').waitFor();
    assert.equal(await browseStatus(page, 'failed').locator('h2').innerText(), 'Could not load mods from Nexus');
    assert.equal(await browseStatus(page, 'failed').locator('p').innerText(), rateLimitSentence);
    assert.equal(await subtitle(page), 'Could not load mods from Nexus.');
    assert.equal(await page.getByRole('button', { name: 'Retry' }).count(), 1);
    assert.equal(await page.locator('[data-card^="mod:"]').count(), 0);
    assert.doesNotMatch(await page.locator('body').innerText(), /No mods found/);
    assert.equal(await page.locator('#status-banner').isHidden(), true, 'the list raises no page-wide error');
    assert.equal(await page.locator('#announcer').innerText(), 'Could not load mods from Nexus.');
    const raw = await browseWire(page, '?sort=downloads&page=1');
    assert.ok(raw.status >= 400 && raw.body.error.code === 'rate_limit', 'the host refuses with the provider code');
    asked.length = 0;
    await page.getByRole('button', { name: 'Retry' }).click();
    await browseStatus(page, 'failed').waitFor();
    assert.deepEqual(asked, ['?sort=downloads&page=1'], 'Retry asks once, for the same sort and page');
    // The newest list does not depend on the browse route.
    await page.getByRole('button', { name: 'Newest added' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    await page.getByRole('button', { name: 'All mods' }).click();
    await browseStatus(page, 'failed').waitFor();
  });
  await installFlow('browse-flaky', async page => {
    // The first answer fails and the second is a page: Retry replaces the failure (the host does not cache a failure).
    await openGameOnly(page);
    await browseStatus(page, 'failed').waitFor();
    await page.getByRole('button', { name: 'Retry' }).click();
    await page.locator('[data-card="mod:1"]').waitFor();
    assert.deepEqual(await cardIds(page), ['mod:1', 'mod:1012']);
    assert.equal(await page.locator('[data-browse-status="failed"]').count(), 0);
    assert.equal(await subtitle(page), 'Results come from Nexus, sorted most downloaded first. Nexus counts 45 mods for this game.');
  });
  await installFlow('browse-empty', async page => {
    await openGameOnly(page);
    await browseStatus(page, 'empty').waitFor();
    assert.equal(await browseStatus(page, 'empty').locator('h2').innerText(), 'No mods found');
    assert.equal(await page.getByRole('button', { name: 'Retry' }).count(), 0);
    assert.equal(await page.locator('#announcer').innerText(), 'No mods found.');
  });
  // ---- Titles from a console-shaped folder (--titles-root / FIXTURE_TITLES_ROOT): the real host's paging routes feed the page's own validator ----
  const titlesRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'modbridge-titles-'));
  try {
    const param = (id, version, name, extra = {}) => ({ applicationDrmType: 'standard', masterVersion: '01.00', titleId: id, ...(version === null ? {} : { contentVersion: version }),
      localizedParameters: { defaultLanguage: 'en-US', 'en-US': { titleName: name }, 'de-DE': { titleName: 'Anderer Name' } }, ...extra });
    const put = (file, value) => { fs.mkdirSync(path.dirname(file), { recursive: true }); fs.writeFileSync(file, typeof value === 'string' ? value : JSON.stringify(value)); };
    const dump = (folder, value) => put(path.join(titlesRoot, 'data', 'homebrew', folder, 'sce_sys', 'param.json'), value);
    const cache = (where, id, value) => put(path.join(titlesRoot, ...where, id, 'param.json'), value);
    const hostile = '<b>Hostile</b> & "quotes" <img src=x onerror="window.__pwned=1">';
    dump('Silent Hill Townfall PPSA33286', param('PPSA33286', '02.003.000', 'SILENT HILL: Townfall'));
    dump('LIZARD ASTRO BOT 01.018.000 PPSA21567', param('PPSA21567', '01.018.000', 'ASTRO BOT'));
    dump('Hostile <b>folder PPSA00031', param('PPSA00031', '1.0', hostile));
    dump('Padded name PPSA00032', param('PPSA00032', null, ' padded'));
    put(path.join(titlesRoot, 'data', 'homebrew', 'etaHEN', 'etaHEN.elf'), 'not a game');
    fs.mkdirSync(path.join(titlesRoot, 'data', 'homebrew', 'payloads'));
    // The same title in the cache with another name and version: the dump is what the game runs from and wins.
    cache(['user', 'appmeta'], 'PPSA33286', param('PPSA33286', '01.000.000', 'Appmeta Townfall'));
    cache(['user', 'appmeta'], 'PPSA00010', param('PPSA00010', '1.0', 'User only'));
    cache(['system_data', 'priv', 'appmeta'], 'PPSA00020', param('PPSA00020', '2.0', 'System only'));
    for (const n of [40, 41, 42, 43]) cache(['user', 'appmeta'], `PPSA000${n}`, param(`PPSA000${n}`, '3.0', `Cached ${n}`));
    cache(['user', 'appmeta'], 'PPSA00099', '{ this is not json');
    const expected = [['PPSA00010', 'user_metadata_cache', 'User only', '1.0'], ['PPSA00020', 'system_private_metadata_cache', 'System only', '2.0'],
      ['PPSA00031', 'homebrew_dump', hostile, '1.0'], ['PPSA00032', 'homebrew_dump', null, null],
      ...[40, 41, 42, 43].map(n => [`PPSA000${n}`, 'user_metadata_cache', `Cached ${n}`, '3.0']),
      ['PPSA21567', 'homebrew_dump', 'ASTRO BOT', '01.018.000'], ['PPSA33286', 'homebrew_dump', 'SILENT HILL: Townfall', '02.003.000']];
    const titleWire = page => page.evaluate(async () => {
      const pages = [];
      for (let path = '/api/detected-titles/pages', n = 0; path && n < 20; n++) {
        const response = await fetch(path, { cache: 'no-store' }), text = await response.text();
        const value = JSON.parse(text);
        pages.push({ status: response.status, text, value });
        path = value.page?.next_cursor ? `/api/detected-titles/pages/${value.page.next_cursor}` : '';
      }
      const id = pages[0].value.snapshot.snapshot_id;
      const current = id ? await (await fetch(`/api/detected-titles/pages/current/${id}`)).json() : null;
      return { pages, current };
    });
    const field = (item, name) => (item[name].status === 'observed' ? item[name].value : null);
    await installFlow('titles', async page => {
      assert.equal(await page.evaluate(async () => (await (await fetch('/api/capabilities')).json()).capabilities.title_inventory_paging), true);
      const wire = await titleWire(page);
      assert.deepEqual(wire.pages.map(entry => entry.status), [200, 200], 'ten titles are two pages');
      for (const entry of wire.pages) { assert.ok(entry.text.length <= 8191); assert.ok(!entry.text.includes(titlesRoot), 'no host path on the wire'); }
      const first = wire.pages[0].value;
      assert.deepEqual([first.schema_version, first.kind, first.snapshot.status, first.snapshot.complete, first.snapshot.source_scope, first.snapshot.reason],
        [2, 'title_inventory_page', 'observed', true, 'console_title_metadata', null]);
      assert.deepEqual(first.snapshot.issues, [{ code: 'provider_observation_incomplete' }], 'the malformed appmeta entry is flagged, the rest is listed');
      assert.deepEqual(wire.pages.map(entry => entry.value.items.length), [8, 2]);
      assert.deepEqual(wire.current, { current: true, kind: 'title_inventory_current', schema_version: 2, snapshot_id: first.snapshot.snapshot_id });
      const items = wire.pages.flatMap(entry => entry.value.items);
      assert.deepEqual(items.map(item => item.title_id.value), expected.map(row => row[0]), 'sorted by title id, one item per id');
      expected.forEach(([id, source, name, version], index) => {
        const item = items[index];
        assert.equal(item.title_id.source, source, id);
        assert.equal(field(item, 'display_name'), name, id);
        assert.equal(field(item, 'build_version'), version, id);
        assert.equal(item.build_fingerprint, null);
        if (name !== null) assert.equal(item.display_name.source, source, id);
      });
      assert.deepEqual(items[3].display_name, { value: null, status: 'error', source: null }, 'text the wire cannot carry is an error, not a failed capture');
      assert.deepEqual(items[3].build_version, { value: null, status: 'unavailable', source: null });
      // The page's own validator accepted both pages: the existing Console target panel lists every title.
      const choices = page.locator('#title-list .title-choice');
      await choices.nth(9).waitFor();
      assert.equal(await choices.count(), 10);
      assert.match(await page.locator('#title-status').innerText(), /Observed titles/);
      const townfall = choices.filter({ hasText: 'PPSA33286' });
      assert.equal(await townfall.count(), 1);
      const text = await townfall.innerText();
      assert.match(text, /SILENT HILL: Townfall/);
      assert.match(text, /02\.003\.000/);
      assert.doesNotMatch(await page.locator('#title-list').innerText(), /Appmeta Townfall|01\.000\.000/);
      // Host text is data: markup in a title name is shown as text and never runs.
      assert.match(await choices.filter({ hasText: 'PPSA00031' }).innerText(), /<b>Hostile<\/b> & "quotes" <img src=x onerror="window\.__pwned=1">/);
      assert.equal(await page.locator('#title-list b, #title-list img').count(), 0);
      assert.equal(await page.evaluate(() => window.__pwned), undefined);
      assert.match(await choices.filter({ hasText: 'PPSA00032' }).innerText(), /Display name unavailable/);
      await townfall.click();
      assert.match(await page.locator('#selected-target').innerText(), /PPSA33286 · SILENT HILL: Townfall/);
      // A new folder shows after Refresh targets (the host keeps its capture until then), and the refresh clears the chosen target.
      dump('Brand New PPSA55555', param('PPSA55555', '1.2.3', 'Brand New'));
      assert.equal((await titleWire(page)).pages.flatMap(entry => entry.value.items).length, 10, 'the cached capture is kept until a refresh');
      await page.locator('#reload-titles').click();
      await choices.nth(10).waitFor();
      assert.equal(await choices.count(), 11);
      assert.match(await page.locator('#selected-target').innerText(), /No console target/);
      const after = (await titleWire(page)).pages.flatMap(entry => entry.value.items);
      assert.deepEqual(after.map(item => item.title_id.value).slice(-2), ['PPSA33286', 'PPSA55555']);
      assert.equal(after[10].title_id.source, 'homebrew_dump');
      // The Nexus games list still loads and opens a game, with or without a title list.
      assert.equal((await page.evaluate(async () => (await (await fetch('/api/games')).json()).items.map(game => game.domain_name))).join(), 'ownedgame');
      await page.getByRole('button', { name: 'Owned Game Fixture' }).first().click();
      await page.locator('[data-card="mod:1"]').waitFor();
    }, { FIXTURE_TITLES_ROOT: titlesRoot });
  } finally {
    fs.rmSync(titlesRoot, { recursive: true, force: true });
  }
  // ---- Runtime profiles: the real host over a console-shaped mirror (a Townfall-like dump with a tiny fake ELF, an encrypted one, one with no
  // executable, an installed-only title) and a stand-in generator (tests/fake_profilegen.py). The status rules run in the host, the page shows
  // only what the host returns. ----
  const profileRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'modbridge-profile-flow-'));
  const profileRelease = path.join(profileRoot, 'go');
  try {
    await installFlow('profiles', async (page, fixture) => {
      assert.ok(fixture.console && fixture.profiles, 'the fixture says where its mirror and profiles are');
      const flags = await page.evaluate(async () => (await (await fetch('/api/capabilities')).json()).capabilities);
      assert.equal(flags.runtime_profiles, true);
      assert.equal(flags.profile_generate, true);
      const card = id => page.locator(`[data-your-game="${id}"]`);
      const line = async id => (await card(id).locator('.your-game-profile').innerText()).replace(/\s+/g, ' ');
      const button = id => card(id).locator(`[data-yg-generate="${id}"]`);
      const reason = id => card(id).locator('[data-yg-profile-reason]');
      // Locator waits only: the real host's content security policy forbids the string evaluation that waitForFunction needs.
      const waitFor = (id, expected, timeout = 12000) => page.locator(`[data-yg-profile="${id}"][data-profile-status="${expected}"]`).waitFor({ timeout });
      const refresh = async id => { await page.locator('#games-view').getByRole('button', { name: 'Refresh games' }).click(); await page.locator(`[data-yg-profile="${id}"]`).waitFor(); };
      const wire = path => page.evaluate(async path => { const response = await fetch(path); return { status: response.status, text: await response.text() }; }, path);
      const townfall = 'PPSA33286', version = '02.003.000', file = path.join(fixture.profiles, townfall, version, 'runtime-profile.json');
      const executable = path.join(fixture.console, 'data', 'homebrew', 'Silent Hill Townfall PPSA33286', 'decrypted', 'eboot.bin');
      const keys = ['confidence', 'generated_utc', 'profile_id', 'reason', 'status', 'unresolved'];
      await card(townfall).waitFor();
      for (const id of [townfall, 'PPSA21567', 'PPSA00031', 'PPSA00010']) await waitFor(id, 'none');
      // Nothing exists yet: every status is "none", the dumps offer Generate profile, and an installed-only title says why it cannot.
      assert.equal(await line(townfall), 'Runtime profile: None yet.');
      assert.equal(fs.existsSync(fixture.profiles), false, 'reading a status creates nothing');
      for (const id of [townfall, 'PPSA21567', 'PPSA00031']) { assert.equal(await button(id).innerText(), 'Generate profile'); assert.equal(await button(id).isEnabled(), true, id); }
      assert.equal(await button('PPSA00010').isDisabled(), true);
      assert.equal(await reason('PPSA00010').innerText(), 'Only a game dump can be profiled.');
      // The host judges the files: an encrypted executable and a dump with none are refused at once, with the catalog's sentences.
      await button('PPSA21567').click();
      await waitFor('PPSA21567', 'failed');
      assert.equal(await line('PPSA21567'), 'Runtime profile: Failed: The game’s executable is encrypted. A profile needs the decrypted file.');
      await button('PPSA00031').click();
      await waitFor('PPSA00031', 'failed');
      assert.equal(await line('PPSA00031'), 'Runtime profile: Failed: This game has no executable ModBridge can read. Only game dumps can be profiled.');
      assert.equal(fs.existsSync(fixture.profiles), false, 'a refused start writes nothing');
      // Generate on the Townfall-like dump: the stand-in generator is held, so the card shows Generating (and the others wait) until it is let go.
      await button(townfall).click();
      await waitFor(townfall, 'generating');
      assert.equal(await card(townfall).locator('.your-game-profile').getAttribute('aria-busy'), 'true');
      assert.equal(await button(townfall).innerText(), 'Generating…');
      assert.equal(await button('PPSA21567').getAttribute('aria-disabled'), 'true');
      const running = await wire(`/api/titles/${townfall}/profile?content_version=${version}`);
      assert.equal(running.status, 200);
      assert.deepEqual(Object.keys(JSON.parse(running.text)).sort(), keys);
      assert.equal(JSON.parse(running.text).status, 'generating');
      const second = await page.evaluate(async () => {
        const { csrf_token: csrf } = await (await fetch('/api/capabilities')).json();
        const response = await fetch('/api/titles/PPSA21567/profile/generate', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf }, body: '{}' });
        return { status: response.status, body: await response.json() };
      });
      assert.equal(second.status, 409, 'the host takes one generation at a time');
      assert.equal(second.body.error.code, 'busy');
      fs.writeFileSync(profileRelease, '');
      await waitFor(townfall, 'generated');
      assert.equal(await line(townfall), 'Runtime profile: Generated 2026-10-07 · 10 high, 3 medium, 2 low confidence · 2 unresolved. Made by reading the game’s executable; not tested on the console.');
      assert.equal(await button(townfall).innerText(), 'Regenerate');
      assert.equal(await button('PPSA21567').getAttribute('aria-disabled'), null);
      // On disk: private folders, a profile that is generated and unverified, and nothing the host wrote into it.
      for (const folder of [fixture.profiles, path.dirname(path.dirname(file)), path.dirname(file)]) assert.equal(fs.statSync(folder).mode & 0o777, 0o700, folder);
      const made = JSON.parse(fs.readFileSync(file, 'utf8'));
      assert.deepEqual([made.status, made.verification], ['generated', null]);
      const reply = await wire(`/api/titles/${townfall}/profile?content_version=${version}`);
      assert.deepEqual(Object.keys(JSON.parse(reply.text)).sort(), keys);
      for (const secret of [fixture.console, fixture.profiles, 'eboot', os.tmpdir()]) assert.ok(!reply.text.includes(secret) && !running.text.includes(secret), `no ${secret} on the wire`);
      // What GPT's receipt ingester would write: a passing verification of exactly this profile. Only then, and only per the host, does the card read Console-verified.
      assert.doesNotMatch(await page.locator('#your-games').innerText(), /Console-verified/);
      const verified = { ...made, status: 'console-verified', verification: { receipt_id: 'integration-receipt', date_utc: '2026-10-07T13:00:00Z', profile_digest: made.profile_digest,
        runtime_build: { build_id: 'integration', sha256: made.binding.executable.sha256 }, verified: ['symbols.high_0'], result: 'pass' } };
      fs.writeFileSync(file, JSON.stringify(verified, null, 2) + '\n');
      const before = fs.readFileSync(file);
      await refresh(townfall);
      await waitFor(townfall, 'console-verified');
      assert.match(await line(townfall), /^Runtime profile: Console-verified for this build · 2026-10-07 · 10 high, 3 medium, 2 low confidence · 2 unresolved\.$/);
      assert.equal(await button(townfall).innerText(), 'Regenerate');
      assert.ok(before.equals(fs.readFileSync(file)), 'reading never touches the file');
      // Regenerate on a verified profile: the generator would refuse (exit 3) and the host never forces it, so the page says so and the file stands.
      await button(townfall).click();
      await waitFor(townfall, 'failed');
      assert.equal(await line(townfall), 'Runtime profile: Failed: This profile carries a console verification, so it was not replaced. Replacing it is a deliberate step outside this page.');
      assert.ok(before.equals(fs.readFileSync(file)), 'a refused Regenerate leaves the verified file alone');
      await refresh(townfall);
      await waitFor(townfall, 'console-verified');
      // The game's executable changes (a new build): the verification is for another build, so it is stale, and the page says so.
      const original = fs.readFileSync(executable);
      fs.writeFileSync(executable, Buffer.concat([original, Buffer.from('patched')]));
      await refresh(townfall);
      await waitFor(townfall, 'stale');
      assert.match(await line(townfall), /^Runtime profile: Stale: Verified for a different build; regenerate\. 2026-10-07/);
      assert.doesNotMatch(await page.locator('#your-games').innerText(), /Console-verified/);
      fs.writeFileSync(executable, original);
      await refresh(townfall);
      await waitFor(townfall, 'console-verified');
      // A forged verification (another digest) is stale; a claim of verification with no block, and a file that is not a profile, are failures, never Generated.
      fs.writeFileSync(file, JSON.stringify({ ...verified, verification: { ...verified.verification, profile_digest: 'f'.repeat(64) } }, null, 2));
      await refresh(townfall);
      await waitFor(townfall, 'stale');
      fs.writeFileSync(file, JSON.stringify({ ...made, status: 'console-verified' }, null, 2));
      await refresh(townfall);
      await waitFor(townfall, 'failed');
      assert.equal(await line(townfall), 'Runtime profile: Failed: The saved profile claims a status it cannot back up.');
      fs.writeFileSync(file, 'this is not a profile\n');
      await refresh(townfall);
      await waitFor(townfall, 'failed');
      assert.equal(await line(townfall), 'Runtime profile: Failed: The saved profile is not a valid profile.');
      assert.doesNotMatch(await page.locator('#your-games').innerText(), /Generated|Console-verified/);
      // Generate again over the broken file: a valid, unverified profile.
      await button(townfall).click();
      await waitFor(townfall, 'generated');
      assert.deepEqual([JSON.parse(fs.readFileSync(file, 'utf8')).status, JSON.parse(fs.readFileSync(file, 'utf8')).verification], ['generated', null]);
      assert.equal(await page.locator('#your-games img, #your-games b').count(), 0);
    }, { FIXTURE_ROOT: profileRoot, FIXTURE_GENERATOR: 'hold', FIXTURE_RELEASE: profileRelease });
  } finally {
    fs.rmSync(profileRoot, { recursive: true, force: true });
  }
  // ---- Sign in with a personal API key: the real host, the real account manager and credential store, the provider boundary faked ----
  const keyRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'modbridge-key-'));
  const secret = 'owned-fixture-key-0123456789', stored = path.join(keyRoot, 'credentials.json');
  const fixtureMods = Array.from({ length: 45 }, (_, index) => { const n = index + 1; return { id: 100 + n, name: `${String.fromCharCode(65 + n * 7 % 26)} fixture mod ${String(n).padStart(2, '0')}` }; });
  const keyOrder = { downloads: fixtureMods, endorsements: [...fixtureMods].reverse(), updated: [...fixtureMods.slice(10), ...fixtureMods.slice(0, 10)], name: [...fixtureMods].sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0)) };
  const keyPosts = page => { const seen = []; page.on('request', request => { if (request.method() === 'POST' && new URL(request.url()).pathname === '/api/auth/key') seen.push(request.postData()); }); return seen; };
  const keyMessage = page => page.locator('#key-sign-in-message').innerText();
  try {
    await installFlow('key', async page => {
      const posts = keyPosts(page), bodies = [];
      page.on('response', async response => { try { bodies.push(await response.text()); } catch (_) { /* a redirect or aborted body has none */ } });
      // First run: signed out, no error banner, the key form is there, masked, with its plain sentence.
      await page.locator('#account').getByText('Nexus account not connected').waitFor();
      assert.deepEqual(await page.evaluate(async () => { const { capabilities } = await (await fetch('/api/capabilities')).json(); return [capabilities.nexus_key_sign_in, capabilities.nexus_auth_start, capabilities.mod_browse]; }), [true, false, true]);
      assert.equal(await page.locator('#status-banner').isHidden(), true, 'a first run is not an error');
      const panel = page.locator('#key-sign-in');
      assert.equal(await panel.isVisible(), true);
      assert.equal(await panel.locator('h2').innerText(), 'Sign in with your Nexus API key');
      assert.equal(await page.locator('#key-input').getAttribute('type'), 'password');
      assert.equal(await page.locator('#key-input').getAttribute('autocomplete'), 'off');
      assert.equal(await page.locator('#key-input').getAttribute('maxlength'), '512');
      assert.match(await page.locator('#key-sign-in-help').innerText(), /account settings, choose API Keys, and copy your Personal API Key/);
      assert.equal(await page.locator('#games-content .card').count(), 0, 'the catalog is asked for after sign-in');
      assert.equal(fs.existsSync(stored), false);
      // The controller reaches it: tab -> field (Down) -> button (Right at the caret end) -> field, and the field gives up Up and Down.
      await page.locator('#tab-games').focus();
      await page.keyboard.press('ArrowDown'); assert.equal(await page.evaluate(() => document.activeElement.id), 'key-input');
      await page.keyboard.press('ArrowRight'); assert.equal(await page.evaluate(() => document.activeElement.id), 'key-submit');
      await page.keyboard.press('ArrowLeft'); assert.equal(await page.evaluate(() => document.activeElement.id), 'key-input');
      await page.keyboard.press('ArrowUp'); assert.match(await page.evaluate(() => document.activeElement.id), /^tab-/);
      // Nothing, or a key with a space in it, is refused by the page with no request.
      await page.locator('#key-input').fill(''); await page.locator('#key-submit').click();
      assert.equal(await keyMessage(page), 'Type or paste your Nexus API key first.');
      await page.locator('#key-input').fill('two words'); await page.locator('#key-input').press('Enter');
      assert.equal(await keyMessage(page), 'An API key has no spaces and is at most 512 characters.');
      assert.deepEqual(posts, [], 'no request for a key the page can see is malformed');
      // A wrong key: the host refuses it, the catalog text shows by the field, the field stays masked and nothing is saved.
      await page.locator('#key-input').fill('not-the-right-key');
      await page.locator('#key-input').press('Enter');
      await page.locator('#key-sign-in-message', { hasText: 'Nexus did not accept that API key.' }).waitFor();
      assert.equal(await keyMessage(page), 'Nexus did not accept that API key. Copy the whole Personal API Key from your Nexus account settings and try again.');
      assert.equal(await page.locator('#key-input').getAttribute('type'), 'password');
      assert.equal(await page.locator('#key-sign-in').isVisible(), true);
      assert.match(await page.locator('#account').innerText(), /^Nexus account not connected\s+Sign in$/);
      assert.equal(fs.existsSync(stored), false, 'a refused key writes nothing');
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      assert.equal(posts.length, 1);
      // The right key: the account status refreshes, the form goes away, the field is empty, the catalog loads, the key is saved privately.
      await page.locator('#key-input').fill(`  ${secret}  `);
      await page.locator('#key-submit').click();
      await page.locator('#account').getByText('Nexus · Key fixture').waitFor();
      await page.getByRole('button', { name: 'Owned Game Fixture' }).waitFor();
      assert.equal(await page.locator('#key-sign-in').isHidden(), true);
      assert.equal(await page.locator('#key-input').inputValue(), '', 'the key does not stay in the page');
      assert.equal(await page.locator('#toast').innerText(), 'Signed in to Nexus.');
      assert.equal(JSON.parse(posts.at(-1)).api_key, secret, 'the surrounding space was trimmed before sending');
      assert.equal(fs.statSync(stored).mode & 0o777, 0o600);
      assert.equal(JSON.parse(fs.readFileSync(stored, 'utf8')).credentials.api_key, secret);
      assert.equal(await page.locator('#account .account-tier').innerText(), 'Premium');
      // All mods through the real client, GraphQL decoder included: 20 to a page, each sort in its own order.
      const asked = browseRequests(page);
      await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
      await page.locator(`[data-card="mod:${keyOrder.downloads[0].id}"]`).waitFor();
      assert.deepEqual(await cardIds(page), keyOrder.downloads.slice(0, 20).map(mod => `mod:${mod.id}`));
      assert.equal(await browseLabel(page), 'Page 1 of 3');
      assert.equal(await subtitle(page), 'Results come from Nexus, sorted most downloaded first. Nexus counts 45 mods for this game.');
      for (const [button, sort] of [['Most endorsed', 'endorsements'], ['Recently updated', 'updated'], ['Name A–Z', 'name']]) {
        await page.getByRole('button', { name: button }).click();
        await page.locator(`[data-card="mod:${keyOrder[sort][0].id}"]`).waitFor();
        assert.deepEqual(await cardIds(page), keyOrder[sort].slice(0, 20).map(mod => `mod:${mod.id}`), sort);
        assert.equal(await pressedSort(page), button);
      }
      assert.deepEqual(asked, ['?sort=downloads&page=1', '?sort=endorsements&page=1', '?sort=updated&page=1', '?sort=name&page=1']);
      await page.locator('[data-browse-pager="top"] [data-browse-page="next"]').click();
      await page.locator(`[data-card="mod:${keyOrder.name[20].id}"]`).waitFor();
      await page.locator('[data-browse-pager="top"] [data-browse-page="next"]').click();
      await page.locator(`[data-card="mod:${keyOrder.name[40].id}"]`).waitFor();
      assert.deepEqual(await cardIds(page), keyOrder.name.slice(40).map(mod => `mod:${mod.id}`), 'the last page holds the last five');
      assert.equal(await page.locator('[data-browse-pager="top"] [data-browse-page="next"]').isDisabled(), true);
      // The key was sent once, in the one request that carried it, and no reply, page text or storage of the page holds it.
      assert.ok(!bodies.some(body => body.includes(secret)), 'no reply echoes the key');
      assert.ok(!(await page.locator('body').innerHTML()).includes(secret), 'the page never shows the key');
      assert.equal(await page.evaluate(secret => JSON.stringify([localStorage, sessionStorage]).includes(secret) || location.href.includes(secret), secret), false);
      await page.reload();
      await page.getByRole('button', { name: 'Owned Game Fixture' }).waitFor();
      assert.equal(await page.locator('#key-sign-in').isHidden(), true, 'still signed in after a reload');
    }, { FIXTURE_ROOT: keyRoot });
    assert.equal(JSON.parse(fs.readFileSync(stored, 'utf8')).credentials.api_key, secret, 'the key outlives the host');
    await installFlow('key', async page => {
      // A new host process on the same private files: signed in without being told the key, and Disconnect removes it.
      const posts = keyPosts(page);
      await page.locator('#account').getByText('Nexus · Key fixture').waitFor();
      await page.getByRole('button', { name: 'Owned Game Fixture' }).waitFor();
      assert.equal(await page.locator('#key-sign-in').isHidden(), true);
      assert.equal(await page.locator('#status-banner').isHidden(), true);
      assert.deepEqual(posts, []);
      await page.getByRole('button', { name: 'Disconnect' }).click();
      await page.locator('#account').getByText('Nexus account not connected').waitFor();
      await page.locator('#key-sign-in').waitFor();
      assert.equal(await page.locator('#key-sign-in').isVisible(), true, 'signed out again: the form is back');
      assert.equal(fs.existsSync(stored), false, 'Disconnect removes the saved key');
      assert.equal(await page.evaluate(async () => (await (await fetch('/api/account')).json()).status), 'disconnected');
    }, { FIXTURE_ROOT: keyRoot });
  } finally {
    fs.rmSync(keyRoot, { recursive: true, force: true });
  }
  await installFlow('key', async page => {
    // Five refused keys lock further attempts for ten minutes, even for the right key; the page says so with the catalog text.
    await page.locator('#key-sign-in').waitFor();
    for (let attempt = 1; attempt <= 5; attempt++) {
      await page.locator('#key-input').fill(`wrong-key-${attempt}`); await page.locator('#key-input').press('Enter');
      await page.locator('#key-sign-in-message', { hasText: 'Nexus did not accept that API key.' }).waitFor();
      await page.locator('#key-submit:not([aria-disabled="true"])').waitFor();
      await page.locator('#key-input').fill('');
      await page.locator('#key-sign-in-message').evaluate(el => { el.textContent = ''; });
    }
    await page.locator('#key-input').fill(secret); await page.locator('#key-input').press('Enter');
    await page.locator('#key-sign-in-message', { hasText: 'Too many failed sign-in attempts.' }).waitFor();
    assert.equal(await keyMessage(page), 'Too many failed sign-in attempts. Wait ten minutes, then try again.');
    assert.equal(await page.locator('#key-sign-in').isVisible(), true);
    assert.match(await page.locator('#account').innerText(), /^Nexus account not connected\s+Sign in$/);
    const raw = await page.evaluate(async key => {
      const { csrf_token: csrf } = await (await fetch('/api/capabilities')).json();
      const response = await fetch('/api/auth/key', { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf }, body: JSON.stringify({ api_key: key }) });
      return { status: response.status, body: await response.json() };
    }, secret);
    assert.equal(raw.status, 429);
    assert.equal(raw.body.error.code, 'too_many_attempts');
    assert.ok(!JSON.stringify(raw).includes(secret));
  });
  // ---- Download progress: the host passes progress= to a download function that accepts it; the Activity card shows the bar ----
  const releaseDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'modbridge-release-'));
  const release = path.join(releaseDirectory, 'go');
  try {
    await installFlow('slow', async page => {
      // The fixture reports the archive's bytes in four steps and holds after the second until this test lets it go on.
      await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
      await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
      await page.getByRole('button', { name: 'Download for analysis' }).click();
      await page.getByRole('tab', { name: 'Activity' }).click();
      const jobs = () => page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
      let held = null;
      for (let n = 0; n < 200 && !held; n++) {
        const [job] = await jobs();
        if (job?.status === 'downloading' && job.progress && job.progress.total >= 1 && job.progress.done === Math.floor(job.progress.total / 2)) held = job;
        else await page.waitForTimeout(50);
      }
      assert.ok(held, 'the host reports half of the archive while the download is held');
      assert.deepEqual(Object.keys(held.progress).sort(), ['done', 'total', 'unit']);
      assert.equal(held.progress.unit, 'bytes');
      const { done, total } = held.progress;
      assert.ok(total > 0 && total < 1024 && done > 0 && done < total, `a small archive's byte counts: ${done} of ${total}`);
      // The page's own poll (every 2.5 s) draws it: one progressbar on this job's card with the values and the same count in words.
      const card = page.locator(`[data-job-id="${held.id}"]`);
      const bar = card.getByRole('progressbar');
      let drawn = null;
      for (let n = 0; n < 120 && drawn === null; n++) {
        if (await bar.count() === 1 && await bar.getAttribute('aria-valuenow') === String(done)) drawn = true;
        else await page.waitForTimeout(100);
      }
      assert.ok(drawn, 'the Activity card shows the progressbar while downloading');
      assert.equal(await page.getByRole('progressbar').count(), 1);
      assert.deepEqual(await bar.evaluate(el => [el.getAttribute('aria-valuemin'), el.getAttribute('aria-valuenow'), el.getAttribute('aria-valuemax'), el.getAttribute('aria-label')]), ['0', String(done), String(total), 'Bytes processed']);
      const unit = value => `${value} ${value === 1 ? 'byte' : 'bytes'}`;
      assert.equal(await card.locator('.progress-words').innerText(), `${unit(done)} of ${unit(total)}`);
      assert.equal(await bar.getAttribute('aria-valuetext'), `${unit(done)} of ${unit(total)}`);
      assert.doesNotMatch(await card.innerText(), /%|The host reports no download progress/);
      // Let the download finish: the bar goes with the step, and the finished job carries no progress record.
      fs.writeFileSync(release, '');
      await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
      assert.equal(await page.getByRole('progressbar').count(), 0);
      assert.equal(await page.locator('.job-count').count(), 0);
      const [done_job] = await jobs();
      assert.equal(done_job.id, held.id);
      assert.equal(done_job.status, 'analyzed');
      assert.ok(!('progress' in done_job));
      assert.equal(done_job.bytes_downloaded, total, 'the bar was measured against the bytes that arrived');
    }, { FIXTURE_RELEASE: release });
  } finally {
    fs.rmSync(releaseDirectory, { recursive: true, force: true });
  }
  console.log('actual-host UI integration passed (owned fixture + C++ analysis; no provider/device execution)');
}

// One owned host in the given FIXTURE_MODE, one browser page, then everything is torn down.
async function installFlow(mode, run, env = {}) {
  const child = spawn(process.env.PYTHON || 'python3', [path.join(__dirname, 'serve_ui_fixture.py')], { stdio: ['ignore', 'pipe', 'pipe'], env: { ...process.env, ...env, FIXTURE_MODE: mode } });
  const stopped = new Promise(resolve => child.once('exit', resolve));
  let browser, diagnostic = '';
  child.stderr.on('data', bytes => { diagnostic += bytes.toString(); });
  try {
    const fixture = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Owned host did not start')), 10000);
      let input = '';
      child.stdout.on('data', bytes => { input += bytes.toString(); if (input.includes('\n')) { clearTimeout(timer); try { resolve(JSON.parse(input.split('\n')[0])); } catch (error) { reject(error); } } });
      child.once('error', error => { clearTimeout(timer); reject(error); });
      child.once('exit', code => { clearTimeout(timer); reject(new Error(`Owned host exited ${code}: ${diagnostic}`)); });
    });
    const launch = { headless: true };
    if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
    else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch(launch);
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    page.setDefaultTimeout(6000);
    const failures = [];
    page.on('pageerror', error => failures.push(error.message));
    await page.route('**/*', route => new URL(route.request().url()).origin !== fixture.origin ? route.abort() : route.continue());
    await page.goto(fixture.origin);
    await run(page, fixture);
    assert.deepEqual(failures, [], `${mode} flow: page errors`);
  } finally {
    if (browser) await browser.close();
    child.kill('SIGTERM');
    await stopped;
  }
}

async function mainFlow() {
  const child = spawn(process.env.PYTHON || 'python3', [path.join(__dirname, 'serve_ui_fixture.py')], { stdio: ['ignore', 'pipe', 'pipe'], env: { ...process.env, FIXTURE_ACCOUNT_DELAY: '3' } });
  let browser;
  const stopped = new Promise(resolve => child.once('exit', resolve));
  let diagnostic = '';
  child.stderr.on('data', bytes => { diagnostic += bytes.toString(); });
  try {
    const fixture = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Owned host did not start')), 10000);
      let input = '';
      child.stdout.on('data', bytes => {
        input += bytes.toString();
        if (input.includes('\n')) {
          clearTimeout(timer);
          try { resolve(JSON.parse(input.split('\n')[0])); } catch (error) { reject(error); }
        }
      });
      child.once('error', error => { clearTimeout(timer); reject(error); });
      child.once('exit', code => { clearTimeout(timer); reject(new Error(`Owned host exited ${code}: ${diagnostic}`)); });
    });
    assert.equal(fixture.evidence, 'owned_desktop_fixture');
    const launch = { headless: true };
    if (process.env.CHROMIUM_EXECUTABLE_PATH) launch.executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
    else if (fs.existsSync('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')) launch.executablePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch(launch);
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    page.setDefaultTimeout(6000);
    const failures = [];
    page.on('pageerror', error => failures.push(error.message));
    await page.route('**/*', route => {
      if (new URL(route.request().url()).origin !== fixture.origin) return route.abort();
      return route.continue();
    });
    await page.goto(fixture.origin);
    await page.getByRole('button', { name: 'Owned Game Fixture' }).click();
    await page.getByRole('button', { name: /Owned Archive Fixture/ }).click();
    await page.getByRole('button', { name: 'Download for analysis' }).click();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor({ timeout: 15000 });
    // The real host attaches compatibility_assessment; the UI must accept it and render exactly one matching badge.
    // An older host that omits the field keeps the original unverified text instead.
    const assessment = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items[0].compatibility_assessment);
    const labels = { compatible: 'Compatible', partially_compatible: 'Partially compatible', incompatible: 'Incompatible', unknown: 'Unknown' };
    if (assessment === undefined) {
      assert.equal(await page.locator('.compat-badge').count(), 0);
      assert.match(await page.locator('body').innerText(), /Compatibility unverified/);
    } else {
      assert.ok(Object.hasOwn(labels, assessment.verdict));
      assert.equal(await page.locator('.compat-badge').count(), 1);
      assert.equal((await page.locator('.compat-badge').innerText()).trim().endsWith(labels[assessment.verdict]), true);
      assert.equal(await page.locator('.compat-reasons li').count(), assessment.reasons.length);
      // The real host's lowercase detail follows each catalog sentence as its own clause; the static-only qualifier shows unless the verdict is Unknown.
      assert.equal(await page.locator('.compat-reason-detail').count(), assessment.reasons.filter(reason => reason.detail.trim()).length);
      assert.equal(await page.locator('.compat-basis').count(), assessment.verdict === 'unknown' ? 0 : 1);
      if (assessment.verdict !== 'unknown') assert.equal(await page.locator('.compat-basis').innerText(), 'Static check. Not tested on PS5.');
      for (const reason of assessment.reasons) assert.match(await page.locator('.compat-reasons').innerText(), new RegExp(reason.detail.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
      assert.doesNotMatch(await page.locator('body').innerText(), /Compatibility unverified/);
    }
    // The fixture's account is premium: the page says so and shows no free-account hint.
    assert.equal(await page.locator('#account .account-tier').innerText(), 'Premium');
    assert.equal(await page.locator('#account .account-hint').count(), 0);
    assert.match(await page.locator('body').innerText(), /Target build is unknown/);
    assert.match(await page.locator('body').innerText(), /Read-only archive inventory complete/);
    assert.match(await page.locator('body').innerText(), /Verified archive hash: [a-f0-9]{64}/);
    const records = await page.evaluate(async () => (await (await fetch('/api/jobs')).json()).items);
    assert.equal(records.length, 1);
    assert.equal(records[0].status, 'analyzed');
    assert.equal(records[0].compatibility, 'unresolved');
    assert.equal(records[0].analysis_report.compatibility_state, 'unverified');
    assert.equal(records[0].analysis_report.install_authority.state, 'unsupported');
    assert.equal(records[0].stage, 'analyzed');
    assert.ok(records[0].bytes_downloaded > 0);
    assert.ok(!JSON.stringify(records).includes('secret=private'));
    assert.ok(!('inventory_path' in records[0]));
    await page.reload();
    await page.getByRole('tab', { name: 'Activity' }).click();
    await page.getByText('Analysis complete', { exact: true }).waitFor();
    fs.mkdirSync(path.join(__dirname, '..', 'build'), { recursive: true });
    await page.screenshot({ path: path.join(__dirname, '..', 'build', 'ui-integration.png'), fullPage: true });
    assert.deepEqual(failures, []);
  } finally {
    if (browser) await browser.close();
    child.kill('SIGTERM');
    await stopped;
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
