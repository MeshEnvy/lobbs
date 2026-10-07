#pragma once

#include "lolog_test_helpers.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

/** In-memory reference tree for fuzz / crash oracle (paths without leading slash). */
class RefFs {
  public:
    bool mkdir(const std::string &path)
    {
        if (path.empty() || files.count(path))
            return false;
        dirs.insert(path);
        return true;
    }

    bool write(const std::string &path, const std::vector<uint8_t> &data)
    {
        if (path.empty())
            return false;
        files[path] = data;
        return true;
    }

    bool unlink(const std::string &path) { return files.erase(path) > 0; }

    bool rename(const std::string &from, const std::string &to)
    {
        auto it = files.find(from);
        if (it == files.end())
            return false;
        if (files.count(to))
            return false;
        files[to] = it->second;
        files.erase(it);
        return true;
    }

    bool existsFile(const std::string &path) const { return files.count(path) > 0; }

    const std::map<std::string, std::vector<uint8_t>> &allFiles() const { return files; }

    void clear()
    {
        files.clear();
        dirs.clear();
    }

  private:
    std::map<std::string, std::vector<uint8_t>> files;
    std::set<std::string> dirs;
};

inline bool lologMatchesRef(LoLog &log, const RefFs &ref)
{
    for (const auto &kv : ref.allFiles()) {
        if (!log.exists(kv.first.c_str()))
            return false;
        std::vector<uint8_t> got;
        if (!lologReadFile(log, kv.first.c_str(), got))
            return false;
        if (got != kv.second)
            return false;
    }
    for (const auto &kv : ref.allFiles()) {
        if (log.exists(kv.first.c_str()) && !ref.existsFile(kv.first))
            return false;
    }
    std::vector<std::string> logPaths;
    for (int i = 0; i < 512; i++) {
        char p[32];
        snprintf(p, sizeof(p), "fuzz_%d.dat", i);
        if (log.exists(p))
            logPaths.push_back(p);
    }
    for (const auto &p : logPaths) {
        if (!ref.existsFile(p))
            return false;
    }
    return true;
}
