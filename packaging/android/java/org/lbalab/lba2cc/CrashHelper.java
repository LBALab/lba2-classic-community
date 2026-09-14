package org.lbalab.lba2cc;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Keeps the system's report of the app's last native crash where a player can send
 * it. The engine's own crash block is in the log the crash wrote; the tombstone is
 * the platform's report of the same crash, with every thread, and is otherwise only
 * readable over adb.
 */
public class CrashHelper {
    private static final String PREFERENCES = "crash_reports";
    private static final String LAST_SAVED = "last_saved_timestamp";
    private static final String PREFIX = "crash-";
    private static final String SUFFIX = ".pb";

    /**
     * Saves the tombstone of the newest native crash not saved before into dir as
     * crash-<time>.pb, replacing the one saved before it, and returns a line for the
     * log. Returns null when there is no such crash, or before Android 11, which
     * keeps no exit records.
     */
    public static String saveLastNativeCrash(Object contextObject, String dir) {
        if (Build.VERSION.SDK_INT < 30) {
            return null;
        }
        Context context = (Context) contextObject;
        ActivityManager manager = context.getSystemService(ActivityManager.class);
        if (manager == null) {
            return null;
        }
        SharedPreferences preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
        long lastSaved = preferences.getLong(LAST_SAVED, 0);

        ApplicationExitInfo crash = null;
        List<ApplicationExitInfo> exits = manager.getHistoricalProcessExitReasons(null, 0, 0);
        for (ApplicationExitInfo exit : exits) {
            if (exit.getReason() == ApplicationExitInfo.REASON_CRASH_NATIVE && exit.getTimestamp() > lastSaved
                    && (crash == null || exit.getTimestamp() > crash.getTimestamp())) {
                crash = exit;
            }
        }
        if (crash == null) {
            return null;
        }
        // Recorded before the copy, so a tombstone that fails to copy is not tried on
        // every launch.
        preferences.edit().putLong(LAST_SAVED, crash.getTimestamp()).apply();

        Date date = new Date(crash.getTimestamp());
        String summary = "Previous run crashed with signal " + crash.getStatus() + " at "
                + new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(date);
        String name = PREFIX + new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(date) + SUFFIX;
        String failure = copyTombstone(crash, new File(dir, name));
        if (failure != null) {
            return summary + "; the system's report of it was " + failure;
        }
        removeOlderReports(new File(dir), name);
        return summary + "; the system's report of it is in " + name;
    }

    /** Copies the tombstone to target. Returns null on success, or why it was not saved. */
    private static String copyTombstone(ApplicationExitInfo crash, File target) {
        if (Build.VERSION.SDK_INT < 31) {
            return "not kept before Android 12";
        }
        File partial = new File(target.getPath() + ".part");
        try {
            InputStream in = crash.getTraceInputStream();
            if (in == null) {
                return "no longer kept by the system";
            }
            try {
                OutputStream out = new FileOutputStream(partial);
                try {
                    byte[] buffer = new byte[16384];
                    int n;
                    while ((n = in.read(buffer)) > 0) {
                        out.write(buffer, 0, n);
                    }
                } finally {
                    out.close();
                }
            } finally {
                in.close();
            }
            if (!partial.renameTo(target)) {
                partial.delete();
                return "not written";
            }
            return null;
        } catch (Exception e) {
            partial.delete();
            return "not written (" + e.getClass().getSimpleName() + ")";
        }
    }

    private static void removeOlderReports(File dir, String keep) {
        File[] files = dir.listFiles();
        if (files == null) {
            return;
        }
        for (File file : files) {
            String name = file.getName();
            if (name.startsWith(PREFIX) && name.endsWith(SUFFIX) && !name.equals(keep)) {
                file.delete();
            }
        }
    }
}
