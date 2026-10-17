// W5/W17: Windows 12 taskbar layouts (key taskbarLayout), run by main.qml syncIslands() through plasmashell
// evaluateScript until it prints "done". On Win12's patched KDE (kde/) moves, adds and index sets all land in one
// pass (immediate panel save, Widget.index fixes), so a switch is 2-3 passes; the loop is the safety net.
// STOCK (spec 11): a stock KDE saves a panel's order only after the script returns (Widget.index reads -1 or a stale
// list until then), so a pass ends after its first layout change (stop()) and order() sets one index: the f70da6e
// rule, 5-10 s and visible jumps per switch. 3 Islands needs the patches: main.qml never asks for it on stock.
// code/islandlogic.js (roles, trayOrder, listsFor, mergeLists) is put in front of this file by syncIslands.
// @ID@ = this widget's id, @WANT@ = layout, @POS@ = Start position (0 left, 1 centre, 2 right), @MAG@ = taskbarMagnetic, @GAP@ = taskbarGap,
// @SCR@ = islandScreen, @ALL@ = islandAllApps, @SCREENS@ = the connected connector names (comma-joined), @STOCK@ = !kdePatched.
// The host panel (holds this widget) keeps the user's lengthMode/alignment/order and the ids moved out in its
// win12Layout key; island panels carry win12Island = L/R (splits), A (other app island), T (tray island).
// split-left: L = weather, host = Start + tasks, R = the widgets after the last Task Manager (tray, clock, ...).
// split-right: host left, R = weather + those widgets. islands: see roles() (table in the spec, 5.2): a Start
// position change re-places the panels and swaps the two app task managers' win12Island roles + launchers.
// layout to layout: no restore, the panels are re-keyed in the same pass (R <-> T, L <-> A) and the weather is kept.
// single (or another look): widgets moved back, islands removed, order/length/alignment/offset put back.
// magnetic: all panels 'custom' length (fits content, the only mode where Plasma 6.8 uses offset), left aligned,
// offsets so the group sits centred with GAP px between islands; each panel carries an org.win12.reveal helper
// (0 px) that the win12islandreveal KWin script uses to show all the islands together.
const ID = @ID@, WANT = '@WANT@', POS = @POS@, MAG = @MAG@, GAP = @GAP@, SCR = '@SCR@', ALLAPPS = @ALL@, NAMES = '@SCREENS@'.split(',').filter(n => n), HELPER = 'org.win12.reveal', STOCK = @STOCK@; let out = 'done', did = false;   // did: a layout change in this pass
// 10: screens by connector name (main.qml passes the connected ones). The islands live on the chosen monitor, else on
// the main one (plasmashell id 0 = primary); a name that is not connected falls back to the main one too. Only ids
// screenForConnector returned are ever written: a panel sent to a missing screen loses its view.
const sid = n => screenForConnector(n), ALLMON = SCR === 'all', MAIN = Math.max(0, SCR && !ALLMON ? sid(SCR) : 0);
const get = (o, g, k, d) => { o.currentConfigGroup = [g]; return String(o.readConfig(k, d)); };
// launchers go out as a list: a comma-joined string reaches the task manager's QML as ONE launcher
const put = (o, k, v) => { o.currentConfigGroup = ['General']; o.writeConfig(k, k === 'launchers' ? String(v).split(',').filter(s => s) : v); };
// a removed widget stays listed until the script ends (a tray even longer, W16): its id goes in gone
let host = null, gone = []; const live = q => q.widgets().filter(w => gone.indexOf(w.id) < 0);
const isTM = w => /icontasks|taskmanager/.test(w.type), byId = (q, id) => live(q).find(w => w.id === id);
// live widgets of q in this order (ids not in q skipped); true when something moved. Setting index k for k = 0.. in
// order converges in one go on the patched shell (each set is saved at once). STOCK: only the first mismatch (the stock
// setter clamps to the second-to-last slot; the first mismatch is never the last item), the rest in later passes.
const order = (q, ids) => { let ch = false; ids = ids.filter(id => byId(q, id));
 for (let i = 0; i < ids.length; i++) { const w = byId(q, ids[i]); if (w.index !== i) { w.index = i; ch = did = true; if (STOCK) break; } } return ch; };
