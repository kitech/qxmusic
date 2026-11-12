package io.fedlet.mobutil;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.util.Log;

public class KeepAliveService extends Service {
    private static final String TAG = "qskmusic";
    private static final int NOTIF_ID = 1001;
    private static final String CHANNEL_ID = "qskmusic_keepalive";

    private static final String ACTION_CMD = "io.fedlet.mobutil.CMD";
    private static final String EXTRA_CMD = "cmd";
    private static final int CMD_PLAY = 1;
    private static final int CMD_PAUSE = 2;
    private static final int CMD_NEXT = 3;
    private static final int CMD_PREV = 4;
    private static final int CMD_TOGGLE = 5;

    private static KeepAliveService sInstance = null;
    private static PowerManager.WakeLock sWakeLock = null;
    private static boolean sPlaying = false;
    private static String sLastError = "";

    /* 会话状态缓存：C++ 可能在会话就绪前就推送 */
    private static String sTitle = "qskmusic";
    private static String sArtist = "";
    private static long sDurationMs = 0;
    private static int sState = PlaybackState.STATE_NONE;
    private static long sPositionMs = 0;
    private static float sSpeed = 1.0f;

    private static boolean sHasFocus = false;
    private static AudioFocusRequest sFocusRequest = null;

    private MediaSession mSession = null;

    /* Java → C++（注册见 KeepAlive::installNativeCallbacks） */
    private static native void nativePlaybackCommand(int cmd);
    private static native void nativeFocusChange(int change);

    private static final AudioManager.OnAudioFocusChangeListener sFocusListener =
        new AudioManager.OnAudioFocusChangeListener() {
            @Override
            public void onAudioFocusChange(int change) {
                Log.i(TAG, "[ENV] focus change=" + change);
                if (change == AudioManager.AUDIOFOCUS_GAIN)
                    sHasFocus = true;
                else if (change == AudioManager.AUDIOFOCUS_LOSS)
                    sHasFocus = false;
                try {
                    nativeFocusChange(change);
                } catch (Throwable t) {
                    Log.e(TAG, "[ENV] nativeFocusChange failed: " + t);
                }
            }
        };

