// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Scanner;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

/**
 * Responsible for the Crash Report directory. It routinely scans the directory
 * for new Minidump files and takes appropriate actions by either uploading new
 * crash dumps or deleting old ones.
 */
public class CrashFileManager {
    private static final String TAG = "CrashFileManager";

    /**
     * The name of the crash directory.
     */
    public static final String CRASH_DUMP_DIR = "Crash Reports";

    // This should mirror the C++ CrashUploadList::kReporterLogFilename variable.
    @VisibleForTesting
    public static final String CRASH_DUMP_LOGFILE = "uploads.log";

    private static final Pattern MINIDUMP_MIME_FIRST_TRY_PATTERN =
            Pattern.compile("\\.dmp([0-9]+)$\\z");

    private static final Pattern MINIDUMP_PATTERN =
            Pattern.compile("\\.dmp([0-9]*)(\\.try([0-9]+))?\\z");

    private static final Pattern UPLOADED_MINIDUMP_PATTERN = Pattern.compile("\\.up([0-9]*)\\z");

    private static final String NOT_YET_UPLOADED_MINIDUMP_SUFFIX = ".dmp";

    private static final String UPLOADED_MINIDUMP_SUFFIX = ".up";

    private static final String UPLOAD_SKIPPED_MINIDUMP_SUFFIX = ".skipped";

    private static final String UPLOAD_FORCED_MINIDUMP_SUFFIX = ".forced";

    private static final String UPLOAD_ATTEMPT_DELIMITER = ".try";

    // A delimiter between uid and the rest of a minidump filename. Only used for WebView minidumps.
    private static final String UID_DELIMITER = "_";

    @VisibleForTesting
    protected static final String TMP_SUFFIX = ".tmp";

    private static final Pattern TMP_PATTERN = Pattern.compile("\\.tmp\\z");

    // The maximum number of non-uploaded crashes that may be kept in the crash reports directory.
    // Chosen to attempt to balance between keeping a generous number of crashes, and not using up
    // too much filesystem storage space for obsolete crash reports.
    @VisibleForTesting
    protected static final int MAX_CRASH_REPORTS_TO_KEEP = 10;

    // The maximum age, in days, considered acceptable for a crash report. Reports older than this
    // age will be removed. The constant is chosen to be quite conservative, while still allowing
    // users to eventually reclaim filesystem storage space from obsolete crash reports.
    private static final int MAX_CRASH_REPORT_AGE_IN_DAYS = 30;

    // The maximum number of non-uploaded crashes to copy to the crash reports directory. The
    // difference between this value and MAX_CRASH_REPORTS_TO_KEEP is that TO_KEEP is only checked
    // when we clean out the crash directory - the TO_UPLOAD value is checked every time we try to
    // copy a minidump - to ensure we don't store too many minidumps before they are cleaned out
    // after being uploaded.
    @VisibleForTesting
    static final int MAX_CRASH_REPORTS_TO_UPLOAD = MAX_CRASH_REPORTS_TO_KEEP * 2;
    // Same as above except this value is enforced per UID, so that one single app can't hog all
    // storage/uploading resources.
    @VisibleForTesting
    static final int MAX_CRASH_REPORTS_TO_UPLOAD_PER_UID = MAX_CRASH_REPORTS_TO_KEEP;

    /**
     * Comparator used for sorting files by modification date.
     * @return Comparator for prioritizing the more recently modified file
     */
    @VisibleForTesting
    protected static final Comparator<File> sFileComparator = new Comparator<File>() {
        @Override
        public int compare(File lhs, File rhs) {
            if (lhs.lastModified() == rhs.lastModified()) {
                return lhs.compareTo(rhs);
            } else if (lhs.lastModified() < rhs.lastModified()) {
                return 1;
            } else {
                return -1;
            }
        }
    };

    /**
     * Delete the file {@param fileToDelete}.
     */
    public static boolean deleteFile(File fileToDelete) {
        boolean isSuccess = fileToDelete.delete();
        if (!isSuccess) {
            Log.w(TAG, "Unable to delete " + fileToDelete.getAbsolutePath());
        }
        return isSuccess;
    }

    public static boolean isMinidumpMIMEFirstTry(String path) {
        return MINIDUMP_MIME_FIRST_TRY_PATTERN.matcher(path).find();
    }

    public static String tryIncrementAttemptNumber(File mFileToUpload) {
        String newName = filenameWithIncrementedAttemptNumber(mFileToUpload.getPath());
        return mFileToUpload.renameTo(new File(newName)) ? newName : null;
    }

