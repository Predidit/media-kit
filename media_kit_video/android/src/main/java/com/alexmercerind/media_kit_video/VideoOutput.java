/**
 * This file is a part of media_kit (https://github.com/media-kit/media-kit).
 * <p>
 * Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
 * All rights reserved.
 * Use of this source code is governed by MIT license that can be found in the LICENSE file.
 */
package com.alexmercerind.media_kit_video;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.Locale;

import io.flutter.view.TextureRegistry;

public class VideoOutput implements TextureRegistry.SurfaceProducer.Callback {
    private static final String TAG = "VideoOutput";
    private static final Handler handler = new Handler(Looper.getMainLooper());

    private long id = 0;
    private long wid = 0;

    private final TextureUpdateCallback textureUpdateCallback;

    private final TextureRegistry.SurfaceProducer surfaceProducer;

    private final Object lock = new Object();

    VideoOutput(TextureRegistry textureRegistryReference, TextureUpdateCallback textureUpdateCallback) {
        this.textureUpdateCallback = textureUpdateCallback;

        surfaceProducer = textureRegistryReference.createSurfaceProducer();
        surfaceProducer.setCallback(this);
    }

    public void dispose() {
        synchronized (lock) {
            try {
                surfaceProducer.getSurface().release();
            } catch (Throwable e) {
                Log.e(TAG, "dispose", e);
            }
            try {
                surfaceProducer.release();
            } catch (Throwable e) {
                Log.e(TAG, "dispose", e);
            }
            onSurfaceCleanup();
        }
    }

    public void setSurfaceSize(int width, int height) {
        setSurfaceSize(width, height, false);
    }

    private void setSurfaceSize(int width, int height, boolean force) {
        synchronized (lock) {
            try {
                if (!force && surfaceProducer.getWidth() == width && surfaceProducer.getHeight() == height) {
                    return;
                }
                surfaceProducer.setSize(width, height);
                onSurfaceAvailable();
            } catch (Throwable e) {
                Log.e(TAG, "setSurfaceSize", e);
            }
        }
    }

    @Override
    public void onSurfaceAvailable() {
        synchronized (lock) {
            Log.i(TAG, "onSurfaceAvailable: id=" + id + ", wid=" + wid + ", width=" + surfaceProducer.getWidth() + ", height=" + surfaceProducer.getHeight());
            id = surfaceProducer.id();
            wid = GlobalObjectRefManager.newGlobalObjectRef(surfaceProducer.getSurface());
            textureUpdateCallback.onTextureUpdate(id, wid, surfaceProducer.getWidth(), surfaceProducer.getHeight());
        }
    }

    @Override
    public void onSurfaceCleanup() {
        synchronized (lock) {
            Log.i(TAG, "onSurfaceCleanup: id=" + id + ", wid=" + wid + ", width=" + surfaceProducer.getWidth() + ", height=" + surfaceProducer.getHeight());
            textureUpdateCallback.onTextureUpdate(id, 0, surfaceProducer.getWidth(), surfaceProducer.getHeight());
            if (wid != 0) {
                final long widReference = wid;
                handler.postDelayed(() -> GlobalObjectRefManager.deleteGlobalObjectRef(widReference), 5000);
            }
        }
    }
}
