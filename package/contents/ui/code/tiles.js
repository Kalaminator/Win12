.pragma library

// Tile groups live in the config as JSON, per page (parsePages):
// { "pages": [{ "groups": [{ "name": "Gaming", "tiles": [{ "id": "steam.desktop", "size": "medium" }] }] }] }
// Grid unit = one small tile. A group is (columns * 2) units wide.

var SIZES = {
    small:  { w: 1, h: 1 },
    medium: { w: 2, h: 2 },
    wide:   { w: 4, h: 2 },
    large:  { w: 4, h: 4 },
    wideLargeH: { w: 8, h: 4 },
    wideLargeV: { w: 4, h: 8 },
    hugeH: { w: 12, h: 6 },
    hugeV: { w: 6, h: 12 }
};

// Tile pages (Start11): { "pages": [{ "groups": [...] }] }. The old flat group list becomes page 1.
function parsePages(json) {
    var d;
    try { d = JSON.parse(json || "[]"); } catch (e) { d = []; }
    var pages = Array.isArray(d) ? [{ groups: d }] : (d && Array.isArray(d.pages) ? d.pages : []);
    pages = pages.filter(function (p) { return p && Array.isArray(p.groups); }).map(function (p) {
        return { groups: p.groups.filter(function (x) { return x && Array.isArray(x.tiles); }) };
    });
    return pages.length ? pages : [{ groups: [] }];
}

// Page p gets these groups; a tile moved here from another page leaves that page (no duplicates).
// Folder ids are per page ("folder|1" on two pages), so only app ids are checked.
function setPage(pages, p, groups) {
    var out = clone(pages);
    out[p] = { groups: groups };
    var ids = [];
    groups.forEach(function (g) { g.tiles.forEach(function (t) {
        (t.kind === "folder" ? t.tiles : [t]).forEach(function (x) { ids.push(x.id); });
    }); });
    out.forEach(function (pg, k) {
        if (k === p) return;
        ids.forEach(function (id) { if (!isFolder(id) && findTile(pg.groups, id)) pg.groups = remove(pg.groups, id); });
    });
    return out;
}

function newPage(pages) {
    return clone(pages).concat([{ groups: [] }]);
}

// Deleting a page keeps its groups: they move to the end of the page before it (or the next one).
function deletePage(pages, p) {
    var out = clone(pages);
    if (out.length < 2 || p < 0 || p >= out.length) return out;
    var gone = out.splice(p, 1)[0];
    var to = Math.max(0, p - 1);
    out[to].groups = p > 0 ? out[to].groups.concat(gone.groups) : gone.groups.concat(out[to].groups);
    return out;
}

// Move group g of page p to the end of page "to".
function moveGroupToPage(pages, p, g, to) {
    var out = clone(pages);
    if (to < 0 || to >= out.length || to === p || !out[p] || !out[p].groups[g]) return out;
    out[to].groups.push(out[p].groups.splice(g, 1)[0]);
    return out;
}

function sizeOf(size) {
    return SIZES[size] || SIZES.medium;
}

// First-fit packing, in list order, like Windows 10 tile groups.
// ponytail: O(tiles * cells) scan, fine for the few dozen tiles a menu holds.
function pack(tiles, columns) {
    var W = Math.max(2, columns * 2);
    var used = [];
    function free(x, y, w, h) {
        for (var j = y; j < y + h; j++)
            for (var i = x; i < x + w; i++)
                if (used[j] && used[j][i]) return false;
        return true;
    }
    function take(x, y, w, h) {
        for (var j = y; j < y + h; j++) {
            used[j] = used[j] || [];
            for (var i = x; i < x + w; i++) used[j][i] = true;
        }
    }
    var pos = [], rows = 0;
    for (var t = 0; t < tiles.length; t++) {
        var s = sizeOf(tiles[t].size);
        var w = Math.min(s.w, W), h = s.h;
        // Bigger tiles snap to 2x2 blocks; small tiles fill a block's 4 cells first.
        var cells = w === 1 ? [[0, 0], [1, 0], [0, 1], [1, 1]] : [[0, 0]];
        var placed = false;
        for (var y = 0; !placed; y += 2) {
            for (var x = 0; x + w <= W && !placed; x += 2) {
                for (var c = 0; c < cells.length; c++) {
                    var cx = x + cells[c][0], cy = y + cells[c][1];
                    if (free(cx, cy, w, h)) {
                        take(cx, cy, w, h);
                        pos.push({ x: cx, y: cy, w: w, h: h });
                        rows = Math.max(rows, cy + h);
                        placed = true;
                        break;
                    }
                }
            }
        }
    }
    return { pos: pos, rows: rows };
}

