# Android debugging cheat sheet

Commands for building, running and debugging the sample on a phone over USB. Written against the Galaxy A56
(SM-A566B, Xclipse 540) it was first run on; nothing here is specific to that phone. How the port is built
and what is and is not checked is in [android-plan.md](android-plan.md).

The package is `dev.rndr.modernvulkan`, the activity `android.app.NativeActivity`, and everything rndr logs
goes to logcat under the tag `Rndr`.

## Shell setup

`JAVA_HOME` and `ANDROID_HOME` are user variables, so a shell opened before they were set does not see them.
Set them inline, and put `adb` on the path:

```powershell
$env:JAVA_HOME = 'F:\Program Files\Java\jdk-21.0.12.1'
$env:ANDROID_HOME = 'F:\Android\Sdk'
$env:Path = "$env:ANDROID_HOME\platform-tools;$env:Path"
```

```bash
export JAVA_HOME='F:\Program Files\Java\jdk-21.0.12.1' ANDROID_HOME='F:\Android\Sdk'
export PATH="/f/Android/Sdk/platform-tools:$PATH"
```

In PowerShell the wrapper is `.\gradlew.bat`; in Git Bash it is `./gradlew`, since Bash eats the backslash in
`.\gradlew.bat`. PowerShell also splits an unquoted `-Pa.b=c` at the dot, so quote Gradle properties.

## The phone

| What | Command |
|---|---|
| Is it attached | `adb devices -l` - `unauthorized` means accept the prompt on the phone |
| Keep the screen on while plugged in | `adb shell svc power stayon usb` (`false` to undo) |
| Wake it | `adb shell input keyevent KEYCODE_WAKEUP` |
| Model, Android version | `adb shell getprop ro.product.model`, `adb shell getprop ro.build.version.release` |
| Vulkan driver and features | `adb shell cmd gpu vkjson` |
| Current rotation | `adb shell dumpsys input \| grep -m1 Orientation:` |
| Auto-rotate on or off | `adb shell settings get system accelerometer_rotation` |

Launch with the screen on. An activity started behind the lock screen gets a window and loses it again at
once, and the run that follows is not the one you meant to test.

Wireless instead of a cable (Android 11+): on the phone, Developer options > Wireless debugging > Pair device
with pairing code, then `adb pair <ip>:<pair-port>` and `adb connect <ip>:<port>`.

## The emulator

The Android emulator runs the sample well enough to test without a phone: with the host GPU it offers Vulkan
1.3 with dynamic rendering, synchronization2, buffer device address and descriptor indexing, which is all Forge
asks for. It is set up on this machine as the AVD `rndr-api36` (Android 16, x86_64, Pixel 7 profile), kept on F:
because the emulator wants 12 GB free where the AVD lives and C: has less.

```bash
ANDROID_AVD_HOME='F:\Android\avd' /f/Android/Sdk/emulator/emulator.exe -avd rndr-api36 -gpu host -no-snapshot-save
adb -e shell getprop sys.boot_completed          # 1 once it has booted
```

It needs its own APK: the image is x86_64, and the ARM translation it offers arm64 apps crashes in
`vkCreateInstance`. The ABIs are a Gradle property, `arm64-v8a` alone by default:

```powershell
.\gradlew.bat installDebug '-Prndr.abis=x86_64'            # the emulator
.\gradlew.bat installDebug '-Prndr.abis=arm64-v8a,x86_64'  # both, one APK
```

With a phone and the emulator attached at once, `adb -d` picks the phone and `adb -e` the emulator; Gradle's
`installDebug` installs on every device whose ABI the APK carries.

What differs from a phone:

- The guest driver (gfxstream, `vulkan.ranchu.so`) reports the host GPU as its device and driver, so Vulkan
  cannot tell it is an emulator. Android can: `ro.kernel.qemu` is 1.
- Its `vkSetDebugUtilsObjectNameEXT` crashes naming an image, so Forge's `SetDebugName` does nothing under the
  emulator (`src/forge/debug.cpp`). Captures taken there have no object names.
- A keyboard works: the sample's O key turns it between landscape and portrait, and the emulator's rotate buttons
  turn the device.

