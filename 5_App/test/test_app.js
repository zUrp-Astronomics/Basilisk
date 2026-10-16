/* SOURCE: test de la page de banc dans un navigateur — la page en FR et en EN (ticket #635) : les mêmes vérifications dans
 *         les deux langues, la bascule en cours de session, la langue retenue, aucun texte d'interface hors du
 *         dictionnaire ; une pageerror fait échouer le test
 * AUTHOR: engineer
 * DATE: 2026-10-16
 * STATUS: actif — joué par ../run.sh ; le faux port rend `v` comme une version de carte (0.9), `t` (ext=1 en tête),
 *         `DEBUG` ; la page lit i, r par leur lettre ; la marque sur `j` (lue, posée, rejointe par les boutons),
 *         `m<n>` un déplacement, `u…`, `cm`, `m`, `ms`, `mg`, `mx` inconnues du faux port ; la page n'en envoie aucune,
 *         ni h, c, n, w, l, d, DUMP, SNIFF, SDRIVE, que la carte ne sert pas (ticket #392) ; le démarrage de `b` occupé
 *         plus longtemps qu'une période de `t`, et l'état attendu plutôt qu'un délai fixe (la lecture de `t` à 1 Hz
 *         manquait la fenêtre de 0,8 s selon sa phase) ; `p0`/`p1` tiennent power_off de `t` (ticket #635)
 *
 * Test de l'app dans un navigateur sans carte : un faux port Web Serial qui parle PROTOCOL.md
 * (lettres, Moonlite, labo). Verifie la connexion, le poll, les panneaux, un deplacement, une
 * ouverture, un démarrage asynchrone (b), la disparition de l'objectif — en FR puis en EN, la langue fixée par celle du
 * navigateur (locale du contexte). Puis la langue : le dictionnaire (deux langues par clé, clés employées), la langue
 * par défaut, la page sans stockage, la bascule connexion ouverte, la langue retenue au rechargement, et aucun texte
 * d'interface hors du dictionnaire (pseudo-langue : chaque texte du dictionnaire marqué ⟦…⟧, la page parcourue, tout
 * texte visible, title ou placeholder qui garde une lettre hors des marques est en dur). Les attendus de chaque langue
 * sont écrits à la main ci-dessous, pas lus dans la page.
 * Lancer : node test_app.js (Playwright + Chromium requis). */
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const PAGE = path.resolve(__dirname, '..', 'emount-bench.html');

/* Les textes attendus, par langue. near : la borne proche, nombre au format de la langue (espace fine en FR). */
const T = {
  fr: { connect: 'Connecter', connected: 'connecté', caps: 'lot étendu (ext=1)', near: '13 723 (proche)', moveEnd: '[app] fin du déplacement',
        stall: '[app] fin du déplacement : stall', stopped: 'arrêté', busyBoot: 'occupé : boot', ready: '· prêt', absent: '· absent (off)',
        fault: '· FAULT : lost' },
  en: { connect: 'Connect', connected: 'connected', caps: 'extended set (ext=1)', near: '13,723 (near)', moveEnd: '[app] end of move',
        stall: '[app] end of move: stall', stopped: 'stopped', busyBoot: 'busy: boot', ready: '· ready', absent: '· no lens (off)',
        fault: '· FAULT: lost' },
};
const norm = s => String(s).replace(/[  ]/g, ' ');   /* l'espace insécable du groupement FR, U+202F ou U+00A0 selon ICU */