    /**
     * @return The file name to rename to after an addition attempt to upload
     */
    @VisibleForTesting
    public static String filenameWithIncrementedAttemptNumber(String filename) {
        int numTried = readAttemptNumber(filename);
        if (numTried >= 0) {
            int newCount = numTried + 1;
            return filename.replace(
                    UPLOAD_ATTEMPT_DELIMITER + numTried, UPLOAD_ATTEMPT_DELIMITER + newCount);
        } else {
            // readAttemptNumber returning -1 means there is no UPLOAD_ATTEMPT_DELIMITER in the file
            // name (or that there is a delimiter but no attempt number). So, we have to add the
            // delimiter and attempt number ourselves.
            return filename + UPLOAD_ATTEMPT_DELIMITER + "1";
        }
    }

    /**
     * Attempts to rename the given file to mark it as a forced upload. This is useful for allowing
     * users to manually initiate previously skipped uploads.
     *
     * @return The renamed file, or null if renaming failed.
     */
    public static File trySetForcedUpload(File fileToUpload) {
        if (fileToUpload.getName().contains(UPLOADED_MINIDUMP_SUFFIX)) {
            Log.w(TAG, "Refusing to reset upload attempt state for a file that has already been "
                            + "successfully uploaded: " + fileToUpload.getName());
            return null;
        }
        File renamedFile = new File(filenameWithForcedUploadState(fileToUpload.getPath()));
        return fileToUpload.renameTo(renamedFile) ? renamedFile : null;
    }

    /**
     * @return True iff the provided File was manually forced (by the user) to be uploaded.
     */
    public static boolean isForcedUpload(File fileToUpload) {
        return fileToUpload.getName().contains(UPLOAD_FORCED_MINIDUMP_SUFFIX);
    }

    /**
     * @return The filename to rename to so as to manually force an upload (including clearing any
     *     previous upload attempt history).
     */
    @VisibleForTesting
    protected static String filenameWithForcedUploadState(String filename) {
        int numTried = readAttemptNumber(filename);
        if (numTried > 0) {
            filename = filename.replace(
                    UPLOAD_ATTEMPT_DELIMITER + numTried, UPLOAD_ATTEMPT_DELIMITER + 0);
        }
        filename = filename.replace(UPLOAD_SKIPPED_MINIDUMP_SUFFIX, UPLOAD_FORCED_MINIDUMP_SUFFIX);
        return filename.replace(NOT_YET_UPLOADED_MINIDUMP_SUFFIX, UPLOAD_FORCED_MINIDUMP_SUFFIX);
    }

    /**
     * Returns how many times we've tried to upload a certain Minidump file.
     * @return the number of attempts to upload the given Minidump file, parsed from its file name,
     *     returns -1 if an attempt-number cannot be parsed from the file-name.
     */
    @VisibleForTesting
    public static int readAttemptNumber(String filename) {
        int tryIndex = filename.lastIndexOf(UPLOAD_ATTEMPT_DELIMITER);
        if (tryIndex >= 0) {
            tryIndex += UPLOAD_ATTEMPT_DELIMITER.length();
            String numTriesString = filename.substring(tryIndex);
            Scanner numTriesScanner = new Scanner(numTriesString).useDelimiter("[^0-9]+");
            try {
                int nextInt = numTriesScanner.nextInt();
                // Only return the number if it occurs just after the UPLOAD_ATTEMPT_DELIMITER.
                return numTriesString.indexOf(Integer.toString(nextInt)) == 0 ? nextInt : -1;
            } catch (NoSuchElementException e) {
                return -1;
            }
        }
        return -1;
    }

    /**
     * Marks a crash dump file as successfully uploaded, by renaming the file.
     *
     * Does not immediately delete the file, for testing reasons. However, if renaming fails,
     * attempts to delete the file immediately.
     */
    public static void markUploadSuccess(File crashDumpFile) {
        CrashFileManager.renameCrashDumpFollowingUpload(crashDumpFile, UPLOADED_MINIDUMP_SUFFIX);
    }

    /**
     * Marks a crash dump file's upload being skipped. An upload might be skipped due to lack of
     * user consent, or due to this client being excluded from the sample of clients reporting
     * crashes.
     *
     * Renames the file rather than deleting it, so that the user can manually upload the file later
     * (via chrome://crashes). However, if renaming fails, attempts to delete the file immediately.
     */
    public static void markUploadSkipped(File crashDumpFile) {
        CrashFileManager.renameCrashDumpFollowingUpload(
                crashDumpFile, UPLOAD_SKIPPED_MINIDUMP_SUFFIX);
    }

