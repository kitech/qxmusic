package io.fedlet.mobutil;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/*
 * 封面下载（Android 专用）：Qt 的 openssl 后端需要额外捆绑 libssl/libcrypto，
 * 本项目未携带；系统 HttpURLConnection 自带 TLS，且无需额外依赖。
 * 阻塞调用，必须在工作线程执行；失败返回 null。
 */
public class ImageFetch {
    public static byte[] fetch(String url) {
        HttpURLConnection conn = null;
        InputStream in = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(20000);
            conn.setInstanceFollowRedirects(true);
            conn.setRequestProperty("User-Agent", "qskmusic/0.1");
            conn.setRequestProperty("Referer", "https://music.163.com");
            final int code = conn.getResponseCode();
            if (code < 200 || code >= 300)
                return null;
            in = conn.getInputStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0)
                out.write(buf, 0, n);
            return out.toByteArray();
        } catch (Throwable t) {
            return null;
        } finally {
            try { if (in != null) in.close(); } catch (Throwable ignored) {}
            try { if (conn != null) conn.disconnect(); } catch (Throwable ignored) {}
        }
    }
}