// First row (in pack units) under tile i that no tile crosses: an opened folder's row goes there.
function rowEnd(pos, i) {
    var end = pos[i] ? pos[i].y + pos[i].h : 0, moved = true;
    while (moved) {
        moved = false;
        for (var j = 0; j < pos.length; j++) {
            if (pos[j].y < end && pos[j].y + pos[j].h > end) { end = pos[j].y + pos[j].h; moved = true; }
        }
    }
    return end;
}

function clone(groups) {
    return JSON.parse(JSON.stringify(groups));
}

// Finds a tile, also inside folders. { g, i, list }: list is the array holding it (group or folder
// tiles); f is the folder's index in its group when the tile sits in a folder.
function findTile(groups, id) {
    for (var g = 0; g < groups.length; g++)
        for (var i = 0; i < groups[g].tiles.length; i++) {
            var t = groups[g].tiles[i];
            if (t.id === id) return { g: g, i: i, list: groups[g].tiles };
            if (t.kind === "folder")
                for (var k = 0; k < t.tiles.length; k++)
                    if (t.tiles[k].id === id) return { g: g, i: k, f: i, list: t.tiles };
        }
    return null;
}

// Folders with one tile turn back into that tile (taking the folder's place and size); empty
// folders and empty unnamed groups vanish.
function tidy(groups) {
    groups.forEach(function (grp) {
        grp.tiles = grp.tiles.filter(function (t) { return t.kind !== "folder" || t.tiles.length > 0; })
            .map(function (t) {
                if (t.kind !== "folder" || t.tiles.length > 1) return t;
                var only = t.tiles[0];
                only.size = t.size;
                return only;
            });
    });
    return groups.filter(function (x) { return x.tiles.length > 0 || x.name !== ""; });
}

function isFolder(id) {
    return typeof id === "string" && id.indexOf("folder|") === 0;
}

// Widget tiles: id "widget|<kind>|<n>" (n makes each one unique). Rendered by ui/widgets/<Kind>.qml.
var WIDGETS = ["clock", "calendar", "sysmon", "drives", "weather", "notes", "photo"];
// Real KDE widgets (child applets of our containment) are tiles "applet|<applet.id>" and count as widgets.
function isWidget(id) {
    return typeof id === "string" && (id.indexOf("widget|") === 0 || isApplet(id));
}
function isApplet(id) {
    return typeof id === "string" && id.indexOf("applet|") === 0;
}
// Add applet tile "applet|<aid>" to group gi of page p (a new group when the page has none).
function addApplet(pages, p, aid, gi) {
    var g = pages[p].groups;
    gi = Math.min(gi || 0, Math.max(0, g.length - 1));
    return setPage(pages, p, place(g, "applet|" + aid, gi, g.length ? g[gi].tiles.length : 0, "large"));
}
// Restore check: drop applet tiles whose applet is gone and repeats of one applet (first one stays);
// applets without a tile get one at the end of the first group of page 0.
function syncApplets(pages, aids) {
    var live = aids.map(String), seen = {};
    var out = clone(pages).map(function (pg) {
        pg.groups.forEach(function (grp) {
            grp.tiles = grp.tiles.filter(function (t) {
                if (!isApplet(t.id)) return true;
                var a = t.id.slice(7);
                if (live.indexOf(a) < 0 || seen[a]) return false;
                return (seen[a] = true);
            });
        });
        return { groups: tidy(pg.groups) };
    });
    live.forEach(function (a) { if (!seen[a]) out = addApplet(out, 0, a, 0); });
    return out;
}
function widgetKind(id) {
    return isWidget(id) ? id.split("|")[1] : "";
}
// A free id for a new widget of this kind (unique across all pages).
function widgetId(pages, kind) {
    var n = 1;
    while (pages.some(function (p) { return findTile(p.groups, "widget|" + kind + "|" + n); })) n++;
    return "widget|" + kind + "|" + n;
}
// Add a widget to group gi (default the first) of page p; a new group when the page has none.
function addWidget(pages, p, kind, gi) {
    var id = widgetId(pages, kind);
    var g = pages[p].groups;
    gi = gi || 0;
    return setPage(pages, p, place(g, id, gi, g.length ? g[gi].tiles.length : 0, kind === "sysmon" ? "wide" : "medium"));
}

