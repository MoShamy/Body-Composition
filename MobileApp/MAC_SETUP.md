# Running the BodyComp App on iPhone (from a Mac)

A copy-paste walkthrough. Follow top to bottom — every step assumes the
previous one succeeded.

---

## 0. What you need before you start

- A Mac running macOS 13 (Ventura) or newer
- An iPhone running iOS 12 or newer
- A USB / Lightning cable that connects the iPhone to the Mac
- An Apple ID (a normal free one is fine)
- The ESP32 BodyComp board, powered on and advertising
- About 30 minutes the first time (mostly Xcode + CocoaPods downloads)

---

## 1. Install the toolchain (one-time, ~20 min)

Open **Terminal** on the Mac and run these in order.

### 1.1 Install Xcode

```bash
xcode-select --install        # command-line tools
```

Then install full **Xcode** from the Mac App Store (free, ~10 GB). Open it
once after install so it can finish setup, accept the license, and install
additional components when it asks.

Verify:

```bash
xcodebuild -version           # should print Xcode 15.x or newer
```

### 1.2 Install Homebrew (if you don't have it)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 1.3 Install Flutter and CocoaPods

```bash
brew install --cask flutter
brew install cocoapods
```

Verify the install is healthy:

```bash
flutter doctor
```

You want green checkmarks for **Flutter**, **Xcode**, and **Connected
device** (the last one will be empty until you plug in the iPhone — that's
fine for now). Ignore the Android line if you don't care about Android.

---

## 2. Get the project onto the Mac

```bash
git clone https://github.com/MoShamy/Body-Composition.git
cd Body-Composition/MobileApp
```

(If you already have the repo, just `cd` into `Body-Composition/MobileApp`.)

---

## 3. One-time iOS project scaffold (~5 min)

```bash
# Generate the ios/ and android/ platform folders
flutter create --project-name body_comp_app --platforms=ios,android \
  --org com.bodycomp .

# Patch Info.plist with Bluetooth usage strings + set iOS 12 deployment target
./ios_setup.sh

# Resolve Dart and iOS dependencies
flutter pub get
cd ios && pod install && cd ..
```

If `pod install` fails complaining about the platform line, open `ios/Podfile`
and make sure the very first non-comment line reads exactly:

```ruby
platform :ios, '12.0'
```

then re-run `cd ios && pod install && cd ..`.

---

## 4. Configure code signing in Xcode (one-time, ~3 min)

```bash
open ios/Runner.xcworkspace
```

> **Use `Runner.xcworkspace`, NOT `Runner.xcodeproj`.** Pods only show up in
> the workspace.

In Xcode:

1. In the left sidebar, click the blue **Runner** project at the top.
2. In the editor panel, select the **Runner** target.
3. Open the **Signing & Capabilities** tab.
4. Check **Automatically manage signing**.
5. Click the **Team** dropdown → **Add an Account…** → sign in with your
   Apple ID. Then pick that team in the dropdown.
6. Change **Bundle Identifier** from the default to something unique, e.g.
   `com.bodycomp.<yourname>.app`. Apple rejects duplicates.

Close Xcode when done.

---

## 5. Plug in the iPhone

1. Connect the iPhone with the cable.
2. On the iPhone, when it asks **"Trust This Computer?"** → tap **Trust**
   and enter your passcode.
3. Confirm the Mac sees it:

```bash
flutter devices
```

You should see a line like:

```
iPhone (mobile) • 00008110-001A1D... • ios • iOS 17.x
```

If it doesn't appear, unplug, re-plug, re-tap Trust.

---

## 6. Power on the ESP32

1. Power the BodyComp board.
2. On a laptop you can monitor logs from, you should see lines like
   `BLE advertising started` from the `ble_service` tag.
3. **Do NOT pair the BodyComp from iOS Settings → Bluetooth.** Let the app
   do the connection.

---

## 7. Run the app

From the `MobileApp` folder:

```bash
flutter run
```

Pick the iPhone if it prompts. First build takes a few minutes; subsequent
builds are seconds.

### First-launch on the iPhone

The app will install but **iOS will refuse to open it** until you trust the
developer profile:

1. On the iPhone: **Settings → General → VPN & Device Management**.
2. Tap your Apple ID under **Developer App**.
3. Tap **Trust "<your Apple ID>"** → **Trust**.
4. Open the app from the home screen.

iOS will then prompt for **Bluetooth** permission — tap **Allow**.

---

## 8. Test the full flow

1. **Scan screen** → tap **Scan**. A `BodyComp` device should appear within
   ~8 s. On iOS the subtitle shows an opaque UUID instead of a MAC address —
   that's normal.
2. Tap **Connect**. You'll land on the **Profile** screen.
3. Enter an **age**, pick **Male** or **Female**, tap **Start Measurement**.
4. Watch the **Status** line and progress bar update as the ESP32 reports
   `measuring → done`.
5. The app will auto-navigate to the **Results** screen showing weight,
   height, BMI, impedance, body-fat %, and fat-free mass.
6. Tap **New Measurement** to repeat, or **Disconnect** to return to scan.

### Quick sanity check without the hardware

If the ESP32 isn't around, you can still smoke-test the app:

1. Install **LightBlue** or **nRF Connect** on a second phone or a Mac.
2. Use it to advertise a custom service with UUID
   `12345678-1234-5678-1234-567812345678` and characteristics
   `…5601…5604` as listed in the README.
3. The BodyComp app should see it in the scan list and connect.

---

## 9. Common problems

| Symptom | Fix |
|---|---|
| `flutter run` errors `CocoaPods could not find compatible versions for pod "..."` | `cd ios && pod repo update && pod install && cd ..` |
| Xcode error: *"No profiles for 'com.bodycomp...' were found"* | Pick a Team in Signing & Capabilities and change the Bundle ID to something unique. |
| App installs but won't open (iOS shows "Untrusted Developer") | Settings → General → VPN & Device Management → Trust the developer profile. |
| Scan list stays empty | Confirm ESP32 logs say *advertising started*. Make sure iPhone Bluetooth is ON. Force-quit the app and reopen so iOS re-prompts for permission. |
| Permission prompt never appears | Settings → Privacy & Security → Bluetooth → enable for `body_comp_app`. |
| App opened fine yesterday, won't open today | Free-tier Apple IDs sign profiles for **7 days only**. Re-run `flutter run` from the Mac to re-sign. |
| `flutter doctor` says Xcode is missing even after install | Run `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` then `sudo xcodebuild -runFirstLaunch`. |
| Build fails: *"The iOS Simulator deployment target is set to 8.0"* | Re-run `./ios_setup.sh`, then `cd ios && pod install && cd ..`. |

---

## 10. Demo-day quick start

Once everything above worked once, day-of you only need:

```bash
cd Body-Composition/MobileApp
flutter run
```

Plug the iPhone in, power the ESP32, and go.