    /**
     * Renames a crash dump file. However, if renaming fails, attempts to delete the file
     * immediately.
     */
    private static void renameCrashDumpFollowingUpload(File crashDumpFile, String suffix) {
        // The pre-upload filename might have been either "foo.dmpN.tryM" or "foo.forcedN.tryM".
        String newName = crashDumpFile.getPath()
                                 .replace(NOT_YET_UPLOADED_MINIDUMP_SUFFIX, suffix)
                                 .replace(UPLOAD_FORCED_MINIDUMP_SUFFIX, suffix);
        boolean renamed = crashDumpFile.renameTo(new File(newName));
        if (!renamed) {
            Log.w(TAG, "Failed to rename " + crashDumpFile);
            if (!crashDumpFile.delete()) {
                Log.w(TAG, "Failed to delete " + crashDumpFile);
            }
        }
    }

    private final File mCacheDir;

    public CrashFileManager(File cacheDir) {
        if (cacheDir == null) {
            throw new NullPointerException("Specified context cannot be null.");
        } else if (!cacheDir.isDirectory()) {
            throw new IllegalArgumentException(cacheDir.getAbsolutePath() + " is not a directory.");
        }
        mCacheDir = cacheDir;
    }

    /**
     * Create the crash directory for this file manager unless it exists already.
     * @return true iff the crash directory exists when this method returns.
     */
    public boolean ensureCrashDirExists() {
        File crashDir = getCrashDirectory();
        if (crashDir.exists()) return true;
        return crashDir.mkdir();
    }

    /**
     * Returns all minidump files that could still be uploaded, sorted by modification time stamp.
     * Forced uploads are not included. Only returns files that we have tried to upload less
     * than {@param maxTries} number of times.
     */
    public File[] getAllMinidumpFiles(int maxTries) {
        return getFilesBelowMaxTries(getAllMinidumpFilesIncludingFailed(), maxTries);
    }

    /**
     * Returns all minidump files sorted by modification time stamp.
     * Forced uploads are not included.
     */
    private File[] getAllMinidumpFilesIncludingFailed() {
        return listCrashFiles(MINIDUMP_PATTERN);
    }

    /**
     * Returns all minidump files with the uid {@param uid} from {@param minidumpFiles}.
     */
    public static List<File> filterMinidumpFilesOnUid(File[] minidumpFiles, int uid) {
        List<File> uidMinidumps = new ArrayList<>();
        for (File minidump : minidumpFiles) {
            if (belongsToUid(minidump, uid)) {
                uidMinidumps.add(minidump);
            }
        }
        return uidMinidumps;
    }

    public void cleanOutAllNonFreshMinidumpFiles() {
        for (File f : getAllUploadedFiles()) {
            deleteFile(f);
        }
        for (File f : getAllTempFiles()) {
            deleteFile(f);
        }

        Set<String> recentCrashes = new HashSet<String>();
        for (File f : listCrashFiles(null)) {
            // The uploads.log file should always be preserved, as it stores the metadata that
            // powers the chrome://crashes UI.
            if (f.getName().equals(CRASH_DUMP_LOGFILE)) {
                continue;
            }

            // Delete any crash reports that are especially old.
            long ageInMillis = new Date().getTime() - f.lastModified();
            long ageInDays = TimeUnit.DAYS.convert(ageInMillis, TimeUnit.MILLISECONDS);
            if (ageInDays > MAX_CRASH_REPORT_AGE_IN_DAYS) {
                deleteFile(f);
                continue;
            }

            // Delete the oldest crash reports that exceed the cap on the number of allowed reports.
            // Each crash typically has two files associated with it: a .dmp file and a .logcat
            // file. These have the same filename other than the file extension.
            String fileNameSansExtension = f.getName().split("\\.")[0];
            if (recentCrashes.size() < MAX_CRASH_REPORTS_TO_KEEP) {
                recentCrashes.add(fileNameSansExtension);
            } else if (!recentCrashes.contains(fileNameSansExtension)) {
                deleteFile(f);
            }
        }
    }

