#include <cstdio>
#include <cstdint>
#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <optional>
#include <ctime>

bool is_noise(const std::string &name)
{
    const std::string ext = ".swp";
    return name.size() >= ext.size() &&
           name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
}

const char *event_name(uint32_t mask)
{
    if (mask & IN_CREATE)
        return "created";
    if (mask & IN_DELETE)
        return "deleted";
    if (mask & IN_CLOSE_WRITE)
        return "modified";
    return "other";
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
    while (true)
    {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1)
        {
            perror("read");
            close(fd);
            return 1;
        }

        const char *ptr = buf;
        while (ptr < buf + n)
        {
            const inotify_event *ev = reinterpret_cast<const inotify_event *>(ptr);

            std::string name = ev->len > 0 ? ev->name : "";

            if (!is_noise(name))
            {
                std::cout << "[" << timestamp() << "] " << event_name(ev->mask);
                if (!name.empty())
                {
                    std::cout << ": " << name;
                }
                std::cout << "\n";
            }

            ptr += sizeof(inotify_event) + ev->len;
        }
    }

    close(fd);
    return 0;
}