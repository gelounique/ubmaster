#!/bin/bash
# UBMaster — macOS installer: .pkg (VST3 + AU [+ AAX] + App, wählbar) in einer .dmg mit Deinstaller
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$HERE/../.."
ART="${1:-$ROOT/build/UBMaster_artefacts/Release}"
VER="${2:-1.0.0}"
OUT="$ROOT/dist"; WORK="$ROOT/build/pkg"
rm -rf "$WORK"; mkdir -p "$WORK"/{vst3,au,aax,app,pkgs,dmg} "$OUT"

cp -R "$ART/VST3/UBMaster.vst3" "$WORK/vst3/"
cp -R "$ART/AU/UBMaster.component" "$WORK/au/"
cp -R "$ART/Standalone/UBMaster.app" "$WORK/app/"
HAS_AAX=0
if [ -d "$ART/AAX/UBMaster.aaxplugin" ]; then cp -R "$ART/AAX/UBMaster.aaxplugin" "$WORK/aax/"; HAS_AAX=1; fi

# ad-hoc signature (keine Developer-ID vorhanden) – nötig für Apple Silicon. AAX bleibt unangetastet (PACE-Signatur).
for b in "$WORK/vst3/UBMaster.vst3" "$WORK/au/UBMaster.component" "$WORK/app/UBMaster.app"; do
  codesign --force --deep --sign - --timestamp=none "$b"
done

chmod +x "$HERE/scripts/postinstall"

noreloc () {
  pkgbuild --analyze --root "$1" "$2"
  i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$2" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$2" 2>/dev/null || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$2"
    i=$((i+1))
  done
  echo "  $2: $i bundle(s) non-relocatable"
}
build_pkg () {   # name dir id location
  noreloc "$WORK/$2" "$WORK/$2.plist"
  pkgbuild --root "$WORK/$2" --component-plist "$WORK/$2.plist" --identifier "$3" --version "$VER" \
    --scripts "$HERE/scripts" --install-location "$4" "$WORK/pkgs/$1.pkg"
}
build_pkg UBMaster-VST3 vst3 de.uniquebaby.ubmaster.vst3 "/Library/Audio/Plug-Ins/VST3"
build_pkg UBMaster-AU   au   de.uniquebaby.ubmaster.au   "/Library/Audio/Plug-Ins/Components"
build_pkg UBMaster-App  app  de.uniquebaby.ubmaster.app  "/Applications"
[ "$HAS_AAX" = 1 ] && build_pkg UBMaster-AAX aax de.uniquebaby.ubmaster.aax "/Library/Application Support/Avid/Audio/Plug-Ins"

python3 - "$HERE/distribution.xml" "$WORK/distribution.xml" "$VER" "$HAS_AAX" <<'PY'
import sys
src, dst, ver, aax = sys.argv[1:5]
s = open(src).read().replace("__VERSION__", ver)
if aax != "1":
    import re
    s = re.sub(r"\s*<!--AAX-->.*?<!--/AAX-->", "", s, flags=re.S)
open(dst, "w").write(s)
PY
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" \
  --package-path "$WORK/pkgs" "$WORK/dmg/UBMaster Installer.pkg"

cp "$HERE/UBMaster deinstallieren.command" "$WORK/dmg/"
chmod +x "$WORK/dmg/UBMaster deinstallieren.command"
cp "$HERE/LIES MICH.txt" "$WORK/dmg/"

rm -f "$OUT/UBMaster-macOS.dmg"
hdiutil create -volname "UBMaster $VER" -srcfolder "$WORK/dmg" -ov -format UDZO "$OUT/UBMaster-macOS.dmg"
shasum -a 256 "$OUT/UBMaster-macOS.dmg" | tee "$OUT/UBMaster-macOS.dmg.sha256"
