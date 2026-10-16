// W5/W17: Windows 12 taskbar layouts (key taskbarLayout), run by main.qml syncIslands() through plasmashell
// evaluateScript until it prints "done". Needs Win12's patched KDE (kde/): moves, adds and index sets all land in
// one pass (immediate panel save, Widget.index fixes), so a switch is 2-3 passes; the loop is the safety net.
// code/islandlogic.js (roles, trayOrder, listsFor, mergeLists) is put in front of this file by syncIslands.
// @ID@ = this widget's id, @WANT@ = layout, @POS@ = Start position (0 left, 1 centre, 2 right), @MAG@ = taskbarMagnetic, @GAP@ = taskbarGap.
// The host panel (holds this widget) keeps the user's lengthMode/alignment/order and the ids moved out in its
// win12Layout key; island panels carry win12Island = L/R (splits), A (other app island), T (tray island).
// split-left: L = fresh weather, host = Start + tasks, R = the widgets after the last Task Manager (tray, clock, ...).
// split-right: host left, R = weather + those widgets. islands: see roles() (table in the spec, 5.2): a Start
// position change re-places the panels and swaps the two app task managers' win12Island roles + launchers.
// single (or another look): widgets moved back, islands removed, order/length/alignment/offset put back.
// magnetic: all panels 'custom' length (fits content, the only mode where Plasma 6.8 uses offset), left aligned,
// offsets so the group sits centred with GAP px between islands; each panel carries an org.win12.reveal helper
// (0 px) that the win12islandreveal KWin script uses to show all the islands together.
const ID = @ID@, WANT = '@WANT@', POS = @POS@, MAG = @MAG@, GAP = @GAP@, HELPER = 'org.win12.reveal'; let out = 'done';
const get = (o, g, k, d) => { o.currentConfigGroup = [g]; return String(o.readConfig(k, d)); };
// launchers go out as a list: a comma-joined string reaches the task manager's QML as ONE launcher
const put = (o, k, v) => { o.currentConfigGroup = ['General']; o.writeConfig(k, k === 'launchers' ? String(v).split(',').filter(s => s) : v); };
// a removed widget stays listed until the script ends (a tray even longer, W16): its id goes in gone
let host = null, gone = []; const live = q => q.widgets().filter(w => gone.indexOf(w.id) < 0);
const isTM = w => /icontasks|taskmanager/.test(w.type), byId = (q, id) => live(q).find(w => w.id === id);
// live widgets of q in this order (ids not in q skipped); true when something moved. Setting index k for k = 0.. in
// order converges in one go on the patched shell (each set is saved at once).
const order = (q, ids) => { let ch = false; ids.filter(id => byId(q, id)).forEach((id, i) => { const w = byId(q, id); if (w.index !== i) { w.index = i; ch = true; } }); return ch; };
// W16/W17: a tray moved before the patch nested its config one level per move. The first tray move after the patch
// still uses a fresh tray (takes the old one's settings and its id in the layout) as the one-time cleanup; the host
// then gets win12TrayClean=true and later moves are normal moves (libplasma patch keeps one config level).
const tray = (q, w, s) => { if (!/systemtray/.test(w.type) || get(host, 'General', 'win12TrayClean', '') === 'true') return q.addWidget(w);
 const n = q.addWidget(w.type); w.currentConfigGroup = n.currentConfigGroup = ['General'];
 for (const k of w.configKeys) n.writeConfig(k, w.readConfig(k)); n.reloadConfig();
 const re = a => a.map(i => i === w.id ? n.id : i); s.moved = re(s.moved); s.order = re(s.order); put(host, 'win12Layout', JSON.stringify(s));
 gone.push(w.id); put(host, 'win12Gone', gone.filter(i => i).join(';')); put(host, 'win12TrayClean', 'true'); w.remove(); return n; };
