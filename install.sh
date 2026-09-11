#!/usr/bin/env bash
# Build, install and optionally enable the Better Transition KWin effect.
set -euo pipefail

cd "$(dirname "$0")"

ENABLE=0
for arg in "$@"; do
    case "$arg" in
        --enable) ENABLE=1 ;;
        -h|--help)
            echo "Usage: $0 [--enable]"
            echo "  --enable   also write bettertransitionEnabled=true and reload effects in the running KWin"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

./build.sh

echo "==> Installing to /usr (sudo)"
sudo cmake --install build

cat <<'EOF'

Installed:
  /usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/bettertransition.so
  /usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/configs/kwin_bettertransition_config.so

Enable it in "System Settings -> Desktop Effects" (search "Better Transition"), or run:

  kwriteconfig6 --file kwinrc --group Plugins --key bettertransitionEnabled true
  qdbus6 org.kde.KWin /KWin reconfigure
EOF

if [ "$ENABLE" -eq 1 ]; then
    echo
    echo "==> Enabling bettertransition in the running KWin session"
    kwriteconfig6 --file kwinrc --group Plugins --key bettertransitionEnabled true
    qdbus6 org.kde.KWin /KWin reconfigure || true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect bettertransition || true
    echo "Done. Check 'System Settings -> Desktop Effects' to confirm it is active."
fi