// Drop tile (or app) id onto tile target: into target's folder, or both become a new folder in
// target's place (Windows 10 tile folders). A folder dropped on a tile is not nested: no change.
function dropOnTile(groups, id, target, size) {
    var out = clone(groups);
    if (id === target || isFolder(id)) return out;
    if (isWidget(id) || isWidget(target)) {   // widgets never join folders: just move before the target
        var w = findTile(out, target);
        return w && w.f === undefined ? place(groups, id, w.g, w.i, size) : groups;
    }
    var tile = { id: id, size: size || "medium" };
    var at = findTile(out, id);
    if (at) tile = at.list.splice(at.i, 1)[0];
    var to = findTile(out, target);
    if (!to) return groups;
    if (to.f !== undefined) {
        to.list.splice(to.i + 1, 0, tile);
    } else if (to.list[to.i].kind === "folder") {
        to.list[to.i].tiles.push(tile);
    } else {
        var n = 1;
        while (findTile(out, "folder|" + n)) n++;
        var t = to.list[to.i];
        to.list[to.i] = { kind: "folder", id: "folder|" + n, name: "", size: t.size, tiles: [t, tile] };
    }
    return tidy(out);
}

// Move (or insert, when the id is not pinned yet) a tile to group g, index i.
// g === groups.length makes a new unnamed group.
function place(groups, id, g, i, size) {
    var out = clone(groups);
    var tile = { id: id, size: size || "medium" };
    var at = findTile(out, id);
    if (at) {
        tile = at.list.splice(at.i, 1)[0];
        if (at.g === g && at.f === undefined && at.i < i) i--;
    }
    if (g >= out.length) {
        out.push({ name: "", tiles: [] });
        g = out.length - 1;
    }
    var list = out[g].tiles;
    list.splice(Math.max(0, Math.min(i, list.length)), 0, tile);
    return tidy(out);
}

function remove(groups, id) {
    var out = clone(groups);
    var at = findTile(out, id);
    if (at) at.list.splice(at.i, 1);
    return tidy(out);
}

function resize(groups, id, size) {
    var out = clone(groups);
    var at = findTile(out, id);
    if (at) at.list[at.i].size = size;
    return out;
}

function sortGroup(groups, g, names) {
    var out = clone(groups);
    out[g].tiles.sort(function (a, b) {
        return (names[a.id] || a.id).localeCompare(names[b.id] || b.id);
    });
    return out;
}

// Per-tile options: icon, caption ("show"/"hide"), color. undefined/"" removes the key.
function setProp(groups, id, key, value) {
    var out = clone(groups);
    var at = findTile(out, id);
    if (at) {
        if (value === undefined || value === "") delete at.list[at.i][key];
        else at.list[at.i][key] = value;
    }
    return out;
}

// Pinned grid (Windows 11 style): a flat list of ids. Move (or insert) id to index i.
function placeInList(list, id, i) {
    var out = list.slice();
    var at = out.indexOf(id);
    if (at >= 0) {
        out.splice(at, 1);
        if (at < i) i--;
    }
    out.splice(Math.max(0, Math.min(i, out.length)), 0, id);
    return out;
}

// Pinned grid folders (Windows 11): entry "folder|name|id1,id2,...".
// Ids are URI-encoded so a pinned url with "," survives (plain app ids look the same either way).
function folderOf(entry) {
    if (!isFolder(entry)) return null;
    var p = entry.split("|");
    var ids = p.slice(2).join("|");
    return { name: p[1], ids: ids ? ids.split(",").map(decodeURIComponent) : [] };
}

function folderEntry(name, ids) {
    return "folder|" + String(name).replace(/[|,]/g, " ") + "|" + ids.map(encodeURIComponent).join(",");
}

// Every pinned id, folders opened up.
function allPins(list) {
    var out = [];
    list.forEach(function (e) { var f = folderOf(e); out = out.concat(f ? f.ids : [e]); });
    return out;
}

