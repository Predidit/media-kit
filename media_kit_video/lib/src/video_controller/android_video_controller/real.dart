/// This file is a part of media_kit (https://github.com/media-kit/media-kit).
///
/// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
/// All rights reserved.
/// Use of this source code is governed by MIT license that can be found in the LICENSE file.
import 'dart:io';
import 'dart:async';
import 'dart:collection';
import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart';
import 'package:synchronized/synchronized.dart';

import 'package:media_kit/media_kit.dart';

import 'package:media_kit_video/src/utils/query_decoders.dart';
import 'package:media_kit_video/src/video_controller/platform_video_controller.dart';

/// {@template android_video_controller}
///
/// AndroidVideoController
/// ----------------------
///
/// The [PlatformVideoController] implementation based on native JNI & C/C++ used on Android.
///
/// {@endtemplate}
class AndroidVideoController extends PlatformVideoController {
  /// Whether [AndroidVideoController] is supported on the current platform or not.
  static bool get supported => Platform.isAndroid;

  /// Pointer address to the global object reference of `android.view.Surface` i.e. `(intptr_t)(*android.view.Surface)`.
  final ValueNotifier<int?> wid = ValueNotifier<int?>(null);

  /// [Lock] used to synchronize [onLoadHooks], [onUnloadHooks] & [subscription].
  final lock = Lock();

  NativePlayer get platform => player.platform as NativePlayer;

  Future<void> setProperty(String key, String value) async {
    await platform.setProperty(key, value, waitForInitialization: false);
  }

  Future<void> setProperties(Map<String, String> properties) async {
    // ORDER IS IMPORTANT.
    for (final entry in properties.entries) {
      await setProperty(entry.key, entry.value);
    }
  }

  Future<void> _updateMPVSize(int width, int height) async {
    String sizeString = "${width}x${height}";
    final handle = await player.handle; // For logging
    debugPrint('[AndroidVideoController._updateMPVSize] handle: $handle - Setting mpv-surface-size to: $sizeString');
    try {
      await setProperty('android-surface-size', sizeString);
      debugPrint('[AndroidVideoController._updateMPVSize] handle: $handle - Successfully set mpv-surface-size to: $sizeString');
    } catch (e, s) {
      debugPrint('[AndroidVideoController._updateMPVSize] handle: $handle - ERROR setting mpv-surface-size: $e $s');
    }
  }

  /// Listener for updating the --wid property.
  Future<void> widListener() {
    return lock.synchronized(() async {
      String? logHandle; // For logging, especially in error cases
      try {
        // Attempt to get handle for logging
        try {
          logHandle = (await player.handle).toString();
        } catch (_) {
          logHandle = "unavailable_during_init_fetch";
        }

        debugPrint('[AndroidVideoController.widListener] ENTER for handle: $logHandle. Current wid.value: ${wid.value}');
        final width = rect.value?.width.toInt() ?? 1;
        final height = rect.value?.height.toInt() ?? 1;
        final androidSurfaceSizeValue = [width, height].join('x');
        final widValue = wid.value?.toString() ?? '0';
        final voValue = widValue == '0' ? 'null' : configuration.vo!;
        final vidValue = widValue == '0' ? 'no' : 'auto';
        
        debugPrint('[AndroidVideoController.widListener] handle: $logHandle - Calculated values: widValue: $widValue, voValue: $voValue, androidSurfaceSizeValue: $androidSurfaceSizeValue, vidValue: $vidValue (applicable if vo=mediacodec_embed)');

        debugPrint('[AndroidVideoController.widListener] handle: $logHandle - About to setProperty vo: null');
        await setProperty('vo', 'null');
        debugPrint('[AndroidVideoController.widListener] handle: $logHandle - Done setProperty vo: null');

        final propertiesToSet = {
          'android-surface-size': androidSurfaceSizeValue,
          'wid': widValue,
          'vo': voValue,
        };
        if (configuration.vo == 'mediacodec_embed') {
          propertiesToSet['vid'] = vidValue;
        }
        debugPrint('[AndroidVideoController.widListener] handle: $logHandle - About to setProperties: $propertiesToSet');
        await setProperties(propertiesToSet);
        debugPrint('[AndroidVideoController.widListener] handle: $logHandle - Done setProperties.');
      } catch (e, s) {
        // Attempt to get handle again for error logging, in case it became available or failed initially
        try {
          logHandle = (await player.handle).toString();
        } catch (_) {
          logHandle = logHandle ?? "unavailable_during_error_fetch"; // Keep previous if it was set
        }
        debugPrint('[AndroidVideoController.widListener] ERROR for handle $logHandle (wid ${wid.value}): $e $s');
      }
    });
  }

