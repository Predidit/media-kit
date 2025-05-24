package com.alexmercerind.media_kit_video;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup; // Added for LayoutParams
import android.widget.FrameLayout; // Added for FrameLayout.LayoutParams
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import io.flutter.plugin.platform.PlatformView;
import java.util.Locale;
import android.util.Log;

public class VideoOutput implements PlatformView, SurfaceHolder.Callback {
    private static final String TAG = "VideoOutput";

    private final SurfaceView surfaceView;
    private Surface surface;
    private final long videoOutputManagerHandle;
    private final VideoOutputManager videoOutputManager;
    private final int id; // PlatformView ID

    private int surfaceWidth = 0;
    private int surfaceHeight = 0;
    private boolean surfaceCreatedCalled = false;

    public VideoOutput(Context context, int id, Object args, VideoOutputManager videoOutputManager, long videoOutputManagerHandle) {
        this.id = id;
        this.videoOutputManager = videoOutputManager;
        this.videoOutputManagerHandle = videoOutputManagerHandle;

        surfaceView = new SurfaceView(context);
        surfaceView.getHolder().addCallback(this);

        Log.i(TAG, String.format(Locale.ENGLISH, "Constructor: Created VideoOutput instance ID: %d for videoOutputManagerHandle: %d. SurfaceView: %s", this.id, this.videoOutputManagerHandle, (surfaceView != null ? surfaceView.toString() : "null")));
    }

    @Nullable
    @Override
    public View getView() {
        return surfaceView;
    }

    @Override
    public void dispose() {
        Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Disposing VideoOutput ID: %d for handle: %d", this.id, this.videoOutputManagerHandle));
        if (surfaceView != null) {
            surfaceView.getHolder().removeCallback(this);
        }
        if (surface != null) {
            surface.release();
            surface = null;
        }
        // Notify VideoOutputManager that this specific PlatformView (id) is disposed.
        // This is important if VideoOutputManager needs to tell the native media player to release the surface.
        Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Calling videoOutputManager.onPlatformViewDisposed for ID: %d, handle: %d", this.id, this.videoOutputManagerHandle));
        videoOutputManager.onPlatformViewDisposed(videoOutputManagerHandle, id);
    }