// an island is never a host (Start dragged into one by hand would split it again every pass)
for (const p of panels()) { const me = p.widgets().find(w => w.id === ID); if (!me || get(p, 'General', 'win12Island', '')) continue; host = p;
 const want = WANT;   // main.qml islandWant: the taskbar look is Windows 12, else 'single'
 // W15: wait until syncPanelLook has put the gap mode's thickness back (islands copy the panel height and layout)
 if (want !== 'single' && get(p, 'General', 'win12Saved_height', '')) { out = 'again'; continue; }
 gone = get(p, 'General', 'win12Gone', '').split(';').map(Number);
 const raw = get(p, 'General', 'win12Layout', ''); let s = raw ? JSON.parse(raw) : null;
 const isl = k => s && s[k] ? panels().find(q => q.id === s[k]) : null;
 const mode = s && s.mode !== want ? 'single' : want;
 if (mode === 'single') { if (!s) continue; out = 'again';
  // everything back in one pass: helpers out, pins merged into the original task manager (pinned island first),
  // moved widgets home in their old order, island panels removed; a switch between looks restores first.
  const isls = ['L', 'R', 'A', 'T'].map(isl).filter(q => q);
  for (const q of [p].concat(isls)) q.widgets(HELPER).forEach(w => { gone.push(w.id); w.remove(); });
  const tm = live(p).find(isTM);
  if (tm && s.atm) { const tms = [tm].concat([s.atm, s.ttm].map(id => isls.map(q => byId(q, id)).find(w => w)).filter(w => w));
   const of = r => tms.filter(w => get(w, 'General', 'win12Island', '') === r).map(w => get(w, 'General', 'launchers', '')).join(',');
   put(tm, 'launchers', mergeLists(of('pinned'), of('open'), of('tray'))); put(tm, 'win12Island', ''); tm.reloadConfig(); }
  for (const id of s.moved) { const w = isls.map(q => byId(q, id)).find(w => w); if (w) tray(p, w, s); }
  for (const q of isls) q.remove();
  const ids = live(p).map(w => w.id); order(p, s.order.filter(i => ids.indexOf(i) >= 0).concat(ids.filter(i => s.order.indexOf(i) < 0)));
  if (want !== 'single') { s.mode = want; for (const k of ['L', 'R', 'A', 'T', 'atm', 'ttm', 'weather']) delete s[k]; put(p, 'win12Layout', JSON.stringify(s)); continue; }
  p.lengthMode = s.lm; p.alignment = s.al; if (s.mx) { p.maximumLength = s.mx; p.minimumLength = s.mn; p.offset = s.off; }
  put(p, 'win12Layout', ''); continue; }
 const tm = live(p).find(isTM);
 if (mode === 'islands' && !tm) continue;   // no task manager in the bar: nothing to split into islands
 if (!s) { const ws = live(p).sort((a, b) => a.index - b.index), tms = ws.map(isTM), t = tms.lastIndexOf(true);
  // Start at Right mirrors the bar (syncCenterPanel): the tray side is then in front of the task manager. moved keeps
  // the Start-left order (trayOrder mirrors it); this widget never moves (an island holding it would split again).
  const side = t >= 0 && ws.findIndex(w => w.id === ID) > t ? ws.slice(0, tms.indexOf(true)).reverse() : ws.slice(t + 1);
  s = { mode: want, lm: p.lengthMode, al: p.alignment, mn: p.minimumLength, mx: p.maximumLength, off: p.offset, order: ws.map(w => w.id),
   moved: side.filter(w => w.id !== ID && w.type !== HELPER && !/marginsseparator|panelspacer/.test(w.type)).map(w => w.id) }; out = 'again'; }
 const mk = (k, al) => { const q = new Panel(); q.location = p.location; q.height = p.height; q.lengthMode = 'fit'; q.alignment = al; put(q, 'win12Island', k); s[k] = q.id; out = 'again'; return q; };
 if (mode === 'islands') {
  if (!isl('A')) s.atm = mk('A', 'center').addWidget(tm.type).id;
  if (!isl('T')) { const q = mk('T', 'right'); s.ttm = q.addWidget(tm.type).id; s.weather = q.addWidget('org.kde.plasma.weather').id; }
 } else for (const k of mode === 'split-left' ? ['L', 'R'] : ['R']) if (!isl(k)) { const q = mk(k, k === 'L' ? 'left' : 'right'); if (k === 'L' || mode === 'split-right') q.addWidget('org.kde.plasma.weather'); }
 put(p, 'win12Layout', JSON.stringify(s));
 const dst = mode === 'islands' ? isl('T') : isl('R');
 for (const id of s.moved) { const w = byId(p, id); if (w) { tray(dst, w, s); out = 'again'; } }
 const lost = live(dst).find(w => w.index < 0);   // safety net: an unplaced widget goes out and in again
 if (lost) { p.addWidget(lost); out = 'again'; continue; }
 const R = roles(POS);
 // the 0 px reveal helper may sit anywhere, so it is left out of the order (one pass less)
 if (order(dst, mode === 'islands' ? trayOrder(POS, s.ttm, s.weather, s.moved) : live(dst).filter(w => s.moved.indexOf(w.id) < 0 && w.type !== HELPER).sort((a, b) => a.index - b.index).map(w => w.id).concat(s.moved))) out = 'again';
 if (mode === 'islands') { const atm = byId(isl('A'), s.atm), ttm = byId(isl('T'), s.ttm), cur = get(tm, 'General', 'win12Island', '');
  if (atm && ttm && cur !== R.host) {   // roles (and the pins with them) follow the Start position; written, then reloaded, so each applet sees its final pair
   const [lh, la] = listsFor(cur, R.host, get(tm, 'General', 'launchers', ''), get(atm, 'General', 'launchers', ''));
   put(tm, 'launchers', lh); put(tm, 'win12Island', R.host); put(atm, 'launchers', la); put(atm, 'win12Island', R.other); put(ttm, 'win12Island', 'tray');
   tm.reloadConfig(); atm.reloadConfig(); ttm.reloadConfig(); out = 'again'; } }
 // islands look like the panel; floating/hiding from the keys (syncPanelLook may not have reached the panel yet).
 const W = { floating: get(me, 'XP', 'taskbarFloating', true) !== 'false', hiding: [get(me, 'XP', 'taskbarFloating', true) !== 'false' ? 'autohide' : p.hiding, 'dodgewindows', 'autohide'][Number(get(me, 'XP', 'taskbarHiding', 0))] || p.hiding, height: p.height };
 const others = ['L', 'R', 'A', 'T'].map(isl).filter(q => q);
 for (const q of [p].concat(others)) if (!q.widgets(HELPER).length) { q.addWidget(HELPER); out = 'again'; }
 // W16: Plasma re-checks "a non-normal panel never de-floats" only when floating changes (W7): nudge after a change.
 for (const q of others) { let ch = false; for (const k in W) if (q[k] !== W[k]) { q[k] = W[k]; ch = true; } if (ch && q.floating) { q.floating = false; q.floating = true; } }
 // placement: lengths are read back (a floating window adds 8 px padding each side); the pass after a change re-checks them
 const row = mode === 'islands' ? R.row.map(k => k === 'host' ? p : isl(k)) : mode === 'split-left' ? [isl('L'), p, isl('R')] : [p, isl('R')], sw = screenGeometry(p.screen).width;
 let x = Math.round((sw - row.reduce((t, q) => t + q.length, 0) - GAP * (row.length - 1)) / 2);
 row.forEach((q, i) => { const lm = MAG ? 'custom' : 'fit', al = MAG ? 'left' : ['left', 'center', 'right'][row.length === 2 ? i * 2 : i], o = Math.max(0, x - (q.floating ? 8 : 0));
  x += q.length + GAP;
  if (q.lengthMode !== lm || q.alignment !== al || MAG && (q.minimumLength > 0 || q.offset !== o)) { q.lengthMode = lm; q.alignment = al; if (MAG) { q.minimumLength = 0; q.offset = o; } out = 'again'; } }); }
print(out);
