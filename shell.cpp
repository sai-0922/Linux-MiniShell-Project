#include <iostream>
#include <algorithm>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include <readline/readline.h>
#include <ext/stdio_filebuf.h>

#include "delep.hpp"
#include "history.hpp"
#include "squashbug.hpp"

using namespace std;

static sigjmp_buf env;

size_t job_number = 1;

volatile bool is_background;
pid_t foreground_pid;
set<pid_t> background_pids;

class Command
{
public:
    string command;
    vector<string> arguments;
    int input_fd, output_fd;
    string input_file, output_file;
    pid_t pid;
    bool pipe_mode = false;

    Command(const string &cmd) : command(cmd), input_fd(STDIN_FILENO), output_fd(STDOUT_FILENO), input_file(""), output_file(""), pid(-1)
    {
        parse_command();
    }

    ~Command()
    {
        if (input_fd != STDIN_FILENO)
            close(input_fd);
        if (output_fd != STDOUT_FILENO)
            close(output_fd);
    }

    void parse_command()
    {
        /* Need to do more improvisations here */

        // Parse the command string into the command and arguments
        stringstream ss(command);
        string arg;
        string temp = "";
        bool backslash = false;
        while (ss >> arg)
        {
            if (arg == "<")
            {
                ss >> input_file;
                backslash = false;
            }
            else if (arg == ">")
            {
                ss >> output_file;
                backslash = false;
            }
            else if (arg[arg.size() - 1] == '\\')
            {
                temp = temp + arg;
                temp[temp.size() - 1] = ' ';
                backslash = true;
            }
            else
            {
                if (backslash)
                {
                    temp = temp + arg;
                    arguments.push_back(temp);
                    temp = "";
                    backslash = false;
                }
                else
                    arguments.push_back(arg);
            }
        }

        // Set the command to the first argument
        command = arguments[0];

        // //Handle wildcards
        vector<string> temp_args;
        for (auto &arg : arguments)
        {
            if (arg.find('*') != string::npos || arg.find('?') != string::npos)
            {
                glob_t glob_result;
                int ret = glob(arg.c_str(), GLOB_TILDE, NULL, &glob_result);
                if (ret != 0)
                {
                    cerr << "No such file or directory";
                    fflush(stdout);
                    siglongjmp(env, 42);
                }
                else
                {
                    for (unsigned int i = 0; i < glob_result.gl_pathc; ++i)
                    {
                        arg = glob_result.gl_pathv[i];
                        temp_args.push_back(arg);
                    }

                    globfree(&glob_result);
                }
            }
            else
                temp_args.push_back(arg);
        }

        arguments.clear();
        arguments = temp_args;

        // Set the input and output file descriptors
        IO_redirection();
    }

