/// This file is a part of media_kit (https://github.com/media-kit/media-kit).
///
/// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
/// All rights reserved.
/// Use of this source code is governed by MIT license that can be found in the LICENSE file.
import 'dart:async';

import 'package:media_kit/media_kit.dart';

import 'package:media_kit_video/src/video_controller/platform_video_controller.dart';
import 'package:media_kit_video/src/windows_d3d11_adapter.dart';

// Stub declaration for avoiding compilation errors on Dart JS using conditional imports.

class NativeVideoController extends PlatformVideoController {
  static const bool supported = false;

  static Future<List<WindowsD3D11Adapter>>
      getAvailableWindowsD3D11Adapters() async {
    throw UnsupportedError(
      'Windows D3D11 adapters are only available on Windows.',
    );
  }

  static Stream<WindowsVideoOutputError> get windowsVideoOutputErrors =>
      const Stream<WindowsVideoOutputError>.empty();

  NativeVideoController._(
    super.player,
    super.configuration,
  );

  static Future<PlatformVideoController> create(
    Player player,
    VideoControllerConfiguration configuration,
  ) =>
      throw UnimplementedError();

  @override
  Future<void> setSize({int? width, int? height}) => throw UnimplementedError();
}
