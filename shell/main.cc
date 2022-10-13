#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/wait.h>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "unistd.h"
#include "parser.h"
using namespace std;

int READ = 0, WRITE = 1;
typedef struct {   
  int fds[2] = {-1, -1};
  int pid = -1;
} child_process;

void clean_mem(vector<child_process*> &children, vector<vector<string> > &commands) {
    commands.clear();
    commands.shrink_to_fit();
    for (child_process* ch : children) {
        delete ch;
    }
    children.clear();
    children.shrink_to_fit();
}

void run_child(vector<string> &tokens) {
    if (tokens[0] == "exit") {
        exit(0);
    }
    vector<char*> command_exec;
    string input_file = "", output_file = "";
    int n = tokens.size();
    for (int i = 0; i < n; i++) {
        if (tokens[i] == "<") {
            input_file = tokens[i + 1];
            i++;
        }
        else if (tokens[i] == ">") {
            output_file = tokens[i + 1];
            i++;
        }
        else {
            command_exec.push_back(strdup(tokens[i].c_str()));
        }
    }
    tokens.clear();
    tokens.shrink_to_fit();
    command_exec.push_back(nullptr);

    int input_redirect = 0;
    if (!input_file.empty()) {
        input_redirect = open(input_file.c_str(), O_RDWR | O_CLOEXEC, 0666);
    }
    int output_redirect = 0;
    if (!output_file.empty()) {
        output_redirect = open(output_file.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    }

    if (input_redirect == -1 || output_redirect == -1) {
        cerr << "ERR: file error" << endl;
        exit(1);
    }
    if (!input_file.empty()) {
        dup2(input_redirect, STDIN_FILENO);
    }
    if (!output_file.empty()) {
        dup2(output_redirect, STDOUT_FILENO);
    }
    execv(command_exec[0], command_exec.data());
    perror(command_exec[0]);
    for (char *arg : command_exec) {
        if (arg) {
            free(arg);
        }
    }
    command_exec.clear();
    command_exec.shrink_to_fit();
    exit(1);
}

void run_and_pipe_commands(vector<vector<string>> &commands) {
    int n = commands.size();
    vector<child_process*> children;
    for (int i = 0; i < n; i++) {
        child_process *cur = new child_process();
        children.push_back(cur);

        if (pipe(cur->fds) < 0) {
            cerr << "ERR: pipe failed" << endl;
            clean_mem(children, commands);
            exit(1);
        }
        // setup close on exec
        int rflags = fcntl(cur->fds[READ], F_GETFD);
        if (rflags == -1) {
            cerr << "ERR: file descriptor error" << endl;
            clean_mem(children, commands);
            exit(1);
        }
        rflags |= FD_CLOEXEC;
        if (fcntl(cur->fds[READ], F_SETFD, rflags) == -1) {
            cerr << "ERR: file descriptor error" << endl;
            clean_mem(children, commands);
            exit(1);
        }
        int wflags = fcntl(cur->fds[WRITE], F_GETFD);
        if (wflags == -1) {
            cerr << "ERR: file descriptor error" << endl;
            clean_mem(children, commands);
            exit(1);
        }
        wflags |= FD_CLOEXEC;
        if (fcntl(cur->fds[WRITE], F_SETFD, wflags) == -1) {
            cerr << "ERR: file descriptor error" << endl;
            clean_mem(children, commands);
            exit(1);
        }

        int pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(children[i - 1]->fds[READ], STDIN_FILENO);
            }
            if (i < (n - 1)) {
                dup2(children[i]->fds[WRITE], STDOUT_FILENO);
            }
            run_child(commands[i]);
        }
        else if (pid > 0) {
            children[i]->pid = pid;
            close(children[i]->fds[WRITE]);
        }
        else {
            cerr << "ERR: fork failed" << endl;
            return;
        }
    }
    for (int i = 0; i < n; i++) {
        if (children[i]->pid > 0) {
            int status = 0;
            waitpid(children[i]->pid, &status, 0);
            close(children[i]->fds[READ]);
            cout << commands[i][0] << " exit status: " << WEXITSTATUS(status) << endl;
        }
    }
    clean_mem(children, commands);
}

bool is_invalid_command(vector<string> &command) {
    int n = command.size();
    if (n == 0) {
        return true;
    }
    bool in_redir = false, out_redir = false;
    for (int i = 0; i < n; i++) {
        if (command[i] == "<") {
            if (in_redir || i == n - 1 || command[i + 1] == ">") {
                return true;
            }
            in_redir = true;
        }
        else if (command[i] == ">") {
            if (out_redir || i == n - 1 || command[i + 1] == "<") {
                return true;
            }
            out_redir = true;
        }
    }
    if (n <= ((int)in_redir + (int)out_redir) * 2) {
        return true;
    }
    return false;
}

vector<vector<string>> get_commands(vector<string> &tokens) {
    vector<vector<string>> commands;
    vector<string> single_command = vector<string>();
    int n = tokens.size();
    for (int i = 0; i < n; i++) {
        if (tokens[i] == "|") {
            if (i == n - 1 || is_invalid_command(single_command)) {
                return {};
            }
            commands.push_back(single_command);
            single_command = vector<string>();
        }
        else {
            single_command.push_back(tokens[i]);
        }
    }
    if (is_invalid_command(single_command)) {
        return {};
    }
    commands.push_back(single_command);
    return commands;
}

void parse_and_run_command(const string &full_command) { 
    vector<string> tokens = parse_from_line(full_command);
    if (tokens.empty()) {
        return;
    }
    if (tokens[0] == "exit") {
        tokens.clear();
        tokens.shrink_to_fit();
        exit(0);
    }
    vector<vector<string>> commands = get_commands(tokens);
    if (commands.empty()) {
        cerr << "ERR: invalid command" << endl;
        cout << full_command << " exit status: 255" << endl;
    }
    else {
        run_and_pipe_commands(commands);
    }
    commands.clear();
    commands.shrink_to_fit();
}

int main(void) {
    string command;
    cout << "> ";
    while (getline(cin, command)) {
        parse_and_run_command(command);
        cout << "> ";
    }
    return 0;
}
