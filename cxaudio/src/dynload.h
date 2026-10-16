#ifndef CXAUDIO_DYNLOAD_H
#define CXAUDIO_DYNLOAD_H

#include <string>
#include <vector>

namespace cx {

class Library {
public:
    Library() {}
    ~Library();

    bool open(const std::string& path);
    void* sym(const char* name) const;
    bool isOpen() const { return handle_ != 0; }
    const std::string& path() const { return path_; }

private:
    Library(const Library&);
    Library& operator=(const Library&);

    void* handle_ = 0;
    std::string path_;
};

bool fileExists(const std::string& path);

/* returns directory entries whose name starts with prefix (or all if empty) */
std::vector<std::string> listDir(const std::string& dir, const std::string& prefix);

/* Directories to search for hot-loaded shared libs. envVar (may be empty)
 * is consulted first. Platform defaults included. */
std::vector<std::string> librarySearchDirs(const std::string& envVar);

} /* namespace cx */

#endif /* CXAUDIO_DYNLOAD_H */
