// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.os.Build;
import android.util.SparseArray;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Video Capture Device extension of VideoCapture to provide common functionality
 * for capture using android.hardware.Camera API (deprecated in API 21). For Normal
 * Android devices, it provides functionality for receiving copies of preview
 * frames via Java-allocated buffers. It also includes class BuggyDeviceHack to
 * deal with troublesome devices.
 **/
@JNINamespace("media")
@SuppressWarnings("deprecation")
// TODO: is this class only used on ICS MR1 (or some later version) and above?
@TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1)
public class VideoCaptureCamera
        extends VideoCapture implements android.hardware.Camera.PreviewCallback {
    private static final String TAG = "VideoCapture";
    private static final int GL_TEXTURE_EXTERNAL_OES = 0x8D65;
    private static final int NUM_CAPTURE_BUFFERS = 3;

    // Map of the equivalent color temperature in Kelvin for the White Balance setting. The
    // values are a mixture of educated guesses and data from Android's Camera2 API. The
    // temperatures must be ordered increasingly.
    private static final SparseArray<String> COLOR_TEMPERATURES_MAP;
    static {
        COLOR_TEMPERATURES_MAP = new SparseArray<String>();
        COLOR_TEMPERATURES_MAP.append(
                2850, android.hardware.Camera.Parameters.WHITE_BALANCE_INCANDESCENT);
        COLOR_TEMPERATURES_MAP.append(
                2940, android.hardware.Camera.Parameters.WHITE_BALANCE_WARM_FLUORESCENT);
        COLOR_TEMPERATURES_MAP.append(
                3000, android.hardware.Camera.Parameters.WHITE_BALANCE_TWILIGHT);
        COLOR_TEMPERATURES_MAP.append(
                4230, android.hardware.Camera.Parameters.WHITE_BALANCE_FLUORESCENT);
        COLOR_TEMPERATURES_MAP.append(
                6000, android.hardware.Camera.Parameters.WHITE_BALANCE_CLOUDY_DAYLIGHT);
        COLOR_TEMPERATURES_MAP.append(
                6504, android.hardware.Camera.Parameters.WHITE_BALANCE_DAYLIGHT);
        COLOR_TEMPERATURES_MAP.append(7000, android.hardware.Camera.Parameters.WHITE_BALANCE_SHADE);
    };

    // Some devices don't support YV12 format correctly, even with JELLY_BEAN or
    // newer OS. To work around the issues on those devices, we have to request
    // NV21. This is supposed to be a temporary hack.
    private static class BuggyDeviceHack {
        private static final String[] COLORSPACE_BUGGY_DEVICE_LIST = {
                "SAMSUNG-SGH-I747", "ODROID-U2",
                // See https://crbug.com/577435 for more info.
                "XT1092", "XT1095", "XT1096", "XT1097",
        };

        static int getImageFormat() {
            for (String buggyDevice : COLORSPACE_BUGGY_DEVICE_LIST) {
                if (buggyDevice.contentEquals(android.os.Build.MODEL)) {
                    return ImageFormat.NV21;
                }
            }
            return ImageFormat.YV12;
        }
    }

    private int mExpectedFrameSize;
    private final Object mPhotoTakenCallbackLock = new Object();

    // Storage of takePicture() callback Id. There can be one such request in flight at most, and
    // needs to be exercised either in case of error or sucess.
    private long mPhotoTakenCallbackId;
    private int mPhotoWidth;
    private int mPhotoHeight;
    private android.hardware.Camera.Area mAreaOfInterest;

    private android.hardware.Camera mCamera;
    // Lock to mutually exclude execution of OnPreviewFrame() and {start/stop}Capture().
    private ReentrantLock mPreviewBufferLock = new ReentrantLock();
    // True when native code has started capture.
    private boolean mIsRunning;

    private int[] mGlTextures;
    private SurfaceTexture mSurfaceTexture;

    private static android.hardware.Camera.CameraInfo getCameraInfo(int id) {
        android.hardware.Camera.CameraInfo cameraInfo = new android.hardware.Camera.CameraInfo();
        try {
            android.hardware.Camera.getCameraInfo(id, cameraInfo);
        } catch (RuntimeException ex) {
            Log.e(TAG, "getCameraInfo: Camera.getCameraInfo: " + ex);
            return null;
        }
        return cameraInfo;
    }

    private static android.hardware.Camera.Parameters getCameraParameters(
            android.hardware.Camera camera) {
        android.hardware.Camera.Parameters parameters;
        try {
            parameters = camera.getParameters();
        } catch (RuntimeException ex) {
            Log.e(TAG, "getCameraParameters: android.hardware.Camera.getParameters: " + ex);
            if (camera != null) camera.release();
            return null;
        }
        return parameters;
    }

    private String getClosestWhiteBalance(int colorTemperature) {
        int minDiff = Integer.MAX_VALUE;
        String matchedTemperature = null;

        for (int i = 0; i < COLOR_TEMPERATURES_MAP.size(); ++i) {
            final int diff = Math.abs(colorTemperature - COLOR_TEMPERATURES_MAP.keyAt(i));
            if (diff >= minDiff) continue;
            minDiff = diff;
            matchedTemperature = COLOR_TEMPERATURES_MAP.valueAt(i);
        }
        return matchedTemperature;
    }

    private class CrErrorCallback implements android.hardware.Camera.ErrorCallback {
        @Override
        public void onError(int error, android.hardware.Camera camera) {
            nativeOnError(mNativeVideoCaptureDeviceAndroid, "Error id: " + error);

            synchronized (mPhotoTakenCallbackLock) {
                if (mPhotoTakenCallbackId == 0) return;
                nativeOnPhotoTaken(
                        mNativeVideoCaptureDeviceAndroid, mPhotoTakenCallbackId, new byte[0]);
                mPhotoTakenCallbackId = 0;
            }
        }
    }

    private class CrPictureCallback implements android.hardware.Camera.PictureCallback {
        @Override
        public void onPictureTaken(byte[] data, android.hardware.Camera camera) {
            synchronized (mPhotoTakenCallbackLock) {
                if (mPhotoTakenCallbackId != 0) {
                    nativeOnPhotoTaken(
                            mNativeVideoCaptureDeviceAndroid, mPhotoTakenCallbackId, data);
                }
                mPhotoTakenCallbackId = 0;
            }
            android.hardware.Camera.Parameters parameters = getCameraParameters(mCamera);
            parameters.setRotation(0);
            mCamera.setParameters(parameters);
            camera.startPreview();
        }
    };

    static int getNumberOfCameras() {
        return android.hardware.Camera.getNumberOfCameras();
    }

    static int getCaptureApiType(int id) {
        if (VideoCaptureCamera.getCameraInfo(id) == null) {
            return VideoCaptureApi.UNKNOWN;
        }
        return VideoCaptureApi.ANDROID_API1;
    }

    static String getName(int id) {
        android.hardware.Camera.CameraInfo cameraInfo = VideoCaptureCamera.getCameraInfo(id);
        if (cameraInfo == null) return null;

        return "camera " + id + ", facing "
                + (cameraInfo.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_FRONT
                                  ? "front"
                                  : "back");
    }

    static VideoCaptureFormat[] getDeviceSupportedFormats(int id) {
        android.hardware.Camera camera;
        try {
            camera = android.hardware.Camera.open(id);
        } catch (RuntimeException ex) {
            Log.e(TAG, "Camera.open: ", ex);
            return null;
        }
        android.hardware.Camera.Parameters parameters = getCameraParameters(camera);
        if (parameters == null) {
            return null;
        }

        ArrayList<VideoCaptureFormat> formatList = new ArrayList<VideoCaptureFormat>();
        // getSupportedPreview{Formats,FpsRange,PreviewSizes}() returns Lists
        // with at least one element, but when the camera is in bad state, they
        // can return null pointers; in that case we use a 0 entry, so we can
        // retrieve as much information as possible.
        List<Integer> pixelFormats = parameters.getSupportedPreviewFormats();
        if (pixelFormats == null) {
            pixelFormats = new ArrayList<Integer>();
        }
        if (pixelFormats.size() == 0) {
            pixelFormats.add(ImageFormat.UNKNOWN);
        }
        for (Integer previewFormat : pixelFormats) {
            int pixelFormat = AndroidImageFormat.UNKNOWN;
            if (previewFormat == ImageFormat.YV12) {
                pixelFormat = AndroidImageFormat.YV12;
            } else if (previewFormat == ImageFormat.NV21) {
                continue;
            }

            List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
            if (listFpsRange == null) {
                listFpsRange = new ArrayList<int[]>();
            }
            if (listFpsRange.size() == 0) {
                listFpsRange.add(new int[] {0, 0});
            }
            for (int[] fpsRange : listFpsRange) {
                List<android.hardware.Camera.Size> supportedSizes =
                        parameters.getSupportedPreviewSizes();
                if (supportedSizes == null) {
                    supportedSizes = new ArrayList<android.hardware.Camera.Size>();
                }
                if (supportedSizes.size() == 0) {
                    supportedSizes.add(camera.new Size(0, 0));
                }
                for (android.hardware.Camera.Size size : supportedSizes) {
                    formatList.add(new VideoCaptureFormat(
                            size.width, size.height, (fpsRange[1] + 999) / 1000, pixelFormat));
                }
            }
        }
        camera.release();
        return formatList.toArray(new VideoCaptureFormat[formatList.size()]);
    }

    VideoCaptureCamera(Context context, int id, long nativeVideoCaptureDeviceAndroid) {
        super(context, id, nativeVideoCaptureDeviceAndroid);
    }

    @Override
    public boolean allocate(int width, int height, int frameRate) {
        Log.d(TAG, "allocate: requested (%d x %d) @%dfps", width, height, frameRate);
        try {
            mCamera = android.hardware.Camera.open(mId);
        } catch (RuntimeException ex) {
            Log.e(TAG, "allocate: Camera.open: " + ex);
            return false;
        }

        android.hardware.Camera.CameraInfo cameraInfo = VideoCaptureCamera.getCameraInfo(mId);
        if (cameraInfo == null) {
            mCamera.release();
            mCamera = null;
            return false;
        }
        mCameraNativeOrientation = cameraInfo.orientation;
        // For Camera API, the readings of back-facing camera need to be inverted.
        mInvertDeviceOrientationReadings =
                (cameraInfo.facing == android.hardware.Camera.CameraInfo.CAMERA_FACING_BACK);
        Log.d(TAG, "allocate: Rotation dev=%d, cam=%d, facing back? %s", getDeviceRotation(),
                mCameraNativeOrientation, mInvertDeviceOrientationReadings);

        android.hardware.Camera.Parameters parameters = getCameraParameters(mCamera);
        if (parameters == null) {
            mCamera = null;
            return false;
        }

        // getSupportedPreviewFpsRange() returns a List with at least one
        // element, but when camera is in bad state, it can return null pointer.
        List<int[]> listFpsRange = parameters.getSupportedPreviewFpsRange();
        if (listFpsRange == null || listFpsRange.size() == 0) {
            Log.e(TAG, "allocate: no fps range found");
            return false;
        }
        final ArrayList<FramerateRange> framerateRanges =
                new ArrayList<FramerateRange>(listFpsRange.size());
        for (int[] range : listFpsRange) {
            framerateRanges.add(new FramerateRange(range[0], range[1]));
        }
        // API fps ranges are scaled up x1000 to avoid floating point.
        int frameRateScaled = frameRate * 1000;
        final FramerateRange chosenFramerateRange =
                getClosestFramerateRange(framerateRanges, frameRateScaled);
        final int[] chosenFpsRange = new int[] {chosenFramerateRange.min, chosenFramerateRange.max};
        Log.d(TAG, "allocate: fps set to [%d-%d]", chosenFpsRange[0], chosenFpsRange[1]);

        // Calculate size.
        List<android.hardware.Camera.Size> listCameraSize = parameters.getSupportedPreviewSizes();
        int minDiff = Integer.MAX_VALUE;
        int matchedWidth = width;
        int matchedHeight = height;
        for (android.hardware.Camera.Size size : listCameraSize) {
            int diff = Math.abs(size.width - width) + Math.abs(size.height - height);
            Log.d(TAG, "allocate: supported (%d, %d), diff=%d", size.width, size.height, diff);
            // TODO(wjia): Remove this hack (forcing width to be multiple
            // of 32) by supporting stride in video frame buffer.
            // Right now, VideoCaptureController requires compact YV12
            // (i.e., with no padding).
            if (diff < minDiff && (size.width % 32 == 0)) {
                minDiff = diff;
                matchedWidth = size.width;
                matchedHeight = size.height;
            }
        }
        if (minDiff == Integer.MAX_VALUE) {
            Log.e(TAG, "allocate: can not find a multiple-of-32 resolution");
            return false;
        }
        Log.d(TAG, "allocate: matched (%d x %d)", matchedWidth, matchedHeight);

        if (parameters.isVideoStabilizationSupported()) {
            Log.d(TAG, "Image stabilization supported, currently: "
                            + parameters.getVideoStabilization() + ", setting it.");
            parameters.setVideoStabilization(true);
        } else {
            Log.d(TAG, "Image stabilization not supported.");
        }

        if (parameters.getSupportedFocusModes().contains(
                    android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
            parameters.setFocusMode(android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
        } else {
            Log.d(TAG, "Continuous focus mode not supported.");
        }

        mCaptureFormat = new VideoCaptureFormat(matchedWidth, matchedHeight,
                chosenFpsRange[1] / 1000, BuggyDeviceHack.getImageFormat());
        parameters.setPictureSize(matchedWidth, matchedHeight);
        parameters.setPreviewSize(matchedWidth, matchedHeight);
        parameters.setPreviewFpsRange(chosenFpsRange[0], chosenFpsRange[1]);
        parameters.setPreviewFormat(mCaptureFormat.mPixelFormat);
        try {
            mCamera.setParameters(parameters);
        } catch (RuntimeException ex) {
            Log.e(TAG, "setParameters: " + ex);
            return false;
        }

        // Set SurfaceTexture. Android Capture needs a SurfaceTexture even if
        // it is not going to be used.
        mGlTextures = new int[1];
        // Generate one texture pointer and bind it as an external texture.
        GLES20.glGenTextures(1, mGlTextures, 0);
        GLES20.glBindTexture(GL_TEXTURE_EXTERNAL_OES, mGlTextures[0]);
        // No mip-mapping with camera source.
        GLES20.glTexParameterf(
                GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(
                GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        // Clamp to edge is only option.
        GLES20.glTexParameteri(
                GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(
                GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

        mSurfaceTexture = new SurfaceTexture(mGlTextures[0]);
        mSurfaceTexture.setOnFrameAvailableListener(null);
        try {
            mCamera.setPreviewTexture(mSurfaceTexture);
        } catch (IOException ex) {
            Log.e(TAG, "allocate: " + ex);
            return false;
        }

        mCamera.setErrorCallback(new CrErrorCallback());

        mExpectedFrameSize = mCaptureFormat.mWidth * mCaptureFormat.mHeight
                * ImageFormat.getBitsPerPixel(mCaptureFormat.mPixelFormat) / 8;
        for (int i = 0; i < NUM_CAPTURE_BUFFERS; i++) {
            byte[] buffer = new byte[mExpectedFrameSize];
            mCamera.addCallbackBuffer(buffer);
        }
        return true;
    }

    @Override
    public boolean startCapture() {
        if (mCamera == null) {
            Log.e(TAG, "startCapture: mCamera is null");
            return false;
        }

        mPreviewBufferLock.lock();
        try {
            if (mIsRunning) {
                return true;
            }
        } finally {
            mPreviewBufferLock.unlock();
        }

        setPreviewCallback(this);
        try {
            mCamera.startPreview();
        } catch (RuntimeException ex) {
            Log.e(TAG, "startCapture: Camera.startPreview: " + ex);
            return false;
        }

        mPreviewBufferLock.lock();
        try {
            nativeOnStarted(mNativeVideoCaptureDeviceAndroid);
            mIsRunning = true;
        } finally {
            mPreviewBufferLock.unlock();
        }
        return true;
    }

    @Override
    public boolean stopCapture() {
        if (mCamera == null) {
            Log.e(TAG, "stopCapture: mCamera is null");
            return true;
        }

        mPreviewBufferLock.lock();
        try {
            if (!mIsRunning) {
                return true;
            }
            mIsRunning = false;
        } finally {
            mPreviewBufferLock.unlock();
        }

        mCamera.stopPreview();
        setPreviewCallback(null);
        return true;
    }

    @Override
    public PhotoCapabilities getPhotoCapabilities() {
        final android.hardware.Camera.Parameters parameters = getCameraParameters(mCamera);
        PhotoCapabilities.Builder builder = new PhotoCapabilities.Builder();
        Log.i(TAG, " CAM params: %s", parameters.flatten());

        // Before the Camera2 API there was no official way to retrieve the supported, if any, ISO
        // values from |parameters|; some platforms had "iso-values", others "iso-mode-values" etc.
        // Ignore them.
        builder.setMinIso(0).setMaxIso(0).setCurrentIso(0).setStepIso(0);

        List<android.hardware.Camera.Size> supportedSizes = parameters.getSupportedPictureSizes();
        int minWidth = Integer.MAX_VALUE;
        int minHeight = Integer.MAX_VALUE;
        int maxWidth = 0;
        int maxHeight = 0;
        for (android.hardware.Camera.Size size : supportedSizes) {
            if (size.width < minWidth) minWidth = size.width;
            if (size.height < minHeight) minHeight = size.height;
            if (size.width > maxWidth) maxWidth = size.width;
            if (size.height > maxHeight) maxHeight = size.height;
        }
        builder.setMinHeight(minHeight).setMaxHeight(maxHeight).setStepHeight(1);
        builder.setMinWidth(minWidth).setMaxWidth(maxWidth).setStepWidth(1);
        final android.hardware.Camera.Size currentSize = parameters.getPreviewSize();
        builder.setCurrentHeight(currentSize.height).setCurrentWidth(currentSize.width);

        int maxZoom = 0;
        int currentZoom = 0;
        int minZoom = 0;
        int stepZoom = 0;
        if (parameters.isZoomSupported()) {
            maxZoom = parameters.getZoomRatios().get(parameters.getMaxZoom());
            currentZoom = parameters.getZoomRatios().get(parameters.getZoom());
            minZoom = parameters.getZoomRatios().get(0);
            if (parameters.getZoomRatios().size() > 1) {
                stepZoom = parameters.getZoomRatios().get(1) - parameters.getZoomRatios().get(0);
            }
        }
        builder.setMinZoom(minZoom).setMaxZoom(maxZoom);
        builder.setCurrentZoom(currentZoom).setStepZoom(stepZoom);

        Log.d(TAG, "parameters.getFocusMode(): %s", parameters.getFocusMode());
        final String focusMode = parameters.getFocusMode();

        // Classify the Focus capabilities. In CONTINUOUS and SINGLE_SHOT, we can call
        // autoFocus(AutoFocusCallback) to configure region(s) to focus onto.
        int jniFocusMode = AndroidMeteringMode.NONE;
        if (focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)
                || focusMode.equals(
                           android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)
                || focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_EDOF)) {
            jniFocusMode = AndroidMeteringMode.CONTINUOUS;
        } else if (focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_AUTO)
                || focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_MACRO)) {
            jniFocusMode = AndroidMeteringMode.SINGLE_SHOT;
        } else if (focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_INFINITY)
                || focusMode.equals(android.hardware.Camera.Parameters.FOCUS_MODE_FIXED)) {
            jniFocusMode = AndroidMeteringMode.FIXED;
        }
        builder.setFocusMode(jniFocusMode);

        // Exposure is usually continuously updated except it not available at all, or if the
        // exposure compensation is locked, in which case we consider it as FIXED.
        int jniExposureMode = parameters.getMaxNumMeteringAreas() == 0
                ? AndroidMeteringMode.NONE
                : AndroidMeteringMode.CONTINUOUS;
        if (parameters.isAutoExposureLockSupported() && parameters.getAutoExposureLock()) {
            jniExposureMode = AndroidMeteringMode.FIXED;
        }
        builder.setExposureMode(jniExposureMode);

        final float step = parameters.getExposureCompensationStep();
        builder.setStepExposureCompensation(step);
        builder.setMinExposureCompensation(parameters.getMinExposureCompensation() * step);
        builder.setMaxExposureCompensation(parameters.getMaxExposureCompensation() * step);
        builder.setCurrentExposureCompensation(parameters.getExposureCompensation() * step);

        int jniWhiteBalanceMode = AndroidMeteringMode.NONE;
        if (parameters.isAutoWhiteBalanceLockSupported()
                && parameters.getSupportedWhiteBalance() != null) {
            jniWhiteBalanceMode = parameters.getWhiteBalance()
                            == android.hardware.Camera.Parameters.WHITE_BALANCE_AUTO
                    ? AndroidMeteringMode.CONTINUOUS
                    : AndroidMeteringMode.FIXED;
        }
        builder.setWhiteBalanceMode(jniWhiteBalanceMode);

        builder.setMinColorTemperature(COLOR_TEMPERATURES_MAP.keyAt(0));
        builder.setMaxColorTemperature(
                COLOR_TEMPERATURES_MAP.keyAt(COLOR_TEMPERATURES_MAP.size() - 1));
        if (jniWhiteBalanceMode == AndroidMeteringMode.FIXED) {
            final int index = COLOR_TEMPERATURES_MAP.indexOfValue(parameters.getWhiteBalance());
            if (index >= 0) builder.setCurrentColorTemperature(COLOR_TEMPERATURES_MAP.keyAt(index));
        }

        if (parameters.getSupportedFlashModes() == null) {
            builder.setFillLightMode(AndroidFillLightMode.NONE);
        } else {
            switch (parameters.getFlashMode()) {
                case android.hardware.Camera.Parameters.FLASH_MODE_OFF:
                    builder.setFillLightMode(AndroidFillLightMode.OFF);
                    break;
                case android.hardware.Camera.Parameters.FLASH_MODE_AUTO:
                    builder.setFillLightMode(AndroidFillLightMode.AUTO);
                    break;
                case android.hardware.Camera.Parameters.FLASH_MODE_RED_EYE:
                    builder.setRedEyeReduction(true);
                    builder.setFillLightMode(AndroidFillLightMode.AUTO);
                    break;
                case android.hardware.Camera.Parameters.FLASH_MODE_ON:
                    builder.setFillLightMode(AndroidFillLightMode.FLASH);
                    break;
                case android.hardware.Camera.Parameters.FLASH_MODE_TORCH:
                    builder.setFillLightMode(AndroidFillLightMode.TORCH);
                    break;
                default:
                    builder.setFillLightMode(AndroidFillLightMode.NONE);
            }
        }

        return builder.build();
    }

    @Override
    public void setPhotoOptions(double zoom, int focusMode, int exposureMode, double width,
            double height, float[] pointsOfInterest2D, boolean hasExposureCompensation,
            double exposureCompensation, int whiteBalanceMode, double iso,
            boolean hasRedEyeReduction, boolean redEyeReduction, int fillLightMode,
            double colorTemperature) {
        android.hardware.Camera.Parameters parameters = getCameraParameters(mCamera);

        if (parameters.isZoomSupported() && zoom > 0) {
            // |zoomRatios| is an ordered list; need the closest zoom index for parameters.setZoom()
            final List<Integer> zoomRatios = parameters.getZoomRatios();
            int i = 1;
            for (; i < zoomRatios.size(); ++i) {
                if (zoom < zoomRatios.get(i)) {
                    break;
                }
            }
            parameters.setZoom(i - 1);
        }

        if (focusMode == AndroidMeteringMode.FIXED) {
            parameters.setFocusMode(android.hardware.Camera.Parameters.FOCUS_MODE_FIXED);
        } else if (focusMode == AndroidMeteringMode.SINGLE_SHOT) {
            parameters.setFocusMode(android.hardware.Camera.Parameters.FOCUS_MODE_AUTO);
        } else if (focusMode == AndroidMeteringMode.CONTINUOUS) {
            parameters.setFocusMode(
                    android.hardware.Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        }

        if (parameters.isAutoExposureLockSupported()) {
            if (exposureMode == AndroidMeteringMode.FIXED) {
                parameters.setAutoExposureLock(true);
            } else if (exposureMode != AndroidMeteringMode.NONE) {
                parameters.setAutoExposureLock(false);
            }
        }
        if (width > 0) mPhotoWidth = (int) Math.round(width);
        if (height > 0) mPhotoHeight = (int) Math.round(height);

        // Upon new |zoom| configuration, clear up the previous |mAreaOfInterest| if any.
        if (mAreaOfInterest != null && !mAreaOfInterest.rect.isEmpty() && zoom > 0) {
            mAreaOfInterest = null;
        }
        // Also clear |mAreaOfInterest| if the user sets it as NONE.
        if (focusMode == AndroidMeteringMode.NONE || exposureMode == AndroidMeteringMode.NONE) {
            mAreaOfInterest = null;
        }

        // Update |mAreaOfInterest| if the camera supports and there are |pointsOfInterest2D|.
        final boolean pointsOfInterestSupported =
                parameters.getMaxNumMeteringAreas() > 0 || parameters.getMaxNumFocusAreas() > 0;
        if (pointsOfInterestSupported && pointsOfInterest2D.length > 0) {
            assert pointsOfInterest2D.length == 1 : "Only 1 point of interest supported";
            assert pointsOfInterest2D[0] <= 1.0 && pointsOfInterest2D[0] >= 0.0;
            assert pointsOfInterest2D[1] <= 1.0 && pointsOfInterest2D[1] >= 0.0;
            // Calculate a Rect of 1/8 the canvas, which is fixed to Rect(-1000, -1000, 1000, 1000),
            // see https://developer.android.com/reference/android/hardware/Camera.Area.html
            final int centerX = Math.round(pointsOfInterest2D[0] * 2000) - 1000;
            final int centerY = Math.round(pointsOfInterest2D[1] * 2000) - 1000;
            final int regionWidth = 2000 / 8;
            final int regionHeight = 2000 / 8;
            final int weight = 1000;

            mAreaOfInterest = new android.hardware.Camera.Area(
                    new Rect(Math.max(-1000, centerX - regionWidth / 2),
                            Math.max(-1000, centerY - regionHeight / 2),
                            Math.min(1000, centerX + regionWidth / 2),
                            Math.min(1000, centerY + regionHeight / 2)),
                    weight);
            Log.d(TAG, "Area of interest %s", mAreaOfInterest.rect.toString());
        }
        if (mAreaOfInterest != null) {
            parameters.setFocusAreas(Arrays.asList(mAreaOfInterest));
            parameters.setMeteringAreas(Arrays.asList(mAreaOfInterest));
        }

        if (hasExposureCompensation) {
            final int unnormalizedExposureCompensation = (int) Math.round(
                    exposureCompensation / parameters.getExposureCompensationStep());
            parameters.setExposureCompensation(unnormalizedExposureCompensation);
        }

        // |iso| setting is not supported, see explanation in getPhotoCapabilities().

        // White Balance mode AndroidMeteringMode.SINGLE_SHOT is not supported.
        // TODO(mcasas): support FIXED mode, i.e. the scene mode.
        if (whiteBalanceMode == AndroidMeteringMode.CONTINUOUS
                && parameters.getSupportedWhiteBalance() != null) {
            // setWhiteBalance() will release the lock set with setAutoWhiteBalanceLock(), if any.
            parameters.setWhiteBalance(android.hardware.Camera.Parameters.WHITE_BALANCE_AUTO);
        } else if (whiteBalanceMode == AndroidMeteringMode.FIXED
                && parameters.isAutoWhiteBalanceLockSupported()) {
            parameters.setAutoWhiteBalanceLock(true);
            if (colorTemperature > 0.0) {
                final String closestSetting = getClosestWhiteBalance((int) colorTemperature);
                if (closestSetting != null) parameters.setWhiteBalance(closestSetting);
            }
        }

        // NONE is only used for getting capabilities, to signify "no flash unit". Ignore it.
        if (parameters.getSupportedFlashModes() != null
                && fillLightMode != AndroidFillLightMode.NOT_SET
                && fillLightMode != AndroidFillLightMode.NONE) {
            switch (fillLightMode) {
                case AndroidFillLightMode.OFF:
                    parameters.setFlashMode(android.hardware.Camera.Parameters.FLASH_MODE_OFF);
                    break;
                case AndroidFillLightMode.AUTO:
                    parameters.setFlashMode(hasRedEyeReduction && redEyeReduction
                                    ? android.hardware.Camera.Parameters.FLASH_MODE_RED_EYE
                                    : android.hardware.Camera.Parameters.FLASH_MODE_AUTO);
                    break;
                case AndroidFillLightMode.FLASH:
                    parameters.setFlashMode(android.hardware.Camera.Parameters.FLASH_MODE_ON);
                    break;
                case AndroidFillLightMode.TORCH:
                    parameters.setFlashMode(android.hardware.Camera.Parameters.FLASH_MODE_TORCH);
                    break;
                default:
            }
        }

        try {
            mCamera.setParameters(parameters);
        } catch (RuntimeException ex) {
            Log.e(TAG, "setParameters: ", ex);
            return;
        }

        if (focusMode != AndroidMeteringMode.SINGLE_SHOT) return;
        mCamera.autoFocus(new android.hardware.Camera.AutoFocusCallback() {
            @Override
            public void onAutoFocus(boolean success, android.hardware.Camera camera) {
                Log.d(TAG, "onAutoFocus() finished: %s ", success ? "success" : "failed");
            }
        });
    }

    @Override
    public boolean takePhoto(final long callbackId) {
        if (mCamera == null || !mIsRunning) {
            Log.e(TAG, "takePhoto: mCamera is null or is not running");
            return false;
        }

        // Only one picture can be taken at once.
        synchronized (mPhotoTakenCallbackLock) {
            if (mPhotoTakenCallbackId != 0) return false;
            mPhotoTakenCallbackId = callbackId;
        }

        android.hardware.Camera.Parameters parameters = getCameraParameters(mCamera);
        parameters.setRotation(getCameraRotation());
        final android.hardware.Camera.Size original_size = parameters.getPictureSize();

        List<android.hardware.Camera.Size> supportedSizes = parameters.getSupportedPictureSizes();
        android.hardware.Camera.Size closestSize = null;
        int minDiff = Integer.MAX_VALUE;
        for (android.hardware.Camera.Size size : supportedSizes) {
            final int diff = ((mPhotoWidth > 0) ? Math.abs(size.width - mPhotoWidth) : 0)
                    + ((mPhotoHeight > 0) ? Math.abs(size.height - mPhotoHeight) : 0);
            if (diff < minDiff) {
                minDiff = diff;
                closestSize = size;
            }
        }
        Log.d(TAG, "requested resolution: (%dx%d)", mPhotoWidth, mPhotoHeight);
        if (minDiff != Integer.MAX_VALUE) {
            Log.d(TAG, " matched (%dx%d)", closestSize.width, closestSize.height);
            parameters.setPictureSize(closestSize.width, closestSize.height);
        }

        try {
            mCamera.setParameters(parameters);
            mCamera.takePicture(null, null, null, new CrPictureCallback());
        } catch (RuntimeException ex) {
            Log.e(TAG, "takePicture ", ex);
            return false;
        }

        // Restore original parameters.
        parameters.setPictureSize(original_size.width, original_size.height);
        try {
            mCamera.setParameters(parameters);
        } catch (RuntimeException ex) {
            Log.e(TAG, "takePicture ", ex);
            return false;
        }
        return true;
    }

    @Override
    public void deallocate() {
        if (mCamera == null) return;

        stopCapture();
        try {
            mCamera.setPreviewTexture(null);
            if (mGlTextures != null) GLES20.glDeleteTextures(1, mGlTextures, 0);
            mCaptureFormat = null;
            mCamera.release();
            mCamera = null;
        } catch (IOException ex) {
            Log.e(TAG, "deallocate: failed to deallocate camera, " + ex);
            return;
        }
    }

    private void setPreviewCallback(android.hardware.Camera.PreviewCallback cb) {
        mCamera.setPreviewCallbackWithBuffer(cb);
    }

    @Override
    public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
        mPreviewBufferLock.lock();
        try {
            if (!mIsRunning) {
                return;
            }
            if (data.length == mExpectedFrameSize) {
                nativeOnFrameAvailable(mNativeVideoCaptureDeviceAndroid, data, mExpectedFrameSize,
                        getCameraRotation());
            }
        } finally {
            mPreviewBufferLock.unlock();
            if (camera != null) {
                camera.addCallbackBuffer(data);
            }
        }
    }
}
