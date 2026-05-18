# BodyComp iOS App

A Flutter companion app for the ESP32 Body Composition Analyzer, targeted at
**iPhone** for demos. Pairs over BLE with the firmware in `../BodyComp`, sends
the user profile, triggers a measurement, and displays weight, height,
impedance, body-fat %, fat-free mass, and BMI.

## Requirements

You need a **Mac** to build for iOS (Apple toolchain restriction).

- macOS with Xcode 15+ installed (and command-line tools)
- Flutter 3.10+ — `brew install --cask flutter`
- CocoaPods — `sudo gem install cocoapods` (or `brew install cocoapods`)
- An Apple ID (free signing is fine for sideload demos)
- A Lightning / USB-C cable for the iPhone
- iPhone running iOS 12 or later

## One-time setup

From this `MobileApp` directory:

```bash
# 1. Scaffold the iOS Xcode project (and Android, optional)
flutter create --project-name body_comp_app --platforms=ios,android \
  --org com.bodycomp .

# 2. Patch Info.plist with Bluetooth usage strings + bump Podfile to iOS 12
./ios_setup.sh

# 3. Resolve pub + CocoaPods deps
flutter pub get
(cd ios && pod install)
```

## Signing the app for your iPhone

1. Open the workspace in Xcode:

   ```bash
   open ios/Runner.xcworkspace
   ```

2. Select the **Runner** project → **Signing & Capabilities** tab.
3. Check **Automatically manage signing** and pick your **Team** (your free
   personal Apple ID team works).
4. Change the **Bundle Identifier** to something unique, e.g.
   `com.bodycomp.<your-name>.app` — Apple rejects duplicates.
5. Plug in the iPhone, trust the Mac on the phone, then in Xcode select your
   device as the run target.

## Running

```bash
flutter run -d <your-iphone-name>
# or just:
flutter run    # and pick the iPhone from the list
```

First launch on the phone: go to **Settings → General → VPN & Device
Management** and trust your developer profile. Then re-open the app.

iOS will prompt for **Bluetooth** permission the first time the app scans —
accept it.

## App flow

1. **Scan** — filters BLE advertisements for the BodyComp service UUID and
   shows matching devices. Tap *Connect*.
2. **Profile** — enter age, pick sex, hit *Start Measurement*. The app writes
   the profile, then writes `0x01` to the start characteristic. Status
   notifications drive a live progress bar.
3. **Results** — when the firmware sends the result notification, all five
   metrics (+ derived BMI) render. Run another measurement or disconnect.

## BLE contract

Matches `BodyComp/main/ble_service.c`:

| Characteristic | UUID | Op | Payload |
|---|---|---|---|
| Service       | `12345678-1234-5678-1234-567812345678` | — | — |
| User Profile  | `…5601` | R/W | `[age:u8, sex:u8]` (0 = female, 1 = male) |
| Start         | `…5602` | W   | `0x01` |
| Status        | `…5603` | R / Notify | `[status:u8, error:u8, progress:u8]` |
| Result        | `…5604` | R / Notify | 5 × little-endian `float32`: weight kg, height cm, Z Ω, BF %, FFM kg |

Advertised device name: **`BodyComp`**.

> **iOS note:** iOS does not expose the peripheral's MAC address. Devices are
> identified by an opaque per-app UUID; that's what shows under the device
> name on the scan screen. This is normal and doesn't affect the demo.

## Demo-day checklist

- [ ] ESP32 powered on and advertising (look for `BodyComp` in iOS Settings →
      Bluetooth, but **do not pair from there** — let the app connect).
- [ ] iPhone Bluetooth enabled, app permission granted.
- [ ] Provisioning profile still valid (free Apple ID profiles expire after
      7 days — re-run `flutter run` from the Mac to refresh).
- [ ] Cable / charger in case Xcode needs to re-sign mid-demo.

## Files

- `lib/main.dart` — app entry
- `lib/ble/ble_constants.dart` — UUIDs, status enum, sex enum
- `lib/ble/body_comp_service.dart` — connect / discover / read / write / notify
- `lib/screens/scan_screen.dart` — BLE scan + connect
- `lib/screens/profile_screen.dart` — input + live status
- `lib/screens/results_screen.dart` — metric cards
- `ios_setup.sh` — patches Info.plist + Podfile after `flutter create`
