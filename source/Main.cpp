#include <cstdlib>
#include <cinttypes>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ANSI.hpp"

//#define DEBUG_INPUT
//#define DEBUG_WORDS
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

        // Запуск.
        if (execvp(path.c_str(), args_arr)) { throw ExecutionException::Execution; };
    }

    return process_id;
}



int main(int argc, char* argv[])
{
    enum class MainException
    {
        OK = 0,
        Execution = 1,
        Structure = 2,
        Pipe      = 3,
        File      = 4,
    };

    // Модификаторы консоли.
    ANSI::Modifier text_modifier = ANSI::Modifier();
    ANSI::Modifier special_modifier = ANSI::Modifier(ANSI::Style::Bold, ANSI::Color::Foreground::BoldRed, ANSI::Color::Background::Reset);

    // Основной цикл работы.
    bool terminated = false;
    while (!terminated)
    {
        // Приглашение к вводу и получение введённой строки.
        std::string input;
        std::cout << special_modifier << std::getenv("PWD") << up_delimeter << text_modifier << std::flush;
        std::getline(std::cin, input);

        #ifdef DEBUG_INPUT
        std::cout << input << std::endl;
        #endif

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
                if (state != States::Whitespace) { words[words.size() - 1].push_back(input[i]); }
            }

            #ifdef DEBUG_WORDS
            std::cout << "Разобранные слова:" << std::endl;
            for (size_t i = 0; i < words.size(); ++i) { std::cout << words[i] << std::endl; }
            #endif
        }

        // Разбор введённых слов.
        try
        {
            // Файловые дескрипторы для стандартного ввода и вывода запускаемых процессов.
            int input_fd = 0;
            int output_fd = 1;

            std::vector<std::string> arguments;
            for (size_t i = 0; i < words.size(); ++i)
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
                    else { arguments.push_back(words[i]); }

                    // Встроенные команды.
                    if (arguments[0] == "exit") { terminated = true; break; }
                    else if (i + 1 == words.size())
                    {
                        // Запуск процесса.
                        execute(arguments[0], arguments, input_fd, output_fd);

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
                        }
                    }
                    throw MainException::Execution;
                }
            }

            pid_t wpid = 0;
            int status = 0;
            while ((wpid = wait(&status)) > 0);
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
                }
            }
            return(0);
        }
    }
    return 0;
}
