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
import android.view.Surface;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Objects;

import io.flutter.embedding.engine.plugins.FlutterPlugin;

public class VideoOutputManager {
    private static final String TAG = "VideoOutputManager";
    private final FlutterPlugin.FlutterPluginBinding flutterPluginBinding;

    // Maps player handle to TextureUpdateCallback
    private final HashMap<Long, TextureUpdateCallback> textureUpdateCallbacks = new HashMap<>();
    // Maps player handle to its active Surface, identified by PlatformView ID.
    private final HashMap<Long, SurfaceHolderDetails> activeSurfaces = new HashMap<>();
    // Tracks PlatformView IDs for which the initial surface ready signal has been sent.
    private final HashSet<Integer> signaledPlatformViewIds = new HashSet<>();
    // Stores references to active VideoOutput instances, keyed by their PlatformView ID.
    private final HashMap<Integer, VideoOutput> videoOutputInstances = new HashMap<>();
    private final Object lock = new Object();

    // Reflection for MediaKitAndroidHelper
    private static final Method newGlobalObjectRef;
    private static final Method deleteGlobalObjectRef;
    private static final HashSet<Long> deletedGlobalObjectRefs = new HashSet<>();
    private static final Handler handler = new Handler(Looper.getMainLooper());


    static {
        try {
            Class<?> mediaKitAndroidHelperClass = Class.forName("com.alexmercerind.mediakitandroidhelper.MediaKitAndroidHelper");
            newGlobalObjectRef = mediaKitAndroidHelperClass.getDeclaredMethod("newGlobalObjectRef", Object.class);
            deleteGlobalObjectRef = mediaKitAndroidHelperClass.getDeclaredMethod("deleteGlobalObjectRef", long.class);
            newGlobalObjectRef.setAccessible(true);
            deleteGlobalObjectRef.setAccessible(true);
        } catch (Throwable e) {
            Log.i("media_kit", "package:media_kit_libs_android_video missing. Make sure you have added it to pubspec.yaml.");
            throw new RuntimeException("Failed to initialize com.alexmercerind.media_kit_video.VideoOutputManager due to missing MediaKitAndroidHelper.", e);
        }
    }

    // To store details about the Surface provided by a PlatformView
    private static class SurfaceHolderDetails {
        final int platformViewId;
        final Surface surface;
        long wid = 0; // Native window ID from newGlobalObjectRef

        SurfaceHolderDetails(int platformViewId, Surface surface) {
            this.platformViewId = platformViewId;
            this.surface = surface;
        }
    }

    VideoOutputManager(FlutterPlugin.FlutterPluginBinding flutterPluginBinding) {
        this.flutterPluginBinding = flutterPluginBinding;
    }

    public void registerViewFactory() {
        final String viewType = "com.alexmercerind/media_kit_video_view";
        Log.i(TAG, "Registering PlatformView factory for view type: " + viewType);
        flutterPluginBinding.getPlatformViewRegistry().registerViewFactory(
                viewType,
                new VideoOutputFactory(this)
        );
        Log.i(TAG, "PlatformView factory registered successfully.");
    }

