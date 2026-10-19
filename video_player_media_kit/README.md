# [package:video_player_media_kit](https://github.com/media-kit/media-kit)

#### package:video_player support for all platforms, based on package:media_kit.

[![](https://img.shields.io/discord/1079685977523617792?color=33cd57&label=Discord&logo=discord&logoColor=discord)](https://discord.gg/h7qf2R9n57) [![Github Actions](https://github.com/media-kit/media-kit/actions/workflows/ci.yml/badge.svg)](https://github.com/media-kit/media-kit/actions/workflows/ci.yml)

After a single line initialization, you can normally use [package:video_player](https://pub.dev/packages/video_player) & enjoy:

- Support for all platforms _i.e_ Android, iOS, macOS, Windows, GNU/Linux & web.
- Support for more video/audio formats & codecs.
- ...

package:video_player_media_kit allows [package:video_player](https://pub.dev/packages/video_player) to use [package:media_kit](https://pub.dev/packages/media_kit) as a backend.

<table>
  <tr>
    <td>
      <img height="320" src="https://github.com/media-kit/media-kit/assets/28951144/72f553e2-1c29-4268-92dc-0c295df0a67f">
    </td>
    <td>
      <img height="320" src="https://github.com/media-kit/media-kit/assets/28951144/7cc3f7f0-801a-4ee7-be9d-58bec7821a54">
    </td>
    <td>
      <img height="320" src="https://github.com/media-kit/media-kit/assets/28951144/4cd5e4f6-1716-40e0-9a6b-21759b0a30f4">
    </td>
    
  </tr>
  <tr>
    <td>
      video_player on macOS
    </td>
    <td>
      video_player on Windows
    </td>
    <td>
      video_player on GNU/Linux
    </td>
  </tr>
</table>

## Installation

```yaml
dependencies:
  video_player_media_kit:
    path: ../media-kit/video_player_media_kit
```

## TL;DR

A quick usage example.

```dart
void main() {
  VideoPlayerMediaKit.ensureInitialized(
    android: true,          // default: false
    iOS: true,              // default: false
    macOS: true,            // default: false
    windows: true,          // default: false
    linux: true,            // default: false
  );

  // USE package:video_player NORMALLY!
  runApp(MyApp());
}
```

Native playback binaries are supplied by the core build hook. Use this checkout
or a revision containing the migration; see the [native build requirements](../media_kit/doc/native_assets.md).

## License

Copyright © 2023 & onwards, Abdelaziz Mahdy <abdelaziz.h.mahdy@gmail.com>

This project & the work under this repository is governed by MIT license that can be found in the [LICENSE](./LICENSE) file.
