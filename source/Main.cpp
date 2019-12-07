#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <filesystem>
//#include <regex>
#include <fnmatch.h>

// Linux.
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/times.h>

// Стили ANSI.
#include "ANSI.hpp"

//#define DEBUG_INPUT
//#define DEBUG_WORDS
//#define DEBUG_ENV
//#define DEBUG_EXECUTE

// Строки-разделители для непривилегированного и привилигерованнонного пользователя.
const std::string up_delimeter = " ☭ ";
const std::string p_delimeter  = "! ";


// Копирующая вариация dup2.
inline int copy_dup2(int oldfd, int newfd)
{
    int copy = dup(newfd);
    if (copy == -1) { return -1; }
    if (dup2(oldfd, newfd) == -1) { return -1; }
    return copy;
}

// Исключения процедуры запуска.
enum class ExecutionException
{
    OK            = 0,
    ChangeInput   = 1,
    ChangeOutput  = 2,
    RestoreInput  = 3,
    RestoreOutput = 4,
    Fork          = 5,
    Execution     = 6,
};

// Запуск исполняемого файла по указанному пути с переопределением ввода и вывода.
pid_t execute(const std::string& path, const std::vector<std::string>& args, int input_fd, int output_fd, int close_fd = -1)
{
    #ifdef DEBUG_EXECUTE
    std::cout << "Ввод: " << input_fd << std::endl << "Вывод: " << output_fd << std::endl;
    #endif

    // Сохранение текущих файловых дескрипторов ввода и вывода.
    int prev_input_fd  = -1;
    int prev_output_fd = -1;
    if ((prev_input_fd  = copy_dup2(input_fd, 0))  == -1) { throw ExecutionException::ChangeInput;  }
    if ((prev_output_fd = copy_dup2(output_fd, 1)) == -1) { throw ExecutionException::ChangeOutput; }

    pid_t process_id = fork(); // fork перед exec'ом.
    if (process_id == -1) { throw ExecutionException::Fork; }
    else if (process_id) // Родитель
    {
        if ((dup2(prev_input_fd, 0)  == -1) || (close(prev_input_fd)  == -1)) { throw ExecutionException::RestoreInput;  };
        if ((dup2(prev_output_fd, 1) == -1) || (close(prev_output_fd) == -1)) { throw ExecutionException::RestoreOutput; };
    }
    else // Ребёнок.
    {
        // Закрытие переданного дополнительного файлового дескриптора со стороны дочернего процесса.
        if (close_fd != -1) { close(close_fd); }

        // Подготовка массива аргументов.
        char** args_arr = new char*[args.size() + 1];
        for(size_t i = 0; i < args.size(); ++i)
        {
            args_arr[i] = new char[args[i].size() + 1];
            memcpy(args_arr[i], args[i].c_str(), args[i].size() + 1);
        }
        args_arr[args.size()] = NULL;

        #ifdef DEBUG_ENV
        std::cout << "PWD: " << std::getenv("PWD") << std::endl;
        #endif

        // Стандартная обработка сигналов.
        signal(SIGINT, SIG_DFL);

        // Запуск.
        if (execvp(path.c_str(), args_arr))
        {
            for(size_t i = 0; i < args.size(); ++i)
            { delete[] args_arr[i]; }
            delete[] args_arr;
            throw ExecutionException::Execution;
        }
    }

    return process_id;
}


// Поиск файлов, подходящих под регулярное выражение.
std::vector<std::string> match_files(std::string argument)
{
    std::vector<std::string> result;
    if (!argument.size()) { result.push_back(argument); }
    else
    {
        bool absolute = false;
        int max_depth = 0;
        for (size_t i = 0; i < argument.size(); ++i)
        { if (argument[i] == '/') { ++max_depth; } }

        std::filesystem::path walk_path;
        if (argument[0] == '/')
        {
            absolute = true;
            --max_depth;
            walk_path = std::filesystem::path("/");
        }
        else
        { walk_path = std::filesystem::current_path(); }

        for (auto iterator = std::filesystem::recursive_directory_iterator(walk_path, std::filesystem::directory_options::skip_permission_denied);
             iterator != std::filesystem::recursive_directory_iterator();
             ++iterator )
        {
            if (iterator.depth() > max_depth) { iterator.pop(); continue; }
            //if (!iterator->is_regular_file()) { continue; }
            if (absolute)
            {
                if (fnmatch(argument.c_str(), iterator->path().c_str(), FNM_PATHNAME) == 0)
                { result.push_back(iterator->path().string()); }
            }
            else
            {
                if (fnmatch((walk_path.string() + "/" + argument).c_str(), iterator->path().c_str(), FNM_PATHNAME) == 0)
                { result.push_back(iterator->path().string()); }
            }
        }
    }

    return result;
}



