#ifndef ANSI_HPP
#define ANSI_HPP
#include <ostream>

namespace ANSI
{
    enum class Style
    {
        Reset     = 0,
        Bold      = 1,
        Dim       = 2,
        Italic    = 3,
        Underline = 4,
        Blink     = 5,
        Rblink    = 6,
        Reversed  = 7,
        Conceal   = 8,
        Crossed   = 9,
    };

    namespace Color
    {
        enum class Foreground
        {
            Black   = 30,
            Red     = 31,
            Green   = 32,
            Yellow  = 33,
            Blue    = 34,
            Magenta = 35,
            Cyan    = 36,
            Gray    = 37,
            Reset   = 39,

            // Жирный.
            BoldBlack   = 90,
            BoldRed     = 91,
            BoldGreen   = 92,
            BoldYellow  = 93,
            BoldBlue    = 94,
            BoldMagenta = 95,
            BoldCyan    = 96,
            BoldGray    = 97,
        };

        enum class Background
        {
            Black   = 40,
            Red     = 41,
            Green   = 42,
            Yellow  = 43,
            Blue    = 44,
            Magenta = 45,
            Cyan    = 46,
            Gray    = 47,
            Reset   = 49,

            // Жирный.
            BoldBlack   = 100,
            BoldRed     = 101,
            BoldGreen   = 102,
            BoldYellow  = 103,
            BoldBlue    = 104,
            BoldMagenta = 105,
            BoldCyan    = 106,
            BoldGray    = 107,
        };
    }

    class Modifier
    {
    public:
        Style style;
        Color::Foreground foreground;
        Color::Background background;

        Modifier(Style init_style = Style::Reset,
                 Color::Foreground init_foreground = Color::Foreground::Reset,
                 Color::Background init_background = Color::Background::Reset)
        {
            style = init_style;
            foreground = init_foreground;
            background = init_background;
        }
        ~Modifier() { }

        friend std::ostream& operator << (std::ostream& os, const Modifier& mod)
        {
            return os << "\033[" << static_cast<int>(mod.style) << "m"
                      << "\033[" << static_cast<int>(mod.foreground) << "m"
                      << "\033[" << static_cast<int>(mod.background) << "m";
        }

        std::string string()
        {
            return "\033[" + std::to_string(static_cast<int>(style)) + "m"
            + "\033[" + std::to_string(static_cast<int>(foreground)) + "m"
            + "\033[" + std::to_string(static_cast<int>(background)) + "m";
        }

    protected:

    private:

    };
}

#endif
