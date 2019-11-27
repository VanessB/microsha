#include "Matcher.hpp"
#include <fnmatch.h>

namespace Matcher
{
    namespace
    {
        std::pair<std::string, std::string> split_path(const std::string &path)
        {
            std::string::size_type last_sep = path.find_last_of("/");
            if (last_sep != std::string::npos)
            {
                return std::make_pair(std::string(path.begin(), path.begin() + last_sep),
                                      std::string(path.begin() + last_sep + 1, path.end()));
            }
            return std::make_pair(".", path);
        }
    }

    Matcher::Matcher(const std::string &pattern):
        _dir(nullptr),
        _dir_entry(nullptr)
    {
        open(pattern);
    }

    Matcher::~Matcher() {
        close();
    }

    void Matcher::open(const std::string &pattern)
    {
        auto dir_and_file = split_path(pattern);
        _dir = opendir(dir_and_file.first.c_str());
        _file_pattern = dir_and_file.second;

        if (_dir != nullptr)
        {
            next();
        }
    }

    void Matcher::close()
    {
        if (_dir != nullptr)
        {
            closedir(_dir);
            _dir = nullptr;
        }
    }

    std::string Matcher::current_match() const
    {
        return _dir_entry->d_name;
    }

    bool Matcher::next()
    {
        while ((_dir_entry = readdir(_dir)) != nullptr)
        {
            if (!fnmatch(_file_pattern.c_str(),
                         _dir_entry->d_name,
                         FNM_CASEFOLD | FNM_NOESCAPE | FNM_PERIOD))
            {
                return true;
            }
        }
        return false;
    }

    bool Matcher::is_valid() const
    {
        return _dir != nullptr && _dir_entry != nullptr;
    }
}
