/* id3smoke：libid3tag 提取/补缺链路 CLI 验证
 * usage: id3smoke <file> [title] [artist] [album]
 *   无可选参数 = 只读模式（打印 version + 三字段现值）
 *   带参数     = 缺失字段才补（与 worker updateNcmId3 同语义） */
#include <id3tag.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

static void dumpText(struct id3_tag* tag, const char* label, const char* fid)
{
    struct id3_frame const* fr = id3_tag_findframe(tag, fid, 0);
    if (!fr) { printf("%s: (no frame)\n", label); return; }
    union id3_field const* fld = id3_frame_field(fr, 1);
    if (!fld || id3_field_getnstrings(fld) == 0) {
        printf("%s: (empty)\n", label);
        return;
    }
    id3_ucs4_t const* u4 = id3_field_getstrings(fld, 0);
    id3_utf8_t* u8 = u4 ? id3_ucs4_utf8duplicate(u4) : 0;
    printf("%s: %s\n", label, u8 ? (const char*)u8 : "(dup fail)");
    free(u8);
}

static bool attachTextFrame(struct id3_tag* tag, const char* fid,
                            const char* val, enum id3_field_textencoding enc)
{
    struct id3_frame* nf = id3_frame_new(fid);
    if (!nf) { fprintf(stderr, "attach %s: frame_new failed\n", fid); return false; }
    id3_ucs4_t* u4 = id3_utf8_ucs4duplicate((id3_utf8_t const*)val);
    union id3_field* fld = id3_frame_field(nf, 1);
    if (!u4) fprintf(stderr, "attach %s: ucs4 dup failed\n", fid);
    if (!fld) fprintf(stderr, "attach %s: field(1) null\n", fid);
    if (u4 && fld) {
        int r1 = id3_field_addstring(fld, u4);
        int r2 = id3_field_settextencoding(id3_frame_field(nf, 0), enc);
        fprintf(stderr, "attach %s: setstring=%d setenc=%d type=%d\n",
                fid, r1, r2, (int)id3_field_type(fld));
        bool ok = (r1 == 0 && r2 == 0);
        if (u4) free(u4);
        if (ok) id3_tag_attachframe(tag, nf);
        else id3_frame_delete(nf);
        return ok;
    }
    if (u4) free(u4);
    id3_frame_delete(nf);
    return false;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: id3smoke <file> [title] [artist] [album]\n");
        return 1;
    }
    const char* path = argv[1];

    struct id3_file* f = id3_file_open(path, ID3_FILE_MODE_READWRITE);
    if (!f) { fprintf(stderr, "open failed\n"); return 1; }
    struct id3_tag* t = id3_file_tag(f);
    if (!t && argc >= 5) {
        /* 无既有 tag：新建渲染 + 前缀手拼（同 worker created 分支） */
        fprintf(stderr, "no tag; creating via id3_tag_render\n");
        static const char* cids[3] = { "TIT2", "TPE1", "TALB" };
        struct id3_tag* nt = id3_tag_new();
        int nfr = 0;
        for (int i = 0; nt && i < 3; ++i) {
            if (argv[2 + i][0] &&
                attachTextFrame(nt, cids[i], argv[2 + i],
                                ID3_FIELD_TEXTENCODING_UTF_16))
                nfr++;
        }
        if (nt && nfr > 0) {
            char buf[2048];
            id3_length_t len = id3_tag_render(nt, (id3_byte_t*)buf);
            if (len > 0 && len < sizeof(buf)) {
                std::string tmp = std::string(path) + ".id3tmp";
                FILE* wp = fopen(tmp.c_str(), "wb");
                FILE* rp = fopen(path, "rb");
                if (wp && rp) {
                    bool ok = fwrite(buf, 1, len, wp) == len;
                    char cp[65536];
                    size_t n;
                    while (ok && (n = fread(cp, 1, sizeof(cp), rp)) > 0)
                        if (fwrite(cp, 1, n, wp) != n) ok = false;
                    fclose(rp);
                    if (fclose(wp) != 0) ok = false;
                    if (ok && rename(tmp.c_str(), path) == 0)
                        printf("created tag (render len=%lu)\n", (unsigned long)len);
                    else remove(tmp.c_str());
                } else {
                    if (wp) fclose(wp);
                    if (rp) fclose(rp);
                }
            } else {
                fprintf(stderr, "render failed len=%lu\n", (unsigned long)len);
            }
        }
        if (nt) id3_tag_delete(nt);
        id3_file_close(f);
        printf("---- after ----\n");
        id3_file* f2 = id3_file_open(path, ID3_FILE_MODE_READONLY);
        struct id3_tag* t2 = f2 ? id3_file_tag(f2) : 0;
        if (t2) {
            printf("version=%u\n", id3_tag_version(t2));
            dumpText(t2, "title", "TIT2");
            dumpText(t2, "artist", "TPE1");
            dumpText(t2, "album", "TALB");
        }
        if (f2) id3_file_close(f2);
        return 0;
    }
    if (!t) {
        fprintf(stderr, "no tag in file (id3_file_tag == NULL), use args to create\n");
        id3_file_close(f);
        return 2;
    }
    printf("version=%u\n", id3_tag_version(t));
    dumpText(t, "title", "TIT2");
    dumpText(t, "artist", "TPE1");
    dumpText(t, "album", "TALB");

    if (argc >= 5) {
        static const char* ids[3] = { "TIT2", "TPE1", "TALB" };
        unsigned ver = id3_tag_version(t) / 256;
        if (ver < 3) {
            printf("v%u old tag, skip append\n", ver);
        } else {
            int nadd = 0;
            for (int i = 0; i < 3; ++i) {
                struct id3_frame* fr = id3_tag_findframe(t, ids[i], 0);
                union id3_field const* fld = fr ? id3_frame_field(fr, 1) : 0;
                if (fld && id3_field_getnstrings(fld) > 0) continue;
                enum id3_field_textencoding enc =
                    (ver >= 4) ? ID3_FIELD_TEXTENCODING_UTF_8
                               : ID3_FIELD_TEXTENCODING_UTF_16;
                if (attachTextFrame(t, ids[i], argv[2 + i], enc)) nadd++;
            }
            if (nadd > 0) {
                int rc = id3_file_update(f);
                printf("update rc=%d frames=%d\n", rc, nadd);
            } else {
                printf("complete, skip update\n");
            }
        }
    }
    id3_file_close(f);

    /* 终态复读 */
    printf("---- after ----\n");
    id3_file* f2 = id3_file_open(path, ID3_FILE_MODE_READONLY);
    struct id3_tag* t2 = f2 ? id3_file_tag(f2) : 0;
    if (t2) {
        printf("version=%u\n", id3_tag_version(t2));
        dumpText(t2, "title", "TIT2");
        dumpText(t2, "artist", "TPE1");
        dumpText(t2, "album", "TALB");
    }
    if (f2) id3_file_close(f2);
    return 0;
}
