/**
 * This file is a part of media_kit (https://github.com/media-kit/media-kit).
 * <p>
 * Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
 * All rights reserved.
 * Use of this source code is governed by MIT license that can be found in the LICENSE file.
 */
package com.alexmercerind.media_kit_video.platformview;

import android.content.Context;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import io.flutter.plugin.platform.PlatformView;
import io.flutter.plugin.platform.PlatformViewFactory;
import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.plugin.common.MethodChannel;

import android.util.Log;
import java.util.Objects;
import java.util.HashMap;
import java.util.function.Consumer;

/**
 * A factory class responsible for creating platform video views that can be embedded in a Flutter
 * app.
 */
public class PlatformVideoViewFactory extends PlatformViewFactory {
    private static final String TAG = "PlatformVideoViewFactory";
    private final MethodChannel channel;

    /**
     * Constructs a new PlatformVideoViewFactory.
     *
     * @param channel The MethodChannel used to communicate with Flutter side.
     */
    public PlatformVideoViewFactory(@NonNull MethodChannel channel) {
        super(StandardMessageCodec.INSTANCE);
        this.channel = channel;
    }

    /**
     * Creates a new instance of platform view.
     *
     * @param context The context in which the view is running.
     * @param id The unique identifier for the view.
     * @param args The arguments for creating the view.
     * @return A new instance of PlatformVideoView.
     */
    @NonNull
    @Override
    public PlatformView create(@NonNull Context context, int id, @Nullable Object args) {
        @SuppressWarnings("unchecked")
        final java.util.Map<String, Object> params = (java.util.Map<String, Object>) Objects.requireNonNull(args);
        final long handle = ((Number) Objects.requireNonNull(params.get("handle"))).longValue();
        final int width = ((Number) Objects.requireNonNull(params.get("width"))).intValue();
        final int height = ((Number) Objects.requireNonNull(params.get("height"))).intValue();

        Log.i(TAG, "Creating PlatformVideoView for handle: " + handle);
        final long finalHandle = handle;
        return new PlatformVideoView(context, handle, width, height,
            (wid) -> channel.invokeMethod("PlatformVideoView.SurfaceAvailable", new HashMap<String, Object>() {{
                put("handle", finalHandle);
                put("wid", wid);
            }})
        );
    }
}

