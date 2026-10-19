# Native assets

`media_kit` owns native playback dependencies. Applications need `media_kit`
and, for Flutter textures, `media_kit_video`. Remove every `media_kit_libs_*`
dependency and override. There are no replacement container packages.

This branch requires Dart 3.10 or newer and Flutter 3.38 or newer. Both packages
must come from the same checkout or migrated revision; previously published
versions do not implement this protocol. Git consumers should pin both package
paths (`media_kit` and `media_kit_video`) to the same immutable commit. Kazumi
uses the OHOS adaptation in the `tignioj/media-kit` fork this way.

## Ownership

* `hook/native_bundles.json` is the single source of archive URLs, SHA-256 hashes,
  target architectures and primary library names. Updating a binary requires
  updating this manifest, without changing Gradle, CMake, CocoaPods or SwiftPM.
* `hook/build.dart` downloads only the target bundle and compiles the event-loop
  bridge with the active Dart SDK headers and target compiler. Flutter/Dart bundle
  the emitted code assets. Web builds do not download or compile native code.
* Downloads and extracted files have a content-addressed cache and checksums.
  A file lock serializes concurrent builds; incomplete extraction is not
  published. Corrupt extracted files are repaired from the verified archive.
* Android's core plugin owns the Java application context and JNI bootstrap.
  The hook bundles both `libmpv.so` and `libmediakitandroidhelper.so`. The video
  plugin directly depends on this core Java API; reflection is unnecessary.
* Desktop and Apple video plugins compile a small C binding with vendored public
  mpv headers. Dart passes the owning player's resolved library path when creating
  a video output. The binding resolves its complete symbol table before publishing
  it and rejects a second, different module. It holds the module for process
  lifetime; it never contains a Dart callback or owns a player handle.
* Apple hooks select the XCFramework slice by OS, CPU and simulator status, thin
  universal binaries, and emit all 18 dependency libraries. Relative install IDs
  support Dart tests; Flutter rewrites them when creating and signing frameworks.
  CocoaPods and SwiftPM only build the texture plugin and the same C binding.

The old Gradle JAR download tasks, CMake library downloads/import libraries,
CocoaPods Makefiles and lockfile scanners, SwiftPM binary targets, optional-library
stubs, redundant library plugins and audio/video package permutations are removed.
The example desktop runners also install the code-assets directory using the
current Flutter CMake layout; they no longer depend on a library plugin to copy
DLLs/shared objects. Audio-only applications use the same default playback binary. To use a smaller
custom build, configure its libraries explicitly below.

## Configuration

The default source is `bundled`; no application configuration is required.
Configuration belongs in the consuming application's `pubspec.yaml`, following
the [Dart hook user-defines contract](https://dart.dev/tools/hooks#hook-configuration).

To share a verified download cache across checkouts/builds:

```yaml
hooks:
  user_defines:
    media_kit:
      source: bundled
      cache-directory: ../native-cache
```

To bundle your own native build:

```yaml
hooks:
  user_defines:
    media_kit:
      source: local
      library-directory: native/windows-x64
```

Paths resolve relative to that pubspec. Supply libraries for the selected target
only. The required primary names are `libmpv-2.dll` (Windows), `libmpv.so.2`
(Linux), and `libmpv.so` (Android). Android also requires its matching JNI helper.
On Apple, provide a directory containing the complete set of dynamic XCFrameworks,
including `Mpv.xcframework`. Local inputs are registered as build dependencies.

Linux/Windows applications may instead set `source: system`. This emits a system
library reference and bundles only the event-loop bridge. The runtime loader must
be able to find the primary library and all its dependencies. A system build must
support the player/video API in use; Windows hardware rendering needs this fork's
DXGI extension. Runtime `libmpv:` and `LIBMPV_LIBRARY_PATH` overrides remain
available, but do not change what the build hook packages.

Windows destruction acknowledges completion only after texture unregistration,
queued render work and `mpv_render_context_free` finish. This keeps deterministic
player disposal from racing an asynchronous renderer teardown.

## Platform boundary

Android arm/arm64/x64, Windows x64/arm64, Linux x64/arm64, and Apple device/simulator
targets have catalog entries. Android ia32 is retained for tooling that supports
it. Apple requires Xcode (including `lipo`, `otool`, `install_name_tool`, `codesign`).
Windows 7z extraction uses the system `tar.exe`; other archives use ZIP or tar.
All native targets require a C compiler for the small lifecycle bridge.

OHOS arm64 is supported with a Flutter-OH SDK that implements Native Assets
(tested with 3.41.9+ohos-1.0.2, Dart 3.11.5). Flutter-OH currently reports a Linux
target to hosted hooks, so the hook identifies its OHOS compiler SDK, selects
`ohos_arm64`, and compiles the event-loop bridge with the OHOS target triple and
sysroot. It does not use Linux binaries. The catalog's primary filename matches
the archive and SONAME: `libmpv.so`.

The OHOS ArkTS renderer still supplies the Flutter texture and native surface.
The hook also bundles the SDK's shared C++ runtime required by libmpv (unless a
local bundle supplies it). Use a native SDK compatible with the consuming
application's minimum HarmonyOS API level.
Only arm64 has a pinned OHOS bundle; other OHOS architectures fail explicitly.
Emulators are not supported by the renderer; validate on a physical device.

