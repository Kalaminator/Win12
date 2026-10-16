// W5: Windows 12 split taskbar (key taskbarLayout), run by main.qml syncIslands() through plasmashell evaluateScript,
// again and again until it prints "done" (one step per pass). @ID@ = this widget's id.
// The panel holding this widget keeps the user's lengthMode/alignment, widget order and the ids moved to the right
// island in its win12Layout key; island panels are marked win12Island (L/R) and their ids are in that key too.
// split-left: L island = a fresh weather, the panel = centred Start + tasks, R island = the widgets after the last
// Task Manager (tray, clock, show desktop). split-right: the panel left, R island = weather + those widgets.
// single (or any other style): widgets moved back, islands removed, order/length/alignment/offset put back.
// W9 magnetic (@MAG@ = taskbarMagnetic): all the panels of the split 'custom' length (= fits its content, the only mode
// where Plasma 6.8 uses offset), left aligned, offsets so the group sits centred with GAP px between the islands.
// W16: GAP = taskbarGap (@GAP@), the same space as under the islands; each split panel carries an org.win12.reveal helper
// (0 px) that the win12islandreveal KWin script uses to show all the islands together.
const ID = @ID@, WANT = '@WANT@', MAG = @MAG@, GAP = @GAP@, HELPER = 'org.win12.reveal'; let out = 'done';
const get = (o, g, k, d) => { o.currentConfigGroup = [g]; return String(o.readConfig(k, d)); };
// one move or index set per pass: several in one script land out of order or leave the item unplaced.
const fix = (q, order) => { const ws = live(q), cur = ws.slice().sort((a, b) => a.index - b.index).map(w => w.id), i = order.findIndex((id, k) => cur[k] !== id);
 if (i >= 0) ws.find(w => w.id === order[i]).index = i; return i >= 0; };
const put = (o, k, v) => { o.currentConfigGroup = ['General']; o.writeConfig(k, v); };
let host = null, gone = []; const live = q => q.widgets().filter(w => gone.indexOf(w.id) < 0);
// W16: a live-moved system tray nests its config one level deeper per move (Plasma 6.8) and shows only some icons
// until plasmashell restarts. So a tray is never moved: a fresh one takes the old one's settings and its id in the
// layout, the old one is removed (it stays listed for undo, hence win12Gone).
const tray = (q, w, s) => { if (!/systemtray/.test(w.type)) return q.addWidget(w);
 const n = q.addWidget(w.type); w.currentConfigGroup = n.currentConfigGroup = ['General'];
 for (const k of w.configKeys) n.writeConfig(k, w.readConfig(k)); n.reloadConfig();
 const re = a => a.map(i => i === w.id ? n.id : i); s.moved = re(s.moved); s.order = re(s.order); put(host, 'win12Layout', JSON.stringify(s));
 gone.push(w.id); put(host, 'win12Gone', gone.filter(i => i).join(';')); w.remove(); };