int main(int argc, char* argv[])
{
    // Коды основных исключений.
    enum class MainException
    {
        OK        = 0,
        Execution = 1,
        Structure = 2,
        Pipe      = 3,
        File      = 4,
        Env       = 5,
    };

    // Модификаторы консоли.
    ANSI::Modifier text_modifier = ANSI::Modifier();
    ANSI::Modifier special_modifier = ANSI::Modifier(ANSI::Style::Bold, ANSI::Color::Foreground::BoldRed, ANSI::Color::Background::Reset);

    // Основной цикл работы.
    bool terminated = false;
    while (!terminated)
    {
        try
        {
            pid_t user_id = getuid(); // Получение ID пользователя.

            // Приглашение к вводу и получение введённой строки.
            std::string input;
            {
                char* ptr = get_current_dir_name();
                if (ptr == nullptr) { throw MainException::Env; }
                std::cout << special_modifier << ptr << (user_id == 0 ? p_delimeter : up_delimeter) << text_modifier << std::flush;
                free(ptr);
            }
            std::getline(std::cin, input);

            // Завершение при пустом вводе.
            //if (input.empty())
            //{
            //    std::cout << std::endl;
            //    return 0;
            //}

            #ifdef DEBUG_INPUT
            std::cout << input << std::endl;
            #endif

            // Переменные для измерения времени выполнения.
            bool time = false;
            tms times_first;
            std::chrono::time_point<std::chrono::system_clock> real_time_first;

            // Массив введённых слов.
            std::vector<std::string> words;

            // Разбиение ввода на слова с учётом символов ' " '.
            {
                // Состояния ДКА.
                enum class States
                {
                    Whitespace = 0,
                    Quoted     = 1,
                    Unquoted   = 2,
                };
                States state = States::Whitespace;
                for (size_t i = 0; i < input.size(); ++i)
                {
                    switch (state)
                    {
                        case States::Whitespace:
                        {
                            switch (input[i])
                            {
                                case ' ':  { break; }
                                case '\t': { break; }
                                case '\"':
                                {
                                    state = States::Quoted;
                                    words.push_back(""); // Встретился непустой символ - создаётся новое слово.
                                    break;
                                }
                                default:
                                {
                                    state = States::Unquoted;
                                    words.push_back(""); // Встретился непустой символ - создаётся новое слово.
                                    break;
                                }
                            }
                            break;
                        }
                        case States::Quoted:
                        {
                            switch (input[i])
                            {
                                case '\"': { state = States::Unquoted; break; }
                                default:   { break; }
                            }
                            break;
                        }
                        case States::Unquoted:
                        {
                            switch (input[i])
                            {
                                case ' ':  { state = States::Whitespace; break; }
                                case '\t': { state = States::Whitespace; break; }
                                case '\"': { state = States::Quoted; break; }
                                default:   { break; }
                            }
                            break;
                        }
                    }

                    // Если символ не пустой, добавляем в слово.
                    if ((state != States::Whitespace) && (input[i] != '\"')) { words[words.size() - 1].push_back(input[i]); }
                }

                #ifdef DEBUG_WORDS
                std::cout << "Разобранные слова:" << std::endl;
                for (size_t i = 0; i < words.size(); ++i) { std::cout << words[i] << std::endl; }
                #endif
            }

            // Пустой ввод.
            if (words.empty()) { throw MainException::Structure; }

            // Разбор введённых слов.
            // Сначала обработка встроенных команд.
            if (words[0] == "exit") { terminated = true; break; }
            else if (words[0] == "cd")
            {
                if (words.size() < 2)
                { if (chdir(std::getenv("HOME"))) { throw MainException::File; } }
                else
                { if (chdir(words[1].c_str())) { throw MainException::File; } }
            }
            else if (words[0] == "set")
            {
                // Ищем разделитель - знак "=".
                size_t delimeter_position = 0; // Положение разделителя.
                if ((words.size() < 2) || ((delimeter_position = words[1].find("=")) == std::string::npos)) { throw MainException::Structure; }
                size_t size = words[1].size();

                // Положения и длины требуемых подстрок.
                size_t name_position  = 0;
                size_t name_length    = delimeter_position;
                size_t value_position = delimeter_position + 1;
                size_t value_length   = size - value_position;

                // Обработка кавычек.
                #ifdef DEBUG_ENV
                std::cout << variable << " = " << value << std::endl;
                std::cout << "value_position: " << value_position << ": " << words[1][value_position] << "  "
                          << "value_position + value_lenth - 1: " << value_position + value_length << ": " << words[1][value_position + value_length - 1] << std::endl;
                #endif
                if ((value_length > 1) && (words[1][value_position] == '\"') && (words[1][value_position + value_length - 1] == '\"'))
                {
                    ++value_position;
                    value_length -= 2;
                }

                std::string variable = words[1].substr(name_position, name_length);
                std::string value = words[1].substr(value_position, value_length);

                #ifdef DEBUG_ENV
                std::cout << variable << " = " << value << std::endl;
                #endif
                setenv(variable.c_str(), value.c_str(), 1);
            }
            else if (words[0] == "get")
            {
                if (words.size() < 2) { throw MainException::Structure; }
                char* ptr = std::getenv(words[1].c_str());
                if (ptr == nullptr) { throw MainException::Env; }
                std::cout << ptr << std::endl;
            }
            else
            {
                // Файловые дескрипторы для стандартного ввода и вывода запускаемых процессов.
                int input_fd = 0;
                int output_fd = 1;

                // Обработка команды time.
                size_t i = 0;
                if (words.size() > 0 && words[0] == "time")
                {
                    time = true;
                    i = 1;
                    times(&times_first);
                    real_time_first = std::chrono::system_clock::now();
                }

                std::vector<std::string> arguments;
                for (; i < words.size(); ++i)
                {
                    // Здесь возможен запуск процесса.
                    try
                    {
                        if (words[i] == "|")
                        {
                            if (output_fd != 1) { throw MainException::Structure; } // Вывод уже переопределён.
                            else
                            {
                                // Создание pipe'а.
                                int pipefd[2];
                                if (pipe(pipefd)) { throw MainException::Pipe; }

                                // Перенаправление вывода от запускаемого процесса в pipe.
                                output_fd = pipefd[1];

                                // Запуск процесса.
                                execute(arguments[0], arguments, input_fd, output_fd, pipefd[0]); // Запуск с закрытием ввода pipe'а со стороны дочернего процесса.
                                close(output_fd);                                                 // Закрытие вывода pipe со стороны родителя.

                                // Обработка входного конца pipe.
                                if (input_fd != 0) { close(input_fd); }

                                // Очистка аргументов.
                                arguments.clear();

                                // Перенаправление ввода/вывода для следующего процесса.
                                input_fd = pipefd[0];
                                output_fd = 1;
                            }
                        }
                        else if (words[i] == "<")
                        {
                            if (input_fd != 0) { throw MainException::Structure; } // Ввод уже переопределён.
                            else
                            {
                                if ((i + 1 >= words.size()) || ( (input_fd = open(words[i + 1].c_str(), O_RDWR)) == -1 )) { throw MainException::File; }
                                else { ++i; }
                            }
                        }
                        else if (words[i] == ">")
                        {
                            if (output_fd != 1) { throw MainException::Structure; } // Вывод уже переопределён.
                            else
                            {
                                if ((i + 1 >= words.size()) || ( (output_fd = open(words[i + 1].c_str(), O_RDWR | O_CREAT, 0666)) == -1)) { throw MainException::File; }
                                else { ++i; }
                            }
                        }
                        else
                        {
                            // Если список аргументов пустой, первый аргумент - имя исполняемой команды, передаётся неизменным.
                            if (arguments.empty())
                            {
                                arguments.push_back(words[i]);
                            }
                            // Иначе, производится поиск подходящих под регулярное выражение аргументов.
                            else
                            {
                                std::vector<std::string> matched = match_files(words[i]);
                                if (matched.empty())
                                { arguments.push_back(words[i]); }
                                else
                                { arguments.insert(arguments.end(), matched.begin(), matched.end()); }
                            }
                        }

                        if (i + 1 == words.size())
                        {
                            // Запуск процесса.
                            execute(arguments[0], arguments, input_fd, output_fd);

                            // Обработка входного конца pipe.
                            if (input_fd != 0) { close(input_fd); }

                            // Очистка аргументов.
                            arguments.clear();

                            // Перенаправление ввода/вывода для следующего процесса.
                            input_fd = 0;
                            output_fd = 1;
                        }
                    }
                    catch (const ExecutionException& exception)
                    {
                        switch (exception)
                        {
                            case ExecutionException::OK: { break; }
                            case ExecutionException::ChangeInput:
                            {
                                std::cerr << "Не удалось переопределить стандартный ввод для исполняемой команды." << std::endl;
                                break;
                            }
                            case ExecutionException::ChangeOutput:
                            {
                                std::cerr << "Не удалось переопределить стандартный вывод для исполняемой команды." << std::endl;
                                break;
                            }
                            case ExecutionException::RestoreInput:
                            {
                                std::cerr << "Не удалось переопределить стандартный ввод для оболочки." << std::endl;
                                break;
                            }
                            case ExecutionException::RestoreOutput:
                            {
                                std::cerr << "Не удалось переопределить стандартный вывод для оболочки." << std::endl;
                                break;
                            }
                            case ExecutionException::Fork:
                            {
                                std::cerr << "Ошибка системного вызова fork." << std::endl;
                                break;
                            }
                            case ExecutionException::Execution:
                            {
                                std::cerr << "Не удалось выполнить команду " << arguments[0] << std::endl;
                                break;
                            }
                        }
                        throw MainException::Execution;
                    }
                }

                // Игнорирование сигнала прерывания.
                signal(SIGINT, SIG_IGN);

                // Ожидание порождённых процессов.
                pid_t wpid = 0;
                int status = 0;
                while ((wpid = wait(&status)) > 0);

                // Стандартная обработка сигналов.
                signal(SIGINT, SIG_DFL);

                // Вывод времени работы детей.
                if (time)
                {
                    tms times_second;
                    times(&times_second);
                    auto real_time_second = std::chrono::system_clock::now();
                    std::cout << std::fixed << std::setprecision(3) << std::endl
                              << "real:\t" << std::chrono::duration<double>(real_time_second - real_time_first).count() << "ms" << std::endl
                              << "user:\t" << 1000.0 * (times_second.tms_cutime - times_first.tms_cutime) / CLOCKS_PER_SEC << "ms" << std::endl
                              << "sys:\t"  << 1000.0 * (times_second.tms_cstime - times_first.tms_cstime) / CLOCKS_PER_SEC << "ms" << std::endl
                              << std::defaultfloat;
                }
            }
        }
        catch (const MainException& exception)
        {
            switch (exception)
            {
                case MainException::OK: { break; }
                case MainException::Execution: { break; }
                case MainException::Structure:
                {
                    std::cerr << "Неверная структура команды." << std::endl;
                    break;
                }
                case MainException::Pipe:
                {
                    std::cerr << "Ошибка при открытии pipe." << std::endl;
                    break;
                }
                case MainException::File:
                {
                    std::cerr << "Ошибка при открытии файла." << std::endl;
                    break;
                }
                case MainException::Env:
                {
                    std::cerr << "Ошибка операции с переменной среды." << std::endl;
                    break;
                }
            }
            return 0;
        }
    }

    return 0;
}
