# KDE Store page (store.kde.org / pling)

Create the product at https://store.kde.org (log in → *Add Product*).

- **Category:** Plasma 6 Applets / Application Launchers
- **Name:** Win12
- **Summary:** Start11-style start menu for KDE Plasma 6, nine menu styles with matching taskbar and windows
- **License:** MIT
- **Link / source:** https://github.com/Kalaminator/Win12
- **Description:**

  > A start menu for KDE Plasma 6 inspired by Stardock Start11. Nine menu styles: Windows 10 (live-tile
  > style groups), Windows 11, Windows 12, Windows 7, XP, Modern, App, Pro and Launcher, all switchable from one settings
  > window. Optional matching taskbar style, title bars, window colours and window glass (transparency, blur,
  > rounded corners) that are undone when you turn them off. Pinned apps, websites, files and folders,
  > grouped search, keyboard navigation, custom Start button, transparency and textures. Translated into
  > Spanish, German, French, Italian, Portuguese (Brazil), Russian, Arabic, Hindi, Japanese and Chinese.
  >
  > Needs python3 (generates the taskbar styles, title bars and colour schemes on first start) and the usual
  > KDE tools (qdbus6, kreadconfig6, kwriteconfig6). Not affiliated with Stardock.

- **Screenshots:** the images from the README.

## Upload a new version

1. Bump `KPlugin.Version` in `package/metadata.json` (e.g. 0.1 → 0.2) and commit. Discover only offers an
   update when the store file version is higher, and the widget re-runs its setup (`code/setup.sh`) when the
   version changes.
2. `./tools/make-release.sh` → `dist/org.win12.menu-<version>.plasmoid` (only git-tracked files; it refuses
   to build if Stardock assets would be inside).
3. On the product page: *Edit* → *Files* → upload the new `.plasmoid`, set its version to the same number,
   and archive (hide) the old file so only one is offered.
4. Optional: `git tag v<version>` and push.