    /**
     * Filters a set of files to keep the ones we have tried to upload only a few times.
     * Given a set of files {@param unfilteredFiles}, returns only the files in that set which we
     * have tried to upload less than {@param maxTries} times.
     */
    @VisibleForTesting
    static File[] getFilesBelowMaxTries(File[] unfilteredFiles, int maxTries) {
        List<File> filesBelowMaxTries = new ArrayList<>();
        for (File file : unfilteredFiles) {
            if (readAttemptNumber(file.getName()) < maxTries) {
                filesBelowMaxTries.add(file);
            }
        }
        return filesBelowMaxTries.toArray(new File[filesBelowMaxTries.size()]);
    }

    /**
     * Returns a sorted and filtered list of files within the crash directory.
     */
    @VisibleForTesting
    File[] listCrashFiles(@Nullable final Pattern pattern) {
        File crashDir = getCrashDirectory();

        FilenameFilter filter = null;
        if (pattern != null) {
            filter = new FilenameFilter() {
                @Override
                public boolean accept(File dir, String filename) {
                    return pattern.matcher(filename).find();
                }
            };
        }
        File[] foundFiles = crashDir.listFiles(filter);
        if (foundFiles == null) {
            Log.w(TAG, crashDir.getAbsolutePath() + " does not exist or is not a directory");
            return new File[] {};
        }
        Arrays.sort(foundFiles, sFileComparator);
        return foundFiles;
    }

    @VisibleForTesting
    public File[] getAllUploadedFiles() {
        return listCrashFiles(UPLOADED_MINIDUMP_PATTERN);
    }

    @VisibleForTesting
    public File getCrashDirectory() {
        return new File(mCacheDir, CRASH_DUMP_DIR);
    }

    public File createNewTempFile(String name) throws IOException {
        File f = new File(getCrashDirectory(), name);
        if (f.exists()) {
            if (f.delete()) {
                f = new File(getCrashDirectory(), name);
            } else {
                Log.w(TAG, "Unable to delete previous logfile" + f.getAbsolutePath());
            }
        }
        return f;
    }

    /**
     * @return the crash file named {@param filename}.
     */
    public File getCrashFile(String filename) {
        return new File(getCrashDirectory(), filename);
    }

    /**
     * Returns the minidump file with the given local ID, or null if no minidump file has the given
     * local ID.
     * NOTE: Crash files that have already been successfully uploaded are not included.
     *
     * @param localId The local ID of the crash report.
     * @return The matching File, or null if no matching file is found.
     */
    public File getCrashFileWithLocalId(String localId) {
        for (File f : listCrashFiles(null)) {
            // Only match non-uploaded or previously skipped files. In particular, do not match
            // successfully uploaded files; nor files which are not minidump files, such as logcat
            // files.
            if (!f.getName().contains(NOT_YET_UPLOADED_MINIDUMP_SUFFIX)
                    && !f.getName().contains(UPLOAD_SKIPPED_MINIDUMP_SUFFIX)
                    && !f.getName().contains(UPLOAD_FORCED_MINIDUMP_SUFFIX)) {
                continue;
            }

            String filenameSansExtension = f.getName().split("\\.")[0];
            if (filenameSansExtension.endsWith(localId)) {
                return f;
            }
        }
        return null;
    }

    /**
     * @return the file used for logging crash upload events.
     */
    public File getCrashUploadLogFile() {
        return new File(getCrashDirectory(), CRASH_DUMP_LOGFILE);
    }

    @VisibleForTesting
    File[] getAllTempFiles() {
        return listCrashFiles(TMP_PATTERN);
    }

    /**
     * Delete the oldest minidump if we have reached our threshold on the number of minidumps to
     * store (either per-app, or globally).
     * @param uid The uid of the app to check the minidump limit for.
     */
    private void enforceMinidumpStorageRestrictions(int uid) {
        File[] allMinidumpFiles = getAllMinidumpFilesIncludingFailed();
        List<File> minidumpFilesWithCurrentUid = filterMinidumpFilesOnUid(allMinidumpFiles, uid);

        // If we have exceeded our cap per uid, delete the oldest minidump of the same uid
        if (minidumpFilesWithCurrentUid.size() >= MAX_CRASH_REPORTS_TO_UPLOAD_PER_UID) {
            // Minidumps are sorted from newest to oldest.
            File oldestFile =
                    minidumpFilesWithCurrentUid.get(minidumpFilesWithCurrentUid.size() - 1);
            if (!oldestFile.delete()) {
                // Note that we will still try to copy the new file if this deletion fails.
                Log.w(TAG, "Couldn't delete old minidump " + oldestFile.getAbsolutePath());
            }
            return;
        }

        // If we have exceeded our minidump cap, delete the oldest minidump.
        if (allMinidumpFiles.length >= MAX_CRASH_REPORTS_TO_UPLOAD) {
            // Minidumps are sorted from newest to oldest.
            File oldestFile = allMinidumpFiles[allMinidumpFiles.length - 1];
            if (!oldestFile.delete()) {
                // Note that we will still try to copy the new file if this deletion fails.
                Log.w(TAG, "Couldn't delete old minidump " + oldestFile.getAbsolutePath());
            }
        }
    }

