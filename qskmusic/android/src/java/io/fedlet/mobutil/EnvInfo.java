package io.fedlet.mobutil;

import android.app.ActivityManager;
import android.app.NotificationManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.PowerManager;

/*
 * 运行环境事实采集（只读，不改变系统状态）。
 * snapshot() 一次返回 "key=value;..."，C++ 侧解析后统一打 [ENV] 日志；
 * 任何一项异常都降级为安全默认值，绝不抛出。
 */
public class EnvInfo {

    public static String snapshot(Context ctx) {
        StringBuilder sb = new StringBuilder();
        sb.append("sdk=").append(Build.VERSION.SDK_INT);
        sb.append(";release=").append(sanitize(Build.VERSION.RELEASE));
        sb.append(";brand=").append(sanitize(Build.BRAND));
        sb.append(";manufacturer=").append(sanitize(Build.MANUFACTURER));
        sb.append(";model=").append(sanitize(Build.MODEL));
        sb.append(";rom=").append(sanitize(romName()));
        sb.append(";abi=").append(sanitize(
            Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "?"));
        sb.append(";notifPerm=").append(bool(hasNotificationPermission(ctx)));
        sb.append(";notifEnabled=").append(bool(notificationsEnabled(ctx)));
        sb.append(";batteryOptIgnored=").append(bool(batteryOptIgnored(ctx)));
        sb.append(";fgsPerm=").append(bool(hasFgsPermission(ctx)));
        sb.append(";interactive=").append(bool(isInteractive(ctx)));
        sb.append(";powerSave=").append(bool(isPowerSaveMode(ctx)));
        sb.append(";lowRam=").append(bool(isLowRam(ctx)));
        sb.append(";emulator=").append(bool(isEmulator()));
        sb.append(";focus26=").append(bool(Build.VERSION.SDK_INT >= 26));
        sb.append(";mediaSession=").append(bool(Build.VERSION.SDK_INT >= 21));
        return sb.toString();
    }

    public static boolean hasNotificationPermission(Context ctx) {
        if (Build.VERSION.SDK_INT < 33)
            return true;
        return ctx.checkSelfPermission("android.permission.POST_NOTIFICATIONS")
            == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean notificationsEnabled(Context ctx) {
        try {
            NotificationManager nm = (NotificationManager)
                ctx.getSystemService(Context.NOTIFICATION_SERVICE);
            return nm == null || nm.areNotificationsEnabled();
        } catch (Exception e) {
            return true;
        }
    }

    public static boolean batteryOptIgnored(Context ctx) {
        try {
            PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            return pm != null && pm.isIgnoringBatteryOptimizations(ctx.getPackageName());
        } catch (Exception e) {
            return true;
        }
    }

    public static boolean hasFgsPermission(Context ctx) {
        if (Build.VERSION.SDK_INT < 34)
            return true;
        return ctx.checkSelfPermission(
            "android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK")
            == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean isInteractive(Context ctx) {
        try {
            PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            return pm == null || pm.isInteractive();
        } catch (Exception e) {
            return true;
        }
    }

    public static boolean isPowerSaveMode(Context ctx) {
        try {
            PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            return pm != null && pm.isPowerSaveMode();
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean isLowRam(Context ctx) {
        try {
            ActivityManager am = (ActivityManager)
                ctx.getSystemService(Context.ACTIVITY_SERVICE);
            return am != null && am.isLowRamDevice();
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean isEmulator() {
        final String fp = Build.FINGERPRINT == null ? "" : Build.FINGERPRINT;
        final String model = Build.MODEL == null ? "" : Build.MODEL;
        final String product = Build.PRODUCT == null ? "" : Build.PRODUCT;
        return fp.startsWith("generic") || fp.contains("vbox")
            || model.contains("google_sdk") || model.contains("Emulator")
            || product.contains("sdk") || product.contains("emulator");
    }

    /* MIUI/HyperOS/ColorOS 等第三方 ROM 判定（诊断后台清理问题用） */
    static String romName() {
        String miui = readProp("ro.miui.ui.version.name");
        if (!miui.isEmpty())
            return "MIUI/HyperOS " + miui;
        String colorOs = readProp("ro.build.version.oplusrom");
        if (!colorOs.isEmpty())
            return "ColorOS " + colorOs;
        String emui = readProp("ro.build.version.emui");
        if (!emui.isEmpty())
            return emui;
        return Build.DISPLAY == null ? "?" : Build.DISPLAY;
    }

    private static String readProp(String key) {
        try {
            Class<?> cls = Class.forName("android.os.SystemProperties");
            Object value = cls.getMethod("get", String.class).invoke(null, key);
            return value instanceof String ? (String) value : "";
        } catch (Throwable t) {
            return "";
        }
    }

    private static String bool(boolean v) { return v ? "1" : "0"; }

    private static String sanitize(String s) {
        if (s == null || s.isEmpty())
            return "?";
        return s.replace(';', '_').replace('=', '_').replace('\n', ' ');
    }
}
