// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.net.Uri;
import android.os.StrictMode;
import android.provider.Browser;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ui.BackendProvider;
import org.chromium.chrome.browser.download.ui.BackendProvider.DownloadDelegate;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.DownloadState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {

    /** Strings indicating how many bytes have been downloaded for different units. */
    @VisibleForTesting
    static final int[] BYTES_DOWNLOADED_STRINGS = {
        R.string.file_size_downloaded_kb,
        R.string.file_size_downloaded_mb,
        R.string.file_size_downloaded_gb
    };

    private static final String TAG = "download";

    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIME_TYPE_DELIMITER = "/";
    private static final String MIME_TYPE_VIDEO = "video";

    private static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";

    private static final long BYTES_PER_KILOBYTE = 1024;
    private static final long BYTES_PER_MEGABYTE = 1024 * 1024;
    private static final long BYTES_PER_GIGABYTE = 1024 * 1024 * 1024;

    @VisibleForTesting
    static final String ELLIPSIS = "\u2026";

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab) {
        // Figure out what tab was last being viewed by the user.
        if (activity == null) activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (tab == null && activity instanceof ChromeTabbedActivity) {
            tab = ((ChromeTabbedActivity) activity).getActivityTab();
        }

        Context appContext = ContextUtils.getApplicationContext();
        if (DeviceFormFactor.isTablet(appContext)) {
            // Download Home shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.DOWNLOADS_URL);
            if (tab == null || !tab.isInitialized()) {
                // Open a new tab, which pops Chrome into the foreground.
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                // Download Home shows up inside an existing tab, but only if the last Activity was
                // the ChromeTabbedActivity.
                tab.loadUrl(params);

                // Bring Chrome to the foreground, if possible.
                Intent intent = Tab.createBringTabToFrontIntent(tab.getId());
                if (intent != null) {
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    IntentUtils.safeStartActivity(appContext, intent);
                }
            }
        } else {
            // Download Home shows up as a new Activity on phones.
            Intent intent = new Intent();
            intent.setClass(appContext, DownloadActivity.class);
            if (tab != null) intent.putExtra(EXTRA_IS_OFF_THE_RECORD, tab.isIncognito());
            if (activity == null) {
                // Stands alone in its own task.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                appContext.startActivity(intent);
            } else {
                // Sits on top of another Activity.
                intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
                activity.startActivity(intent);
            }
        }

        return true;
    }

    /**
     * @return Whether or not the Intent corresponds to a DownloadActivity that should show off the
     *         record downloads.
     */
    public static boolean shouldShowOffTheRecordDownloads(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
    }

    /**
     * Records metrics related to downloading a page. Should be called after a tap on the download
     * page button.
     * @param tab The Tab containing the page being downloaded.
     */
    public static void recordDownloadPageMetrics(Tab tab) {
        RecordHistogram.recordPercentageHistogram("OfflinePages.SavePage.PercentLoaded",
                tab.getProgress());
    }

    /**
     * Shows a "Downloading..." toast. Should be called after a download has been started.
     * @param context The {@link Context} used to make the toast.
     */
    public static void showDownloadStartToast(Context context) {
        Toast.makeText(context, R.string.download_started, Toast.LENGTH_SHORT).show();
    }

    /**
     * Issues a request to the {@link DownloadDelegate} associated with backendProvider to check
     * for externally removed downloads.
     * See {@link DownloadManagerService#checkForExternallyRemovedDownloads}.
     *
     * @param backendProvider The {@link BackendProvider} associated with the DownloadDelegate used
     *                        to check for externally removed downloads.
     * @param isOffTheRecord  Whether to check downloads for the off the record profile.
     */
    public static void checkForExternallyRemovedDownloads(BackendProvider backendProvider,
            boolean isOffTheRecord) {
        if (isOffTheRecord) {
            backendProvider.getDownloadDelegate().checkForExternallyRemovedDownloads(true);
        }
        backendProvider.getDownloadDelegate().checkForExternallyRemovedDownloads(false);
        RecordUserAction.record(
                "Android.DownloadManager.CheckForExternallyRemovedItems");
    }

    /**
     * Trigger the download of an Offline Page.
     * @param context Context to pull resources from.
     */
    public static void downloadOfflinePage(Context context, Tab tab) {
        if (tab.isShowingErrorPage()) {
            // The download needs to be scheduled to happen at later time due to current network
            // error.
            ClientId clientId = ClientId.createGuidClientIdForNamespace(
                    OfflinePageBridge.ASYNC_NAMESPACE);
            final OfflinePageBridge bridge = OfflinePageBridge.getForProfile(tab.getProfile());
            bridge.savePageLater(tab.getUrl(), clientId, true /* userRequested */);
        } else {
            // Otherwise, the download can be started immediately.
            final OfflinePageDownloadBridge bridge =
                    new OfflinePageDownloadBridge(tab.getProfile());
            bridge.startDownload(tab);
            bridge.destroy();
            DownloadUtils.recordDownloadPageMetrics(tab);
        }
    }

    /**
     * Whether the user should be allowed to download the current page.
     * @param tab Tab displaying the page that will be downloaded.
     * @return    Whether the "Download Page" button should be enabled.
     */
    public static boolean isAllowedToDownloadPage(Tab tab) {
        if (tab == null) return false;

        // Offline pages isn't supported in Incognito. This should be checked before calling
        // OfflinePageBridge.getForProfile because OfflinePageBridge instance will not be found
        // for incognito profile.
        if (tab.isIncognito()) return false;

        // Check if the page url is supported for saving. Only HTTP and HTTPS pages are allowed.
        if (!OfflinePageBridge.canSavePage(tab.getUrl())) return false;

        // Download will only be allowed for the error page if download button is shown in the page.
        if (tab.isShowingErrorPage()) {
            final OfflinePageBridge bridge = OfflinePageBridge.getForProfile(tab.getProfile());
            return bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents());
        }

        if (tab.isShowingInterstitialPage()) return false;

        // Don't allow re-downloading the currently displayed offline page.
        if (tab.isOfflinePage()) return false;

        return true;
    }

    /**
     * Creates an Intent to open the file in another app by firing an Intent to Android.
     * @param fileUri  Uri pointing to the file.
     * @param mimeType MIME type for the file.
     * @return Intent that can be used to start an Activity for the file.
     */
    public static Intent createViewIntentForDownloadItem(Uri fileUri, String mimeType) {
        Intent fileIntent = new Intent(Intent.ACTION_VIEW);
        String normalizedMimeType = Intent.normalizeMimeType(mimeType);
        if (TextUtils.isEmpty(normalizedMimeType)) {
            fileIntent.setData(fileUri);
        } else {
            fileIntent.setDataAndType(fileUri, normalizedMimeType);
        }
        fileIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        fileIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return fileIntent;
    }

    /**
     * Creates an Intent to share {@code items} with another app by firing an Intent to Android.
     *
     * Sharing a DownloadItem shares the file itself, while sharing an OfflinePageItem shares the
     * URL.
     *
     * @param items Items to share.
     * @return      Intent that can be used to share the items.
     */
    public static Intent createShareIntent(List<DownloadHistoryItemWrapper> items) {
        Intent shareIntent = new Intent();
        String intentAction;
        ArrayList<Uri> itemUris = new ArrayList<Uri>();
        StringBuilder offlinePagesString = new StringBuilder();
        int selectedItemsFilterType = items.get(0).getFilterType();

        String intentMimeType = "";
        String[] intentMimeParts = {"", ""};

        for (int i = 0; i < items.size(); i++) {
            DownloadHistoryItemWrapper wrappedItem  = items.get(i);

            if (wrappedItem instanceof DownloadHistoryItemWrapper.OfflinePageItemWrapper) {
                if (offlinePagesString.length() != 0) {
                    offlinePagesString.append("\n");
                }
                offlinePagesString.append(wrappedItem.getUrl());
            } else {
                itemUris.add(getUriForItem(wrappedItem.getFile()));
            }

            if (selectedItemsFilterType != wrappedItem.getFilterType()) {
                selectedItemsFilterType = DownloadFilter.FILTER_ALL;
            }
            if (wrappedItem.getFilterType() == DownloadFilter.FILTER_OTHER) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.DownloadManager.OtherExtensions.Share",
                        wrappedItem.getFileExtensionType(),
                        DownloadHistoryItemWrapper.FILE_EXTENSION_BOUNDARY);
            }

            String mimeType = Intent.normalizeMimeType(wrappedItem.getMimeType());

            // If a mime type was not retrieved from the backend or could not be normalized,
            // set the mime type to the default.
            if (TextUtils.isEmpty(mimeType)) {
                intentMimeType = DEFAULT_MIME_TYPE;
                continue;
            }

            // If the intent mime type has not been set yet, set it to the mime type for this item.
            if (TextUtils.isEmpty(intentMimeType)) {
                intentMimeType = mimeType;
                if (!TextUtils.isEmpty(intentMimeType)) {
                    intentMimeParts = intentMimeType.split(MIME_TYPE_DELIMITER);
                    // Guard against invalid mime types.
                    if (intentMimeParts.length != 2) intentMimeType = DEFAULT_MIME_TYPE;
                }
                continue;
            }

            // Either the mime type is already the default or it matches the current item's mime
            // type. In either case, intentMimeType is already the correct value.
            if (TextUtils.equals(intentMimeType, DEFAULT_MIME_TYPE)
                    || TextUtils.equals(intentMimeType, mimeType)) {
                continue;
            }

            String[] mimeParts = mimeType.split(MIME_TYPE_DELIMITER);
            if (!TextUtils.equals(intentMimeParts[0], mimeParts[0])) {
                // The top-level types don't match; fallback to the default mime type.
                intentMimeType = DEFAULT_MIME_TYPE;
            } else {
                // The mime type should be {top-level type}/*
                intentMimeType = intentMimeParts[0] + MIME_TYPE_DELIMITER + "*";
            }
        }

        // Use Action_SEND if there is only one downloaded item or only text to share.
        if (itemUris.size() == 0 || (itemUris.size() == 1 && offlinePagesString.length() == 0)) {
            intentAction = Intent.ACTION_SEND;
        } else {
            intentAction = Intent.ACTION_SEND_MULTIPLE;
        }

        if (itemUris.size() == 1 && offlinePagesString.length() == 0) {
            // Sharing a DownloadItem.
            shareIntent.putExtra(Intent.EXTRA_STREAM, getUriForItem(items.get(0).getFile()));
        } else {
            shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, itemUris);
        }

        if (offlinePagesString.length() != 0) {
            shareIntent.putExtra(Intent.EXTRA_TEXT, offlinePagesString.toString());
        }

        shareIntent.setAction(intentAction);
        shareIntent.setType(intentMimeType);
        shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        recordShareHistograms(items.size(), selectedItemsFilterType);

        return shareIntent;
    }

    private static Intent createShareIntent(Uri fileUri, String mimeType) {
        if (TextUtils.isEmpty(mimeType)) mimeType = DEFAULT_MIME_TYPE;

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra(Intent.EXTRA_STREAM, fileUri);
        intent.setType(mimeType);
        return intent;
    }

    /**
     * Creates an Intent that allows viewing the given file in an internal media viewer.
     * @param fileUri    URI pointing at the file, ideally in file:// form.  Used only when
     *                   the media viewer is trying to locate the file on disk.
     * @param contentUri content:// URI pointing at the file.
     * @param mimeType   MIME type of the file.
     * @return Intent that can be fired to open the file.
     */
    public static Intent getMediaViewerIntentForDownloadItem(
            Uri fileUri, Uri contentUri, String mimeType) {
        Context context = ContextUtils.getApplicationContext();
        Intent viewIntent = createViewIntentForDownloadItem(contentUri, mimeType);

        Bitmap closeIcon = BitmapFactory.decodeResource(
                context.getResources(), R.drawable.ic_arrow_back_white_24dp);
        Bitmap shareIcon = BitmapFactory.decodeResource(
                context.getResources(), R.drawable.ic_share_white_24dp);

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setToolbarColor(Color.BLACK);
        builder.setCloseButtonIcon(closeIcon);
        builder.setShowTitle(true);

        // Create a PendingIntent that can be used to view the file externally.
        // TODO(dfalcantara): Check if this is problematic in multi-window mode, where two
        //                    different viewers could be visible at the same time.
        Intent chooserIntent = Intent.createChooser(viewIntent, null);
        chooserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        String openWithStr = context.getString(R.string.download_manager_open_with);
        PendingIntent pendingViewIntent = PendingIntent.getActivity(
                context, 0, chooserIntent, PendingIntent.FLAG_CANCEL_CURRENT);
        builder.addMenuItem(openWithStr, pendingViewIntent);

        // Create a PendingIntent that shares the file with external apps.
        PendingIntent pendingShareIntent = PendingIntent.getActivity(
                context, 0, createShareIntent(contentUri, mimeType), 0);
        builder.setActionButton(
                shareIcon, context.getString(R.string.share), pendingShareIntent, true);

        // The color of the media viewer is dependent on the file type.
        int backgroundRes;
        if (DownloadFilter.fromMimeType(mimeType) == DownloadFilter.FILTER_IMAGE) {
            backgroundRes = R.color.image_viewer_bg;
        } else {
            backgroundRes = R.color.media_viewer_bg;
        }
        int mediaColor = ApiCompatibilityUtils.getColor(context.getResources(), backgroundRes);

        // Build up the Intent further.
        Intent intent = builder.build().intent;
        intent.setPackage(context.getPackageName());
        intent.setData(contentUri);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_MEDIA_VIEWER, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_MEDIA_VIEWER_URL, fileUri.toString());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE,
                isMimeTypeVideo(mimeType));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR, mediaColor);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TOOLBAR_COLOR, mediaColor);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        IntentHandler.addTrustedIntentExtras(intent);

        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(context, ChromeLauncherActivity.class);
        return intent;
    }

    /**
     * Returns a URI that points at the file.
     * @param file File to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    public static Uri getUriForItem(File file) {
        Uri uri = null;

        // #getContentUriFromFile causes a disk read when it calls into FileProvider#getUriForFile.
        // Obtaining a content URI is on the critical path for creating a share intent after the
        // user taps on the share button, so even if we were to run this method on a background
        // thread we would have to wait. As it depends on user-selected items, we cannot
        // know/preload which URIs we need until the user presses share.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // Try to obtain a content:// URI, which is preferred to a file:// URI so that
            // receiving apps don't attempt to determine the file's mime type (which often fails).
            uri = ContentUriUtils.getContentUriFromFile(ContextUtils.getApplicationContext(), file);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Could not create content uri: " + e);
        }
        StrictMode.setThreadPolicy(oldPolicy);

        if (uri == null) uri = Uri.fromFile(file);

        return uri;
    }

    /**
     * Opens a file in Chrome or in another app if appropriate.
     * @param file path to the file to open.
     * @param mimeType mime type of the file.
     * @param isOffTheRecord whether we are in an off the record context.
     * @return whether the file could successfully be opened.
     */
    public static boolean openFile(File file, String mimeType, boolean isOffTheRecord) {
        Context context = ContextUtils.getApplicationContext();
        Intent viewIntent = createViewIntentForDownloadItem(getUriForItem(file), mimeType);
        DownloadManagerService service = DownloadManagerService.getDownloadManagerService(context);

        // Check if Chrome should open the file itself.
        if (service.isDownloadOpenableInBrowser(isOffTheRecord, mimeType)) {
            // Share URIs use the content:// scheme when able, which looks bad when displayed
            // in the URL bar.
            Uri fileUri = Uri.fromFile(file);
            Uri contentUri = getUriForItem(file);
            String normalizedMimeType = Intent.normalizeMimeType(mimeType);

            Intent intent =
                    getMediaViewerIntentForDownloadItem(fileUri, contentUri, normalizedMimeType);
            IntentHandler.startActivityForTrustedIntent(intent);
            return true;
        }

        // Check if any apps can open the file.
        try {
            context.startActivity(viewIntent);
            return true;
        } catch (ActivityNotFoundException e) {
            // Can't launch the Intent.
            Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                         Toast.LENGTH_SHORT)
                    .show();
            return false;
        }
    }

    private static void recordShareHistograms(int count, int filterType) {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Share.FileTypes",
                filterType, DownloadFilter.FILTER_BOUNDARY);

        RecordHistogram.recordLinearCountHistogram("Android.DownloadManager.Share.Count",
                count, 1, 20, 20);
    }

    /**
     * Fires an Intent to open a downloaded item.
     * @param context Context to use.
     * @param intent  Intent that can be fired.
     * @return Whether an Activity was successfully started for the Intent.
     */
    static boolean fireOpenIntentForDownload(Context context, Intent intent) {
        try {
            if (TextUtils.equals(intent.getPackage(), context.getPackageName())) {
                IntentHandler.startActivityForTrustedIntent(intent);
            } else {
                context.startActivity(intent);
            }
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.d(TAG, "Activity not found for " + intent.getType() + " over "
                    + intent.getData().getScheme(), ex);
        } catch (SecurityException ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        }

        return false;
    }

    /**
     * Create a string that represents the percentage of the file that has downloaded.
     * @param percentage Current percentage of the file.
     * @return String representing the percentage of the file that has been downloaded.
     */
    public static String getPercentageString(int percentage) {
        NumberFormat formatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return formatter.format(percentage / 100.0);
    }

    /**
     * Determine what String to show for a given download.
     * @param item Download to check the status of.
     * @return ID of a String resource to use, or 0 if the status couldn't be determined.
     */
    public static String getStatusString(DownloadItem item) {
        Context context = ContextUtils.getApplicationContext();
        DownloadInfo info = item.getDownloadInfo();

        int state = info.state();
        if (state == DownloadState.COMPLETE) {
            return context.getString(R.string.download_notification_completed);
        }

        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry = helper.getDownloadSharedPreferenceEntry(item.getId());
        boolean isDownloadPending =
                entry != null && state == DownloadState.INTERRUPTED && entry.isAutoResumable;

        if (isDownloadPending) {
            return context.getString(R.string.download_notification_pending);
        } else if (isDownloadPaused(item)) {
            return context.getString(R.string.download_notification_paused);
        }

        if (info.getBytesReceived() == 0
                || (!item.isIndeterminate() && info.getTimeRemainingInMillis() < 0)) {
            // We lack enough information about the download to display a useful string.
            return context.getString(R.string.download_started);
        } else if (item.isIndeterminate()) {
            // Count up the bytes.
            long bytes = info.getBytesReceived();
            return DownloadUtils.getStringForDownloadedBytes(context, bytes);
        } else {
            // Count down the time.
            long msRemaining = info.getTimeRemainingInMillis();
            return DownloadNotificationService.formatRemainingTime(context, msRemaining);
        }
    }

    /**
     * Query the Download backends about whether a download is paused.
     *
     * The Java-side contains more information about the status of a download than is persisted
     * by the native backend, so it is queried first.
     *
     * @param item Download to check the status of.
     * @return Whether the download is paused or not.
     */
    public static boolean isDownloadPaused(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry = helper.getDownloadSharedPreferenceEntry(item.getId());

        if (entry != null) {
            // The Java downloads backend knows more about the download than the native backend.
            return !entry.isAutoResumable;
        } else {
            // Only the native downloads backend knows about the download.
            if (item.getDownloadInfo().state() == DownloadState.IN_PROGRESS) {
                return item.getDownloadInfo().isPaused();
            } else {
                return item.getDownloadInfo().state() == DownloadState.INTERRUPTED;
            }
        }
    }

    /**
     * Format the number of bytes into KB, or MB, or GB and return the corresponding string
     * resource. Uses default download-related set of strings.
     * @param context Context to use.
     * @param bytes Number of bytes.
     * @return A formatted string to be displayed.
     */
    public static String getStringForDownloadedBytes(Context context, long bytes) {
        return getStringForBytes(context, BYTES_DOWNLOADED_STRINGS, bytes);
    }

    /**
     * Format the number of bytes into KB, or MB, or GB and return the corresponding string
     * resource.
     * @param context Context to use.
     * @param stringSet The string resources for displaying bytes in KB, MB and GB.
     * @param bytes Number of bytes.
     * @return A formatted string to be displayed.
     */
    public static String getStringForBytes(Context context, int[] stringSet, long bytes) {
        int resourceId;
        float bytesInCorrectUnits;

        if (bytes < BYTES_PER_MEGABYTE) {
            resourceId = stringSet[0];
            bytesInCorrectUnits = bytes / (float) BYTES_PER_KILOBYTE;
        } else if (bytes < BYTES_PER_GIGABYTE) {
            resourceId = stringSet[1];
            bytesInCorrectUnits = bytes / (float) BYTES_PER_MEGABYTE;
        } else {
            resourceId = stringSet[2];
            bytesInCorrectUnits = bytes / (float) BYTES_PER_GIGABYTE;
        }

        return context.getResources().getString(resourceId, bytesInCorrectUnits);
    }

    /**
     * Abbreviate a file name into a given number of characters with ellipses.
     * e.g. "thisisaverylongfilename.txt" => "thisisave....txt".
     * @param fileName File name to abbreviate.
     * @param limit Character limit.
     * @return Abbreviated file name.
     */
    public static String getAbbreviatedFileName(String fileName, int limit) {
        assert limit >= 1;  // Abbreviated file name should at least be 1 characters (a...)

        if (TextUtils.isEmpty(fileName)) return fileName;

        if (fileName.length() <= limit) return fileName;

        // Find the file name extension
        int index = fileName.lastIndexOf(".");
        int extensionLength = fileName.length() - index;

        // If the extension is too long, just use truncate the string from beginning.
        if (extensionLength >= limit) {
            return fileName.substring(0, limit) + ELLIPSIS;
        }
        int remainingLength = limit - extensionLength;
        return fileName.substring(0, remainingLength) + ELLIPSIS + fileName.substring(index);
    }

    /**
     * Return an icon for a given file type.
     * @param fileType Type of the file as returned by DownloadFilter.
     */
    public static int getIconResId(int fileType) {
        switch (fileType) {
            case DownloadFilter.FILTER_PAGE:
                return R.drawable.ic_drive_site_white;
            case DownloadFilter.FILTER_VIDEO:
                return R.drawable.ic_play_arrow_white;
            case DownloadFilter.FILTER_AUDIO:
                return R.drawable.ic_music_note_white;
            case DownloadFilter.FILTER_IMAGE:
                return R.drawable.ic_image_white;
            case DownloadFilter.FILTER_DOCUMENT:
                return R.drawable.ic_drive_text_white;
            default:
                return R.drawable.ic_drive_file_white;
        }
    }

    /**
     * Return a background color for the file type icon.
     * @param context Context from which to extract the resources.
     * @return Background color.
     */
    public static int getIconBackgroundColor(Context context) {
        return ApiCompatibilityUtils.getColor(context.getResources(), R.color.light_active_color);
    }

    /**
     * Return a foreground color list for the file type icon.
     * @param context Context from which to extract the resources.
     * @return a foreground color list.
     */
    public static ColorStateList getIconForegroundColorList(Context context) {
        return ApiCompatibilityUtils.getColorStateList(
                context.getResources(), R.color.white_mode_tint);
    }

    private static boolean isMimeTypeVideo(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return false;

        String[] pieces = mimeType.split(MIME_TYPE_DELIMITER);
        if (pieces.length != 2) return false;

        return MIME_TYPE_VIDEO.equals(pieces[0]);
    }
}