  /// Hook to attach --wid & --vo properties before video output is initialized.
  Future<void> onLoadHook() async {
    // This setup is important to take away control of android.view.Surface from libmpv, when the currently playing gets switched.
    // Not doing so will cause MediaCodec usage inside libavcodec to incorrectly fail with error (because this android.view.Surface would be used twice):
    // "native_window_api_connect returned an error: Invalid argument (-22)" & next less-efficient hwdec will be used redundantly.
    return lock.synchronized(() async {
      if ((rect.value?.width ?? 0.0) <= 1.0 ||
          (rect.value?.height ?? 0.0) <= 1.0) {
        // Do not set --vo if the rect is currently not available.
        return;
      }
      await setProperty('vo', configuration.vo!);
    });
  }

  /// Hook to detach --wid & --vo properties before video output is disposed.
  Future<void> onUnloadHook() async {
    return lock.synchronized(() async {
      await setProperty('vo', 'null');
    });
  }

  /// [StreamSubscription] for listening to video [Rect].
  StreamSubscription<VideoParams>? videoParamsSubscription;

  /// {@macro android_video_controller}
  AndroidVideoController._(
    super.player,
    super.configuration,
  ) {
    wid.addListener(widListener);
    platform.onLoadHooks.add(onLoadHook);
    platform.onUnloadHooks.add(onUnloadHook);
    videoParamsSubscription = player.stream.videoParams.listen(
      (event) => lock.synchronized(() async {
        debugPrint('[AndroidVideoController.videoParamsSubscription] Received VideoParams: event.dw=${event.dw}, event.dh=${event.dh}, event.rotate=${event.rotate}, event.aspect=${event.aspect}, event.par=${event.par}');
        if ([0, null].contains(event.dw) || [0, null].contains(event.dh)) {
          return;
        }

        final int handle = await player.handle;

        final int width;
        final int height;
        if (event.rotate == 0 || event.rotate == 180) {
          width = event.dw ?? 0;
          height = event.dh ?? 0;
        } else {
          // width & height are swapped for 90 or 270 degrees rotation.
          width = event.dh ?? 0;
          height = event.dw ?? 0;
        }
        debugPrint('[AndroidVideoController.videoParamsSubscription] Calculated for SetSurfaceSize: handle=$handle, targetWidth=$width, targetHeight=$height');
        await _channel.invokeMethod(
          'VideoOutputManager.SetSurfaceSize',
          {
            'handle': handle.toString(),
            'width': width.toString(),
            'height': height.toString(),
          },
        );

        rect.value = Rect.fromLTWH(
          0.0,
          0.0,
          width.toDouble(),
          height.toDouble(),
        );

        if (!waitUntilFirstFrameRenderedCompleter.isCompleted) {
          waitUntilFirstFrameRenderedCompleter.complete();
        }
      }),
    );
  }

