package io.fedlet.mobutil;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import android.util.Log;

/*
 * 封面下载（Android 专用）：Qt 的 openssl 后端需要额外捆绑 libssl/libcrypto，
 * 本项目未携带；系统 HttpURLConnection 自带 TLS，且无需额外依赖。
 * 阻塞调用，必须在工作线程执行；失败返回 null。
 */
public class ImageFetch {
    private static final String TAG = "ImageFetch";

    public static byte[] fetch(String url) {
        // NCM CDN 返回 http 封面；Android 9+ 默认禁明文 → 先升级 https
        // （CDN 支持 https），失败再回落原始地址（manifest 已开 cleartext）。
        if (url != null && url.startsWith("http://")) {
            byte[] data = fetchOne("https://" + url.substring("http://".length()));
            if (data != null)
                return data;
            Log.w(TAG, "https upgrade failed, fallback http: " + url);
        }
        return fetchOne(url);
    }

    private static byte[] fetchOne(String url) {
        HttpURLConnection conn = null;
        InputStream in = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(20000);
            conn.setInstanceFollowRedirects(true);
            conn.setRequestProperty("User-Agent",
                "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                + "AppleWebKit/537.36 (KHTML, like Gecko) "
                + "Chrome/124.0.0.0 Safari/537.36 Edg/124.0.0.0");
            conn.setRequestProperty("Referer", "https://music.163.com");
            final int code = conn.getResponseCode();
            if (code < 200 || code >= 300) {
                // 静默 null 曾让 cleartext 拦截排查两轮 → 打状态码
                Log.w(TAG, "http " + code + " url=" + url);
                return null;
            }
            in = conn.getInputStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0)
                out.write(buf, 0, n);
            return out.toByteArray();
        } catch (Throwable t) {
            // 关键诊断：cleartext 拒绝（CLEARTEXT communication not permitted）等
            Log.w(TAG, "fetch fail url=" + url + " err=" + t);
            return null;
        } finally {
            try { if (in != null) in.close(); } catch (Throwable ignored) {}
            try { if (conn != null) conn.disconnect(); } catch (Throwable ignored) {}
        }
    }
}