/* ---- simulateur de carte, injecté avant la page ---- */
function sim() {
  /* boot : `b` rejoue un démarrage, busy=boot pendant 10 réponses à `e` (5 Hz, 2 s au moins) : plus long que la période
     de `t` (1 s), que la page lit pour l'afficher ; à 4 (0,8 s), la lecture de `t` tombait ou non dans la fenêtre selon
     sa phase */
  const S = { present: true, pos: 16384, min: 13723, max: 30988, moving: 0, fnum: 1.8, busy: '-', target: null, mark: null, limitsAfter: 13723, g: '-', stallNext: false, fault: false, powerOff: false };
  window.__sim = S;
  function reply(cmd) {
    const c = cmd[0];
    if (cmd.startsWith(':')) { const m = cmd.slice(1, -1); if (m === 'GP') return S.pos.toString(16).toUpperCase().padStart(4, '0') + '#'; if (m === 'GI') return S.moving ? '01#' : '00#'; if (m === 'GV') return '10'; return null; }
    if (/^[A-Z]/.test(cmd)) { if (cmd === 'DEBUG') return 'ok reset=poweron dropped=0 move_rc=-'; if (cmd === 'LOG ON') return 'ok log=1 all=0'; return 'er nocap'; }
    if (c === 'v') return '0.9';
    if (c === 't') return `ext=1 present=${S.present ? 1 : 0} boot_state=${!S.present ? 'off' : S.fault ? 'fault' : S.busy === 'boot' ? 'powering' : 'ready'} busy=${S.busy} power_off=${S.powerOff ? 1 : 0} last_op=${S.fault ? 'boot:er:"lost"' : 'boot:ok'} mf=- oss=- ring=-`;
    if (c === 'g') return S.g;                     /* le résultat du dernier déplacement suivi (PROTOCOL.md § 2) */
    if (c === 'p') { if (cmd.length === 1) return '1 1'; S.powerOff = cmd === 'p0'; return 'ok'; }   /* seul power_off de `t` suit */
    if (c === 'b') { S.busy = 'boot'; S.busyN = 10; S.limitsAfter = 13700; return 'ok'; }
    if (!S.present) return 'fread cs'.includes(c) ? 'nc' : 'er nolens';
    if (S.fault) return c === 'e' ? 'n' : 'er fault';   /* PROTOCOL.md § 7 : en fault, e = n */
    if (c === 'm' && !/^m\d/.test(cmd)) return 'er nocap'; /* m<n> seul, un déplacement comme f<n> */
    if (c === 'f' || c === 'm') {
      if (cmd.length === 1) return String(S.pos);
      if (S.busy !== '-') return 'er busy ' + S.busy;
      let t = cmd[1] === '+' || cmd[1] === '-' ? S.pos + parseInt(cmd.slice(1), 10) : parseInt(cmd.slice(1), 10);
      if (isNaN(t)) return 'er range 65535';
      S.target = Math.max(S.min, Math.min(S.max, t)); S.moving = 3; return 'ok';
    }
    if (c === 'e') { if (S.moving) { S.moving--; if (!S.moving && S.target !== null) { if (S.stallNext) S.g = 'stall'; else { S.pos = S.target; S.g = 'ok'; } S.stallNext = false; S.target = null; } return 'y'; } if (S.busy !== '-') { if (--S.busyN <= 0) { S.busy = '-'; S.min = S.limitsAfter; } return 'y'; } return 'n'; }
    if (c === 'r') return `${S.min}-${S.max}`;
    if (c === 'a') { if (cmd.length === 1) return '1.8-22'; const v = parseFloat(cmd.slice(1)); S.fnum = cmd[1] === '+' || cmd[1] === '-' ? S.fnum + v : v; return 'ok'; }
    if (c === 'o') return S.fnum.toFixed(1);
    if (c === 'i') return 'Samyang AF 135mm f/1.8-22 fw1.05';
    if (c === 'j') { /* la marque */
      if (cmd === 'j') return S.mark === null ? '-' : String(S.mark);
      if (cmd === 'js') { S.mark = S.pos; return 'ok'; }
      if (cmd === 'jx') { S.mark = null; return 'ok'; }
      if (cmd === 'jg') { if (S.mark === null) return 'er range nomark'; S.target = S.mark; S.moving = 3; return 'ok'; }
      return 'er nocap';
    }
    if (c === 'u') return 'er nocap';
    if (c === 'q') { if (S.moving) { S.moving = 0; S.target = null; S.g = 'aborted'; } return 'ok'; }
    return 'er nocap cmd';
  }
  /* une réponse due après « Déconnecter » (la page a annulé la lecture, le flux est fermé) est perdue, comme sur un port
     fermé : sans le try, l'enqueue lève dans la page */
  const enc = new TextEncoder(), dec = new TextDecoder();
  let ctrl = null; let inbuf = '';
  const readable = new ReadableStream({ start(c) { ctrl = c; } });
  const writable = new WritableStream({ write(chunk) { inbuf += dec.decode(chunk); let i; while ((i = inbuf.search(/[\n#]/)) >= 0) { const cmd = inbuf.slice(0, i + (inbuf[i] === '#' ? 1 : 0)); inbuf = inbuf.slice(i + 1); if (!cmd || cmd === '#') continue; const r = reply(cmd); window.__sent = (window.__sent || []).concat(cmd); if (r !== null && ctrl) setTimeout(() => { try { ctrl.enqueue(enc.encode(r + (r.endsWith('#') || r === '10' ? '' : '\n'))); } catch (e) {} }, 2); } } });
  const port = { open: async () => {}, close: async () => {}, readable, writable };
  Object.defineProperty(navigator, 'serial', { value: { requestPort: async () => port }, configurable: true });
}

let fails = 0;
const check = (c, m) => { console.log((c ? '  ok   ' : '  FAIL ') + m); if (!c) fails++; };
/* Toute exception JS non rattrapée dans une page est un échec, même si les vérifications fonctionnelles passent : on
   l'affiche tout de suite et on la compte à la fin (le process.exit final décide seul du code de sortie, un
   process.exitCode posé ici serait écrasé). */
const pageErrors = [];
/* Une page dans un contexte neuf (stockage vierge) : la langue du navigateur est la locale du contexte ; `init`, des
   scripts injectés de plus. */
async function open(browser, locale, ...init) {
  const ctx = await browser.newContext({ locale });
  await ctx.addInitScript(sim);
  for (const f of init) await ctx.addInitScript(f);
  const page = await ctx.newPage();
  page.on('pageerror', e => { console.log('PAGE ERROR', e.message); pageErrors.push(e); });
  await page.goto('file://' + PAGE);
  return { ctx, page };
}

/* ---- la page, connexion et panneaux : les mêmes vérifications dans les deux langues ---- */
async function scenario(browser, lang) {
  console.log('--- page en ' + lang.toUpperCase());
  const X = T[lang];
  const { ctx, page } = await open(browser, lang === 'fr' ? 'fr-FR' : 'en-US');
  check(await page.evaluate(() => document.documentElement.lang) === lang && await page.title() === 'emount-bench',
        '<html lang="' + lang + '">, le titre reste emount-bench');
  await page.click('#btnConnect');
  await page.waitForTimeout(1500);
  check(await page.textContent('#connTxt') === X.connected, 'connexion');
  if (fails) { console.log(await page.textContent('#log')); }
  check((await page.textContent('#s_id')).includes('Samyang AF 135mm'), 'identite affichee (i)');
  check((await page.textContent('#s_board')).startsWith('fw 0.9 · reset poweron · boot ready'), 'la carte, v et DEBUG (« ' + (await page.textContent('#s_board')) + ' »)');
  check(await page.textContent('#s_caps') === X.caps, 'ext lu au lieu de caps');
  if (fails) { console.log(await page.textContent('#log')); console.log(await page.evaluate(() => JSON.stringify(window.__sent))); }
  check(await page.textContent('#s_focus') === '16384', 'position lue par f');
  check(norm(await page.textContent('#scMin')) === X.near, 'bornes lues par r, nombre au format de la langue (« ' + (await page.textContent('#scMin')) + ' »)');
  check(!(await page.isDisabled('#focusRange')), 'curseur focus actif (goto + bornes)');
  check(await page.textContent('#s_fnum') === 'f/1.8', 'ouverture courante par o');
  /* L09-16 : ce que la carte ne sert pas n'est plus sur la page */
  for (const id of ['btnHome', 'btnCalib', 'btnDump', 'dumpType', 'btnSniffOn', 'btnSniffOff', 'btnSdriveScan', 'btnSdriveStop', 'sdriveState', 's_mods', 's_fw', 's_focal', 's_dist'])
    check(await page.$('#' + id) === null, 'retiré de la page : #' + id);
  check(await page.$('button[data-cmd]') === null, 'retirés de la page : DUMP et le profil SDRIVE (data-cmd)');
  await page.click('#btnFp100');
  await page.waitForTimeout(1200);
  check(await page.textContent('#s_focus') === '16484', 'f+100 puis poll : position suivie');
  await page.fill('#focusTarget', '20000'); await page.click('#btnFgoto');
  await page.waitForTimeout(1200);
  check(await page.textContent('#s_focus') === '20000', 'aller a 20000');
  await page.click('#btnMarkSet'); await page.waitForTimeout(1200);
  check(await page.evaluate(() => window.__sim.mark) === 20000 && await page.evaluate(() => document.getElementById('scMark').style.display) === '',
        'Marquer -> js, la marque 20000 relue par j et affichee');
  await page.click('#btnFp100'); await page.waitForTimeout(1200);
  await page.click('#btnMarkGo'); await page.waitForTimeout(1200);
  check(await page.textContent('#s_focus') === '20000', 'Aller a la marque -> jg, de retour en 20000');
  /* L09-15 : un déplacement de la page fini bloqué (g = stall) est signalé dans le panneau Focus ; lu sans l'attendre
     (null s'il n'existe pas) */
  const sMove = () => page.evaluate(() => { const e = document.getElementById('s_move'); return e ? e.textContent : null; });
  check(await sMove() === '', 'deplacements arrives : rien de signale');
  await page.evaluate(() => { window.__sim.stallNext = true; });
  await page.click('#btnFp100'); await page.waitForTimeout(1200);
  check(String(await sMove()).includes('stall') && (await page.textContent('#log')).includes(X.stall),
        'bloque (g = stall) : signale au panneau Focus et au journal (« ' + (await sMove()) + ' »)');
  /* Un Stop de l'humain (q) pendant un déplacement de la page : g = aborted, un arrêt demandé, pas un échec (PROTOCOL.md
     § 2, ligne q) — au journal une ligne d'événement, aucune d'erreur, et le panneau Focus dit « arrêté ». Le Stop part
     une fois le déplacement accepté par le faux port (sinon q le retire de la file). */
  const moveLines = () => page.$$eval('#log .l', (ls, m) => ls.filter(l => l.textContent.includes(m)).map(l => l.className + ' | ' + l.textContent), X.moveEnd);
  const before = (await moveLines()).length;
  await page.click('#btnFp100');
  await page.waitForFunction(() => window.__sim.moving > 0, null, { timeout: 2000 }).catch(() => {});
  await page.click('#btnStop'); await page.waitForTimeout(1200);
  const stopLines = (await moveLines()).slice(before);
  check(await page.evaluate(() => window.__sim.g) === 'aborted' && stopLines.length === 1 && /\bev\b/.test(stopLines[0]) && !/\berr\b/.test(stopLines[0]),
        'arrete (g = aborted) : une ligne d\'evenement au journal, pas d\'erreur (' + JSON.stringify(stopLines) + ')');
  check(String(await sMove()).includes(X.stopped) && !String(await sMove()).includes('aborted'),
        'arrete (g = aborted) : le panneau Focus dit « ' + X.stopped + ' » (« ' + (await sMove()) + ' »)');
  /* L'arrêt non confirmé (aborted passé à unconfirmed, relu pendant la fenêtre de l'arrêt) reste une erreur. */
  await page.evaluate(() => { window.__sim.g = 'unconfirmed'; }); await page.waitForTimeout(1500);
  const unconf = (await moveLines()).slice(before + 1);
  check(unconf.length === 1 && /\berr\b/.test(unconf[0]) && unconf[0].includes('unconfirmed') && String(await sMove()).includes('unconfirmed'),
        'aborted puis unconfirmed : signale en erreur (' + JSON.stringify(unconf) + ', « ' + (await sMove()) + ' »)');
  check(await page.evaluate(() => window.__sent.filter(c => c === 'DEBUG').length) === 1 && await page.evaluate(() => window.__sent.includes('g')),
        'la fin se lit par g ; DEBUG une fois, a la connexion seulement');
  await page.click('#btnIp'); await page.waitForTimeout(1500);
  check((await page.textContent('#s_fnum')).startsWith('f/2'), '+1/3 : f/2 rapporte par o');
  /* L09-17 : pointerdown sans change (relâché sans bouger, pointercancel) laissait le glissement ouvert, et le curseur ne
     suivait plus la position */
  await page.dispatchEvent('#focusRange', 'pointerdown'); await page.dispatchEvent('#focusRange', 'pointerup');
  await page.evaluate(() => { window.__sim.pos = 21000; }); await page.waitForTimeout(700);
  check(await page.$eval('#focusRange', e => e.value) === '21000', 'focus relache sans bouger : le curseur suit la position (' + await page.$eval('#focusRange', e => e.value) + ')');
  await page.dispatchEvent('#focusRange', 'pointerdown'); await page.dispatchEvent('#focusRange', 'pointercancel');
  await page.evaluate(() => { window.__sim.pos = 21500; }); await page.waitForTimeout(700);
  check(await page.$eval('#focusRange', e => e.value) === '21500', 'focus annule (pointercancel) : le curseur suit la position (' + await page.$eval('#focusRange', e => e.value) + ')');
  await page.dispatchEvent('#irisRange', 'pointerdown'); await page.dispatchEvent('#irisRange', 'pointerup');
  await page.evaluate(() => { window.__sim.fnum = 4; }); await page.waitForTimeout(1500);
  check(await page.textContent('#irisRangeVal') === 'f/4', 'ouverture relachee sans bouger : le curseur suit l ouverture rapportee (« ' + await page.textContent('#irisRangeVal') + ' »)');
  await page.evaluate(() => { window.__sim.pos = 20000; });       /* la position que :GP# relit plus bas (4E20#) */
  await page.click('#btnReboot');
  const vu = sel => page.waitForFunction(t => document.getElementById('s_state').textContent.includes(t), sel, { timeout: 4000 }).then(() => true, () => false);
  check(await vu(X.busyBoot), 'b : demarrage, etat occupe via t');
  await page.waitForFunction(r => document.getElementById('s_state').textContent.includes(r) && document.getElementById('scMin').textContent.replace(/\D/g, '') === '13700', X.ready, { timeout: 5000 }).catch(() => {});
  check((await page.textContent('#s_state')).includes(X.ready) && (await page.textContent('#scMin')).replace(/\D/g, '') === '13700', 'fin du demarrage : bornes relues');
  await page.evaluate(() => { window.__sim.present = false; });
  await page.waitForTimeout(1500);
  check((await page.textContent('#s_state')).includes(X.absent), 'objectif disparu : etat absent');
  check(await page.isDisabled('#btnFp100'), 'boutons focus desactives sans objectif');
  await page.evaluate(() => { window.__sim.present = true; });
  await page.waitForTimeout(1500);
  check((await page.textContent('#s_state')).includes(X.ready), 'objectif revenu');
  /* L09-14 : en fault, le statut le dit avec sa raison, et les panneaux qui ont besoin d'un objectif prêt sont inactifs */
  await page.evaluate(() => { window.__sim.fault = true; });
  await page.waitForTimeout(1500);
  check(await page.textContent('#s_state') === X.fault, 'fault : statut et raison (« ' + (await page.textContent('#s_state')) + ' »)');
  check(await page.isDisabled('#btnFp100') && await page.isDisabled('#focusRange') && await page.isDisabled('#btnMarkGo') && await page.isDisabled('#irisRange') && await page.isDisabled('#btnIp'),
        'fault : panneaux Focus et Ouverture inactifs');
  await page.evaluate(() => { window.__sim.fault = false; });
  await page.waitForTimeout(1500);
  check((await page.textContent('#s_state')).includes(X.ready) && !(await page.isDisabled('#btnFp100')), 'sortie de fault : pret, panneaux actifs');
  await page.fill('#cliIn', ':GP#'); await page.press('#cliIn', 'Enter'); await page.waitForTimeout(500);
  const logTxt = await page.textContent('#log');
  check(/4E20#/.test(logTxt), 'Moonlite :GP# depuis la ligne de commande -> 4E20#');
  const sent = (await page.evaluate(() => window.__sent)) || [];
  const star = sent.filter(c => !/^[a-z:A-Z]/.test(c));
  check(star.length === 0, 'rien d\'autre que des lettres, Moonlite et majuscules n\'est parti (' + sent.length + ' commandes)');
  const retired = sent.filter(c => /^(u.*|cm|m|ms|mg|mx)$/.test(c));
  check(retired.length === 0 && sent.includes('j') && sent.includes('js') && sent.includes('jg'),
        'ni u…, ni cm, ni m, ms, mg, mx ; la marque par j, js, jg (' + retired.join(' ') + ')');
  const dead = sent.filter(c => /^([hcnwld]|DUMP.*|SNIFF.*|SDRIVE.*)$/.test(c));
  check(dead.length === 0, 'ni h, c, n, w, l, d, DUMP, SNIFF, SDRIVE : non servis par la carte (' + dead.join(' ') + ')');
  await ctx.close();
}

/* ---- le dictionnaire : deux langues par clé, toute clé employée y est, toute clé y est employée ---- */
async function dictionary(browser) {
  console.log('--- le dictionnaire');
  const html = fs.readFileSync(PAGE, 'utf8');
  const { ctx, page } = await open(browser, 'fr-FR');
  const { langs, texts } = await page.evaluate(() => ({ langs: LANGS, texts: TEXTS }));
  check(langs.join() === 'fr,en', 'deux langues, fr et en (' + langs.join() + ')');
  const marks = s => (s.match(/\{\d\}/g) || []).sort().join();
  const bad = Object.entries(texts).filter(([k, v]) => !Array.isArray(v) || v.length !== 2 || v.some(x => typeof x !== 'string' || !x.trim()) || marks(v[0]) !== marks(v[1]));
  check(Object.keys(texts).length > 0 && bad.length === 0, 'chaque clé a son texte dans les deux langues, non vide, avec les mêmes {n} (' + bad.map(b => b[0]).join(' ') + ')');
  const used = new Set([...html.matchAll(/\btr\('([^']+)'/g), ...html.matchAll(/data-i18n(?:-title|-placeholder)?="([^"]+)"/g)].map(m => m[1]));
  const missing = [...used].filter(k => !(k in texts)), unused = Object.keys(texts).filter(k => !used.has(k));
  check(missing.length === 0, 'toute clé employée par la page est au dictionnaire (manquent : ' + missing.join(' ') + ')');
  check(unused.length === 0, 'toute clé du dictionnaire est employée (inemployées : ' + unused.join(' ') + ')');
  /* gen_lens_names.py réécrit tout ce qui est entre ses marqueurs : le dictionnaire n'y entre pas */
  const b = html.indexOf('/*LENS_NAMES_BEGIN*/'), e = html.indexOf('/*LENS_NAMES_END*/'), d = html.indexOf('const TEXTS=');
  check(b > 0 && e > b && d > 0 && (d < b || d > e), 'le dictionnaire est hors des marqueurs de gen_lens_names.py');
  await ctx.close();
}

/* ---- la langue par défaut : celle du navigateur, fr… -> FR, toute autre ou absente -> EN ; la page sans stockage ---- */
async function defaults(browser) {
  console.log('--- la langue par défaut');
  const noLanguage = () => Object.defineProperty(Navigator.prototype, 'language', { get: () => undefined, configurable: true });
  for (const [locale, want] of [['fr-FR', 'fr'], ['fr-CA', 'fr'], ['en-US', 'en'], ['de-DE', 'en'], [null, 'en']]) {
    const { ctx, page } = locale ? await open(browser, locale) : await open(browser, 'fr-FR', noLanguage);
    const got = await page.evaluate(() => [navigator.language, document.documentElement.lang]);
    check((locale ? got[0] === locale : got[0] === undefined) && got[1] === want && await page.textContent('#btnConnect') === T[want].connect,
          'navigator.language ' + (locale || 'absent') + ' -> ' + want + ' (' + JSON.stringify(got) + ')');
    await ctx.close();
  }
  /* Sans stockage (localStorage refusé) : la page se charge, bascule et se connecte, sans exception */
  const noStorage = () => { for (const o of [window, Window.prototype]) try { Object.defineProperty(o, 'localStorage', { get() { throw new Error('stockage refusé'); }, configurable: true }); } catch (e) {} };
  const { ctx, page } = await open(browser, 'fr-FR', noStorage);
  const refused = await page.evaluate(() => { try { localStorage; return false; } catch (e) { return true; } });
  await page.click('#btnLang');
  await page.click('#btnConnect'); await page.waitForTimeout(1500);
  check(refused && await page.evaluate(() => document.documentElement.lang) === 'en' && await page.textContent('#connTxt') === T.en.connected,
        'sans stockage : la page bascule en EN et se connecte (stockage refusé : ' + refused + ')');
  await ctx.close();
}

/* ---- la bascule en cours de session, puis la langue retenue au rechargement ---- */
async function toggle(browser) {
  console.log('--- la bascule, connexion ouverte');
  const { ctx, page } = await open(browser, 'fr-FR');
  await page.click('#btnConnect');
  await page.waitForFunction(r => document.getElementById('s_state').textContent === r, T.fr.ready, { timeout: 5000 }).catch(() => {});
  await page.waitForTimeout(500);
  const shown = () => page.$$eval('#log .l', ls => ls.map(l => l.innerHTML));
  const logBefore = await shown(), linesBefore = await page.evaluate(() => lines.slice());
  const count = c => page.evaluate(c => window.__sent.filter(x => x === c).length, c);
  const nf = await count('f'), nv = await count('v');
  check(linesBefore.some(l => l.endsWith('[app] carte : firmware 0.9')) && await page.textContent('#connTxt') === T.fr.connected, 'connectée en FR, le journal en FR');
  await page.click('#btnLang');
  const txt = s => page.textContent(s), attr = (s, a) => page.getAttribute(s, a);
  check(await page.evaluate(() => document.documentElement.lang) === 'en', 'la bascule : <html lang="en">');
  check(await txt('#connTxt') === 'connected' && await txt('#btnDisconnect') === 'Disconnect' && await txt('#btnReboot') === 'Restart lens'
        && await txt('#s_state') === '· ready' && await txt('#s_caps') === 'extended set (ext=1)' && norm(await txt('#scMin')) === '13,723 (near)'
        && (await txt('#s_board')).endsWith(' · last op. boot:ok') && (await attr('#btnStop', 'title')).startsWith('q: stop')
        && (await attr('#cliIn', 'placeholder')).startsWith('command (') && await page.title() === 'emount-bench',
        'les textes d\'interface passent en EN sur place, états affichés compris (« ' + [await txt('#s_state'), await txt('#scMin'), await txt('#s_board')].join(' | ') + ' »)');
  check(await page.evaluate(() => document.getElementById('connDot').classList.contains('on')) && !(await page.isDisabled('#btnDisconnect'))
        && await count('v') === nv && await count('DEBUG') === 1, 'la connexion reste ouverte : ni déconnexion, ni nouveau v, ni nouveau DEBUG');
  await page.waitForTimeout(1000);
  check(await count('f') > nf + 2, 'le poll continue (f : ' + nf + ' -> ' + (await count('f')) + ')');
  check((await txt('#focusRangeVal')).endsWith(' steps'), 'le curseur focus en pas : « steps » (« ' + (await txt('#focusRangeVal')) + ' »)');
  const logAfter = (await shown()).slice(0, logBefore.length), linesAfter = (await page.evaluate(() => lines.slice())).slice(0, linesBefore.length);
  check(JSON.stringify(logAfter) === JSON.stringify(logBefore) && JSON.stringify(linesAfter) === JSON.stringify(linesBefore),
        'les lignes déjà écrites au journal (et donc à l\'export) ne changent pas (' + logBefore.length + ' lignes)');
  await page.evaluate(() => { window.__sim.stallNext = true; });
  await page.click('#btnFp100'); await page.waitForTimeout(1200);
  const linesNow = (await page.evaluate(() => lines.slice())).slice(linesBefore.length);
  check(linesNow.some(l => l.endsWith(T.en.stall)) && !linesNow.some(l => l.includes('[app] fin du déplacement')) && await txt('#s_move') === 'last move: stall',
        'après la bascule, les nouvelles lignes [app] sont en EN (' + JSON.stringify(linesNow.filter(l => l.includes('[app]'))) + ')');
  console.log('--- la langue retenue');
  check(await page.evaluate(() => localStorage.getItem('emount-bench.lang')) === 'en', 'le choix est retenu (localStorage)');
  await page.reload();
  check(await page.evaluate(() => document.documentElement.lang) === 'en' && await txt('#btnConnect') === 'Connect',
        'rechargée (navigateur en fr-FR) : la page revient en EN');
  await page.click('#btnLang'); await page.reload();
  check(await page.evaluate(() => document.documentElement.lang) === 'fr' && await txt('#btnConnect') === 'Connecter',
        'rebasculée puis rechargée : la page revient en FR');
  await ctx.close();
}

/* ---- aucun texte d'interface hors du dictionnaire ----
   Chaque texte du dictionnaire est marqué ⟦…⟧ (valeurs insérées comprises, elles sont dans la marque), la page réécrite
   (applyLang), puis parcourue par ses états : avant connexion, connectée, en mouvement, déplacement bloqué, alims
   coupées, démarrage, fault, objectif absent, déconnectée. À chacun, tout texte de la page, visible ou caché, tout title
   et tout placeholder, privé de ses marques et des ouvertures (f/1.8), ne doit plus contenir de lettre. Exclus : le
   journal (un relevé, contrôlé à part), l'identité de l'objectif (s_id) et la ligne `t` (s_t), données de la carte, le
   nom de la page (h1), les commandes LOG ON/ALL/OFF (le protocole ; leur title est contrôlé) et les codes FR/EN de la
   bascule (son title est contrôlé). Un libellé ajouté en dur, ou dans une seule langue, garde ses lettres. */
const strip = s => { let r = String(s), p; do { p = r; r = r.replace(/⟦[^⟦⟧]*⟧/g, ''); } while (r !== p); return r.replace(/f\/[\d.—]*/g, ''); };
const hasLetter = s => /\p{L}/u.test(strip(s));
async function untranslated(browser) {
  console.log('--- aucun texte d\'interface hors du dictionnaire');
  const { ctx, page } = await open(browser, 'fr-FR');
  await page.evaluate(() => { for (const k in TEXTS) TEXTS[k] = TEXTS[k].map(v => '⟦' + v + '⟧'); applyLang(); });
  const n0 = await page.evaluate(() => lines.length);
  const leaks = async what => {
    const found = await page.evaluate(() => {
      const strip = s => { let r = String(s), p; do { p = r; r = r.replace(/⟦[^⟦⟧]*⟧/g, ''); } while (r !== p); return r.replace(/f\/[\d.—]*/g, ''); };
      const letter = s => /\p{L}/u.test(strip(s)), out = [];
      const w = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
      for (let n; (n = w.nextNode());) {
        const e = n.parentElement;
        if (e.closest('script, style, #log, #s_id, #s_t, h1, #btnLogOn, #btnLogAll, #btnLogOff, #btnLang')) continue;
        if (letter(n.nodeValue)) out.push((e.id || e.tagName) + ' : « ' + n.nodeValue + ' »');
      }
      for (const e of document.querySelectorAll('[title], [placeholder]'))
        for (const a of ['title', 'placeholder']) if (e.hasAttribute(a) && letter(e.getAttribute(a))) out.push((e.id || e.tagName) + ' @' + a + ' : « ' + e.getAttribute(a) + ' »');
      return out;
    });
    check(found.length === 0, what + ' : aucun texte hors du dictionnaire' + (found.length ? ' (' + found.join(' ; ') + ')' : ''));
  };
  const until = (f, arg) => page.waitForFunction(f, arg, { timeout: 5000 }).then(() => true, () => false);
  const stateHas = s => until(s => document.getElementById('s_state').textContent.includes(s), s);
  await leaks('avant connexion');
  await page.click('#btnConnect');
  check(await until(() => document.getElementById('s_state').textContent.includes('prêt') && document.getElementById('s_irange').textContent !== '—'), 'pseudo-langue : connectée, objectif prêt');
  await leaks('connectée, objectif prêt');
  await page.click('#btnFp100');
  check(await until(() => document.getElementById('s_moving').textContent !== ''), 'pseudo-langue : en mouvement');
  await leaks('en mouvement');
  await page.evaluate(() => { window.__sim.stallNext = true; }); await page.click('#btnFp100');
  check(await until(() => document.getElementById('s_move').textContent.includes('stall')), 'pseudo-langue : déplacement bloqué');
  await leaks('déplacement bloqué');
  await page.click('#btnPowerOff');
  check(await until(() => document.getElementById('s_power').textContent !== ''), 'pseudo-langue : alims coupées');
  await leaks('alims coupées');
  await page.click('#btnPowerOn'); await until(() => document.getElementById('s_power').textContent === '');
  await page.click('#btnReboot');
  check(await stateHas('occupé'), 'pseudo-langue : démarrage');
  await leaks('démarrage (occupé)');
  await stateHas('prêt');
  await page.evaluate(() => { window.__sim.fault = true; });
  check(await stateHas('FAULT'), 'pseudo-langue : fault');
  await leaks('fault');
  await page.evaluate(() => { window.__sim.fault = false; }); await stateHas('prêt');
  await page.evaluate(() => { window.__sim.present = false; });
  check(await stateHas('absent'), 'pseudo-langue : objectif absent');
  await leaks('objectif absent');
  await page.evaluate(() => { window.__sim.present = true; }); await stateHas('prêt');
  /* le journal : les lignes [app] écrites par la page depuis le marquage */
  const app = (await page.evaluate(n => lines.slice(n), n0)).map(l => l.slice(l.indexOf(' ') + 1)).filter(l => l.startsWith('[app] '));
  const appBad = app.filter(l => hasLetter(l.slice(6)));
  check(app.length >= 5 && appBad.length === 0, 'journal : les ' + app.length + ' lignes [app] viennent du dictionnaire' + (appBad.length ? ' (' + appBad.join(' ; ') + ')' : ''));
  /* le journal : ce que la page ajoute au décodage d'une trame (référence inconnue, checksum, table, non pavé, longueur,
     fragments) ; l'en-tête décodé (len= cls= seq=) et les noms de trames sont le protocole */
  const decs = await page.evaluate(() => ['* tx 70017 ~03 s12', '* rx 1 F0 0A 00 01 02 99 00 00 00 55', '* rx 2 F0 09 00 01 02 10 00 00 00 55',
    '* rx 3 +0/20#1 F0 14 00', '* rx 3 +0/20#1 F0 14 01'].map(l => { addLine(l.slice(2, 4), l); return [...lastEl.querySelectorAll('.dec')].map(d => d.textContent); }));
  const decBad = decs.flat().filter(d => hasLetter(d.replace(/len=\d+ cls=\d+ seq=\d+/, '').replace(/\b[0-9A-F]{2}:\S+/g, '')));
  check(decs.every(d => d.length > 0) && decBad.length === 0, 'journal : les annotations du décodage viennent du dictionnaire' + (decBad.length ? ' (' + decBad.join(' ; ') + ')' : ''));
  await page.click('#btnDisconnect');
  check(await until(() => !document.getElementById('connDot').classList.contains('on')), 'pseudo-langue : déconnectée');
  await leaks('déconnectée');
  await ctx.close();
}

(async () => {
  const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || fs.readdirSync('/opt/pw-browsers').filter(d => d.startsWith('chromium-')).map(d => '/opt/pw-browsers/' + d + '/chrome-linux/chrome')[0] });
  await scenario(browser, 'fr');
  await scenario(browser, 'en');
  await dictionary(browser);
  await defaults(browser);
  await toggle(browser);
  await untranslated(browser);
  check(pageErrors.length === 0, 'aucune erreur JavaScript dans la page (' + pageErrors.length + ' pageerror)');
  for (const e of pageErrors) console.log('       ' + (e.stack || e.message).split('\n').join('\n       '));
  console.log(fails + ' echec(s)');
  await browser.close();
  process.exit(fails ? 1 : 0);
})();
