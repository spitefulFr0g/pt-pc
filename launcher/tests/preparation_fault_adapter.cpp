// Disposable Linux test child only. No production bypass or fault hook.
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
extern "C" int rename(const char* from, const char* to) noexcept {
    auto real = reinterpret_cast<int(*)(const char*, const char*)>(dlsym(RTLD_NEXT, "rename"));
    int result = real(from, to);
    const char* boundary = std::getenv("PT_TEST_RENAME_BOUNDARY");
    if (result == 0 && boundary && std::strstr(from, boundary)) raise(SIGSTOP);
    return result;
}
#include <cerrno>
#include <cstdio>
extern "C" ssize_t read(int fd, void* buffer, size_t size) {
    auto real = reinterpret_cast<ssize_t(*)(int, void*, size_t)>(dlsym(RTLD_NEXT, "read"));
    if (std::getenv("PT_TEST_PROC_READ_RACE")) {
        char link[64], name[1024];
        std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        auto length = readlink(link, name, sizeof(name)-1);
        if (length > 0) {
            name[length] = 0;
            if (std::strncmp(name, "/proc/", 6) == 0 && std::strstr(name, "/cmdline")) {
                unsetenv("PT_TEST_PROC_READ_RACE"); errno = ESRCH; return -1;
            }
        }
    }
    return real(fd, buffer, size);
}