  /// {@macro android_video_controller}
  static Future<PlatformVideoController> create(
    Player player,
    VideoControllerConfiguration configuration,
  ) async {
    debugPrint('[AndroidVideoController.create] ENTER');
    Future<String> getDefaultHwdec() async {
      // Enforce software rendering in emulators.
      bool hw = configuration.enableHardwareAcceleration;
      final bool isEmulator = await _channel.invokeMethod('Utils.IsEmulator');
      if (isEmulator) {
        hw = false;
        debugPrint('media_kit: Emulator detected.');
        debugPrint('media_kit: Enforcing S/W rendering.');
      }
      return hw ? 'auto-safe' : 'no';
    }

    // Update [configuration] to have default values.
    configuration = configuration.copyWith(
      vo: configuration.vo ?? (Platform.isAndroid ? 'gpu-next' : 'gpu'),
      hwdec: configuration.hwdec ?? await getDefaultHwdec(),
    );

    // Retrieve the native handle of the [Player].
    final handle = await player.handle;
    debugPrint('[AndroidVideoController.create] Player handle: $handle');
    // Return the existing [VideoController] if it's already created.
    if (_controllers.containsKey(handle)) {
      debugPrint('[AndroidVideoController.create] Controller already exists for handle: $handle. Returning existing.');
      return _controllers[handle]!;
    }

    // In case no video-decoders are found, this means media_kit_libs_***_audio is being used.
    // Thus, --vid=no is required to prevent libmpv from trying to decode video (otherwise bad things may happen).
    //
    // Search for common H264 decoder to check if video support is available.
    final decoders = await queryDecoders(handle);
    if (!decoders.contains('h264')) {
      throw UnsupportedError(
        '[VideoController] is not available.'
        ' '
        'Please use media_kit_libs_***_video instead of media_kit_libs_***_audio.',
      );
    }

    // Creation:
    final controller = AndroidVideoController._(
      player,
      configuration,
    );
    controller.id.value = handle; // <-- ADD THIS LINE
    debugPrint('[AndroidVideoController.create] Explicitly set controller.id.value to: ${controller.id.value}'); // <-- ADD THIS LINE

    // Register [_dispose] for execution upon [Player.dispose].
    player.platform?.release.add(controller._dispose);

    // Store the [VideoController] in the [_controllers].
    _controllers[handle] = controller;

    // The Completer logic previously here was removed as it was causing a hang.
    // The creation of VideoOutputManager and its readiness is now handled asynchronously
    // without blocking the create method. The native side will inform Dart when the
    // surface (and thus WID) is ready via a method channel call, which is handled by
    // the _channel.setMethodCallHandler.

    debugPrint('[AndroidVideoController.create] Calling VideoOutputManager.Create for handle: $handle');
    await _channel.invokeMethod(
      'VideoOutputManager.Create',
      {
        'handle': handle.toString(),
      },
    );
    debugPrint('[AndroidVideoController.create] Called VideoOutputManager.Create for handle: $handle');

    // No longer waiting for completer.future here.
    // The necessary WID will be updated via the method channel handler when the surface is ready.
    // The controller.id (player handle) is already available.

    debugPrint('[AndroidVideoController.create] About to set initial properties for handle: $handle');
    await controller.setProperties(
      {
        // 'vo': 'null', // REMOVED: 'vo' is now handled by widListener.
                            // The comment "It is necessary to set vo=null here to avoid SIGSEGV, --wid must be assigned before vo=gpu is set."
                            // is relevant for the operations *within* widListener or if wid was being set *here*.
                            // But widListener has already run and taken care of the vo=null -> wid -> vo=gpu sequence.
        'hwdec': configuration.hwdec!,
        'vid': 'auto',
        'opengl-es': 'yes',
        'force-window': 'yes',
        'gpu-context': 'android',
        'sub-use-margins': 'no',
        'sub-font-provider': 'none',
        'sub-scale-with-window': 'yes',
        'hwdec-codecs': 'h264,hevc,mpeg4,mpeg2video,vp8,vp9,av1',
      },
    );
    debugPrint('[AndroidVideoController.create] Finished setting initial properties for handle: $handle');

    // Return the [PlatformVideoController].
    debugPrint('[AndroidVideoController.create] EXIT for handle: $handle');
    return controller;
  }

  /// Sets the required size of the video output.
  /// This may yield substantial performance improvements if a small [width] & [height] is specified.
  ///
  /// Remember:
  /// * “Premature optimization is the root of all evil”
  /// * “With great power comes great responsibility”
  @override
  Future<void> setSize({
    int? width,
    int? height,
  }) {
    throw UnsupportedError(
      '[AndroidVideoController.setSize] is not available on Android',
    );
  }

  /// Disposes the instance. Releases allocated resources back to the system.
  Future<void> _dispose() async {
    super.dispose();
    wid.dispose();
    wid.removeListener(widListener);
    platform.onLoadHooks.remove(onLoadHook);
    platform.onUnloadHooks.remove(onUnloadHook);
    await videoParamsSubscription?.cancel();
    final handle = await player.handle;
    _controllers.remove(handle);
    await _channel.invokeMethod(
      'VideoOutputManager.Dispose',
      {
        'handle': handle.toString(),
      },
    );
  }