// Unpin id from the list or from its folder; a folder left with one app turns back into it.
function removePin(list, id) {
    var out = [];
    list.forEach(function (e) {
        var f = folderOf(e);
        if (!f) { if (e !== id) out.push(e); return; }
        var ids = f.ids.filter(function (x) { return x !== id; });
        if (ids.length > 1) out.push(folderEntry(f.name, ids));
        else if (ids.length === 1) out.push(ids[0]);
    });
    return out;
}

// Drop id onto list entry target: into target's folder, or both become a new folder in target's
// place. Folders do not nest.
function placeInFolder(list, id, target, name) {
    if (id === target || isFolder(id) || list.indexOf(target) < 0) return list;
    var f = folderOf(target);
    var out = removePin(list, id);
    var i = out.indexOf(target);
    if (i < 0) return list;
    out[i] = f ? folderEntry(f.name, f.ids.concat([id])) : folderEntry(name || "Folder", [target, id]);
    return out;
}

// App canvas user tabs (key canvasLayout): { tabs: [{ name, groups: [{ name, colour, ids }] }] }.
// colour "" = automatic hue. All helpers return a new object.
function parseCanvas(json) {
    try {
        var c = JSON.parse(json);
        if (c && Array.isArray(c.tabs)) return c;
    } catch (e) {}
    return { tabs: [] };
}
function canvasAddTab(c, name) {
    c = clone(c);
    c.tabs.push({ name: name, groups: [] });
    return c;
}
function canvasDeleteTab(c, t) {
    c = clone(c);
    c.tabs.splice(t, 1);
    return c;
}
function canvasMoveTab(c, t, d) {
    if (t + d < 0 || t + d >= c.tabs.length) return c;
    c = clone(c);
    c.tabs.splice(t + d, 0, c.tabs.splice(t, 1)[0]);
    return c;
}
function canvasAddGroup(c, t, name) {
    c = clone(c);
    c.tabs[t].groups.push({ name: name, colour: "", ids: [] });
    return c;
}
function canvasDeleteGroup(c, t, g) {
    c = clone(c);
    c.tabs[t].groups.splice(g, 1);
    return c;
}
// g < 0 renames the tab itself.
function canvasRename(c, t, g, name) {
    c = clone(c);
    (g < 0 ? c.tabs[t] : c.tabs[t].groups[g]).name = name;
    return c;
}
// Puts app id into group g of tab t before app "before" (end when absent) and out of the tab's
// other groups. g past the last group makes a new group named "name".
function canvasPlace(c, t, id, g, before, name) {
    if (id === before) return c;
    c = clone(c);
    var gs = c.tabs[t].groups;
    if (g >= gs.length) {
        gs.push({ name: name || "", colour: "", ids: [] });
        g = gs.length - 1;
    }
    gs.forEach(function (x) { var k = x.ids.indexOf(id); if (k >= 0) x.ids.splice(k, 1); });
    var i = gs[g].ids.indexOf(before);
    gs[g].ids.splice(i < 0 ? gs[g].ids.length : i, 0, id);
    return c;
}
function canvasRemove(c, t, id) {
    c = clone(c);
    c.tabs[t].groups.forEach(function (x) { x.ids = x.ids.filter(function (y) { return y !== id; }); });
    return c;
}
// Group settings (T16): colour, tint ("" | "icons" | "auto"), rows (0 = all), iconSize (0 = default),
// texture (0 = by position, -1 = none, 1..24), folder (url: the group lists that folder's files).
function canvasSet(c, t, g, key, value) {
    c = clone(c);
    c.tabs[t].groups[g][key] = value;
    return c;
}
// Search history: newest first, no duplicates, at most 10.
function pushHistory(list, q) {
    q = String(q || "").trim();
    if (!q) return Array.from(list);
    return [q].concat(Array.from(list).filter(function (x) { return x !== q; })).slice(0, 10);
}
// Resort A-Z by names { id: name }; ids without a name keep their order at the end.
function canvasSort(c, t, g, names) {
    c = clone(c);
    var x = c.tabs[t].groups[g];
    var named = x.ids.filter(function (id) { return names[id] !== undefined; });
    named.sort(function (a, b) { return String(names[a]).localeCompare(String(names[b])); });
    x.ids = named.concat(x.ids.filter(function (id) { return names[id] === undefined; }));
    return c;
}
// Theme icon name for a file of a folder group (FolderListModel has no mime type).
function fileIcon(name, isDir) {
    if (isDir) return "folder";
    var ext = String(name).toLowerCase().split(".").pop();
    var map = { pdf: "application-pdf", png: "image-x-generic", jpg: "image-x-generic", jpeg: "image-x-generic",
                svg: "image-x-generic", gif: "image-x-generic", webp: "image-x-generic", mp3: "audio-x-generic",
                ogg: "audio-x-generic", flac: "audio-x-generic", wav: "audio-x-generic", mp4: "video-x-generic",
                mkv: "video-x-generic", webm: "video-x-generic", zip: "package-x-generic", gz: "package-x-generic",
                xz: "package-x-generic", tar: "package-x-generic", txt: "text-plain", md: "text-plain",
                sh: "text-x-script", py: "text-x-script", js: "text-x-script", html: "text-html",
                odt: "x-office-document", docx: "x-office-document", ods: "x-office-spreadsheet",
                xlsx: "x-office-spreadsheet", desktop: "application-x-desktop" };
    return map[ext] || "text-x-generic";
}

