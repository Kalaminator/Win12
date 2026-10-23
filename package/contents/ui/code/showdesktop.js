// Show desktop strip, run by main.qml syncShowDesktop through plasmashell evaluateScript until it prints "done".
// @STRIP@ = a Windows taskbar look: Plasma's Show Desktop widget becomes org.win12.showdesktop (extras/win12-showdesktop,
// a thin strip with no icon); false (KDE Plasma look) = Plasma's widget back. @ID@ = this widget's id, @STOCK@ = !kdePatched.
// Only the panel with this widget (the host) and the island panels (win12Island) are touched, never the user's other panels.
// A swap keeps the place: the new widget carries the old index in win12At until it sits there (STOCK: a widget just
// added reads index -1 until the script ends, and a pass ends after its first layout change), the old widget's
// settings (win12Orig, put back on the way back) and its spot in the host's saved orders (win12Layout, win12PanelOrder).
// A removed widget stays listed until the script ends: its id goes in the host's win12Gone, like islands.js does.
const ID = @ID@, STRIP = @STRIP@, STOCK = @STOCK@, OLD = 'org.kde.plasma.showdesktop', NEW = 'org.win12.showdesktop';
const FROM = STRIP ? OLD : NEW, TO = STRIP ? NEW : OLD; let out = 'done', did = false;
const get = (o, k) => { o.currentConfigGroup = ['General']; return String(o.readConfig(k, '')); };
const put = (o, k, v) => { o.currentConfigGroup = ['General']; o.writeConfig(k, v); };
const host = panels().find(p => !get(p, 'win12Island') && p.widgets().some(w => w.id === ID));
// the strip plugin is installed by code/setup.sh: before that, wait (main.qml runs this again after setup)
if (host && (!STRIP || Array.prototype.indexOf.call(knownWidgetTypes, NEW) >= 0)) {
 const gone = get(host, 'win12Gone').split(';').map(Number), qs = [host].concat(panels().filter(q => get(q, 'win12Island')));
 // every index read first: on STOCK an index read after a layout change in the same pass is stale
 const ws = [].concat(...qs.map(q => q.widgets().filter(w => gone.indexOf(w.id) < 0 && (w.type === FROM || w.type === TO)).map(w => ({ q, w, i: w.index }))));
 const swapId = (a, b) => { const s = get(host, 'win12Layout'), po = get(host, 'win12PanelOrder');
  if (s) { const o = JSON.parse(s); for (const k of ['order', 'moved']) if (o[k]) o[k] = o[k].map(x => x === a ? b : x); put(host, 'win12Layout', JSON.stringify(o)); }
  if (po) put(host, 'win12PanelOrder', po.split(';').map(x => Number(x) === a ? b : x).join(';')); };
 for (const { w, i } of ws) { const at = get(w, 'win12At'); if (w.type !== TO || at === '') continue;
  if (i === Number(at)) put(w, 'win12At', ''); else { out = 'again'; if (i >= 0 && !(STOCK && did)) { w.index = Number(at); did = true; } } }
 for (const { q, w, i } of ws) { if (w.type !== FROM || STOCK && did) continue; if (i < 0) { out = 'again'; continue; }   // not laid out yet
  const n = q.addWidget(TO), keep = {}; w.currentConfigGroup = ['General']; for (const k of w.configKeys) keep[k] = w.readConfig(k, '');
  const orig = STRIP ? keep : JSON.parse(keep.win12Orig || '{}');
  if (STRIP) put(n, 'win12Orig', JSON.stringify(orig)); else for (const k in orig) put(n, k, orig[k]);
  put(n, 'win12At', i); n.reloadConfig();
  if (!STOCK) n.index = i;   // patched KDE: an add is saved at once, so the new one can take the old slot now
  swapId(w.id, n.id); gone.push(w.id); put(host, 'win12Gone', gone.filter(x => x).join(';')); w.remove(); out = 'again'; did = true; } }
print(out);
