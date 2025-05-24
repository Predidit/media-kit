/**
 * This file is a part of media_kit (https://github.com/media-kit/media-kit).
 * <p>
 * Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
 * All rights reserved.
 * Use of this source code is governed by MIT license that can be found in the LICENSE file.
 */
package com.alexmercerind.media_kit_video;

import androidx.annotation.NonNull;
import android.util.Log; // Import Log
import java.util.Locale; // Import Locale
import java.util.HashMap;

import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;

/**
 * MediaKitVideoPlugin
 */
public class MediaKitVideoPlugin implements FlutterPlugin, MethodCallHandler {
    private static final String TAG = "MediaKitVideoPlugin"; // Define TAG
    private MethodChannel channel;
    private VideoOutputManager videoOutputManager;

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {
        Log.i(TAG, "onAttachedToEngine: Initializing MediaKitVideoPlugin.");
        channel = new MethodChannel(flutterPluginBinding.getBinaryMessenger(), "com.alexmercerind/media_kit_video");
        channel.setMethodCallHandler(this);

        videoOutputManager = new VideoOutputManager(flutterPluginBinding);
        videoOutputManager.registerViewFactory();
        Log.i(TAG, "onAttachedToEngine: VideoOutputManager factory registered.");
    }

    @Override
    public void onMethodCall(@NonNull MethodCall call, @NonNull Result result) {
        Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall: Received call: %s", call.method));
        switch (call.method) {
            case "VideoOutputManager.Create": {
                final long handle = Long.parseLong(call.argument("handle"));
                Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[VideoOutputManager.Create]: Called for handle: %d", handle));
                // The TextureUpdateCallback is used by VideoOutputManager to send surface details (like WID) back.
                // Original `id` from texture registry is now effectively the `handle` for PlatformView context.
                // `wid` is the native window ID (Surface reference).
                videoOutputManager.create(handle, (playerHandle, surfaceWid, width, height) -> {
                    Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[VideoOutputManager.Create]: TextureUpdateCallback invoked for playerHandle: %d, surfaceWid: %d, width: %d, height: %d", playerHandle, surfaceWid, width, height));
                    HashMap<String, Object> args = new HashMap<String, Object>() {{
                        put("handle", playerHandle); // This is the player handle
                        // "id" in the original "VideoOutput.Resize" sent to Dart was the texture ID.
                        // For PlatformView, the concept of a separate texture ID managed by Flutter's TextureRegistry is gone.
                        // The crucial part is the `surfaceWid` (native surface reference) and the `playerHandle`.
                        // We can choose to send `playerHandle` as `id` or introduce a new parameter if Dart side needs distinction.
                        // For now, let's assume Dart side's "VideoOutput.Resize" handler primarily cares about `handle` and `wid`.
                        // Sending playerHandle as 'id' for now to maintain structure, but it's not a texture_id.
                        put("id", playerHandle);
                        put("wid", surfaceWid);
                        put("rect", new HashMap<String, Object>() {{
                            put("left", 0);
                            put("top", 0);
                            put("width", width);
                            put("height", height);
                        }});
                    }};
                    Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[VideoOutputManager.Create]: Sending VideoOutput.Resize to Dart with playerHandle: %d, surfaceWid: %d", playerHandle, surfaceWid));
                    channel.invokeMethod("VideoOutput.Resize", args);
                });
                result.success(null);
                break;
            }
            case "VideoOutputManager.SetSurfaceSize": {
                final long handle = Long.parseLong(call.argument("handle"));
                final int width = Integer.parseInt(call.argument("width"));
                final int height = Integer.parseInt(call.argument("height"));
                Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[VideoOutputManager.SetSurfaceSize]: Called for handle: %d, width: %d, height: %d", handle, width, height));
                videoOutputManager.setSurfaceSize(handle, width, height);
                result.success(null);
                break;
            }
            case "VideoOutputManager.Dispose": {
                final long handle = Long.parseLong(call.argument("handle"));
                Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[VideoOutputManager.Dispose]: Called for handle: %d", handle));
                videoOutputManager.dispose(handle);
                result.success(null);
                break;
            }
            case "Utils.IsEmulator": {
                boolean isEmulator = Utils.isEmulator();
                Log.i(TAG, String.format(Locale.ENGLISH, "onMethodCall[Utils.IsEmulator]: Called. Result: %b", isEmulator));
                result.success(isEmulator);
                break;
            }
            default: {
                result.notImplemented();
                break;
            }
        }
    }

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
        Log.i(TAG, "onDetachedFromEngine: Cleaning up MediaKitVideoPlugin.");
        channel.setMethodCallHandler(null);
    }
}
