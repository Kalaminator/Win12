#!/bin/sh
# Extract strings into po/template.pot and merge into every po/*.po
cd "$(dirname "$0")/.."
mkdir -p explorer/po
xgettext -L C++ --from-code=UTF-8 --keyword=i18n --keyword=i18nc:1c,2 --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3 \
  -o explorer/po/template.pot $(find explorer/src -name '*.cpp' | sort)
xgettext -L JavaScript --from-code=UTF-8 --keyword=i18n:1 --keyword=i18nc:1c,2 --keyword=i18np:1,2 \
  -o po/template.pot $(find package/contents -name '*.qml' -o -name '*.js' | sort)
for f in po/*.po; do msgmerge -q -U --backup=none "$f" po/template.pot; done
