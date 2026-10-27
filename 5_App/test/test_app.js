/* SOURCE: test de la page de banc dans un navigateur — la page en FR et en EN (ticket #635) : les mêmes vérifications dans
 *         les deux langues, la bascule en cours de session, la langue retenue, aucun texte d'interface hors du
 *         dictionnaire ; une pageerror fait échouer le test
 * AUTHOR: engineer
 * DATE: 2026-10-16
 * STATUS: actif — joué par ../run.sh ; le faux port rend `v` comme une version de carte (1.3), `t` (ext=1 en tête),
 *         `DEBUG` ; la page lit i, r par leur lettre ; la marque sur `j` (lue, posée, rejointe par les boutons),
 *         `m<n>` un déplacement, `u…`, `cm`, `m`, `ms`, `mg`, `mx` inconnues du faux port ; la page n'en envoie aucune,
 *         ni h, c, n, w, l, d, DUMP, SNIFF, SDRIVE, que la carte ne sert pas (ticket #392) ; le démarrage (insertion, `b`)
 *         et la reprise passent par les états de la carte, un par lecture de `t`, et l'état attendu plutôt qu'un délai
 *         fixe ; « prêt » à boot_state=ready seulement (ticket #703) ; `p0`/`p1` tiennent power_off de `t` (ticket #635) ; `z`, la
 *         température, l'humidité et le seuil de condensation de la carte, affichés objectif monté ou non (ticket #641)
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
        stall: '[app] fin du déplacement : stall', stopped: 'arrêté', busy: '· occupé : ', ready: '· prêt', absent: '· absent (off)',
        fault: '· FAULT : lost', noIdentity: '· prête, identité non lue', announce: '[app] prêt : Samyang AF 135mm f/1.8-22 fw1.05', near13750: '13 750 (proche)', near13760: '13 760 (proche)',
        /* `z` : la carte rend 21.37 48.02 9.91, puis -3.05 87.50 -4.27, puis -0.50 0.00 - (le seuil non défini) */
        dew: ['point de rosée', '21,37 °C', '48,02 %HR', '9,91 °C'], frost: ['point de givre', '-3,05 °C', '87,50 %HR', '-4,27 °C'],
        noCond: ['point de rosée ou de givre', '-0,50 °C', '0,00 %HR', '—'] },
  en: { connect: 'Connect', connected: 'connected', caps: 'extended set (ext=1)', near: '13,723 (near)', moveEnd: '[app] end of move',
        stall: '[app] end of move: stall', stopped: 'stopped', busy: '· busy: ', ready: '· ready', absent: '· no lens (off)',
        fault: '· FAULT: lost', noIdentity: '· ready, identity not read', announce: '[app] ready: Samyang AF 135mm f/1.8-22 fw1.05', near13750: '13,750 (near)', near13760: '13,760 (near)',
        dew: ['dew point', '21.37 °C', '48.02 %RH', '9.91 °C'], frost: ['frost point', '-3.05 °C', '87.50 %RH', '-4.27 °C'],
        noCond: ['dew or frost point', '-0.50 °C', '0.00 %RH', '—'] },
};
/* l'espace insécable du groupement FR, U+202F ou U+00A0 selon ICU ; le signe moins, U+2212 si ICU le donne */
const norm = s => String(s).replace(/[  ]/g, ' ').replace(/−/g, '-');

