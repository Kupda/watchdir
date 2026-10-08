#include <cstdio>
#include <cstdint>
#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <optional>
#include <ctime>
#include <map>
#include <csignal>
#include <cerrno>

volatile sig_atomic_t g_stop = 0;

void on_sigint(int)
{
    g_stop = 1;
}

bool is_noise(const std::string &name)
{
    const std::string ext = ".swp";
    return name.size() >= ext.size() &&
           name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
}

std::optional<uint64_t> hash_file(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    uint64_t h = 14695981039346656037ULL;
    char buf[4096];
    while (in.read(buf, sizeof(buf)) || in.gcount() > 0)
    {
        for (std::streamsize i = 0; i < in.gcount(); ++i)
        {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

std::string timestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    localtime_r(&now, &tm_now);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &tm_now);
    return buf;
}

int main(int argc, char *argv[])
{
    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    const char *path = ".";
    if (argc > 1)
    {
        path = argv[1];
    }

    int fd = inotify_init1(0);
    if (fd == -1)
    {
        perror("inotify_init1");
        return 1;
    }

    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_CLOSE_WRITE);
    if (wd == -1)
    {
        perror("inotify_add_watch");
        close(fd);
        return 1;
    }

    std::cout << "watching " << path << "...\n";

    alignas(inotify_event) char buf[4096];
    std::map<std::string, uint64_t> hashes;
    while (!g_stop)
    {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            close(fd);
            return 1;
        }

        const char *ptr = buf;
        while (ptr < buf + n)
        {
            const inotify_event *ev = reinterpret_cast<const inotify_event *>(ptr);

            std::string name = ev->len > 0 ? ev->name : "";

            if (!name.empty() && !is_noise(name))
            {
                std::string full = std::string(path) + "/" + name;
                const char *label = nullptr;

                if (ev->mask & IN_CREATE)
                {
                    auto h = hash_file(full);
                    if (h)
                        hashes[full] = *h;
                    label = "created";
                }
                else if (ev->mask & IN_DELETE)
                {
                    hashes.erase(full);
                    label = "deleted";
                }
                else if (ev->mask & IN_CLOSE_WRITE)
                {
                    auto h = hash_file(full);
                    if (h)
                    {
                        auto it = hashes.find(full);
                        bool changed = (it == hashes.end() || it->second != *h);
                        hashes[full] = *h;
                        if (changed)
                            label = "modified";
                    }
                }

                if (label)
                {
                    std::cout << "[" << timestamp() << "] " << label << ": " << name << "\n";
                }
            }

            ptr += sizeof(inotify_event) + ev->len;
        }
    }

    std::cout << "\nstopping...\n";
    close(fd);
    return 0;
}