// Filtered folders (key filteredFolders): [{ name, path, include, exclude }], regexes over file names.
function parseFiltered(json) {
    try {
        var a = JSON.parse(json || "[]");
        return Array.isArray(a) ? a.filter(function (f) { return f && f.name && f.path; }) : [];
    } catch (e) { return []; }
}
// A bad regex counts as no filter.
function filterFile(f, name) {
    function test(re) { try { return new RegExp(re, "i").test(name); } catch (e) { return null; } }
    if (f.include && test(f.include) === false) return false;
    return !(f.exclude && test(f.exclude) === true);
}
// "~/x", "/x" or "file:///x" -> file url; home = file url of the home folder.
function dirUrl(path, home) {
    path = String(path || "");
    if (path.startsWith("file:")) return path;
    if (path === "~" || path.startsWith("~/")) return String(home).replace(/\/$/, "") + path.slice(1);
    return "file://" + path;
}

// Move group "from" so it ends up before the group now at "to" (to === length: last).
function moveGroup(groups, from, to) {
    var out = clone(groups);
    if (from < 0 || from >= out.length) return out;
    var g = out.splice(from, 1)[0];
    if (from < to) to--;
    out.splice(Math.max(0, Math.min(to, out.length)), 0, g);
    return out;
}

// Pinned websites, files and folders use the rail's custom id: "custom|name|icon|target".
function custom(id) {
    if (typeof id !== "string" || id.indexOf("custom|") !== 0) return null;
    var p = id.split("|");
    return { id: id, name: p[1], icon: p[2], target: p.slice(3).join("|") };
}

// ponytail: icon by file extension, not a real mime lookup (QML has no KIO mime API).
var EXT_ICONS = {
    pdf: "application-pdf", txt: "text-plain", md: "text-markdown", html: "text-html",
    png: "image-x-generic", jpg: "image-x-generic", jpeg: "image-x-generic", gif: "image-x-generic",
    svg: "image-svg+xml", webp: "image-x-generic", mp3: "audio-x-generic", ogg: "audio-x-generic",
    flac: "audio-x-generic", wav: "audio-x-generic", mp4: "video-x-generic", mkv: "video-x-generic",
    webm: "video-x-generic", zip: "application-zip", tar: "application-x-tar", gz: "application-x-compressed-tar",
    odt: "x-office-document", docx: "x-office-document", doc: "x-office-document",
    ods: "x-office-spreadsheet", xlsx: "x-office-spreadsheet", odp: "x-office-presentation", pptx: "x-office-presentation",
    sh: "application-x-shellscript", py: "text-x-python", js: "text-x-javascript", iso: "application-x-cd-image"
};

