package com.alexmercerind.media_kit_video;

import android.content.Context;
import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.plugin.platform.PlatformView;
import io.flutter.plugin.platform.PlatformViewFactory;

import java.util.HashMap;
import java.util.Locale;
import android.util.Log;

public class VideoOutputFactory extends PlatformViewFactory {
    private static final String TAG = "VideoOutputFactory";
    private final VideoOutputManager videoOutputManager;

    public VideoOutputFactory(VideoOutputManager videoOutputManager) {
        super(StandardMessageCodec.INSTANCE);
        this.videoOutputManager = videoOutputManager;
        Log.i(TAG, "VideoOutputFactory initialized");
    }

    @SuppressWarnings("unchecked")
    @NonNull
    @Override
    public PlatformView create(@NonNull Context context, int viewId, @Nullable Object args) {
        Log.i(TAG, String.format(Locale.ENGLISH, "create: ENTER - viewId: %d, args: %s", viewId, (args != null ? args.toString() : "null")));
        // Arguments from Dart side: Map<String, Object>
        // We expect "handle" to be one of the arguments.
        if (args instanceof HashMap) {
            HashMap<String, Object> arguments = (HashMap<String, Object>) args;
            Object handleObject = arguments.get("handle");
            if (handleObject instanceof Number) {
                long handle = ((Number) handleObject).longValue();
                Log.i(TAG, String.format(Locale.ENGLISH, "create: Successfully parsed handle: %d for viewId: %d", handle, viewId));
                // Store the viewId and handle mapping in VideoOutputManager if needed,
                // or pass VideoOutputManager to VideoOutput to call back.
                Log.i(TAG, String.format(Locale.ENGLISH, "create: About to return new VideoOutput instance for viewId: %d, handle: %d", viewId, handle));
                VideoOutput videoOutput = new VideoOutput(context, viewId, args, videoOutputManager, handle);
                videoOutputManager.registerVideoOutputView(viewId, videoOutput);
                Log.i(TAG, String.format(Locale.ENGLISH, "create: Registered VideoOutput instance with VideoOutputManager for viewId: %d", viewId));
                return videoOutput;
            } else {
                Log.e(TAG, String.format(Locale.ENGLISH, "create: ERROR - 'handle' argument is not a Number. Actual type: %s", (handleObject != null ? handleObject.getClass().getName() : "null")));
                throw new IllegalArgumentException("Invalid 'handle' argument type. Expected Long or Integer.");
            }
        }
        Log.e(TAG, String.format(Locale.ENGLISH, "create: ERROR - args is not a HashMap. Actual type: %s", (args != null ? args.getClass().getName() : "null")));
        throw new IllegalArgumentException("Invalid arguments passed to VideoOutputFactory. Expected HashMap with 'handle'.");
    }
}
