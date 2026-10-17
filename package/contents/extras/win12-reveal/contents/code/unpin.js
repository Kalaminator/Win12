// Drag an app from a task manager to another island: Plasma makes a new Icon widget there and keeps the launcher (a
// copy). In Win12 islands it is a move: the org.win12.reveal helper of the island that got the Icon widget runs this
// (main.qml, appletAdded) with @ID@ = the Icon widget's id, and the same app leaves the launchers of the task managers
// in the other islands (panels with a helper) of that screen. An open window of the app still shows there.
// Apps match by desktop file name: applications:firefox.desktop = file:///usr/share/applications/firefox.desktop.
const ID = @ID@, app = u => (String(u).split(/[?#]/)[0].match(/[^/:]+\.desktop$/) || [''])[0];
const home = panels().find(p => p.widgetIds.indexOf(ID) >= 0), icon = home && home.widgets().find(w => w.id === ID);
if (icon) { icon.currentConfigGroup = []; const a = app(icon.readConfig('url', ''));
 if (a) for (const p of panels()) { if (p.id === home.id || p.screen !== home.screen || !p.widgets('org.win12.reveal').length) continue;
  for (const w of p.widgets()) { if (!/icontasks|taskmanager/.test(w.type)) continue; w.currentConfigGroup = ['General'];
   // launchers still at the task manager's built-in default read as '' (nothing to drop, like main.qml setTaskbarPin)
   const l = String(w.readConfig('launchers', '')).split(',').filter(s => s), n = l.filter(s => app(s) !== a);
   if (n.length < l.length) { w.writeConfig('launchers', n); w.reloadConfig(); } } } }