The compiler supplied by Flutter-OH normally locates the SDK automatically.
If it is unavailable, configure the consuming application's OHOS builds only:

```yaml
hooks:
  user_defines:
    media_kit:
      target: ohos
      ohos-sdk: /absolute/path/to/openharmony/native
```

`OHOS_NDK_HOME` and `HOS_SDK_HOME` are also accepted for explicit OHOS targets.
Setting these environment variables alone does not redirect normal Linux builds.
The dependency ranges support Flutter-OH's pinned `meta 1.17` as well as newer
hook/code-assets versions, without overriding Flutter SDK dependencies.

Apple deployment minimums are also constrained by the selected Flutter SDK's
native-asset framework generator. Flutter 3.47.5 emits iOS 15 / macOS 13 minimums;
the consuming Xcode project must use compatible deployment targets. The older
minimums of the previous binary packages are not a compatibility guarantee.

## Validation

```sh
cd media_kit
flutter pub get
flutter test test/hook test/src/player/native/core/native_event_loop_test.dart test/src/player/native/core/native_player_lifecycle_test.dart
cd tool/lifecycle_test
flutter pub get
flutter test
```

The second package compiles the production video binding and lifecycle bridge,
checks failed/repeated/mismatched library binding against real mpv, and kills
150 isolates while native callbacks and disposal are active. It also uses an ABI
fixture for deterministic borrowed-buffer and teardown assertions.

For real Flutter texture creation, rendering and disposal (software and hardware):

```sh
cd media_kit_test
flutter pub get
flutter test integration_test/native_assets_test.dart -d windows
```

Verified locally: 19 hook/core/native-binding tests and 2 Windows integration
tests (8 render/dispose cycles), Kazumi Android arm64 release packaging,
and Windows x64 release compilation. Android mpv and JNI helper hashes are
unchanged. Linux and Apple release archives were downloaded and SHA-256 verified;
Apple framework metadata and architecture selections were inspected. Their
application builds and runtime behavior still require Linux/macOS hosts. No APK
was installed, overwritten or uninstalled on the connected Android device.


OHOS validation (2026-10-17): the hook/core suite passed 19 tests on macOS;
Kazumi passed 213 Flutter tests and built both smoke and normal Debug HAPs with
Flutter-OH 3.41.9+ohos-1.0.2. The HAP contains the OHOS libmpv, C event-loop
bridge, shared C++ runtime and their `ohos_arm64` Native Assets mappings.
On a MatePad mini (HarmonyOS API 26), two runs each completed 12 render/dispose
cycles across software/hardware decoding and synchronous/asynchronous player
modes, plus 3 disposals during initialization. Hardware decoding reported
`ohcodec`. Concurrent/repeated disposal completed, the native handle became
null, and disposal took 69–136 ms across both runs. A device screenshot confirmed
the test video was displayed through the Flutter texture. The normal-entry
Kazumi HAP was then installed and launched successfully. OHOS emulators and other OHOS architectures were not validated.


Release AOT follow-up (2026-10-17): after installing Rosetta for Flutter-OH's
x86_64 macOS snapshot compiler, both Kazumi smoke and normal-entry Release HAPs
built and passed CLI signing/verification. Both contain arm64 `libapp.so`, have
`debug=false`, and omit `kernel_blob.bin`. On the same MatePad mini, two Release
runs each passed 12 render/dispose cycles and 3 disposals during initialization
(24 and 6 total). Software (`no`) and OHOS hardware (`ohcodec`) decoding worked
in synchronous and asynchronous modes; concurrent/repeated disposal completed
with a null native handle in 76–160 ms. A device screenshot confirmed actual
hardware-decoded texture rendering. The normal-entry Release HAP was then
installed in place of the smoke build.
