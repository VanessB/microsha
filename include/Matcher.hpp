#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <string>
#include <dirent.h>

namespace Matcher
{
    class Matcher
    {
    public:
        Matcher(const std::string &pattern);
        ~Matcher();

        operator bool() const
        {return is_valid(); }

        void open(const std::string &pattern);
        void close();

        std::string current_match() const;
        bool next();
        bool is_valid() const;

    private:
        Matcher(const Matcher &) = delete;
        void operator =(const Matcher &) = delete;

    private:
        std::string _file_pattern;
        DIR* _dir;
        struct dirent *_dir_entry;
    };

}

#endif