for (const p of panels()) { const me = p.widgets().find(w => w.id === ID); if (!me) continue; host = p;
 const want = WANT;   // main.qml islandWant: the taskbar look (T43 taskbarLook or the style) is Windows 12
 // W15: wait until syncPanelLook has put the gap mode's thickness back (islands copy the panel height and layout)
 if (want !== 'single' && get(p, 'General', 'win12Saved_height', '')) { out = 'again'; continue; }
 gone = get(p, 'General', 'win12Gone', '').split(';').map(Number);
 const raw = get(p, 'General', 'win12Layout', ''), s = raw ? JSON.parse(raw) : null;
  const isl = k => s && panels().find(q => q.id === s[k]);
 const mode = s && s.mode !== want ? 'single' : want;
 if (mode === 'single') { if (!s) continue; out = 'again';
  const hs = [p, isl('L'), isl('R')].filter(q => q).map(q => q.widgets(HELPER)).reduce((a, b) => a.concat(b), []);
  if (hs.length) { hs.forEach(w => w.remove()); continue; }
  const back = s.moved.map(id => ['L', 'R'].map(isl).filter(q => q).map(q => q.widgets().find(w => w.id === id)).find(w => w)).filter(w => w);
  if (back.length) { tray(p, back[0], s); continue; }
  const L = isl('L'), R = isl('R'); if (L || R) { if (L) L.remove(); if (R) R.remove(); continue; }
  const ids = live(p).map(w => w.id); if (fix(p, s.order.filter(i => ids.indexOf(i) >= 0).concat(ids.filter(i => s.order.indexOf(i) < 0)))) continue;
  // a switch between the two splits keeps the first saved values (read-backs right after a restore can be stale)
  if (want !== 'single') { s.mode = want; delete s.L; delete s.R; put(p, 'win12Layout', JSON.stringify(s)); continue; }
  p.lengthMode = s.lm; p.alignment = s.al; if (s.mx) { p.maximumLength = s.mx; p.minimumLength = s.mn; p.offset = s.off; }
  put(p, 'win12Layout', ''); continue; }
 if (!s) { const ws = live(p).sort((a, b) => a.index - b.index), t = ws.map(w => /icontasks|taskmanager/.test(w.type)).lastIndexOf(true);
  put(p, 'win12Layout', JSON.stringify({ mode: want, lm: p.lengthMode, al: p.alignment, mn: p.minimumLength, mx: p.maximumLength, off: p.offset, order: ws.map(w => w.id),
   moved: ws.slice(t + 1).filter(w => !/marginsseparator|panelspacer/.test(w.type)).map(w => w.id) })); out = 'again'; continue; }
 for (const k of mode === 'split-left' ? ['L', 'R'] : ['R']) if (!isl(k)) { const q = new Panel();
  q.location = p.location; q.height = p.height; q.lengthMode = 'fit'; q.alignment = k === 'L' ? 'left' : 'right'; put(q, 'win12Island', k);
  if (k === 'L' || mode === 'split-right') q.addWidget('org.kde.plasma.weather');
  s[k] = q.id; put(p, 'win12Layout', JSON.stringify(s)); out = 'again'; break; }
 if (out === 'again') continue;
 const R = isl('R'), lost = live(R).find(w => w.index < 0);   // a move into a just-made empty panel can leave it unplaced: move it out and in again
 if (lost) { p.addWidget(lost); out = 'again'; continue; }
 const w = s.moved.map(id => p.widgets().find(w => w.id === id)).find(w => w); if (w) { tray(R, w, s); out = 'again'; continue; }
 const own = live(R).filter(w => s.moved.indexOf(w.id) < 0).sort((a, b) => a.index - b.index).map(w => w.id); if (fix(R, own.concat(s.moved))) { out = 'again'; continue; }
 // islands look like the panel; floating/hiding from the keys (syncPanelLook may not have reached the panel yet).
 const W = { floating: get(me, 'XP', 'taskbarFloating', true) !== 'false', hiding: [get(me, 'XP', 'taskbarFloating', true) !== 'false' ? 'autohide' : p.hiding, 'dodgewindows', 'autohide'][Number(get(me, 'XP', 'taskbarHiding', 0))] || p.hiding, height: p.height };
 for (const q of [isl('L'), p, R]) if (q && !q.widgets(HELPER).length) { q.addWidget(HELPER); out = 'again'; break; }
 if (out === 'again') continue;
 // W16: Plasma re-checks "a non-normal panel never de-floats" only when floating changes (W7): nudge after a change,
 // else an island made with hiding none next to a maximized window keeps floatingness 0 and touches the screen edge.
 for (const q of [isl('L'), R]) if (q) { let ch = false; for (const k in W) if (q[k] !== W[k]) { q[k] = W[k]; ch = true; }
  if (ch && q.floating) { q.floating = false; q.floating = true; } }
 // one panel set per pass; length = content width read back, a floating window adds 8 px padding each side
 // (Plasma's default floating margin, the islands' theme has no floating prefix).
 const row = mode === 'split-left' ? [isl('L'), p, R] : [p, R], sw = screenGeometry(p.screen).width;
 let x = Math.round((sw - row.reduce((t, q) => t + q.length, 0) - GAP * (row.length - 1)) / 2);
 for (const [i, q] of row.entries()) {
  const lm = MAG ? 'custom' : 'fit', al = MAG ? 'left' : q === p ? (mode === 'split-left' ? 'center' : 'left') : i ? 'right' : 'left', o = Math.max(0, x - (q.floating ? 8 : 0));
  x += q.length + GAP;
  if (q.lengthMode !== lm || q.alignment !== al || MAG && (q.minimumLength > 0 || q.offset !== o)) {
   q.lengthMode = lm; q.alignment = al; if (MAG) { q.minimumLength = 0; q.offset = o; } out = 'again'; break; } } }
print(out);