    public void registerVideoOutputView(int platformViewId, VideoOutput view) {
        synchronized(lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "registerVideoOutputView: Registering VideoOutput instance for platformViewId: %d", platformViewId));
            videoOutputInstances.put(platformViewId, view);
        }
    }

    // Called when a new player instance is created (from Dart)
    // to associate its handle with a callback for surface updates.
    public void create(long handle, TextureUpdateCallback textureUpdateCallback) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "create: called for handle: %d", handle));
            if (!textureUpdateCallbacks.containsKey(handle)) {
                textureUpdateCallbacks.put(handle, textureUpdateCallback);
                Log.i(TAG, String.format(Locale.ENGLISH, "create: TextureUpdateCallback stored for handle: %d", handle));
            } else {
                Log.w(TAG, String.format(Locale.ENGLISH, "create: Handle %d already has a TextureUpdateCallback.", handle));
            }
        }
    }

    // Renamed from onSurfaceAvailable. This is called internally by trySignalSurfaceReady.
    private void actuallySignalSurfaceReady(long handle, int platformViewId, Surface surface, int width, int height) {
        // This method assumes it's already within a synchronized (lock) block from trySignalSurfaceReady.
        Log.i(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: ENTER for handle: %d, platformViewId: %d, surface: %s, width: %d, height: %d", handle, platformViewId, (surface != null ? surface.toString() : "null"), width, height));
        TextureUpdateCallback callback = textureUpdateCallbacks.get(handle);
        if (callback == null) {
            Log.e(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: No TextureUpdateCallback found for handle: %d. Exiting.", handle));
                return;
            }

            // If there was an old surface for this handle, release its WID
            SurfaceHolderDetails oldSurfaceDetails = activeSurfaces.get(handle);
            if (oldSurfaceDetails != null && oldSurfaceDetails.wid != 0) {
                final long oldWid = oldSurfaceDetails.wid;
                 // It's important this runs on the main thread or wherever deleteGlobalObjectRef is safe to call
                handler.post(() -> deleteGlobalObjectRef(oldWid));
                Log.i(TAG, String.format(Locale.ENGLISH, "Released old WID %d for handle %d", oldWid, handle));
            }
            
            long wid = newGlobalObjectRef(surface);
            Log.i(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: newGlobalObjectRef returned WID: %d for handle: %d", wid, handle));
            if (wid == 0) {
                Log.e(TAG, String.format(Locale.ENGLISH, "Failed to create newGlobalObjectRef for surface on handle: %d", handle));
                Log.e(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: WID is 0, cannot use this surface for handle: %d.", handle));
                // Notify with wid = 0 to indicate failure or cleanup
                callback.onTextureUpdate(handle, 0, 0, 0); // Assuming 0,0 for size until changed
                return;
            }

            activeSurfaces.put(handle, new SurfaceHolderDetails(platformViewId, surface));
            activeSurfaces.get(handle).wid = wid;

            // Notify Dart/C++ about the new surface (WID) and its dimensions.
            Log.i(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: About to invoke TextureUpdateCallback for handle: %d with WID: %d, Width: %d, Height: %d", handle, wid, width, height));
            callback.onTextureUpdate(handle, wid, width, height);
            Log.i(TAG, String.format(Locale.ENGLISH, "Notified TextureUpdateCallback for handle: %d with WID: %d, Size: %dx%d", handle, wid, width, height));
        // This method is now private and called from trySignalSurfaceReady, which is synchronized.
        // No separate synchronized block needed here.
        Log.i(TAG, String.format(Locale.ENGLISH, "actuallySignalSurfaceReady: EXIT for handle: %d, platformViewId: %d", handle, platformViewId));
    }

    public void trySignalSurfaceReady(long handle, int platformViewId, Surface surface, int width, int height) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "trySignalSurfaceReady: called for handle: %d, platformViewId: %d, surface: %s, width: %d, height: %d", handle, platformViewId, (surface != null ? surface.toString() : "null"), width, height));
            // If already signaled for this specific PlatformView ID & Surface instance, and dimensions are the same or invalid, ignore.
            // This check has been refined to be more specific about what "already signaled" means.
            SurfaceHolderDetails existingSurface = activeSurfaces.get(handle);
            if (signaledPlatformViewIds.contains(platformViewId) && existingSurface != null && existingSurface.surface == surface && existingSurface.platformViewId == platformViewId) {
                 Log.i(TAG, String.format(Locale.ENGLISH, "trySignalSurfaceReady: Already signaled for PlatformView ID: %d. Skipping.", platformViewId));
                 return;
            }
            // If it was signaled, but this is a new surface (e.g. after resume) or different handle, allow re-signal.
            // onSurfaceDestroyed should have cleared signaledPlatformViewIds for this platformViewId if it was truly destroyed.

            if (surface == null || !surface.isValid() || width <= 0 || height <= 0) { // surface.isValid() check added
                Log.w(TAG, String.format(Locale.ENGLISH, "trySignalSurfaceReady: Conditions not met. Surface null or invalid dimensions for PlatformView ID: %d. Surface: %s, Width: %d, Height: %d", platformViewId, (surface != null ? surface.toString() : "null"), width, height));
                return;
            }
            
            Log.i(TAG, String.format(Locale.ENGLISH, "trySignalSurfaceReady: Conditions met. Calling actuallySignalSurfaceReady for PlatformView ID: %d, handle: %d", platformViewId, handle));
            signaledPlatformViewIds.add(platformViewId);
            actuallySignalSurfaceReady(handle, platformViewId, surface, width, height);
        }
    }

    // Called by VideoOutput when its surface is destroyed
    public void onSurfaceDestroyed(long handle, int platformViewId, Surface surface) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "Surface destroyed for handle: %d, PlatformView ID: %d", handle, platformViewId));
            signaledPlatformViewIds.remove(platformViewId); // Allow re-signaling if this ID is reused.
            TextureUpdateCallback callback = textureUpdateCallbacks.get(handle);
            SurfaceHolderDetails activeSurface = activeSurfaces.get(handle);

            if (activeSurface != null && activeSurface.platformViewId == platformViewId) {
                if (callback != null) {
                    // Notify Dart/C++ that surface is gone (WID = 0)
                    callback.onTextureUpdate(handle, 0, 0, 0);
                    Log.i(TAG, String.format(Locale.ENGLISH, "Notified TextureUpdateCallback (surface destroyed) for handle: %d", handle));
                }
                if (activeSurface.wid != 0) {
                    final long widToRelease = activeSurface.wid;
                    handler.post(() -> deleteGlobalObjectRef(widToRelease)); // Ensure deletion on correct thread
                    Log.i(TAG, String.format(Locale.ENGLISH, "Scheduled release of WID %d for handle %d", widToRelease, handle));
                }
                activeSurfaces.remove(handle);
            } else {
                Log.w(TAG, String.format(Locale.ENGLISH, "Surface destroyed for handle %d, but no matching active surface/PlatformView ID %d found.", handle, platformViewId));
            }
        }
    }
    
    // Called by VideoOutput when the PlatformView itself is disposed
    public void onPlatformViewDisposed(long handle, int platformViewId) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "onPlatformViewDisposed: Disposing for platformViewId: %d, handle: %d", platformViewId, handle));
            signaledPlatformViewIds.remove(platformViewId); // Allow re-signaling if this ID is reused.
            Log.i(TAG, String.format(Locale.ENGLISH, "onPlatformViewDisposed: Removing VideoOutput instance for platformViewId: %d", platformViewId));
            videoOutputInstances.remove(platformViewId);
            // This method might be redundant if onSurfaceDestroyed already handles cleanup.
            // However, it's good for logging and ensuring state consistency.
            SurfaceHolderDetails activeSurface = activeSurfaces.get(handle);
            if (activeSurface != null && activeSurface.platformViewId == platformViewId) {
                // If surface wasn't already destroyed and WID released, do it now.
                // This is a safeguard.
                if (activeSurface.wid != 0) {
                    final long widToRelease = activeSurface.wid;
                    Log.w(TAG, String.format(Locale.ENGLISH, "PlatformView %d for handle %d disposed, ensuring WID %d is released.", platformViewId, handle, widToRelease));
                    handler.post(() -> deleteGlobalObjectRef(widToRelease));
                }
                activeSurfaces.remove(handle);
            }
        }
    }


    public void dispose(long handle) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Disposing for handle: %d", handle));
            textureUpdateCallbacks.remove(handle);

            SurfaceHolderDetails activeSurface = activeSurfaces.remove(handle);
            if (activeSurface != null) {
                signaledPlatformViewIds.remove(activeSurface.platformViewId); 
                Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Removing VideoOutput instance for platformViewId: %d due to handle %d disposal.", activeSurface.platformViewId, handle));
                videoOutputInstances.remove(activeSurface.platformViewId);
                if (activeSurface.wid != 0) {
                    final long widToRelease = activeSurface.wid;
                    Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Releasing WID %d for disposed handle %d", widToRelease, handle));
                    handler.post(() -> deleteGlobalObjectRef(widToRelease));
                }
            }
            Log.i(TAG, String.format(Locale.ENGLISH, "dispose: Cleaned up resources for handle: %d", handle));
        }
    }

    // This method might not be directly used if SurfaceView handles its own size.
    // However, the player might need to be informed of the size.
    // The TextureUpdateCallback sends width/height, so this might be less relevant now.
    public void setSurfaceSize(long handle, int width, int height) {
        synchronized (lock) {
            Log.i(TAG, String.format(Locale.ENGLISH, "setSurfaceSize called for handle: %d, Size: %dx%d", handle, width, height));
            
            SurfaceHolderDetails activeSurfaceDetails = activeSurfaces.get(handle);
            if (activeSurfaceDetails == null) {
                Log.w(TAG, String.format(Locale.ENGLISH, "setSurfaceSize: No active surface details found for handle: %d. Cannot set fixed surface size.", handle));
                // Optionally, still proceed to notify Dart if a callback exists, or return.
                // For now, let's allow it to proceed to the callback logic.
            } else {
                VideoOutput videoOutputInstance = videoOutputInstances.get(activeSurfaceDetails.platformViewId);
                if (videoOutputInstance != null) {
                    Log.i(TAG, String.format(Locale.ENGLISH, "setSurfaceSize: Calling setFixedSurfaceSize(%d, %d) on VideoOutput instance for handle %d, platformViewId %d", width, height, handle, activeSurfaceDetails.platformViewId));
                    videoOutputInstance.setFixedSurfaceSize(width, height);
                } else {
                    Log.w(TAG, String.format(Locale.ENGLISH, "setSurfaceSize: No VideoOutput instance found for platformViewId: %d (handle: %d). Cannot set fixed surface size.", activeSurfaceDetails.platformViewId, handle));
                }
            }

            // With PlatformView, the SurfaceView's SurfaceHolder.Callback (surfaceChanged)
            // will provide size information. The VideoOutput can then call back to VideoOutputManager,
            // which can then use TextureUpdateCallback to inform Dart/C++.
            // This direct call might still be used by the player to enforce a size.

            TextureUpdateCallback callback = textureUpdateCallbacks.get(handle);
            // SurfaceHolderDetails activeSurface = activeSurfaces.get(handle); // Renamed to activeSurfaceDetails

            if (callback != null && activeSurfaceDetails != null && activeSurfaceDetails.wid != 0) {
                // Notify about size change. The WID remains the same.
                callback.onTextureUpdate(handle, activeSurfaceDetails.wid, width, height);
                Log.i(TAG, String.format(Locale.ENGLISH, "Notified TextureUpdateCallback (size change) for handle: %d, WID: %d, Size: %dx%d", handle, activeSurfaceDetails.wid, width, height));
            } else {
                Log.w(TAG, String.format(Locale.ENGLISH, "Cannot set surface size for handle %d. Callback or active surface not found.", handle));
            }
        }
    }

    // Static methods for JNI bridge (GlobalObjectRef)
    private static long newGlobalObjectRef(Object object) {
        Log.i(TAG, String.format(Locale.ENGLISH, "static newGlobalObjectRef: input surface = %s", (object != null ? object.toString() : "null")));
        try {
            // Ensure this is called on a thread where JNI operations are safe, typically main thread for UI objects.
            long resultHandle = (long) Objects.requireNonNull(newGlobalObjectRef.invoke(null, object));
            Log.i(TAG, String.format(Locale.ENGLISH, "static newGlobalObjectRef: returned WID = %d for surface = %s", resultHandle, (object != null ? object.toString() : "null")));
            return resultHandle;
        } catch (Throwable e) {
            Log.e(TAG, "static newGlobalObjectRef: ERROR", e);
            return 0;
        }
    }

    private static void deleteGlobalObjectRef(long ref) {
        if (ref == 0) {
            Log.w(TAG, "static deleteGlobalObjectRef: Attempted to delete a global object reference of 0. Ignoring.");
            return;
        }
        // Synchronize access to deletedGlobalObjectRefs if accessed from multiple threads, though handler.post should serialize it.
        if (deletedGlobalObjectRefs.contains(ref)) {
            Log.i(TAG, String.format(Locale.ENGLISH, "static deleteGlobalObjectRef: WID = %d ALREADY DELETED or scheduled for deletion. Skipping.", ref));
            return;
        }
        if (deletedGlobalObjectRefs.size() > 100) { // Simple cleanup to prevent unbounded growth
            Log.i(TAG, String.format(Locale.ENGLISH, "static deleteGlobalObjectRef: Cleared deletedGlobalObjectRefs cache (size was %d).", deletedGlobalObjectRefs.size()));
            deletedGlobalObjectRefs.clear();
        }
        deletedGlobalObjectRefs.add(ref);
        Log.i(TAG, String.format(Locale.ENGLISH, "static deleteGlobalObjectRef: called for WID = %d", ref));
        try {
            // Ensure this is called on a thread where JNI operations are safe.
            deleteGlobalObjectRef.invoke(null, ref);
            Log.i(TAG, String.format(Locale.ENGLISH, "static deleteGlobalObjectRef: Successfully deleted WID = %d", ref));
        } catch (Throwable e) {
            Log.e(TAG, "static deleteGlobalObjectRef: ERROR for WID = " + ref, e);
        }
    }
}