/* ---- simulateur de carte, injecté avant la page ---- */
function sim() {
  /* Le démarrage, comme la carte (PROTOCOL.md § 5, § 7 ; host.c, session.c) : à l'insertion et à `b`, `powering`,
     `identifying`, `homing`, `restoring`, puis `ready` ; une reprise (S.recover()), `recovering`, `powering`,
     `identifying`, `restoring`, puis `ready` ; `busy=boot` dans chacun. La durée de chaque état n'est pas celle de la
     carte : il dure jusqu'au premier `t` servi 400 ms au moins après le `t` qui l'a montré, une lecture de `t` de la page
     (1 Hz) par état, pour qu'un test voie chacun ; S.hold le tient tant qu'il est vrai. Hors `ready`, `i r a j f o c d`
     (et `m<n>`, `js` `jg` `jx`) rendent `er busy boot`, `e` rend `y` ; `q` rend `er busy home` en `homing`, arrête le
     retour en `restoring` (`ready`), `ok` ailleurs. La fin d'un démarrage pose les bornes `limitsAfter`. S.leaveAtI : la
     carte sort de `ready` entre `t` et `i` (une reprise commence, S.recover()), une fois ; `i` rend alors `er busy boot`. */
  const S = { present: true, pos: 16384, min: 13723, max: 30988, moving: 0, fnum: 1.8, target: null, mark: null, limitsAfter: 13723, g: '-', stallNext: false, fault: false, powerOff: false, z: '21.37 48.02 9.91',
              seq: [], hold: false, shownAt: 0, wasPresent: true, leaveAtI: false };
  const BOOT = ['powering', 'identifying', 'homing', 'restoring'], RECOVER = ['recovering', 'powering', 'identifying', 'restoring'];
  S.state = () => !S.present ? 'off' : S.fault ? 'fault' : S.seq.length ? S.seq[0] : 'ready';
  const start = states => { S.seq = states.slice(); S.shownAt = 0; S.moving = 0; S.target = null; };
  const toReady = () => { S.seq = []; S.min = S.limitsAfter; };
  S.recover = () => start(RECOVER);
  window.__sim = S;
  function reply(cmd) {
    const c = cmd[0];
    if (S.present && !S.wasPresent) start(BOOT);  /* l'insertion */
    if (!S.present) S.seq = [];
    S.wasPresent = S.present;
    if (cmd.startsWith(':')) { const m = cmd.slice(1, -1); if (m === 'GP') return S.pos.toString(16).toUpperCase().padStart(4, '0') + '#'; if (m === 'GI') return S.moving ? '01#' : '00#'; if (m === 'GV') return '10'; return null; }
    if (/^[A-Z]/.test(cmd)) { if (cmd === 'DEBUG') return 'ok reset=poweron dropped=0 move_rc=-'; if (cmd === 'LOG ON') return 'ok log=1 all=0'; return 'er nocap'; }
    if (c === 'v') return '1.3';
    if (c === 't') {
      if (S.seq.length && !S.hold && S.shownAt && Date.now() - S.shownAt >= 400) { S.seq.shift(); S.shownAt = 0; if (!S.seq.length) toReady(); }
      if (S.seq.length && !S.shownAt) S.shownAt = Date.now();
      return `ext=1 present=${S.present ? 1 : 0} boot_state=${S.state()} busy=${S.seq.length && !S.fault ? 'boot' : '-'} power_off=${S.powerOff ? 1 : 0} last_op=${S.fault ? 'boot:er:"lost"' : 'boot:ok'} mf=- oss=- ring=-`;
    }
    if (c === 'g') return S.g;                     /* le résultat du dernier déplacement suivi (PROTOCOL.md § 2) */
    if (cmd === 'z') return S.z;                   /* le capteur de la carte, dans tout état (PROTOCOL.md § 2) */
    if (c === 'p') { if (cmd.length === 1) return '1 1'; S.powerOff = cmd === 'p0'; return 'ok'; }   /* seul power_off de `t` suit */
    if (c === 'q') {                               /* dans tout état (PROTOCOL.md § 2, ligne q ; host.c) */
      if (S.state() === 'homing') return 'er busy home';
      if (S.state() === 'restoring') { toReady(); return 'ok'; }
      if (S.state() === 'ready' && S.moving) { S.moving = 0; S.target = null; S.g = 'aborted'; }
      return 'ok';
    }
    if (c === 'b') { if (!S.present) return 'er nolens'; S.fault = false; S.limitsAfter = 13700; start(BOOT); return 'ok'; }
    if (!S.present) return 'fread cs'.includes(c) ? 'nc' : 'er nolens';
    if (S.fault) return c === 'e' ? 'n' : 'er fault';   /* PROTOCOL.md § 7 : en fault, e = n */
    if (c === 'm' && !/^m\d/.test(cmd)) return 'er nocap'; /* m<n> seul, un déplacement comme f<n> */
    if (cmd === 'i' && S.leaveAtI && !S.seq.length) { S.leaveAtI = false; S.recover(); }
    if (S.seq.length) {                            /* un démarrage ou une reprise */
      if (c === 'e') return 'y';
      if (/^([iraofcd]|[fma].+|j[sgx]?)$/.test(cmd)) return 'er busy boot';
    }
    if (cmd === 'c') return 'ok';                  /* en READY, rien envoyé à l'objectif */
    if (cmd === 'd') return '-';                   /* non servie : toujours - en READY */
    if (c === 'f' || c === 'm') {
      if (cmd.length === 1) return String(S.pos);
      let t = cmd[1] === '+' || cmd[1] === '-' ? S.pos + parseInt(cmd.slice(1), 10) : parseInt(cmd.slice(1), 10);
      if (isNaN(t)) return 'er range 65535';
      S.target = Math.max(S.min, Math.min(S.max, t)); S.moving = 3; return 'ok';
    }
    if (c === 'e') { if (S.moving) { S.moving--; if (!S.moving && S.target !== null) { if (S.stallNext) S.g = 'stall'; else { S.pos = S.target; S.g = 'ok'; } S.stallNext = false; S.target = null; } return 'y'; } return 'n'; }
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
/* L'effet d'une action (un clic, une commande, un état du faux port changé) se lit quand la page le montre, pas après un
   délai fixe : la page lit f/e à 5 Hz, le reste à 1 Hz, et un déplacement du faux port prend trois `e` ; sur un runner
   chargé, un délai fixe ne les contient plus (run 2897, ticket #784). `pred` (async, lit la page) est relu toutes les
   50 ms jusqu'à vrai, au plus `ms` : une propriété fausse rougit à l'échéance, la suite ne pend pas. Ne lève pas.
   checkSoon : la vérification elle-même, attendue ; son libellé (une fonction si elle cite des valeurs lues) est
   calculé après. */
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function eventually(pred, ms = 10000) {
  const end = Date.now() + ms;
  for (;;) {
    let v = false;
    try { v = await pred(); } catch (e) {}
    if (v || Date.now() >= end) return !!v;
    await sleep(50);
  }
}
const checkSoon = async (pred, what) => { const ok = await eventually(pred); check(ok, typeof what === 'function' ? await what() : what); };
/* « Une fois » a une moitié négative, pas de deuxième ligne ni de deuxième annonce : la première attendue, une fenêtre
   d'observation où un doublon paraîtrait. 1200 ms couvrent un tour de la lecture lente (1 Hz : `t` et `g` relu pendant
   la fenêtre d'un arrêt), d'où viendrait un doublon. checkOnce : la propriété attendue, puis relue entière après la
   fenêtre. */
const settle = page => page.waitForTimeout(1200);   /* vérification négative (pas de doublon) : la fenêtre d'observation */
const checkOnce = async (page, pred, what) => { await eventually(pred); await settle(page); check(await pred(), await what()); };
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

/* Le capteur de la carte tel que la page l'affiche : [libellé du seuil, température, humidité, seuil], normalisé (norm).
   La lecture de `z` est à 1 Hz : on attend que la page montre `want`. */
const climate = page => page.evaluate(() => ['k_cond', 's_temp', 's_rh', 's_cond'].map(id => document.getElementById(id).textContent)).then(a => a.map(norm));
const climateIs = (page, want, what) => checkSoon(async () => JSON.stringify(await climate(page)) === JSON.stringify(want),
                                                  async () => what + ' (« ' + (await climate(page)).join(' | ') + ' »)');

/* ---- la page, connexion et panneaux : les mêmes vérifications dans les deux langues ---- */
async function scenario(browser, lang) {
  console.log('--- page en ' + lang.toUpperCase());
  const X = T[lang];
  const { ctx, page } = await open(browser, lang === 'fr' ? 'fr-FR' : 'en-US');
  check(await page.evaluate(() => document.documentElement.lang) === lang && await page.title() === 'emount-bench',
        '<html lang="' + lang + '">, le titre reste emount-bench');
  await page.click('#btnConnect');
  await checkSoon(async () => await page.textContent('#connTxt') === X.connected, 'connexion');
  if (fails) { console.log(await page.textContent('#log')); }
  await checkSoon(async () => (await page.textContent('#s_id')).includes('Samyang AF 135mm'), 'identite affichee (i)');
  await checkSoon(async () => (await page.textContent('#s_board')).startsWith('fw 1.3 · reset poweron · boot ready'), async () => 'la carte, v et DEBUG (« ' + (await page.textContent('#s_board')) + ' »)');
  await checkSoon(async () => await page.textContent('#s_caps') === X.caps, 'ext lu au lieu de caps');
  if (fails) { console.log(await page.textContent('#log')); console.log(await page.evaluate(() => JSON.stringify(window.__sent))); }
  await checkSoon(async () => await page.textContent('#s_focus') === '16384', 'position lue par f');
  await checkSoon(async () => norm(await page.textContent('#scMin')) === X.near, async () => 'bornes lues par r, nombre au format de la langue (« ' + (await page.textContent('#scMin')) + ' »)');
  await checkSoon(async () => !(await page.isDisabled('#focusRange')), 'curseur focus actif (goto + bornes)');
  await checkSoon(async () => await page.textContent('#s_fnum') === 'f/1.8', 'ouverture courante par o');
  await climateIs(page, X.dew, 'z, objectif monté : température, humidité, point de rosée');
  /* L09-16 : ce que la carte ne sert pas n'est plus sur la page */
  for (const id of ['btnHome', 'btnCalib', 'btnDump', 'dumpType', 'btnSniffOn', 'btnSniffOff', 'btnSdriveScan', 'btnSdriveStop', 'sdriveState', 's_mods', 's_fw', 's_focal', 's_dist'])
    check(await page.$('#' + id) === null, 'retiré de la page : #' + id);
  check(await page.$('button[data-cmd]') === null, 'retirés de la page : DUMP et le profil SDRIVE (data-cmd)');
  await page.click('#btnFp100');
  await checkSoon(async () => await page.textContent('#s_focus') === '16484', 'f+100 puis poll : position suivie');
  await page.fill('#focusTarget', '20000'); await page.click('#btnFgoto');
  await checkSoon(async () => await page.textContent('#s_focus') === '20000', 'aller a 20000');
  await page.click('#btnMarkSet');
  await checkSoon(async () => await page.evaluate(() => window.__sim.mark) === 20000 && await page.evaluate(() => document.getElementById('scMark').style.display) === '',
                  'Marquer -> js, la marque 20000 relue par j et affichee');
  /* la marque quittée (f+100 arrivé), puis rejointe */
  await page.click('#btnFp100'); await eventually(async () => await page.textContent('#s_focus') === '20100');
  const nGo = await page.evaluate(() => window.__sent.length);
  await page.click('#btnMarkGo');
  await checkSoon(async () => await page.textContent('#s_focus') === '20000', 'Aller a la marque -> jg, de retour en 20000');
  /* L09-15 : un déplacement de la page fini bloqué (g = stall) est signalé dans le panneau Focus ; lu sans l'attendre
     (null s'il n'existe pas). « Rien de signalé » se lit une fois la fin du déplacement traitée par la page : son `g`
     suivi d'une autre commande (la file de la page est sérielle, et le poll suivant ne part qu'après moveEnd). */
  const sMove = () => page.evaluate(() => { const e = document.getElementById('s_move'); return e ? e.textContent : null; });
  /* le `g` de `jg` : après un `e` envoyé après `jg` (celui du f+100 peut partir après `jg`, mais jamais après un `e` qui
     le suit) */
  await eventually(() => page.evaluate(n => { const s = window.__sent.slice(n), j = s.indexOf('jg'), e = s.indexOf('e', j + 1), i = s.indexOf('g', e + 1);
                                              return j >= 0 && e > j && i > e && i < s.length - 1; }, nGo));
  check(await sMove() === '', 'deplacements arrives : rien de signale');
  await page.evaluate(() => { window.__sim.stallNext = true; });
  await page.click('#btnFp100');
  await checkSoon(async () => String(await sMove()).includes('stall') && (await page.textContent('#log')).includes(X.stall),
                  async () => 'bloque (g = stall) : signale au panneau Focus et au journal (« ' + (await sMove()) + ' »)');
  /* Un Stop de l'humain (q) pendant un déplacement de la page : g = aborted, un arrêt demandé, pas un échec (PROTOCOL.md
     § 2, ligne q) — au journal une ligne d'événement, aucune d'erreur, et le panneau Focus dit « arrêté ». Le Stop part
     une fois le déplacement accepté par le faux port (sinon q le retire de la file). */
  const moveLines = () => page.$$eval('#log .l', (ls, m) => ls.filter(l => l.textContent.includes(m)).map(l => l.className + ' | ' + l.textContent), X.moveEnd);
  const before = (await moveLines()).length;
  await page.click('#btnFp100');
  await page.waitForFunction(() => window.__sim.moving > 0, null, { timeout: 10000 }).catch(() => {});
  await page.click('#btnStop');
  let stopLines = [];
  await checkOnce(page, async () => { stopLines = (await moveLines()).slice(before);
                                      return await page.evaluate(() => window.__sim.g) === 'aborted' && stopLines.length === 1 && /\bev\b/.test(stopLines[0]) && !/\berr\b/.test(stopLines[0]); },
                  () => 'arrete (g = aborted) : une ligne d\'evenement au journal, pas d\'erreur (' + JSON.stringify(stopLines) + ')');
  await checkSoon(async () => String(await sMove()).includes(X.stopped) && !String(await sMove()).includes('aborted'),
                  async () => 'arrete (g = aborted) : le panneau Focus dit « ' + X.stopped + ' » (« ' + (await sMove()) + ' »)');
  /* L'arrêt non confirmé (aborted passé à unconfirmed, relu pendant la fenêtre de l'arrêt) reste une erreur. */
  await page.evaluate(() => { window.__sim.g = 'unconfirmed'; });
  let unconf = [];
  await checkOnce(page, async () => { unconf = (await moveLines()).slice(before + 1);
                                      return unconf.length === 1 && /\berr\b/.test(unconf[0]) && unconf[0].includes('unconfirmed') && String(await sMove()).includes('unconfirmed'); },
                  async () => 'aborted puis unconfirmed : signale en erreur (' + JSON.stringify(unconf) + ', « ' + (await sMove()) + ' »)');
  check(await page.evaluate(() => window.__sent.filter(c => c === 'DEBUG').length) === 1 && await page.evaluate(() => window.__sent.includes('g')),
        'la fin se lit par g ; DEBUG une fois, a la connexion seulement');
  await page.click('#btnIp');
  await checkSoon(async () => (await page.textContent('#s_fnum')).startsWith('f/2'), '+1/3 : f/2 rapporte par o');
  /* L09-17 : pointerdown sans change (relâché sans bouger, pointercancel) laissait le glissement ouvert, et le curseur ne
     suivait plus la position */
  const focusRange = () => page.$eval('#focusRange', e => e.value);
  await page.dispatchEvent('#focusRange', 'pointerdown'); await page.dispatchEvent('#focusRange', 'pointerup');
  await page.evaluate(() => { window.__sim.pos = 21000; });
  await checkSoon(async () => await focusRange() === '21000', async () => 'focus relache sans bouger : le curseur suit la position (' + await focusRange() + ')');
  await page.dispatchEvent('#focusRange', 'pointerdown'); await page.dispatchEvent('#focusRange', 'pointercancel');
  await page.evaluate(() => { window.__sim.pos = 21500; });
  await checkSoon(async () => await focusRange() === '21500', async () => 'focus annule (pointercancel) : le curseur suit la position (' + await focusRange() + ')');
  await page.dispatchEvent('#irisRange', 'pointerdown'); await page.dispatchEvent('#irisRange', 'pointerup');
  await page.evaluate(() => { window.__sim.fnum = 4; });
  await checkSoon(async () => await page.textContent('#irisRangeVal') === 'f/4',
                  async () => 'ouverture relachee sans bouger : le curseur suit l ouverture rapportee (« ' + await page.textContent('#irisRangeVal') + ' »)');
  await page.evaluate(() => { window.__sim.pos = 20000; });       /* la position que :GP# relit plus bas (4E20#) */
  await page.click('#btnReboot');
  /* #703 : le faux port passe par les états du démarrage (powering… restoring), un par lecture de `t` : la page montre
     le premier, puis `ready` et les bornes relues à la fin, plus tard qu'avec l'ancien faux (busy=boot sur 10 `e`) */
  const vu = sel => page.waitForFunction(t => document.getElementById('s_state').textContent === t, sel, { timeout: 10000 }).then(() => true, () => false);
  check(await vu(X.busy + 'powering'), 'b : demarrage, l etat de la carte via t (« ' + X.busy + 'powering »)');
  await page.waitForFunction(r => document.getElementById('s_state').textContent.includes(r) && document.getElementById('scMin').textContent.replace(/\D/g, '') === '13700', X.ready, { timeout: 10000 }).catch(() => {});
  check((await page.textContent('#s_state')).includes(X.ready) && (await page.textContent('#scMin')).replace(/\D/g, '') === '13700', 'fin du demarrage : bornes relues');
  await page.evaluate(() => { window.__sim.present = false; });
  await checkSoon(async () => (await page.textContent('#s_state')).includes(X.absent), 'objectif disparu : etat absent');
  await checkSoon(() => page.isDisabled('#btnFp100'), 'boutons focus desactives sans objectif');
  await climateIs(page, X.dew, 'z, sans objectif : toujours lu et affiché');
  await page.evaluate(() => { window.__sim.z = '-3.05 87.50 -4.27'; });
  await climateIs(page, X.frost, 'z, sans objectif : un seuil négatif est un point de givre');
  await page.evaluate(() => { window.__sim.z = '-0.50 0.00 -'; });
  await climateIs(page, X.noCond, 'z, sans objectif : le seuil non défini (-) s\'affiche « — »');
  await page.evaluate(() => { window.__sim.z = '-'; });
  await climateIs(page, [X.noCond[0], '—', '—', '—'], 'z, sans objectif : la mesure inconnue (-) s\'affiche « — »');
  await page.evaluate(() => { window.__sim.z = '-3.05 87.50 -4.27'; });
  await page.evaluate(() => { window.__sim.present = true; });
  /* #703 : l'insertion passe par le démarrage (le faux port, comme la carte) : « prêt » à sa fin, pas 1,5 s après */
  await page.waitForFunction(r => document.getElementById('s_state').textContent === r, X.ready, { timeout: 10000 }).catch(() => {});
  check((await page.textContent('#s_state')).includes(X.ready), 'objectif revenu');
  await climateIs(page, X.frost, 'z, objectif monté : un point de givre');
  await page.evaluate(() => { window.__sim.z = '21.37 48.02 9.91'; });
  /* L09-14 : en fault, le statut le dit avec sa raison, et les panneaux qui ont besoin d'un objectif prêt sont inactifs */
  await page.evaluate(() => { window.__sim.fault = true; });
  await checkSoon(async () => await page.textContent('#s_state') === X.fault, async () => 'fault : statut et raison (« ' + (await page.textContent('#s_state')) + ' »)');
  await checkSoon(async () => await page.isDisabled('#btnFp100') && await page.isDisabled('#focusRange') && await page.isDisabled('#btnMarkGo') && await page.isDisabled('#irisRange') && await page.isDisabled('#btnIp'),
                  'fault : panneaux Focus et Ouverture inactifs');
  await page.evaluate(() => { window.__sim.fault = false; });
  await checkSoon(async () => (await page.textContent('#s_state')).includes(X.ready) && !(await page.isDisabled('#btnFp100')), 'sortie de fault : pret, panneaux actifs');
  await page.fill('#cliIn', ':GP#'); await page.press('#cliIn', 'Enter');
  await checkSoon(async () => /4E20#/.test(await page.textContent('#log')), 'Moonlite :GP# depuis la ligne de commande -> 4E20#');
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

/* ---- #703 : « prêt » quand la carte l'est (boot_state=ready), et pas avant ----
   Le faux port passe par les états de la carte (sim) ; la page les montre, n'annonce « prêt » qu'à `ready`, une fois,
   avec le nom que rend `i`, et ne présente pendant ce temps que Stop parmi les commandes de mouvement. Le relevé : chaque
   texte de la ligne d'état (s_state), et chaque ligne du journal avec l'état du faux port au moment où la page l'écrit. */
const MOTION = ['btnFm100', 'btnFm10', 'btnFp10', 'btnFp100', 'btnMarkSet', 'btnMarkGo', 'btnFgoto', 'focusRange', 'focusTarget', 'btnIopen', 'btnIm', 'btnIp', 'irisRange'];
async function startup(browser, lang) {
  console.log('--- « prêt » à boot_state=ready seulement, en ' + lang.toUpperCase());
  const X = T[lang];
  const { ctx, page } = await open(browser, lang === 'fr' ? 'fr-FR' : 'en-US');
  const stateIs = (want, ms) => page.waitForFunction(w => document.getElementById('s_state').textContent === w, want, { timeout: ms || 12000 }).then(() => true, () => false);
  await page.click('#btnConnect');
  check(await stateIs(X.ready), 'connectée, objectif prêt');
  await page.evaluate(() => {
    window.__rec = [];
    new MutationObserver(() => window.__rec.push({ state: document.getElementById('s_state').textContent }))
      .observe(document.getElementById('s_state'), { childList: true, characterData: true, subtree: true });
    const orig = addLine; addLine = (cls, text) => { window.__rec.push({ log: text, sim: window.__sim.state() }); return orig(cls, text); };
  });
  const record = async () => { const r = await page.evaluate(() => window.__rec.splice(0)); const st = []; for (const e of r) if (e.state !== undefined && e.state !== st[st.length - 1]) st.push(e.state); return { st, logs: r.filter(e => e.log !== undefined) }; };
  const READY_WORD = /prêt|ready/i;
  /* la page écrit l'annonce « prêt » juste après avoir montré `ready` : le relevé se prend quand elle y est */
  const announced = () => eventually(() => page.evaluate(() => window.__rec.some(e => e.log !== undefined && /^\[app\] (prêt|ready)/.test(e.log))));
  /* l'ordre des états montrés, rien de « prêt » avant `ready` (ligne d'état et journal), une annonce, avec le nom */
  const arrival = (what, rec, states) => {
    const before = rec.st.slice(0, -1), early = rec.logs.filter(e => e.sim !== 'ready' && READY_WORD.test(e.log)), ann = rec.logs.filter(e => /^\[app\] (prêt|ready)/.test(e.log));
    check(JSON.stringify(rec.st) === JSON.stringify(states.map(s => X.busy + s).concat(X.ready)), what + ' : l\'état affiché suit la carte (' + JSON.stringify(rec.st) + ')');
    check(!before.some(s => READY_WORD.test(s)) && early.length === 0, what + ' : rien de « prêt » / « ready » avant ready (' + JSON.stringify(early.map(e => e.sim + ' ' + e.log)) + ')');
    check(ann.length === 1 && ann[0].log === X.announce && ann[0].sim === 'ready', what + ' : « prêt » annoncé une fois, à ready, avec le nom de i (' + JSON.stringify(ann) + ')');
  };
  /* critère 1 : l'insertion */
  await page.evaluate(() => { window.__sim.present = false; });
  check(await stateIs(X.absent), 'objectif retiré');
  await record();
  await page.evaluate(() => { window.__sim.limitsAfter = 13750; window.__sim.present = true; });
  await stateIs(X.ready, 15000); await announced(); await settle(page);
  arrival('insertion', await record(), ['powering', 'identifying', 'homing', 'restoring']);
  check(norm(await page.textContent('#scMin')) === X.near13750 && await page.textContent('#s_id') === 'Samyang AF 135mm f/1.8-22 fw1.05',
        'insertion : à ready, le nom et les bornes de r (« ' + norm(await page.textContent('#scMin')) + ' »)');
  /* critères 3 et 4 : dans chaque état de démarrage et de reprise (tenu, S.hold), les commandes de mouvement inactives et
     muettes, Stop actif ; la console libre envoie ce qu'on y tape */
  const sentSince = n => page.evaluate(n => window.__sent.slice(n), n);
  for (const st of ['powering', 'identifying', 'homing', 'restoring', 'recovering']) {
    await page.evaluate(st => { Object.assign(window.__sim, { seq: [st], hold: true, shownAt: 0 }); }, st);
    check(await stateIs(X.busy + st), st + ' : l\'état affiché (« ' + await page.textContent('#s_state') + ' »)');
    const off = await page.evaluate(ids => ids.filter(id => !document.getElementById(id).disabled), MOTION);
    const n = (await page.evaluate(() => window.__sent.length));
    await page.evaluate(ids => ids.forEach(id => document.getElementById(id).click()), MOTION);
    await page.waitForTimeout(400);   /* vérification négative (aucune commande envoyée) : une fenêtre d'observation */
    const moves = (await sentSince(n)).filter(c => /^(f[+-]?\d+|m\d+|j[sgx]|a.+)$/.test(c));
    check(off.length === 0 && moves.length === 0, st + ' : commandes de mouvement inactives, aucune envoyée (actives : ' + off.join(' ') + ' ; envoyées : ' + moves.join(' ') + ')');
    check(!(await page.isDisabled('#btnStop')), st + ' : Stop actif');
    /* #732 : la section Ouverture s'atténuait en plus de ses contrôles, Focus non. Le rendu d'un contrôle : son opacité
       multipliée par celle de ses ancêtres, et son curseur. */
    const look = await page.evaluate(ids => {
      const seen = id => { let o = 1; for (let e = document.getElementById(id); e; e = e.parentElement) o *= +getComputedStyle(e).opacity;
                           return o.toFixed(3) + ' ' + getComputedStyle(document.getElementById(id)).cursor; };
      return { off: ids.map(id => id + ' ' + seen(id)), stop: seen('btnStop') };
    }, MOTION);
    const looks = new Set(look.off.map(s => s.slice(s.indexOf(' ') + 1)));
    check(looks.size === 1 && parseFloat(look.stop) > parseFloat([...looks][0]) && look.stop.endsWith(' pointer'),
          st + ' : Focus et Ouverture inactifs présentés de même, Stop visiblement actif (' + look.off.join(', ') + ' ; Stop ' + look.stop + ')');
  }
  { const n = (await page.evaluate(() => window.__sent.length));
    await page.fill('#cliIn', 'f+10'); await page.press('#cliIn', 'Enter');
    await checkSoon(async () => (await sentSince(n)).includes('f+10') && (await page.textContent('#log')).includes('er busy boot'), 'console libre, hors ready : f+10 part tel quel, la réponse de la carte affichée'); }
  /* critère 4 : Stop en homing (la carte refuse : sa réponse s'affiche), puis en restoring (la carte arrête le retour) ;
     les lignes du journal relevées quand celles qu'on attend (want) y sont */
  const stop = async (...want) => { const n = await page.evaluate(() => window.__rec.length); await page.$eval('#btnStop', e => e.click());
                                    let logs = [];
                                    await eventually(async () => { logs = (await page.evaluate(n => window.__rec.slice(n), n)).filter(e => e.log !== undefined).map(e => e.log);
                                                                   return want.every(w => logs.includes(w)); });
                                    return logs; };
  await page.evaluate(() => { Object.assign(window.__sim, { seq: ['homing'], hold: true, shownAt: 0 }); });
  check(await stateIs(X.busy + 'homing'), 'homing tenu');
  let shown = await stop('> q', 'er busy home');
  check(shown.includes('> q') && shown.includes('er busy home'), 'Stop en homing : q envoyé, la réponse de la carte affichée (' + JSON.stringify(shown) + ')');
  await page.evaluate(() => { Object.assign(window.__sim, { seq: ['restoring'], hold: true, shownAt: 0 }); });
  check(await stateIs(X.busy + 'restoring'), 'restoring tenu');
  await record();
  shown = await stop('> q', 'ok');
  check(shown.includes('> q') && shown.includes('ok') && await page.evaluate(() => window.__sim.state()) === 'ready',
        'Stop en restoring : q envoyé, le retour arrêté (ready), la réponse affichée (' + JSON.stringify(shown) + ')');
  await page.evaluate(() => { window.__sim.hold = false; });
  check(await stateIs(X.ready), 'après Stop en restoring : prêt');
  await announced(); await settle(page);
  { const ann = (await record()).logs.filter(e => /^\[app\] (prêt|ready)/.test(e.log));
    check(ann.length === 1 && ann[0].log === X.announce, 'après Stop en restoring : « prêt » annoncé une fois, avec le nom (' + JSON.stringify(ann) + ')'); }
  /* critère 4, fault : Stop reste actif dès qu'un objectif est là */
  await page.evaluate(() => { window.__sim.fault = true; });
  check(await stateIs(X.fault) && !(await page.isDisabled('#btnStop')) && await page.isDisabled('#btnFp100'), 'fault : Stop actif, le reste inactif');
  await page.evaluate(() => { window.__sim.fault = false; });
  check(await stateIs(X.ready), 'sortie de fault : prêt');
  await announced();                                /* son annonce écrite, hors du relevé de la reprise */
  /* critère 2 : la reprise, recovering, un nouvel essai d'init, le retour (restoring), ready ; la position et les bornes
     relues */
  await record();
  await page.evaluate(() => { window.__sim.recover(); window.__sim.pos = 17000; window.__sim.limitsAfter = 13760; });
  await stateIs(X.busy + 'recovering'); await stateIs(X.ready, 15000); await announced(); await settle(page);
  arrival('reprise', await record(), ['recovering', 'powering', 'identifying', 'restoring']);
  check(await page.textContent('#s_focus') === '17000' && norm(await page.textContent('#scMin')) === X.near13760 && !(await page.isDisabled('#btnFp100')),
        'reprise : à ready, la position et les bornes relues, les commandes actives (« ' + await page.textContent('#s_focus') + ' », « ' + norm(await page.textContent('#scMin')) + ' »)');
  /* #750 : `t` rend ready, puis la carte en sort avant `i` (une reprise commence) : `i` rend `er busy boot`. La ligne d'état
     le dit (#732) ; les commandes de mouvement, lues dans le DOM à cet instant, sont inactives et Stop actif. Le relevé
     est pris quand la ligne d'état change (MutationObserver), après la fin du traitement de `i` par la page, avant le `t`
     suivant. Les commandes étaient actives juste avant (reprise ci-dessus), et la page passe par `restoring` pour
     revenir à ready. */
  await page.evaluate(([ids, want]) => {
    window.__noId = null;
    new MutationObserver(() => { if (window.__noId === null && document.getElementById('s_state').textContent === want)
      window.__noId = { on: ids.filter(id => !document.getElementById(id).disabled), stop: !document.getElementById('btnStop').disabled, sim: window.__sim.state() }; })
      .observe(document.getElementById('s_state'), { childList: true, characterData: true, subtree: true });
    Object.assign(window.__sim, { seq: ['restoring'], hold: false, shownAt: 0, leaveAtI: true });
  }, [MOTION, X.noIdentity]);
  check(await page.waitForFunction(() => window.__noId !== null, null, { timeout: 12000 }).then(() => true, () => false), 'ready, i en erreur : « ' + X.noIdentity + ' » affiché');
  { const r = await page.evaluate(() => window.__noId) || { on: ['?'], stop: false, sim: '?' };
    check(r.on.length === 0 && r.stop, 'ready, i en erreur (er busy boot, la carte en ' + r.sim + ') : les ' + MOTION.length + ' commandes de mouvement inactives dans le DOM, Stop actif (actives : ' + r.on.join(' ') + ' ; Stop ' + (r.stop ? 'actif' : 'inactif') + ')'); }
  check(await stateIs(X.ready, 15000) && !(await page.isDisabled('#btnFp100')), 'après la reprise, i lu : prêt, les commandes actives');
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
  await page.click('#btnConnect');
  await checkSoon(async () => refused && await page.evaluate(() => document.documentElement.lang) === 'en' && await page.textContent('#connTxt') === T.en.connected,
                  'sans stockage : la page bascule en EN et se connecte (stockage refusé : ' + refused + ')');
  await ctx.close();
}

/* ---- la bascule en cours de session, puis la langue retenue au rechargement ---- */
async function toggle(browser) {
  console.log('--- la bascule, connexion ouverte');
  const { ctx, page } = await open(browser, 'fr-FR');
  await page.click('#btnConnect');
  await page.waitForFunction(r => document.getElementById('s_state').textContent === r, T.fr.ready, { timeout: 10000 }).catch(() => {});
  await eventually(() => page.evaluate(a => lines.some(l => l.endsWith(a)), T.fr.announce));   /* le relevé, annonce écrite */
  const shown = () => page.$$eval('#log .l', ls => ls.map(l => l.innerHTML));
  const logBefore = await shown(), linesBefore = await page.evaluate(() => lines.slice());
  const count = c => page.evaluate(c => window.__sent.filter(x => x === c).length, c);
  const nf = await count('f'), nv = await count('v');
  check(linesBefore.some(l => l.endsWith('[app] carte : firmware 1.3')) && await page.textContent('#connTxt') === T.fr.connected, 'connectée en FR, le journal en FR');
  await page.click('#btnLang');
  const txt = s => page.textContent(s), attr = (s, a) => page.getAttribute(s, a);
  check(await page.evaluate(() => document.documentElement.lang) === 'en', 'la bascule : <html lang="en">');
  check(await txt('#connTxt') === 'connected' && await txt('#btnDisconnect') === 'Disconnect' && await txt('#btnReboot') === 'Restart lens'
        && await txt('#s_state') === '· ready' && await txt('#s_caps') === 'extended set (ext=1)' && norm(await txt('#scMin')) === '13,723 (near)'
        && (await txt('#s_board')).endsWith(' · last op. boot:ok') && (await attr('#btnStop', 'title')).startsWith('q: stop')
        && (await attr('#cliIn', 'placeholder')).startsWith('command (') && await page.title() === 'emount-bench',
        'les textes d\'interface passent en EN sur place, états affichés compris (« ' + [await txt('#s_state'), await txt('#scMin'), await txt('#s_board')].join(' | ') + ' »)');
  check(JSON.stringify(await climate(page)) === JSON.stringify(T.en.dew), 'le capteur de la carte passe en EN sur place (« ' + (await climate(page)).join(' | ') + ' »)');
  check(await page.evaluate(() => document.getElementById('connDot').classList.contains('on')) && !(await page.isDisabled('#btnDisconnect'))
        && await count('v') === nv && await count('DEBUG') === 1, 'la connexion reste ouverte : ni déconnexion, ni nouveau v, ni nouveau DEBUG');
  await checkSoon(async () => await count('f') > nf + 2, async () => 'le poll continue (f : ' + nf + ' -> ' + (await count('f')) + ')');
  check((await txt('#focusRangeVal')).endsWith(' steps'), 'le curseur focus en pas : « steps » (« ' + (await txt('#focusRangeVal')) + ' »)');
  const logAfter = (await shown()).slice(0, logBefore.length), linesAfter = (await page.evaluate(() => lines.slice())).slice(0, linesBefore.length);
  check(JSON.stringify(logAfter) === JSON.stringify(logBefore) && JSON.stringify(linesAfter) === JSON.stringify(linesBefore),
        'les lignes déjà écrites au journal (et donc à l\'export) ne changent pas (' + logBefore.length + ' lignes)');
  await page.evaluate(() => { window.__sim.stallNext = true; });
  await page.click('#btnFp100');
  let linesNow = [];
  await checkSoon(async () => { linesNow = (await page.evaluate(() => lines.slice())).slice(linesBefore.length);
                                return linesNow.some(l => l.endsWith(T.en.stall)) && !linesNow.some(l => l.includes('[app] fin du déplacement')) && await txt('#s_move') === 'last move: stall'; },
                  () => 'après la bascule, les nouvelles lignes [app] sont en EN (' + JSON.stringify(linesNow.filter(l => l.includes('[app]'))) + ')');
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
  const until = (f, arg) => page.waitForFunction(f, arg, { timeout: 10000 }).then(() => true, () => false);
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
  await startup(browser, 'fr');
  await startup(browser, 'en');
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