Setting it up again elsewhere: install the command-line tools into `<sdk>/cmdline-tools/latest`, then

```bash
android sdk install "system-images/android-36/google_apis/x86_64"
avdmanager create avd -n rndr-api36 -k "system-images;android-36;google_apis;x86_64" -d pixel_7
```

`android` is the SDK's new CLI, beside `sdkmanager`, which now only forwards to it.

## Build, install, run

```powershell
cd samples/android
.\gradlew.bat installDebug                  # build rndr + the sample for arm64, compile shaders, install
adb shell am start -n dev.rndr.modernvulkan/android.app.NativeActivity
adb shell am force-stop dev.rndr.modernvulkan
adb shell pidof dev.rndr.modernvulkan       # prints nothing once it is gone
```

| What | Command |
|---|---|
| Build without installing | `.\gradlew.bat assembleDebug`, APK at `modern-vulkan/build/outputs/apk/debug/modern-vulkan-debug.apk` |
| Install an APK by hand | `adb install -r <apk>` |
| Uninstall | `adb uninstall dev.rndr.modernvulkan` |
| Wipe the app's data | `adb shell pm clear dev.rndr.modernvulkan` |

The APK's assets are copied to `files/assets` on first launch, and a file already there with the same size is
skipped. A changed asset of unchanged size therefore keeps the old copy: `pm clear` forces a fresh extract.
The log says how many were copied (`Extracted 1 of 7 assets ...`).

## Logs

An activity's stdout and stderr go nowhere; only logcat reaches you.

```bash
adb logcat -c                               # clear, so the next dump is only this run
adb logcat -s Rndr                          # follow rndr's log
adb logcat -d -s Rndr                       # dump what is there and exit
adb logcat -d --pid=$(adb shell pidof dev.rndr.modernvulkan)   # everything the process logged
adb logcat -d -s Rndr threaded_app          # plus the native glue's lifecycle events
adb logcat -d -b crash                      # the crash buffer: fatal signals and backtraces
adb logcat -d > run.log                     # keep a run for later
```

What to look for:

- `Swap chain extent: (w, h), rotated N degrees` - once per swap chain creation. Many in a row means it is
  being recreated every frame.
- `[Vulkan Validation] ...` - validation messages, under the `Rndr` tag. Only the debug APK carries the layer;
  `Vulkan validation layer enabled.` at startup confirms it loaded.
- `Forge - modern-vulkan - CPU ... GPU ...` - every two seconds while frames are presented. None means the loop
  is stuck, or has no window (backgrounded).
- `threaded_app: APP_CMD_INIT_WINDOW` / `APP_CMD_TERM_WINDOW` - the activity gaining and losing its window.
- `F libc : Fatal signal 6 (SIGABRT)` - an abort, usually a `Require` in the sample that failed. The `E Rndr`
  line just before it says which call.

## Crashes and stack traces

The backtrace in `logcat -b crash` names `libmodern-vulkan.so` and an offset. The symbols are in the unstripped
library the build keeps:

```
samples/android/modern-vulkan/build/intermediates/cxx/Debug/<hash>/obj/arm64-v8a/libmodern-vulkan.so
```

Either feed the whole log through `ndk-stack`:

```bash
adb logcat -d -b crash | /f/Android/Sdk/ndk/30.0.16248370/ndk-stack.cmd -sym samples/android/modern-vulkan/build/intermediates/cxx/Debug/<hash>/obj/arm64-v8a
```

or resolve single frames:

```bash
/f/Android/Sdk/ndk/30.0.16248370/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-addr2line.exe -C -f -i \
    -e <that .so> 0x4dc890 0x1b8ef0
```

**Check the Build ID first.** The crash line prints `BuildId: ...`; compare it with
`llvm-readelf -n <that .so> | grep "Build ID"`. Rebuild after the crash and the addresses resolve to the wrong
functions without any warning.

## Looking at the screen

| What | Command |
|---|---|
| Screenshot | `adb exec-out screencap -p > screen.png` |
| Record 10 s | `adb shell screenrecord --time-limit 10 /sdcard/rec.mp4` then `adb pull /sdcard/rec.mp4` |

Screenshots are in the display's current orientation - 2340x1080 in landscape.

## Simulating input

