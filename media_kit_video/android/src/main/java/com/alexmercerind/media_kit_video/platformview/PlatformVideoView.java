/**
 * This file is a part of media_kit (https://github.com/media-kit/media-kit).
 * <p>
 * Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
 * All rights reserved.
 * Use of this source code is governed by MIT license that can be found in the LICENSE file.
 */
package com.alexmercerind.media_kit_video.platformview;

import java.util.function.Consumer;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import androidx.annotation.NonNull;

import io.flutter.plugin.platform.PlatformView;

import com.alexmercerind.media_kit_video.GlobalObjectRefManager;
/**
 * A class used to create a native video view that can be embedded in a Flutter app.
 * It wraps a SurfaceView and connects it to libmpv.
 */
public final class PlatformVideoView implements PlatformView {
    private static final String TAG = "PlatformVideoView";
    private static final Handler handler = new Handler(Looper.getMainLooper());
    @NonNull
    private final SurfaceView surfaceView;
    private final long handle;
    private final int width;
    private final int height;
    private long wid = 0;
    private final Consumer<Long> onSurfaceAvailable;

    /**
     * Constructs a new PlatformVideoView.
     *
     * @param context The context in which the view is running.
     * @param handle The handle (player ID) of the video player.
     * @param width The width of the video.
     * @param height The height of the video.
     * @param onSurfaceAvailable The callback to be called when the Surface is available.
     */
    public PlatformVideoView(
            @NonNull Context context,
            long handle,
            int width,
            int height,
            @NonNull Consumer<Long> onSurfaceAvailable) {
        this.handle = handle;
        this.width = width;
        this.height = height;
        this.onSurfaceAvailable = onSurfaceAvailable;
        this.surfaceView = new SurfaceView(context);

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            // Avoid blank space instead of a video on Android versions below 8 by adjusting video's
            // z-layer within the Android view hierarchy:
            surfaceView.setZOrderMediaOverlay(true);
        }

        setupSurface();
    }

    private void setupSurface() {
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                Log.i(TAG, "surfaceCreated: handle=" + handle + ", width=" + width + ", height=" + height);
                if (holder.getSurface() != null) {
                    // Clean up old wid if it exists
                    if (wid != 0) {
                        Log.i(TAG, "surfaceCreated: cleaning up old wid=" + wid);
                        GlobalObjectRefManager.deleteGlobalObjectRef(wid);
                        wid = 0;
                    }
                    // Get global reference to the Surface only once when it's first created
                    wid = GlobalObjectRefManager.newGlobalObjectRef(holder.getSurface());
                    Log.i(TAG, "surfaceCreated: created new wid=" + wid);
                    holder.setFixedSize(width, height);
                    // Notify Dart side about the PlatformView Surface availability
                    // Use 0x0 for size initially, actual video size will be set later via SetSurfaceSize
                    // videoOutputManager.notifyPlatformViewSurfaceAvailable(handle, wid, width, height);
                    onSurfaceAvailable.accept(wid);
                }
            }

            @Override
            public void surfaceChanged(
                    @NonNull SurfaceHolder holder, int format, int width, int height) {
                Log.i(TAG, String.format("surfaceChanged: handle=%d, width=%d, height=%d, wid=%d", handle, width, height, wid));
                // videoOutputManager.notifyPlatformViewSurfaceAvailable(handle, wid, width, height);
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                Log.i(TAG, "surfaceDestroyed: handle=" + handle + ", wid=" + wid);
                // videoOutputManager.notifyPlatformViewSurfaceAvailable(handle, 0, width, height);
                if (wid != 0) {
                    final long widReference = wid;
                    handler.postDelayed(() -> GlobalObjectRefManager.deleteGlobalObjectRef(widReference), 5000);
                    wid = 0;
                }
            }
        });
    }

    /**
     * Returns the view associated with this PlatformView.
     *
     * @return The SurfaceView used to display the video.
     */
    @NonNull
    @Override
    public View getView() {
        return surfaceView;
    }

    /** Disposes of the resources used by this PlatformView. */
    @Override
    public void dispose() {
        Log.i(TAG, "dispose: handle=" + handle);
        if (wid != 0) {
            GlobalObjectRefManager.deleteGlobalObjectRef(wid);
            wid = 0;
        }
        if (surfaceView.getHolder().getSurface() != null) {
            surfaceView.getHolder().getSurface().release();
        }
    }
}

