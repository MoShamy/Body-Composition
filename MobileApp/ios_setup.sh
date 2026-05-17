#!/usr/bin/env bash
# Run AFTER `flutter create --platforms=ios .`
# Patches ios/Runner/Info.plist with Bluetooth usage descriptions and
# bumps the iOS deployment target in Podfile to 12.0 (required by flutter_blue_plus).
set -euo pipefail

PLIST="ios/Runner/Info.plist"
PODFILE="ios/Podfile"

if [[ ! -f "$PLIST" ]]; then
  echo "ERROR: $PLIST not found. Run 'flutter create --platforms=ios .' first." >&2
  exit 1
fi

# PlistBuddy ships with Xcode command line tools on macOS.
PB=/usr/libexec/PlistBuddy

add_string() {
  local key="$1" value="$2"
  if $PB -c "Print :$key" "$PLIST" >/dev/null 2>&1; then
    $PB -c "Set :$key $value" "$PLIST"
  else
    $PB -c "Add :$key string $value" "$PLIST"
  fi
}

add_string "NSBluetoothAlwaysUsageDescription" \
  "Connects to the BodyComp scale to receive your body composition measurements."
add_string "NSBluetoothPeripheralUsageDescription" \
  "Connects to the BodyComp scale to receive your body composition measurements."
add_string "UIBackgroundModes" "" 2>/dev/null || true
# Allow background BLE (optional, useful if the demo is interrupted)
if ! $PB -c "Print :UIBackgroundModes" "$PLIST" >/dev/null 2>&1; then
  $PB -c "Add :UIBackgroundModes array" "$PLIST"
fi
if ! $PB -c "Print :UIBackgroundModes" "$PLIST" 2>/dev/null | grep -q bluetooth-central; then
  $PB -c "Add :UIBackgroundModes: string bluetooth-central" "$PLIST"
fi

echo "Info.plist patched."

# Bump Podfile platform :ios, '12.0'
if [[ -f "$PODFILE" ]]; then
  if grep -qE "^\s*#?\s*platform :ios" "$PODFILE"; then
    sed -i.bak -E "s/^\s*#?\s*platform :ios.*/platform :ios, '12.0'/" "$PODFILE"
    rm -f "$PODFILE.bak"
    echo "Podfile platform set to iOS 12.0."
  else
    echo "WARNING: could not find 'platform :ios' line in Podfile - edit manually." >&2
  fi
fi

echo "Done. Next: (cd ios && pod install) && flutter run"
