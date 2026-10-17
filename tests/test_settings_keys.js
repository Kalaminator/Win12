#!/usr/bin/env node
// Settings reorganisation guard: every main.xml key that has a control on a settings page keeps exactly one.
// A `property <type|alias> cfg_<key>` line on a page = that page edits the key, unless the line says `read only`
// (pages declare the style keys read-only to gate rows; Plasma fills every cfg_ property a page declares).
//   node tests/test_settings_keys.js             LOST keys fail; duplicates are listed (allowed mid-reorg)
//   node tests/test_settings_keys.js --strict    duplicates fail too (install.sh, final state)
//   node tests/test_settings_keys.js --baseline  rewrite tests/settings-keys.txt from the current pages
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "..");
const xml = fs.readFileSync(path.join(root, "package/contents/config/main.xml"), "utf8");
const keys = [...xml.matchAll(/<entry name="([A-Za-z0-9_]+)"/g)].map(m => m[1]);
// resetRequest is a request channel, not a setting: Start menu ("positions") and Advanced ("backup|restore|all") both write it.
const sharedRequests = ["resetRequest"];
const dir = path.join(root, "package/contents/ui/config");
const owners = {};
for (const f of fs.readdirSync(dir).filter(f => f.endsWith(".qml"))) {
    for (const line of fs.readFileSync(path.join(dir, f), "utf8").split("\n")) {
        const m = line.match(/^\s*property\s+\w+\s+cfg_([A-Za-z0-9_]+)/);
        if (!m || /read only/.test(line)) continue;
        (owners[m[1]] = owners[m[1]] || []).push(f);
    }
}
const edited = keys.filter(k => owners[k]);
const baselineFile = path.join(__dirname, "settings-keys.txt");
if (process.argv.includes("--baseline")) {
    fs.writeFileSync(baselineFile, edited.join("\n") + "\n");
    console.log(`settings keys: baseline written, ${edited.length} keys`);
    process.exit(0);
}
const baseline = fs.readFileSync(baselineFile, "utf8").split("\n").filter(Boolean);
const lost = baseline.filter(k => !owners[k]);
const dups = edited.filter(k => new Set(owners[k]).size > 1 && !sharedRequests.includes(k));
const unknown = Object.keys(owners).filter(k => !keys.includes(k));
const strict = process.argv.includes("--strict");
if (lost.length) console.log("LOST (had a control, none now): " + lost.join(" "));
if (dups.length) console.log(`${dups.length} duplicates: ` + dups.map(k => `${k} (${[...new Set(owners[k])].join(", ")})`).join("; "));
if (unknown.length) console.log("cfg_ without a main.xml key: " + unknown.join(" "));
const fail = lost.length || unknown.length || (strict && dups.length);
console.log(fail ? "settings keys FAIL" : `settings keys OK (${edited.length} keys${dups.length ? ", duplicates allowed" : ""})`);
process.exit(fail ? 1 : 0);
