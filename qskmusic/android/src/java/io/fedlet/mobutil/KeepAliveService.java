package io.fedlet.mobutil;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

public class KeepAliveService extends Service {
    private static final String TAG = "qskmusic";
    private static final int NOTIF_ID = 1001;
    private static final String CHANNEL_ID = "qskmusic_keepalive";

    private static KeepAliveService sInstance = null;
    private static PowerManager.WakeLock sWakeLock = null;
    private static boolean sPlaying = false;
    private static String sLastError = "";

    @Override
    public void onCreate() {
        super.onCreate();
        sInstance = this;
        createNotificationChannel();
    }

    @Override
    public void onDestroy() {
        releaseWakeLock();
        sInstance = null;
        super.onDestroy();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        try {
            startForeground(NOTIF_ID, buildNotification());
            sLastError = "";
            Log.i(TAG, "[ENV] fgs started type=mediaPlayback");
        } catch (Exception ex) {
            sLastError = "startForeground failed: " + ex;
            Log.e(TAG, "[ENV] " + sLastError);
        }
        return START_NOT_STICKY;
    }

    /* C++ 侧可读取最近一次前台服务启动错误（供 [ENV] 诊断日志） */
    public static String lastError() { return sLastError; }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    private Notification buildNotification() {
        Intent launchIntent = getPackageManager()
            .getLaunchIntentForPackage(getPackageName());
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        PendingIntent pi = PendingIntent.getActivity(
            this, 0, launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        return new Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("qskmusic")
            .setContentText(sPlaying ? "正在播放" : "后台待命")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setOngoing(true)
            .setContentIntent(pi)
            .build();
    }

    /* C++ 侧（Playback::playingChanged）驱动：
     * 播放中持 PARTIAL_WAKE_LOCK（熄屏不断播）；暂停释放；通知文案同步 */
    public static void setPlaybackActive(Context ctx, boolean playing) {
        sPlaying = playing;
        PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
        try {
            if (playing) {
                if (sWakeLock == null) {
                    sWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                                               "qskmusic:playback");
                    sWakeLock.setReferenceCounted(false);
                }
                if (!sWakeLock.isHeld())
                    sWakeLock.acquire();
                Log.i(TAG, "[ENV] wake lock acquired");
            } else {
                releaseWakeLock();
                Log.i(TAG, "[ENV] wake lock released");
            }
        } catch (Exception ex) {
            sLastError = "wake lock failed: " + ex;
            Log.e(TAG, "[ENV] " + sLastError);
        }
        if (sInstance != null)
            sInstance.updateNotification();
    }

    private static void releaseWakeLock() {
        if (sWakeLock != null && sWakeLock.isHeld())
            sWakeLock.release();
    }

    private void updateNotification() {
        if (sInstance == null) return;
        getSystemService(NotificationManager.class).notify(NOTIF_ID, buildNotification());
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "qskmusic Service",
                NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("Keep app alive in background");
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }
    }

    public static void startService(Context ctx) {
        Intent intent = new Intent(ctx, KeepAliveService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ctx.startForegroundService(intent);
        } else {
            ctx.startService(intent);
        }
    }

    public static void stopService(Context ctx) {
        ctx.stopService(new Intent(ctx, KeepAliveService.class));
    }
}
