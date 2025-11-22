/// This file is a part of media_kit (https://github.com/media-kit/media-kit).
///
/// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
/// All rights reserved.
/// Use of this source code is governed by MIT license that can be found in the LICENSE file.
import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

/// A widget that displays a video player using a platform view on Android.
class PlatformViewVideo extends StatelessWidget {
  /// Creates a new instance of [PlatformViewVideo].
  const PlatformViewVideo({
    super.key,
    required this.handle,
    required this.width,
    required this.height,
  });

  /// The handle (player ID) of the video player.
  final int handle;
  final int width;
  final int height;

  @override
  Widget build(BuildContext context) {
    const String viewType = 'com.alexmercerind/media_kit_video_platform_view';
    final Map<String, dynamic> creationParams = {
      'handle': handle,
      'width': width,
      'height': height,
    };

    // IgnorePointer so that GestureDetector can be used above the platform view.
    return IgnorePointer(
      child: PlatformViewLink(
        viewType: viewType,
        surfaceFactory:
            (BuildContext context, PlatformViewController controller) {
          return AndroidViewSurface(
            controller: controller as AndroidViewController,
            gestureRecognizers:
                const <Factory<OneSequenceGestureRecognizer>>{},
            hitTestBehavior: PlatformViewHitTestBehavior.opaque,
          );
        },
        onCreatePlatformView: (PlatformViewCreationParams params) {
          return PlatformViewsService.initSurfaceAndroidView(
            id: params.id,
            viewType: viewType,
            layoutDirection:
                Directionality.maybeOf(context) ?? TextDirection.ltr,
            creationParams: creationParams,
            creationParamsCodec: const StandardMessageCodec(),
            onFocus: () => params.onFocusChanged(true),
          )
            ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
            ..create();
        },
      ),
    );
  }
}

