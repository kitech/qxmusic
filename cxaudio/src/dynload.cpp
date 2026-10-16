#include "dynload.h"

#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dirent.h>
#  include <dlfcn.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace cx {

Library::~Library() {
    if (handle_) {
#if defined(_WIN32)
        FreeLibrary((HMODULE)handle_);
#else
        dlclose(handle_);
#endif
        handle_ = 0;
    }
}

bool Library::open(const std::string& path) {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA(path.c_str());
    if (!h) return false;
    handle_ = (void*)h;
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) return false;
    handle_ = h;
#endif
    path_ = path;
    return true;
}

void* Library::sym(const char* name) const {
    if (!handle_) return 0;
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)handle_, name);
#else
    return dlsym(handle_, name);
#endif
}

bool fileExists(const std::string& path) {
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

std::vector<std::string> listDir(const std::string& dir, const std::string& prefix) {
    std::vector<std::string> out;
#if defined(_WIN32)
    std::string pat = dir;
    if (!pat.empty() && pat[pat.size() - 1] != '\\' && pat[pat.size() - 1] != '/')
        pat += "\\";
    pat += prefix.empty() ? "*" : (prefix + "*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            out.push_back(dir + "/" + fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    struct dirent* e;
    while ((e = readdir(d)) != 0) {
        if (e->d_name[0] == '.' && e->d_name[1] == '\0') continue;
        if (e->d_name[0] == '.' && e->d_name[1] == '.' && e->d_name[2] == '\0') continue;
        if (!prefix.empty() && strncmp(e->d_name, prefix.c_str(), prefix.size()) != 0)
            continue;
        std::string full = dir;
        if (!full.empty() && full[full.size() - 1] != '/') full += "/";
        full += e->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            out.push_back(full);
    }
    closedir(d);
#endif
    return out;
}

static void pushDir(std::vector<std::string>& v, const char* s) {
    if (s && *s) v.push_back(s);
}

std::vector<std::string> librarySearchDirs(const std::string& envVar) {
    std::vector<std::string> dirs;
    if (!envVar.empty()) pushDir(dirs, getenv(envVar.c_str()));

#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(0, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::string exe(buf, n);
        size_t p = exe.find_last_of("\\/");
        if (p != std::string::npos) dirs.push_back(exe.substr(0, p));
    }
    pushDir(dirs, getenv("PATH"));
#elif defined(__APPLE__)
    pushDir(dirs, getenv("DYLD_LIBRARY_PATH"));
    pushDir(dirs, "/usr/local/lib");
    pushDir(dirs, "/opt/homebrew/lib");
    pushDir(dirs, "/usr/lib");
    /* app bundle Frameworks next to the executable */
    {
        char buf[4096];
        uint32_t sz = sizeof(buf);
        extern int _NSGetExecutablePath(char*, uint32_t*);
        if (_NSGetExecutablePath(buf, &sz) == 0) {
            std::string exe(buf);
            size_t p = exe.find_last_of('/');
            if (p != std::string::npos) {
                std::string d = exe.substr(0, p);
                dirs.push_back(d);
                dirs.push_back(d + "/../Frameworks");
            }
        }
    }
#else
    pushDir(dirs, getenv("LD_LIBRARY_PATH"));
    pushDir(dirs, "/usr/lib/x86_64-linux-gnu");
    pushDir(dirs, "/usr/lib/aarch64-linux-gnu");
    pushDir(dirs, "/usr/lib");
    pushDir(dirs, "/usr/local/lib");
    pushDir(dirs, "/system/lib64");
    pushDir(dirs, "/system/lib");
    pushDir(dirs, "/vendor/lib64");
    pushDir(dirs, "/vendor/lib");
    /* Android: app native library dir (contains bundled .so) */
    pushDir(dirs, getenv("CXAUDIO_LIBDIR"));
#endif
    return dirs;
}

} /* namespace cx */
