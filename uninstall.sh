#!/usr/bin/env bash
# Uninstall the Better Transition KWin effect.
set -euo pipefail

cd "$(dirname "$0")"

EFFECT_ID="bettertransition"
CONFIG_MODULE="kwin_${EFFECT_ID}_config"
CONFIG_GROUP="Effect-${EFFECT_ID}"
CONFIG_KEYS=(
    FadeOutDuration
    HoldDuration
    FadeInDuration
    FadeStrength
    OnlyOverlapping
    IncludeSpecialWindows
    LargeCovererScreenRatio
    ScaleInThreshold
    ScaleInDuration
    ScaleInStartScale
)

echo "==> Disabling ${EFFECT_ID} in kwinrc"
if command -v kwriteconfig6 >/dev/null 2>&1; then
    kwriteconfig6 --file kwinrc --group Plugins --key "${EFFECT_ID}Enabled" false || true
fi
if command -v qdbus6 >/dev/null 2>&1; then
    qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
fi

if [ -f build/install_manifest.txt ]; then
    echo "==> Removing installed files recorded in build/install_manifest.txt"
    sudo xargs -r -a build/install_manifest.txt rm -v
else
    echo "==> build/install_manifest.txt not found; removing the known plugin locations"
    for plugin_dir in \
        /usr/lib/x86_64-linux-gnu/qt6/plugins \
        /usr/lib/qt6/plugins \
        /usr/lib64/qt6/plugins ; do
        sudo rm -vf \
            "${plugin_dir}/kwin/effects/plugins/${EFFECT_ID}.so" \
            "${plugin_dir}/kwin/effects/configs/${CONFIG_MODULE}.so" 2>/dev/null || true
    done
fi

if [ "${1:-}" = "--purge" ]; then
    echo "==> Deleting the ${CONFIG_GROUP} configuration group"
    if command -v kwriteconfig6 >/dev/null 2>&1; then
        for key in "${CONFIG_KEYS[@]}"; do
            kwriteconfig6 --file kwinrc --group "${CONFIG_GROUP}" --key "${key}" --delete || true
        done
    fi
fi

cat <<EOF

Removed. If the "Desktop Effects" list still shows the entry, log out and back in.

Configuration group [${CONFIG_GROUP}] in ~/.config/kwinrc was left untouched;
run "./uninstall.sh --purge" to delete it as well.
EOF
