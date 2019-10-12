#include <cstdlib>
#include <cinttypes>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ANSI.hpp"

// Строки-разделители для непривилегированного и привилигерованнонного пользователя.
const std::string up_delimeter = " ☭ ";
const std::string p_delimeter  = "! ";

// Копирующая вариация dup2.
int copy_dup2(int oldfd, int newfd)
{
    int copy = dup(oldfd);
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
void execute(const std::string& path, const std::vector<std::string>& args, int input_fd, int output_fd)
{
    // Сохранение текущих файловых дескрипторов ввода и вывода.
    int prev_input_fd  = -1;
    int prev_output_fd = -1;
    if ((prev_input_fd  = copy_dup2(0, input_fd))  == -1) { throw ExecutionException::ChangeInput;  }
    if ((prev_output_fd = copy_dup2(1, output_fd)) == -1) { throw ExecutionException::ChangeOutput; }

    pid_t process_id = fork(); // fork перед exec'ом.
    if (process_id == -1) { throw ExecutionException::Fork; }
    else if (process_id) // Родитель
    {
        if ((dup2(prev_input_fd, 0)  == -1) || (close(prev_input_fd)  == -1)) { throw ExecutionException::RestoreInput;  };
        if ((dup2(prev_output_fd, 1) == -1) || (close(prev_output_fd) == -1)) { throw ExecutionException::RestoreOutput; };
    }
    else // Ребёнок.
    {
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

    return;
}

int main(int argc, char* argv[])
{
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

        // Конвертация строки в поток ввода.
        std::stringstream input_stream(input, std::ios_base::in);

        do
        {
            // По договорённости, каждое первое слово в компоненте конвейера - команда.
            std::string comand;
            input_stream >> comand;

            // Встроенные команды.
            if (comand == "exit") { terminated = true; break; }
            // Сторонние программы.
            else
            {
                try
                {
                    // Аргументы.
                    std::vector<std::string> args(1);
                    args[0] = comand; // Первый аргумент - имя команды.

                    std::string next_arg;
                    while (input_stream)
                    {
                        args.push_back(next_arg);
                        //std::cout << next_arg << std::endl;
                    }

                    // Запуск.
                    execute(comand, args, 0, 1);

                    // Ожидание завершения.
                    int status;
                    wait(&status);
                }
                catch (const ExecutionException& Exception)
                {
                    switch (Exception)
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
                            std::cerr << "Не удалось выполнить команду " << comand << std::endl;
                        }
                    }
                    return 0;
                }
            }
        }
        while (input_stream);
    }
    return 0;
}