    void IO_redirection()
    {
        // Open the input file for reading, if it exists
        if (!input_file.empty())
        {
            input_fd = open(input_file.c_str(), O_RDONLY);
            if (input_fd == -1)
            {
                cerr << "Error opening input file: " << input_file << endl;
                exit(EXIT_FAILURE);
            }
        }

        // Open the output file for writing, creating it if it does not exist
        if (!output_file.empty())
        {
            output_fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (output_fd == -1)
            {
                cerr << "Error opening output file: " << output_file << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
};

int execute_command(Command &command, bool background)
{
    // Execute the command
    // If the command is not found, print an error message
    // If the command is found, execute it
    char **args;
    args = new char *[command.arguments.size() + 1];
    for (int i = 0; i < (int)command.arguments.size(); i++)
    {
        args[i] = strdup(command.arguments[i].c_str());
    }
    args[command.arguments.size()] = NULL;

    // Redirect the input and output
    dup2(command.input_fd, STDIN_FILENO);
    dup2(command.output_fd, STDOUT_FILENO);

    // Execute the command
    int ret = execvp(command.command.c_str(), args);
    if (ret == -1)
    {
        cerr << "Error executing command: " << command.command << endl;
        exit(EXIT_FAILURE);
    }
    return 0;
}

string shell_prompt()
{
    // Print the shell prompt in user@pcname:current_directory$
    char *user = getenv("USER");
    char *pcname = new char[1];
    size_t capacity = 1;
    while (gethostname(pcname, capacity) < 0)
    {
        delete[] pcname;
        capacity *= 2;
        pcname = new char[capacity];
    }
    char *current_directory = new char[1];
    capacity = 1;
    while (!(current_directory = getcwd(current_directory, capacity)))
    {
        delete[] current_directory;
        capacity *= 2;
        current_directory = new char[capacity];
    }
    string prompt = string(user) + "@" + string(pcname) + ":" + string(current_directory) + "$ ";
    delete[] current_directory;
    delete[] pcname;
    return prompt;
}

void delim_remove(string &command)
{
    // Remove the starting and ending spaces
    while (command[0] == ' ')
        command.erase(0, 1);
    while (command[command.length() - 1] == ' ')
        command.pop_back();
}

history h;
char *curr_line = (char *)NULL;

static int key_up_arrow(int count, int key)
{
    if (count == 0)
        return 0;
    if (h.curr_ind == h.get_size())
        curr_line = strdup(rl_line_buffer);
    h.decrement_history();
    string line = h.get_curr();
    rl_replace_line(line.c_str(), 0);
    rl_point = line.size();
    return 0;
}

static int key_down_arrow(int count, int key)
{
    if (count == 0)
        return 0;
    if (h.curr_ind == h.get_size())
        curr_line = strdup(rl_line_buffer);
    h.increment_history();
    if (h.curr_ind < h.get_size())
    {
        string line = h.get_curr();
        rl_replace_line(line.c_str(), 0);
        rl_point = line.size();
    }
    else
    {
        rl_replace_line(curr_line, 0);
        rl_point = rl_end;
    }
    return 0;
}

static int key_ctrl_a(int count, int key)
{
    if (count == 0)
        return 0;
    rl_point = 0;
    return 0;
}

static int key_ctrl_e(int count, int key)
{
    if (count == 0)
        return 0;
    rl_point = rl_end;
    return 0;
}

void ctrl_c_handler(int signum)
{
    if (foreground_pid == 0)
    {
        siglongjmp(env, 42);
    }
    cout << endl;
    kill(foreground_pid, SIGKILL);
    foreground_pid = 0;
}

void ctrl_z_handler(int signum)
{
    if (foreground_pid == 0)
    {
        siglongjmp(env, 42);
    }
    cout << endl
         << "[" << job_number++ << "] " << foreground_pid << endl;
    kill(foreground_pid, SIGCONT);
    background_pids.insert(foreground_pid);
    foreground_pid = 0;
}

void child_signal_handler(int signum)
{
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0)
        background_pids.erase(pid);
}

int main()
{
    char *input;

    rl_initialize();
    rl_bind_keyseq("\\e[A", key_up_arrow);
    rl_bind_keyseq("\\e[B", key_down_arrow);
    rl_bind_keyseq("\\C-a", key_ctrl_a);
    rl_bind_keyseq("\\C-e", key_ctrl_e);
    rl_bind_key('\t', rl_insert);

    // Register the signal handlers
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = &ctrl_c_handler;
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &ctrl_z_handler;
    sigaction(SIGTSTP, &sa, NULL);

    struct sigaction sa_child;
    memset(&sa_child, 0, sizeof(sa_child));
    sa_child.sa_handler = &child_signal_handler;
    sigaction(SIGCHLD, &sa_child, NULL);

    while (1)
    {
        if (sigsetjmp(env, 1) == 42)
        {
            cout << endl;
            continue;
        }

        string prompt = shell_prompt();
        input = readline(prompt.c_str());

        // Exit the shell on CTRL+D
        if (input == NULL)
        {
            cout << "exit" << endl;
            exit(EXIT_SUCCESS);
        }

        string command = string(input);
        free(input);
        input = (char *)NULL;

        // Add the command to the history

        delim_remove(command);
        if (command == "")
        {
            continue;
        }
        h.add_history(command);
        vector<string> commands;
        int i = 0;
        while (i < (int)command.length())
        {
            if (command[i] == '|')
            {
                commands.push_back(command.substr(0, i));
                command.erase(0, i + 1);
                i = 0;
            }
            else
                i++;
        }
        delim_remove(command);
        if (command != "")
            commands.push_back(command);
        int pipefd[2];
        for (int i = 0; i < (int)commands.size(); i++)
        {
            try
            {
                delim_remove(commands[i]);
                const string cmd = commands[i];
                Command shell_command(cmd);

                is_background = false;
                if (shell_command.arguments[shell_command.arguments.size() - 1] == "&")
                {
                    is_background = true;
                    shell_command.arguments.pop_back();
                }

                // Pipe the output of one command to the input of the next
                if (i > 0)
                {
                    shell_command.input_fd = pipefd[0];
                }
                if (i < (int)commands.size() - 1)
                {
                    pipe(pipefd);
                    if (pipefd[0] == -1 || pipefd[1] == -1)
                    {
                        cerr << "Error creating pipe" << endl;
                        exit(EXIT_FAILURE);
                    }
                    shell_command.output_fd = pipefd[1];
                }

                // Check if the command is a built-in command
                if (shell_command.command == "exit")
                {
                    cout << "exit" << endl;
                    exit(EXIT_SUCCESS);
                }
                else if (shell_command.command == "cd")
                {
                    if (shell_command.arguments.size() == 1)
                    {
                        chdir(getenv("HOME"));
                    }
                    else if (shell_command.arguments.size() == 2)
                    {
                        chdir(shell_command.arguments[1].c_str());
                    }
                    else
                    {
                        cerr << "Invalid number of arguments" << endl;
                    }
                }
                else if (shell_command.command == "pwd")
                {
                    char *current_directory = new char[1];
                    size_t capacity = 1;
                    while (!(current_directory = getcwd(current_directory, capacity)))
                    {
                        delete[] current_directory;
                        capacity *= 2;
                        current_directory = new char[capacity];
                    }
                    write(shell_command.output_fd, current_directory, strlen(current_directory));
                    write(shell_command.output_fd, "\n", 1);
                    delete[] current_directory;
                }
                else
                {
                    // If not, fork a child process and execute the command
                    int pipefddp[2];
                    pipe(pipefddp);
                    foreground_pid = fork();
                    if (foreground_pid == 0)
                    {
                        // Child process

                        // Register the signal handlers
                        struct sigaction sa_int;
                        memset(&sa_int, 0, sizeof(sa_int));
                        sa_int.sa_handler = SIG_IGN;
                        sigaction(SIGINT, &sa_int, NULL);

                        // Execute the command
                        if (shell_command.command == "delep")
                        {
                            if (shell_command.arguments.size() == 2)
                            {
                                delep((char *)shell_command.arguments[1].c_str(), pipefddp[1]);
                            }
                            else
                            {
                                cerr << "Invalid number of arguments" << endl;
                            }
                        }
                        else if (shell_command.command == "sb")
                        {
                            printf("sb\n");
                            if ((int)shell_command.arguments.size() > 3 || (int)shell_command.arguments.size() == 1)
                            {
                                cerr << "Invalid number of arguments" << endl;
                            }
                            else if ((int)shell_command.arguments.size() == 3)
                            {
                                auto itt = find(shell_command.arguments.begin(), shell_command.arguments.end(), "-suggest");
                                if (itt != shell_command.arguments.end())
                                {
                                    shell_command.arguments.erase(itt);
                                    squashbug sb(atoi(shell_command.arguments[1].c_str()), true);
                                    sb.run();
                                }
                                else
                                {
                                    cerr << "Invalid argument" << endl;
                                }
                            }
                            else if ((int)shell_command.arguments.size() == 2)
                            {
                                squashbug sb(atoi(shell_command.arguments[1].c_str()), false);
                                sb.run();
                            }
                        }
                        else if (execute_command(shell_command, is_background) < 0)
                        {
                            exit(EXIT_FAILURE);
                        }
                        exit(EXIT_SUCCESS);
                    }
                    else
                    {
                        // Parent process
                        if (is_background)
                        {
                            cout << "[" << job_number++ << "] " << foreground_pid << endl;
                            background_pids.insert(foreground_pid);
                        }
                        else
                        {
                            waitpid(foreground_pid, NULL, WUNTRACED);
                            if (shell_command.command == "delep")
                            {
                                string pids = "";
                                set<int> pids_set_nolock;
                                set<int> pids_set_lock;
                                char bufff[1024];
                                memset(bufff, 0, 1024);
                                int nread;
                                nread = read(pipefddp[0], bufff, 1024);
                                if (nread > 0)
                                {
                                    while (bufff[nread - 1] != '\0')
                                    {
                                        pids += bufff;
                                        memset(bufff, 0, 1024);
                                        nread = read(pipefddp[0], bufff, 1024);
                                        if (nread == 0)
                                        {
                                            break;
                                        }
                                        if (nread < 0)
                                        {
                                            perror("read");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                }
                                else if (nread < 0)
                                {
                                    perror("read");
                                    exit(EXIT_FAILURE);
                                }
                                pids += bufff;
                                istringstream is_line1(pids);
                                string entry;
                                while (getline(is_line1, entry, ','))
                                {
                                    istringstream is_line2(entry);
                                    string type;
                                    getline(is_line2, type, ':');
                                    if (type == "Lock")
                                    {
                                        string pidin;
                                        if (getline(is_line2, pidin))
                                            pids_set_lock.insert(atoi(pidin.c_str()));
                                    }
                                    else if (type == "NoLock")
                                    {
                                        string pidin;
                                        if (getline(is_line2, pidin))
                                            pids_set_nolock.insert(atoi(pidin.c_str()));
                                    }
                                }

                                // append two sets into one
                                set<int> pids_combined(pids_set_lock.begin(), pids_set_lock.end());
                                pids_combined.insert(pids_set_nolock.begin(), pids_set_nolock.end());

                                if ((int)pids_combined.size() == 0)
                                    printf("No process has the file open\n");
                                else
                                {
                                    // kill all the pids using the file
                                    cout << "Following PIDs have opened the given file in lock mode: " << endl;
                                    for (auto itr = pids_set_lock.begin(); itr != pids_set_lock.end(); itr++)
                                    {
                                        cout << *itr << endl;
                                    }
                                    cout << "Following PIDs have opened the given file in normal mode: " << endl;
                                    for (auto itr = pids_set_nolock.begin(); itr != pids_set_nolock.end(); itr++)
                                    {
                                        cout << *itr << endl;
                                    }
                                    printf("Are you want to kill all the processes using the file? (yes/no): ");
                                    string response;
                                    cin >> response;
                                    if (response == "yes")
                                    {
                                        for (auto it = pids_combined.begin(); it != pids_combined.end(); it++)
                                        {
                                            kill(*it, SIGKILL);
                                            printf("Killed process %d\n", *it);
                                        }
                                        int del = remove(shell_command.arguments[1].c_str());
                                        if (del == 0)
                                            printf("Deleted file %s\n", shell_command.arguments[1].c_str());
                                        else
                                            printf("Error deleting file %s\n", shell_command.arguments[1].c_str());
                                    }
                                    else
                                    {
                                        printf("Exiting...\n");
                                    }
                                }
                            }
                            foreground_pid = 0;
                        }
                    }
                }
            }
            catch (const char *msg)
            {
                cerr << msg << endl;
            }
        }
    }
    return 0;
}