// Build the custom id for a url. kind: "web", "file" or "folder".
function customFor(url, kind, favicons) {
    var name, icon;
    if (kind === "web") {
        var host = (url.match(/^\w+:\/\/([^\/:?#]+)/) || [null, url])[1];
        name = host.replace(/^www\./, "");
        icon = favicons === false ? "internet-web-browser" : "https://icons.duckduckgo.com/ip3/" + host + ".ico";
    } else {
        var path = url.replace(/\/+$/, "");
        name = decodeURIComponent(path.slice(path.lastIndexOf("/") + 1)) || "/";
        var dot = name.lastIndexOf(".");
        icon = kind === "folder" ? "folder" : (dot > 0 && EXT_ICONS[name.slice(dot + 1).toLowerCase()]) || "text-x-generic";
    }
    return ["custom", name.replace(/\|/g, "/"), icon, url].join("|");
}

// .desktop text for a custom pin on the taskbar (T32): Task Manager launchers only take .desktop files.
// Exec args are double-quoted; per the spec the quote escape's backslash is itself escaped (\\"), % is %%.
function pinDesktop(c) {
    var arg = function (s) { return '"' + String(s).replace(/[\\"`$]/g, "\\\\$&").replace(/%/g, "%%").replace(/\n/g, " ") + '"'; };
    var exec = c.target.indexOf("cmd:") === 0 ? "sh -c " + arg(c.target.slice(4)) : "xdg-open " + arg(c.target);
    var icon = /^https?:/.test(c.icon) ? "internet-web-browser" : c.icon;
    return "[Desktop Entry]\nType=Application\nName=" + c.name.replace(/\n/g, " ") + "\nIcon=" + icon
        + "\nExec=" + exec + "\nNoDisplay=true\n";
}

// Keyboard arrows: index of the rect nearest to rects[cur] in direction (dx, dy), -1 if none.
// Rects are {x, y, width, height}; sideways distance counts double. cur < 0: the top-left rect.
function nearest(rects, cur, dx, dy) {
    var best = -1, bestScore = Infinity;
    for (var i = 0; i < rects.length; i++) {
        var r = rects[i];
        var score;
        if (cur < 0) {
            score = r.y * 10000 + r.x;
        } else {
            if (i === cur) continue;
            var a = rects[cur];
            var ox = r.x + r.width / 2 - a.x - a.width / 2;
            var oy = r.y + r.height / 2 - a.y - a.height / 2;
            var along = ox * dx + oy * dy;
            if (along < 1) continue;
            score = along + 2 * (Math.abs(ox * dy) + Math.abs(oy * dx));
        }
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

// Backup/restore (T13): quote any text for /bin/sh, and the command that writes text to a file.
// Shell snippet resolving the Qt6 qdbus binary (name differs per distro).
var QDBUS = "$(command -v qdbus6 || command -v qdbus-qt6 || { [ -x /usr/lib/qt6/bin/qdbus ] && echo /usr/lib/qt6/bin/qdbus; } || echo qdbus)";
// Spec 11.2: prints "ok" on Win12's patched KDE (kde/build.sh). One check, two users: main.qml picks the engine
// (fast or one step per pass), the Taskbar page shows its note. No pacman (another distro) = stock. The marker file
// forces stock on a patched PC (the dev PC's live test of the stock path; not a user setting).
var PATCH_CHECK = "pacman -T libplasma-win12 plasma-workspace-win12 plasma-desktop-win12 >/dev/null 2>&1"
    + " && [ ! -e \"${XDG_CONFIG_HOME:-$HOME/.config}/win12-force-stock\" ] && echo ok";
// Spec 12.3: prints "ok" with Win12's kwin (kde/build.sh kwin; readable glass layer B). Kept apart from PATCH_CHECK:
// layouts never depend on kwin. Only the Appearance page reads it (its note); main.qml writes the keys either way.
var KWIN_CHECK = "pacman -T kwin-win12 >/dev/null 2>&1 && echo ok";

function shQuote(s) {
    return "'" + String(s).replace(/'/g, "'\\''") + "'";
}
function writeFileCmd(text, file) {
    return "sh -c " + shQuote("printf %s " + shQuote(text) + " > " + shQuote(file));
}

// Design mode dividers (T40): key `splits` = JSON { "<style>" or "<style>/<part>": width in unscaled px }.
function splitGet(json, name) {
    try { const v = JSON.parse(json || "{}")[name]; return typeof v === "number" ? v : null; } catch (e) { return null; }
}
// w null removes the entry.
function splitSet(json, name, w) {
    let m = {};
    try { m = JSON.parse(json || "{}") || {}; } catch (e) {}
    if (w === null) delete m[name];
    else m[name] = Math.round(w);
    return JSON.stringify(m);
}
// Reset one style: drops "<style>" and every "<style>/<part>".
function splitReset(json, style) {
    let m = {};
    try { m = JSON.parse(json || "{}") || {}; } catch (e) {}
    for (const k of Object.keys(m)) {
        if (k === style || k.startsWith(style + "/")) delete m[k];
    }
    return JSON.stringify(m);
}