    private static void safeCommand(int cmd) {
        try {
            nativePlaybackCommand(cmd);
        } catch (Throwable t) {
            Log.e(TAG, "[ENV] nativePlaybackCommand failed: " + t);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        sInstance = this;
        createNotificationChannel();
        createSession();
    }

    @Override
    public void onDestroy() {
        if (mSession != null) {
            mSession.setActive(false);
            mSession.release();
            mSession = null;
            Log.i(TAG, "[ENV] media session released");
        }
        abandonAudioFocus(this);
        releaseWakeLock();
        sInstance = null;
        super.onDestroy();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_CMD.equals(intent.getAction())) {
            int cmd = intent.getIntExtra(EXTRA_CMD, 0);
            if (cmd > 0) {
                Log.i(TAG, "[ENV] notification command=" + cmd);
                safeCommand(cmd);
            }
        }
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

    /* C++ 侧可读取最近一次启动错误（供 [ENV] 诊断日志） */
    public static String lastError() { return sLastError; }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    /* ── 音频焦点 ── */
    public static boolean requestAudioFocus(Context ctx) {
        if (sHasFocus)
            return true;
        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        if (am == null)
            return true;

        if (Build.VERSION.SDK_INT >= 26) {
            if (sFocusRequest == null) {
                sFocusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build())
                    .setOnAudioFocusChangeListener(sFocusListener,
                        new Handler(Looper.getMainLooper()))
                    .build();
            }
            sHasFocus = am.requestAudioFocus(sFocusRequest)
                == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        } else {
            sHasFocus = am.requestAudioFocus(sFocusListener,
                AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
                == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        }
        Log.i(TAG, "[ENV] focus request granted=" + sHasFocus);
        return sHasFocus;
    }

    public static void abandonAudioFocus(Context ctx) {
        if (!sHasFocus)
            return;
        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        if (am != null) {
            if (Build.VERSION.SDK_INT >= 26 && sFocusRequest != null)
                am.abandonAudioFocusRequest(sFocusRequest);
            else
                am.abandonAudioFocus(sFocusListener);
        }
        sHasFocus = false;
        Log.i(TAG, "[ENV] focus abandoned");
    }

    /* ── 媒体会话 ── */
    private void createSession() {
        try {
            mSession = new MediaSession(this, "qskmusic");
            mSession.setCallback(new MediaSession.Callback() {
                @Override public void onPlay()           { safeCommand(CMD_PLAY); }
                @Override public void onPause()          { safeCommand(CMD_PAUSE); }
                @Override public void onStop()           { safeCommand(CMD_PAUSE); }
                @Override public void onSkipToNext()     { safeCommand(CMD_NEXT); }
                @Override public void onSkipToPrevious() { safeCommand(CMD_PREV); }
            }, new Handler(Looper.getMainLooper()));
            mSession.setSessionActivity(launchIntent());
            mSession.setActive(true);
            applyMetadata();
            applyState();
            Log.i(TAG, "[ENV] media session created");
        } catch (Throwable t) {
            sLastError = "media session failed: " + t;
            Log.e(TAG, "[ENV] " + sLastError);
            mSession = null;
        }
    }

    public static void setMetadata(String title, String artist, long durationMs) {
        sTitle = (title == null || title.isEmpty()) ? "qskmusic" : title;
        sArtist = artist == null ? "" : artist;
        sDurationMs = durationMs;
        if (sInstance != null)
            sInstance.applyMetadata();
    }

    private void applyMetadata() {
        if (mSession == null)
            return;
        try {
            MediaMetadata metadata = new MediaMetadata.Builder()
                .putString(MediaMetadata.METADATA_KEY_TITLE, sTitle)
                .putString(MediaMetadata.METADATA_KEY_ARTIST, sArtist)
                .putLong(MediaMetadata.METADATA_KEY_DURATION, sDurationMs)
                .build();
            mSession.setMetadata(metadata);
            Log.i(TAG, "[ENV] metadata set title=" + sTitle);
        } catch (Throwable t) {
            Log.e(TAG, "[ENV] metadata failed: " + t);
        }
    }

    public static void setPlaybackState(int state, long positionMs, float speed) {
        sState = state;
        sPositionMs = positionMs;
        sSpeed = speed;
        if (sInstance != null)
            sInstance.applyState();
    }

    private void applyState() {
        if (mSession == null)
            return;
        try {
            long actions = PlaybackState.ACTION_PLAY
                | PlaybackState.ACTION_PAUSE
                | PlaybackState.ACTION_PLAY_PAUSE
                | PlaybackState.ACTION_SKIP_TO_NEXT
                | PlaybackState.ACTION_SKIP_TO_PREVIOUS
                | PlaybackState.ACTION_STOP;
            PlaybackState state = new PlaybackState.Builder()
                .setActions(actions)
                .setState(sState, sPositionMs, sSpeed)
                .build();
            mSession.setPlaybackState(state);
        } catch (Throwable t) {
            Log.e(TAG, "[ENV] state push failed: " + t);
        }
        updateNotification();
    }

    /* ── 通知 ── */
    private PendingIntent launchIntent() {
        Intent intent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        return PendingIntent.getActivity(this, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private PendingIntent commandIntent(int requestCode, int cmd) {
        Intent intent = new Intent(this, KeepAliveService.class)
            .setAction(ACTION_CMD)
            .putExtra(EXTRA_CMD, cmd);
        return PendingIntent.getService(this, requestCode, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private Notification buildNotification() {
        Notification.Action prev = new Notification.Action.Builder(
            android.R.drawable.ic_media_previous, "上一首",
            commandIntent(101, CMD_PREV)).build();
        Notification.Action toggle = new Notification.Action.Builder(
            sPlaying ? android.R.drawable.ic_media_pause : android.R.drawable.ic_media_play,
            sPlaying ? "暂停" : "播放", commandIntent(102, CMD_TOGGLE)).build();
        Notification.Action next = new Notification.Action.Builder(
            android.R.drawable.ic_media_next, "下一首",
            commandIntent(103, CMD_NEXT)).build();

        Notification.Builder builder = (Build.VERSION.SDK_INT >= 26)
            ? new Notification.Builder(this, CHANNEL_ID)
            : new Notification.Builder(this);

        builder.setContentTitle(sTitle)
            .setContentText(sArtist)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setOngoing(true)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setContentIntent(launchIntent())
            .addAction(prev)
            .addAction(toggle)
            .addAction(next);

        if (mSession != null) {
            builder.setStyle(new Notification.MediaStyle()
                .setMediaSession(mSession.getSessionToken())
                .setShowActionsInCompactView(0, 1, 2));
        }
        return builder.build();
    }

    private void updateNotification() {
        if (sInstance == null)
            return;
        try {
            getSystemService(NotificationManager.class)
                .notify(NOTIF_ID, buildNotification());
        } catch (Throwable t) {
            Log.e(TAG, "[ENV] notify failed: " + t);
        }
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "qskmusic 播放控制",
                NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("正在播放 / 后台待命");
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }
    }

    /* ── WakeLock（阶段1，保留）── */
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