`adb shell input [<source>] <command>` injects events as if they came from the hardware, so they reach the app
through the same `AInputEvent` path a finger does. Coordinates are pixels in the display's current orientation,
the same space as a screenshot. Only `swipe` has been tried on the A56 so far; the rest are standard `input`
commands not yet run there.

| What | Command |
|---|---|
| Tap | `adb shell input tap 1170 540` |
| Drag, over 400 ms | `adb shell input swipe 1000 540 1300 540 400` |
| Long press | `adb shell input swipe 1170 540 1170 540 1000` - a swipe that does not move |
| Finger down, move, up in separate steps | `adb shell input motionevent DOWN 1000 540`, then `MOVE 1100 540`, ..., `UP 1100 540` |
| Key press and release | `adb shell input keyevent KEYCODE_W` |
| Key held | `adb shell input keyevent --longpress KEYCODE_W` |
| Type text | `adb shell input text hello` (`%s` for a space) |
| Mouse instead of touch | `adb shell input mouse swipe 1000 540 1300 540 400` |
| Mouse wheel | `adb shell input mouse scroll 1170 540 --axis VSCROLL,1` |

What rndr makes of each (`src/platform/android-application.cpp`):

- **Touch** - the first finger is the left mouse button: down on touch, up on release, and its movement is
  mouse motion, which is what turns the sample's camera. A second finger is ignored. A new touch starts where
  it lands, so the first move of a drag has no jump in it.
- **Mouse** - its own path: position, buttons diffed from the button state, and scroll as the wheel.
- **Keys** - mapped through `TranslateKey` to the same `InputPrimitive`s a desktop keyboard gives. A key with no
  mapping (volume, media) is left to the system.
- **`KEYCODE_BACK`** - closes the window on release, and `android_main` returns. Item 5 of the Phase 4 check.
- **`KEYCODE_HOME`** - backgrounds the activity, which loses its window; `am start` brings it back and the
  surface is rebuilt. Item 3 of the Phase 4 check.

A scripted input followed by a screenshot is a check that repeats exactly, which a hand on the screen is not:

```bash
adb shell input swipe 1000 540 1100 540 300 && sleep 1 && adb exec-out screencap -p > after.png
```

`input` has no multi-touch. Two fingers need a hand, or raw `sendevent` writes, which are specific to the
phone's touch controller.

Which way up the app is shown is its own to ask for: `GenericWindowDesc::orientation` at creation and
`GenericWindow::SetOrientation` after, which Android receives as `Activity.setRequestedOrientation`. The sample
asks for `ScreenOrientation::Landscape`, so of the four rotations only 90 and 270 apply to it. For a test the
device's rotation can be forced, and the settings put back afterwards:

```bash
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 3        # 0, 1, 2, 3 = 0, 90, 180, 270 degrees
adb shell settings put system accelerometer_rotation 1
```

## The app's files

The debug APK is debuggable, so `run-as` reaches its private storage without root:

```bash
adb shell run-as dev.rndr.modernvulkan ls -l files/assets
adb exec-out run-as dev.rndr.modernvulkan cat files/assets/Suzanne.gltf > Suzanne.gltf
```

## Shaders

The SPIR-V the APK carries is under `modern-vulkan/build/generated/rndr-spirv/`. Disassemble it with the Vulkan
SDK's `spirv-dis`, and compare it with what the desktop compiles - the two are meant to be the same bytes
(see the `[forge]` case "Forge slangc and ShaderCompiler produce the same module").

When the picture is wrong and the log is clean, narrow it down in the shader: write a hardcoded
`output.Pos`, then put the real transforms back one at a time. The shader is rebuilt with the APK, so each step
is one `installDebug`. That is how the two Android-only shader problems so far were found: slangc's column-major
default, and the Xclipse driver misreading `model[instanceIndex]` through a buffer device address.

## A debugger, frame captures

- **LLDB**: Android Studio > File > Profile or Debug APK, open the debug APK, point it at the `obj/arm64-v8a`
  directory above for symbols, and run it under the debugger.
- **GPU frame capture**: Android GPU Inspector (AGI) or RenderDoc's Android support capture a Vulkan frame from
  the debuggable APK.
