#pragma once

#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

namespace zappy {

inline const std::string &cdnBase()
{
    static const std::string base = [] {
        const char *env = std::getenv("ZAPPY_CDN_URL");
        return std::string(env ? env
            : "https://pub-6d9e00b90894418295af58a44ecfe1df.r2.dev");
    }();
    return base;
}

namespace detail {
inline std::mutex &storeMutex() { static std::mutex m; return m; }
inline std::unordered_map<std::string, std::string> &store()
{
    static std::unordered_map<std::string, std::string> s;
    return s;
}

inline bool localAssetsAvailable()
{
    static const bool ok = [] {
        if (const char *f = std::getenv("ZAPPY_FORCE_CDN"); f && *f && *f != '0')
            return false;
        std::ifstream probe("assets/fonts/GAMEDAY.ttf", std::ios::binary);
        return probe.good();
    }();
    return ok;
}

inline std::string fetchRaw(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (f.good())
        return std::string((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

    if (localAssetsAvailable())
        return {};

    const std::string prefix = "assets/";
    if (path.rfind(prefix, 0) != 0)
        return {};
    std::string rel = path.substr(prefix.size());

    std::string cmd = "curl -fsSL --connect-timeout 10 --max-time 120 '"
                    + cdnBase() + "/" + rel + "'";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return {};
    std::string out;
    char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0)
        out.append(buf, n);
    if (pclose(p) != 0 || out.empty())
        std::cerr << "[cdn] WARN: échec du chargement de " << rel << "\n";
    return out;
}
}

inline const std::string &assetBytesKept(const std::string &path)
{
    auto &st = detail::store();
    auto &mx = detail::storeMutex();
    {
        std::lock_guard<std::mutex> lk(mx);
        auto it = st.find(path);
        if (it != st.end()) return it->second;
    }
    std::string data = detail::fetchRaw(path);
    if (data.empty()) {
        static const std::string empty;
        return empty;
    }
    std::lock_guard<std::mutex> lk(mx);
    return st.emplace(path, std::move(data)).first->second;
}

inline std::string assetBytes(const std::string &path)
{
    return assetBytesKept(path);
}

inline std::string assetBytesIfCached(const std::string &path)
{
    auto &st = detail::store();
    std::lock_guard<std::mutex> lk(detail::storeMutex());
    auto it = st.find(path);
    return (it != st.end()) ? it->second : std::string();
}

inline void prefetch(const std::vector<std::string> &paths,
                     std::atomic<size_t> *done, int concurrency = 12)
{
    std::atomic<size_t> idx{0};
    auto worker = [&] {
        size_t i;
        while ((i = idx.fetch_add(1)) < paths.size()) {

            for (int tries = 0; tries < 4; ++tries)
                if (!assetBytesKept(paths[i]).empty()) break;
            if (done) done->fetch_add(1);
        }
    };
    std::vector<std::thread> ts;
    int n = concurrency < 1 ? 1 : concurrency;
    for (int k = 0; k < n; ++k) ts.emplace_back(worker);
    for (auto &t : ts) t.join();
}

}