// W16/W17: a tray moved before the patch nested its config one level per move. The first tray move after the patch
// still uses a fresh tray (takes the old one's settings and its id in the layout) as the one-time cleanup; the host
// then gets win12TrayClean=true and later moves are normal moves (libplasma patch keeps one config level). STOCK: no
// such patch, so every tray move is a fresh copy (f70da6e); the key is still written for a patched KDE later.
const tray = (q, w, s) => { did = true; if (!/systemtray/.test(w.type) || !STOCK && get(host, 'General', 'win12TrayClean', '') === 'true') return q.addWidget(w);
 const n = q.addWidget(w.type); w.currentConfigGroup = n.currentConfigGroup = ['General'];
 for (const k of w.configKeys) n.writeConfig(k, w.readConfig(k)); n.reloadConfig();
 const re = a => a.map(i => i === w.id ? n.id : i); s.moved = re(s.moved); s.order = re(s.order); put(host, 'win12Layout', JSON.stringify(s));
 gone.push(w.id); put(host, 'win12Gone', gone.filter(i => i).join(';')); put(host, 'win12TrayClean', 'true'); w.remove(); return n; };
// the patched task manager files its launchers in the island registry under the island it has at that moment: the
// island goes off first, then the launchers, then the new island (a reload is seen at once)
const setTM = (w, k, l) => { put(w, 'win12Island', ''); w.reloadConfig(); put(w, 'launchers', l); w.reloadConfig(); if (k) { put(w, 'win12Island', k); w.reloadConfig(); } };
// an island is never a host (Start dragged into one by hand would split it again every pass)
for (const p of panels()) { const me = p.widgets().find(w => w.id === ID); if (!me || get(p, 'General', 'win12Island', '')) continue; host = p;
 const want = WANT;   // main.qml islandWant: the taskbar look is Windows 12, else 'single'
 // W15: wait until syncPanelLook has put the gap mode's thickness back (islands copy the panel height and layout)
 if (want !== 'single' && get(p, 'General', 'win12Saved_height', '')) { out = 'again'; continue; }
 gone = get(p, 'General', 'win12Gone', '').split(';').map(Number);
 const raw = get(p, 'General', 'win12Layout', ''); let s = raw ? JSON.parse(raw) : null;
 const isl = k => s && s[k] ? panels().find(q => q.id === s[k]) : null, isls = () => ['L', 'R', 'A', 'T'].map(isl).filter(q => q);
 // STOCK: after a layout change (add, move, remove, index set) the pass ends here, s saved; config may share the pass
 const stop = () => STOCK && did && (put(p, 'win12Layout', JSON.stringify(s)), out = 'again', true);
 // 10: copies for the other monitors live in s.mon[connector] = { h: host copy panel, me: Start copy, tm, A: app island, atm }
 const byPid = id => panels().find(q => q.id === id), monPanels = () => !s || !s.mon ? [] : Object.keys(s.mon).map(n => [s.mon[n].h, s.mon[n].A]).reduce((a, b) => a.concat(b), []).map(byPid).filter(q => q);
 const tm = live(p).find(isTM), mode = want;
 // leaving 3 Islands: one pin list again for the original task manager (pinned island first), stock again; island TMs go
 const unisland = () => { if (!tm || !s.atm) return; const tms = [tm].concat([s.atm, s.ttm].map(id => isls().map(q => byId(q, id)).find(w => w)).filter(w => w));
  const of = r => tms.filter(w => get(w, 'General', 'win12Island', '') === r).map(w => get(w, 'General', 'launchers', '')).join(',');
  setTM(tm, '', mergeLists(of('pinned'), of('open'), of('tray'))); for (const w of tms.slice(1)) { gone.push(w.id); w.remove(); did = true; } delete s.atm; delete s.ttm; };
 if (mode === 'single') { if (!s) continue; out = 'again';
  // everything back in one pass (STOCK: one step per pass): helpers out, pins merged, moved widgets home in their old
  // order, island panels removed (also on a switch to another look)
  const qs = isls().concat(monPanels());
  for (const q of [p].concat(qs)) q.widgets(HELPER).forEach(w => { gone.push(w.id); w.remove(); did = true; });
  if (stop()) continue;
  unisland(); if (stop()) continue;
  for (const id of s.moved) { const w = qs.map(q => byId(q, id)).find(w => w); if (w) { tray(p, w, s); if (STOCK) break; } }
  if (stop()) continue;
  for (const q of qs) { q.remove(); did = true; } if (stop()) continue;
  const ids = live(p).map(w => w.id); order(p, s.order.filter(i => ids.indexOf(i) >= 0).concat(ids.filter(i => s.order.indexOf(i) < 0)));
  if (stop()) continue;
  p.lengthMode = s.lm; p.alignment = s.al; if (s.mx) { p.maximumLength = s.mx; p.minimumLength = s.mn; p.offset = s.off; }
  // the bar now has its unmirrored order: the Start position sync (centerLoop) saves it afresh. Its old saved order
  // was written while this was an island (no moved widgets), and mirroring that flipped them every pass.
  if (s.scr !== undefined && p.screen !== s.scr && NAMES.some(n => sid(n) === s.scr)) p.screen = s.scr;   // 10: home screen, if still there
  put(p, 'win12Layout', ''); put(p, 'win12PanelOrder', ''); continue; }
 if (mode === 'islands' && !tm) continue;   // no task manager in the bar: nothing to split into islands
 // layout to layout in this pass, no restore: the panel with the moved widgets (R in splits, T in islands) and the
 // other one (L in split-left, A in islands) are kept and re-keyed; a panel no layout needs goes (its weather stays)
 if (s && s.mode !== mode) { if (s.mode === 'islands') unisland();
  if (stop()) continue;
  // STOCK: the weather move and the panel removal below share a pass (the panel's id leaves s here)
  const key = m => m === 'islands' ? ['T', 'A'] : ['R', m === 'split-left' ? 'L' : ''], k0 = key(s.mode), k1 = key(mode);
  for (const i of [0, 1]) { const q = k0[i] && isl(k0[i]); if (!q || k0[i] === k1[i]) continue; delete s[k0[i]];
   if (k1[i]) { put(q, 'win12Island', k1[i]); s[k1[i]] = q.id; } else { const w = live(q).find(w => /weather/.test(w.type)), r = isl(k1[0]); if (w && r) r.addWidget(w); q.remove(); did = true; } }
  s.mode = mode; out = 'again'; }
 if (stop()) continue;
 if (!s) { const ws = live(p).sort((a, b) => a.index - b.index), tms = ws.map(isTM), t = tms.lastIndexOf(true);
  // Start at Right mirrors the bar (syncCenterPanel): the tray side is then in front of the task manager. moved keeps
  // the Start-left order (trayOrder mirrors it); this widget never moves (an island holding it would split again).
  // order = the unmirrored one syncCenterPanel keeps in win12PanelOrder (else a restore at Left came back mirrored)
  const po = get(p, 'General', 'win12PanelOrder', '').split(';').map(Number), ids = ws.map(w => w.id);
  const side = t >= 0 && ws.findIndex(w => w.id === ID) > t ? ws.slice(0, tms.indexOf(true)).reverse() : ws.slice(t + 1);
  s = { mode: want, scr: p.screen, lm: p.lengthMode, al: p.alignment, mn: p.minimumLength, mx: p.maximumLength, off: p.offset, order: po.filter(i => ids.indexOf(i) >= 0).concat(ids.filter(i => po.indexOf(i) < 0)),
   moved: side.filter(w => w.id !== ID && w.type !== HELPER && !/marginsseparator|panelspacer/.test(w.type)).map(w => w.id) }; out = 'again'; }
 const mk = (k, al) => { const q = new Panel(); did = true; q.location = p.location; q.height = p.height; q.lengthMode = 'fit'; q.alignment = al; put(q, 'win12Island', k); if (k !== 'M') s[k] = q.id; out = 'again'; return q; };
 if (mode === 'islands') {
  // a new island task manager gets an explicit empty pin list: with no key it shows KDE's default pins, and its
  // island would claim those apps from the pinned island
  const fresh = q => { const w = q.addWidget(tm.type); did = true; put(w, 'launchers', ''); return w.id; };
  const A = isl('A') || mk('A', 'center'), T = isl('T') || mk('T', 'right');
  if (!byId(A, s.atm)) s.atm = fresh(A);
  if (!byId(T, s.ttm)) s.ttm = fresh(T);
 } else for (const k of mode === 'split-left' ? ['L', 'R'] : ['R']) if (!isl(k)) { mk(k, k === 'L' ? 'left' : 'right'); if (STOCK) break; }
 if (stop()) continue;
 // one weather (not one of the bar's own widgets), kept across layout switches: in L in split-left, else with the moved widgets
 const wq = isl(mode === 'split-left' ? 'L' : mode === 'islands' ? 'T' : 'R'), wx = [p].concat(isls()).map(q => live(q).find(w => /weather/.test(w.type) && s.order.indexOf(w.id) < 0)).find(w => w);
 if (!wx || !byId(wq, wx.id)) did = true;   // one weather added or moved in
 if (wx && !byId(wq, wx.id)) wq.addWidget(wx);
 s.weather = wx ? wx.id : wq.addWidget('org.kde.plasma.weather').id;
 if (stop()) continue;
 put(p, 'win12Layout', JSON.stringify(s));
 const dst = mode === 'islands' ? isl('T') : isl('R');
 for (const id of s.moved) { const w = byId(p, id); if (w) { tray(dst, w, s); out = 'again'; if (STOCK) break; } }
 if (stop()) continue;
 const lost = live(dst).find(w => w.index < 0);   // safety net: an unplaced widget goes out and in again
 if (lost) { p.addWidget(lost); out = 'again'; continue; }
 const R = roles(POS);
 // the 0 px reveal helper may sit anywhere, so it is left out of the order (one pass less)
 if (order(dst, mode === 'islands' ? trayOrder(POS, s.ttm, s.weather, s.moved) : live(dst).filter(w => s.moved.indexOf(w.id) < 0 && w.type !== HELPER).sort((a, b) => a.index - b.index).map(w => w.id).concat(s.moved))) out = 'again';
 if (stop()) continue;
 if (mode === 'islands') { const atm = byId(isl('A'), s.atm), ttm = byId(isl('T'), s.ttm), cur = get(tm, 'General', 'win12Island', '');
  if (atm && ttm && cur !== R.host) {   // roles (and the pins with them) follow the Start position (setTM: registry-safe order)
   const [lh, la] = listsFor(cur, R.host, get(tm, 'General', 'launchers', ''), get(atm, 'General', 'launchers', ''));
   setTM(tm, R.host, lh); setTM(atm, R.other, la); put(ttm, 'win12Island', 'tray'); ttm.reloadConfig(); out = 'again'; } }
 // 10: All monitors: every other connected screen gets a copy of the host (a Start copy with this widget's settings +
 // a task manager like the host's: same win12Island role and pins) and, in 3 Islands, the other app island; the tray
 // island stays on the main screen. Marks: panel win12Island = 'M' (never a host, like any island), Start copy
 // win12Mirror = this id (main.qml: no engine, no panel or theme syncs). Pins are one-way (main monitor wins).
 const copyCfg = (a, b) => { let ch = false; a.currentConfigGroup = [];
  for (const g of a.configGroups.slice()) { a.currentConfigGroup = b.currentConfigGroup = [g];
   for (const k of a.configKeys.slice()) { if (/Request$|^win12Mirror$/.test(k)) continue; const v = a.readConfig(k, ''); if (String(b.readConfig(k, '')) !== String(v)) { b.writeConfig(k, v); ch = true; } } }
  if (get(b, 'General', 'win12Mirror', '') !== String(ID)) { put(b, 'win12Mirror', ID); ch = true; }
  if (ch) b.reloadConfig(); };   // a copy is complete in one pass: never 'again' for it
 const likeTM = (src, dst) => { const k = get(src, 'General', 'win12Island', ''), l = get(src, 'General', 'launchers', '');
  if (get(dst, 'General', 'win12Island', '') !== k || get(dst, 'General', 'launchers', '') !== l) { setTM(dst, k, l); out = 'again'; }
  if (get(dst, 'General', 'showOnlyCurrentScreen', '') !== String(!ALLAPPS)) { put(dst, 'showOnlyCurrentScreen', !ALLAPPS); dst.reloadConfig(); out = 'again'; } };
 s.mon = s.mon || {}; const mons = ALLMON ? NAMES.filter(n => sid(n) >= 0 && sid(n) !== MAIN) : [];
 for (const n in s.mon) if (mons.indexOf(n) < 0) { for (const id of [s.mon[n].h, s.mon[n].A]) { const q = byPid(id); if (q) { q.remove(); did = true; } } delete s.mon[n]; out = 'again'; }
 if (stop()) continue;
 for (const n of mons) { const m = s.mon[n] = s.mon[n] || {}, k = sid(n), at = q => { if (q.screen !== k) { q.screen = k; out = 'again'; } return q; };
  const h = at(byPid(m.h) || mk('M', 'center')); m.h = h.id; if (stop()) break;
  let mw = byId(h, m.me); if (!mw) { mw = h.addWidget(me.type); m.me = mw.id; out = 'again'; did = true; } copyCfg(me, mw); if (stop()) break;
  if (tm) { let mt = byId(h, m.tm); if (!mt) { mt = h.addWidget(tm.type); put(mt, 'launchers', ''); m.tm = mt.id; out = 'again'; did = true; } likeTM(tm, mt); if (stop()) break;
   if (order(h, POS === 2 ? [mt.id, mw.id] : [mw.id, mt.id])) out = 'again'; if (stop()) break; }
  if (mode === 'islands') { const a = at(byPid(m.A) || mk('M', 'center')); m.A = a.id;
   let mat = byId(a, m.atm); if (!mat) { mat = a.addWidget(tm.type); put(mat, 'launchers', ''); m.atm = mat.id; out = 'again'; } likeTM(byId(isl('A'), s.atm), mat); }
  else if (m.A) { const a = byPid(m.A); if (a) a.remove(); delete m.A; delete m.atm; out = 'again'; did = true; } }
 if (stop()) continue;   // the inner breaks land here
 put(p, 'win12Layout', JSON.stringify(s));
 // islands look like the panel; floating/hiding from the keys (syncPanelLook may not have reached the panel yet).
 const W = { floating: get(me, 'XP', 'taskbarFloating', true) !== 'false', hiding: [get(me, 'XP', 'taskbarFloating', true) !== 'false' ? 'autohide' : p.hiding, 'dodgewindows', 'autohide'][Number(get(me, 'XP', 'taskbarHiding', 0))] || p.hiding, height: p.height };
 const others = ['L', 'R', 'A', 'T'].map(isl).filter(q => q).concat(monPanels());
 for (const q of [p].concat(others)) if (!q.widgets(HELPER).length) { q.addWidget(HELPER); out = 'again'; if (STOCK) break; }
 // W16: Plasma re-checks "a non-normal panel never de-floats" only when floating changes (W7): nudge after a change.
 for (const q of others) { let ch = false; for (const k in W) if (q[k] !== W[k]) { q[k] = W[k]; ch = true; } if (ch && q.floating) { q.floating = false; q.floating = true; } }
 for (const q of [p].concat(isls())) if (q.screen !== MAIN) { q.screen = MAIN; out = 'again'; }   // 10: all on the chosen monitor
 // placement per screen: lengths are read back (a floating window adds 8 px padding each side); the pass after a change re-checks them
 const place = (k, row) => { const sw = screenGeometry(k).width; let x = Math.round((sw - row.reduce((t, q) => t + q.length, 0) - GAP * (row.length - 1)) / 2);
  row.forEach((q, i) => { const lm = MAG ? 'custom' : 'fit', al = MAG ? 'left' : ['left', 'center', 'right'][row.length === 2 ? i * 2 : i], o = Math.max(0, x - (q.floating ? 8 : 0));
   x += q.length + GAP;
   if (q.lengthMode !== lm || q.alignment !== al || MAG && (q.minimumLength > 0 || q.offset !== o)) { q.lengthMode = lm; q.alignment = al; if (MAG) { q.minimumLength = 0; q.offset = o; } out = 'again'; } }); };
 place(MAIN, mode === 'islands' ? R.row.map(k => k === 'host' ? p : isl(k)) : mode === 'split-left' ? [isl('L'), p, isl('R')] : [p, isl('R')]);
 for (const n of mons) { const m = s.mon[n]; place(sid(n), mode === 'islands' ? R.row.filter(k => k !== 'T').map(k => byPid(k === 'host' ? m.h : m.A)) : [byPid(m.h)]); } }
print(out);
