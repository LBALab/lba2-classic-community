package org.lbalab.lba2cc;

import android.app.Activity;
import android.app.Application;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

public class PermissionHelper {
    /* Written on the UI thread by the lifecycle callbacks, read on the game's
       thread while it waits for an answer. */
    private static volatile boolean sLeft = false;
    private static volatile boolean sReturned = false;
    private static Application.ActivityLifecycleCallbacks sWatcher = null;

    public static boolean isExternalStorageManager() {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        return true;
    }

    /* Whether the game went to the background after Settings was opened. */
    public static boolean hasLeftForSettings() {
        return sLeft;
    }

    /* Whether the game has come back to the front since it left. */
    public static boolean hasReturnedFromSettings() {
        return sReturned;
    }

    public static void openExternalStorageManagerSettings(Object activityObj) {
        if (Build.VERSION.SDK_INT >= 30) {
            if (!Environment.isExternalStorageManager()) {
                Activity activity = (Activity) activityObj;
                watch(activity);
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + activity.getPackageName()));
                activity.startActivity(intent);
            }
        }
    }

    /* Some devices keep a backgrounded app running, so the game cannot tell
       from its own clock that the player is back. The activity's pause and
       resume say so directly. */
    private static synchronized void watch(final Activity game) {
        sLeft = false;
        sReturned = false;
        if (sWatcher != null) {
            return;
        }
        sWatcher = new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityPaused(Activity a) {
                if (a == game) {
                    sLeft = true;
                }
            }

            @Override
            public void onActivityResumed(Activity a) {
                if (a == game && sLeft) {
                    sReturned = true;
                }
            }

            @Override
            public void onActivityCreated(Activity a, Bundle b) {}

            @Override
            public void onActivityStarted(Activity a) {}

            @Override
            public void onActivityStopped(Activity a) {}

            @Override
            public void onActivitySaveInstanceState(Activity a, Bundle b) {}

            @Override
            public void onActivityDestroyed(Activity a) {}
        };
        game.getApplication().registerActivityLifecycleCallbacks(sWatcher);
    }

    /* Called once the game has its answer. */
    public static synchronized void stopWatching(Object activityObj) {
        if (sWatcher != null && activityObj != null) {
            ((Activity) activityObj).getApplication().unregisterActivityLifecycleCallbacks(sWatcher);
        }
        sWatcher = null;
    }
}
