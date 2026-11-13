#!/bin/bash
# UBMaster — Deinstaller (entfernt VST3, AU, AAX und App)
echo "UBMaster wird entfernt … (Admin-Passwort nötig)"
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/UBMaster.vst3" \
            "/Library/Audio/Plug-Ins/Components/UBMaster.component" \
            "/Library/Application Support/Avid/Audio/Plug-Ins/UBMaster.aaxplugin" \
            "/Applications/UBMaster.app" \
            "$HOME/Library/Audio/Plug-Ins/VST3/UBMaster.vst3" \
            "$HOME/Library/Audio/Plug-Ins/Components/UBMaster.component"
for id in de.uniquebaby.ubmaster.vst3 de.uniquebaby.ubmaster.au de.uniquebaby.ubmaster.aax de.uniquebaby.ubmaster.app; do sudo pkgutil --forget "$id" >/dev/null 2>&1; done
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Fertig – UBMaster ist deinstalliert. Fenster kann geschlossen werden."