    // SurfaceHolder.Callback methods

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        Log.i(TAG, String.format(Locale.ENGLISH, "surfaceCreated: Surface created for VideoOutput ID: %d (handle: %d). Surface: %s. Holder: %s", this.id, this.videoOutputManagerHandle, (this.surface != null ? this.surface.toString() : "null"), holder.toString()));
        this.surface = holder.getSurface();
        this.surfaceCreatedCalled = true;
        // Attempt to notify if dimensions are already known (e.g. from a previous surfaceChanged or if holder has size)
        // It's also possible surfaceChanged will be called immediately after this with actual dimensions.
        // We primarily rely on surfaceChanged to provide definitive dimensions.
        // If surfaceWidth and surfaceHeight are still 0, VideoOutputManager will wait.
        Log.i(TAG, String.format(Locale.ENGLISH, "surfaceCreated: Calling videoOutputManager.trySignalSurfaceReady for ID: %d, handle: %d, width: %d, height: %d", this.id, this.videoOutputManagerHandle, this.surfaceWidth, this.surfaceHeight));
        videoOutputManager.trySignalSurfaceReady(videoOutputManagerHandle, id, this.surface, this.surfaceWidth, this.surfaceHeight);
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, String.format(Locale.ENGLISH, "surfaceChanged: Surface changed for VideoOutput ID: %d (handle: %d). New Format: %d, Width: %d, Height: %d. Holder: %s", this.id, this.videoOutputManagerHandle, format, width, height, holder.toString()));
        boolean dimensionsChanged = (this.surfaceWidth != width || this.surfaceHeight != height);
        this.surfaceWidth = width;
        this.surfaceHeight = height;
        if (this.surfaceCreatedCalled && dimensionsChanged && width > 0 && height > 0) {
            // If surface is already created and dimensions are now valid and have changed,
            // or if this is the first time we get valid dimensions after creation.
            Log.i(TAG, String.format(Locale.ENGLISH, "surfaceChanged: Conditions met (surfaceCreated, dimensionsChanged, valid size). Calling videoOutputManager.trySignalSurfaceReady for ID: %d, handle: %d, new width: %d, new height: %d", this.id, this.videoOutputManagerHandle, this.surfaceWidth, this.surfaceHeight));
            videoOutputManager.trySignalSurfaceReady(videoOutputManagerHandle, id, this.surface, this.surfaceWidth, this.surfaceHeight);
        } else {
            Log.i(TAG, String.format(Locale.ENGLISH, "surfaceChanged: Conditions NOT met for calling trySignalSurfaceReady. surfaceCreatedCalled: %b, dimensionsChanged: %b, width: %d, height: %d", this.surfaceCreatedCalled, dimensionsChanged, width, height));
        }
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        Log.i(TAG, String.format(Locale.ENGLISH, "surfaceDestroyed: Surface destroyed for VideoOutput ID: %d (handle: %d). Holder: %s", this.id, this.videoOutputManagerHandle, holder.toString()));
        this.surfaceCreatedCalled = false;
        // Notify VideoOutputManager that surface is destroyed
        if (this.surface != null) {
            Log.i(TAG, String.format(Locale.ENGLISH, "surfaceDestroyed: Calling videoOutputManager.onSurfaceDestroyed for ID: %d, handle: %d", this.id, this.videoOutputManagerHandle));
            videoOutputManager.onSurfaceDestroyed(videoOutputManagerHandle, id, this.surface);
            this.surface.release(); // Release the surface
            this.surface = null;
            Log.i(TAG, String.format(Locale.ENGLISH, "surfaceDestroyed: Local surface reference nulled for ID: %d", this.id));
        } else {
            Log.w(TAG, String.format(Locale.ENGLISH, "surfaceDestroyed: Local surface reference was already null for ID: %d", this.id));
        }
    }

    // Method to get the surface, though direct passing in callbacks is better.
    @Nullable
    public Surface getSurface() {
        return surface;
    }

    public void setFixedSurfaceSize(int width, int height) {
        if (surfaceView != null && surfaceView.getHolder() != null && width > 0 && height > 0) {
            Log.i(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. Holder.setFixedSize(%d, %d)", this.id, width, height));
            surfaceView.getHolder().setFixedSize(width, height);

            // New part: Update SurfaceView's LayoutParams
            android.view.ViewGroup.LayoutParams params = surfaceView.getLayoutParams();
            if (params != null) {
                params.width = width;
                params.height = height;
                surfaceView.setLayoutParams(params);
                Log.i(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. Updated SurfaceView LayoutParams to %d x %d.", this.id, width, height));
                surfaceView.requestLayout();
                Log.i(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. Called requestLayout() on SurfaceView.", this.id));
            } else {
                // This case should ideally not happen if SurfaceView is in a valid layout.
                // If it does, creating default FrameLayout.LayoutParams.
                Log.w(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. SurfaceView LayoutParams were null. Creating new FrameLayout.LayoutParams(%d, %d). This might indicate a layout issue higher up.", this.id, width, height));
                android.widget.FrameLayout.LayoutParams newParams = new android.widget.FrameLayout.LayoutParams(width, height);
                surfaceView.setLayoutParams(newParams);
                surfaceView.requestLayout();
                Log.i(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. Called requestLayout() on SurfaceView.", this.id));
            }

        } else {
            Log.w(TAG, String.format(Locale.ENGLISH, "setFixedSurfaceSize: VideoOutput ID: %d. Conditions not met for setting fixed size/layout params (width=%d, height=%d, holder/surfaceView null?).", this.id, width, height));
        }
    }
}