    /**
     * Copy a minidump from the File Descriptor {@param fd}.
     * Use {@param tmpDir} as an intermediate location to store temporary files.
     * @return The new minidump file copied with the contents of the File Descriptor, or null if the
     *         copying failed.
     */
    public File copyMinidumpFromFD(FileDescriptor fd, File tmpDir, int uid) throws IOException {
        File crashDirectory = getCrashDirectory();
        if (!crashDirectory.isDirectory() && !crashDirectory.mkdir()) {
            throw new RuntimeException("Couldn't create " + crashDirectory.getAbsolutePath());
        }
        if (!tmpDir.isDirectory() && !tmpDir.mkdir()) {
            throw new RuntimeException("Couldn't create " + tmpDir.getAbsolutePath());
        }
        if (tmpDir.getCanonicalPath().equals(crashDirectory.getCanonicalPath())) {
            throw new RuntimeException("The tmp-dir and the crash dir can't have the same paths.");
        }

        enforceMinidumpStorageRestrictions(uid);

        // Make sure the temp file doesn't overwrite an existing file.
        File tmpFile = createMinidumpTmpFile(tmpDir);
        FileInputStream in = null;
        FileOutputStream out = null;
        // TODO(gsennton): ensure that the copied file is indeed a minidump.
        try {
            in = new FileInputStream(fd);
            out = new FileOutputStream(tmpFile);
            final int bufSize = 4096;
            byte[] buf = new byte[bufSize];
            final int maxSize = 1024 * 1024; // 1MB maximum size
            int curCount = in.read(buf);
            int totalCount = curCount;
            while ((curCount != -1) && (totalCount < maxSize)) {
                out.write(buf, 0, curCount);
                curCount = in.read(buf);
                totalCount += curCount;
            }
            if (curCount != -1) {
                // We are trying to keep on reading beyond our maximum threshold (1MB) - bail!
                Log.w(TAG, "Tried to copy a file of size > 1MB, deleting the file and bailing!");
                if (!tmpFile.delete()) {
                    Log.w(TAG, "Couldn't delete file " + tmpFile.getAbsolutePath());
                }
                return null;
            }
        } finally {
            try {
                if (out != null) out.close();
            } catch (IOException e) {
                Log.w(TAG, "Couldn't close minidump output stream ", e);
            }
            try {
                if (in != null) in.close();
            } catch (IOException e) {
                Log.w(TAG, "Couldn't close minidump input stream ", e);
            }
        }
        File minidumpFile = new File(crashDirectory, createUniqueMinidumpNameForUid(uid));
        if (tmpFile.renameTo(minidumpFile)) {
            return minidumpFile;
        }
        return null;
    }

    /**
     * Returns whether the {@param minidump} belongs to the uid {@param uid}.
     */
    private static boolean belongsToUid(File minidump, int uid) {
        return minidump.getName().startsWith(uid + UID_DELIMITER);
    }

    /**
     * Returns a unique minidump name based on {@param uid} to differentiate between minidumps from
     * different packages.
     * The 'uniqueness' of the file name lies in it being created from a UUID. A UUID is a
     * Universally Unique ID - it is simply a 128-bit value that can be used to uniquely identify
     * some entity. A uid, on the other hand, is a unique identifier for Android packages.
     */
    private static String createUniqueMinidumpNameForUid(int uid) {
        return uid + UID_DELIMITER + UUID.randomUUID() + NOT_YET_UPLOADED_MINIDUMP_SUFFIX;
    }

    /**
     * Create a temporary file to store a minidump in before renaming it with a real minidump name.
     * @return a new temporary file with prefix {@param prefix} stored in the directory
     * {@param directory}.
     *
     */
    private static File createMinidumpTmpFile(File directory) throws IOException {
        return File.createTempFile("webview_minidump", TMP_SUFFIX, directory);
    }
}