  /// Currently created [AndroidVideoController]s.
  static final _controllers = HashMap<int, AndroidVideoController>();

  /// [MethodChannel] for invoking platform specific native implementation.
  static final _channel =
      const MethodChannel('com.alexmercerind/media_kit_video')
        ..setMethodCallHandler(
          (MethodCall call) async {
            try {
              // General log for any incoming method call
              // debugPrint('[AndroidVideoController.MethodHandler] Received method: ${call.method}, arguments: ${call.arguments}');
              switch (call.method) {
                case 'VideoOutput.Resize':
                  {
                    debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] Received: arguments: ${call.arguments}');
                    // Notify about updated texture ID & [Rect].
                    final int handle = call.arguments['handle'];
                    final Rect parsedRect = Rect.fromLTWH(
                      call.arguments['rect']['left'] * 1.0,
                      call.arguments['rect']['top'] * 1.0,
                      call.arguments['rect']['width'] * 1.0,
                      call.arguments['rect']['height'] * 1.0,
                    );
                    final int parsedId = call.arguments['id']; // This is playerHandle
                    final int parsedWid = call.arguments['wid']; // This is native surface WID
                    debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] Parsed: handle: $handle, id(playerHandle): $parsedId, wid(surfaceWID): $parsedWid, rect: $parsedRect');

                    final controller = _controllers[handle];
                    if (controller == null) {
                      debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] No controller found for handle $handle');
                      return;
                    }

                    final oldWid = controller.wid.value;

                    controller.rect.value = parsedRect;
                    controller.id.value = parsedId; // Store playerHandle in controller's id
                    // Only on Android:
                    controller.wid.value = parsedWid; // Store WID in controller's wid
                    debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] Updated controller for handle $handle: id.value=${controller.id.value}, wid.value=${controller.wid.value}, oldWid: $oldWid');
                    
                    if (parsedWid != 0 && parsedWid == oldWid && parsedRect.width > 0.0 && parsedRect.height > 0.0) {
                      // WID is the same as before and is not zero, new dimensions are valid.
                      // This means it's a size update for the existing surface.
                      debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] handle: $handle - WID unchanged ($parsedWid), rect changed to: ${parsedRect.width}x${parsedRect.height}. Explicitly calling _updateMPVSize.');
                      await controller._updateMPVSize(parsedRect.width.toInt(), parsedRect.height.toInt());
                    } else if (parsedWid != oldWid) {
                      // This case is implicitly handled because controller.wid.value changing will trigger widListener,
                      // which already calls _updateMPVSize.
                      // Add a log for clarity.
                      debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.Resize] handle: $handle - WID changed from $oldWid to $parsedWid. widListener will handle surface size update.');
                    }
                    break;
                  }
                case 'VideoOutput.WaitUntilFirstFrameRenderedNotify':
                  {
                    debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.WaitUntilFirstFrameRenderedNotify] Received: arguments: ${call.arguments}');
                    // Notify about updated texture ID & [Rect].
                    final int handle = call.arguments['handle'];
                    debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.WaitUntilFirstFrameRenderedNotify] Parsed handle: $handle');
                    // Notify about the first frame being rendered.
                    final completer = _controllers[handle]
                        ?.waitUntilFirstFrameRenderedCompleter;
                    if (!(completer?.isCompleted ?? true)) {
                      completer?.complete();
                      debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.WaitUntilFirstFrameRenderedNotify] Completed completer for handle $handle');
                    } else {
                      debugPrint('[AndroidVideoController.MethodHandler.VideoOutput.WaitUntilFirstFrameRenderedNotify] Completer already completed or null for handle $handle');
                    }
                    break;
                  }
                default:
                  {
                    debugPrint('[AndroidVideoController.MethodHandler] Received unhandled method: ${call.method}');
                    break;
                  }
              }
            } catch (exception, stacktrace) {
              // Log error with method name if possible
              debugPrint('[AndroidVideoController.MethodHandler] ERROR during method ${call.method}: $exception StackTrace: $stacktrace');
            }
          },
        );
}
