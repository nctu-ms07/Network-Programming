#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

struct Command {
  vector<string> args;
  array<int, 2> pipe = {STDIN_FILENO, STDOUT_FILENO}; // 0: fd_in, 1: fd write to fd_in
  int fd_out = STDOUT_FILENO;
  int fd_err = STDERR_FILENO;
};

void executeCommand(const Command &command) {
  if (command.args[0] == "exit") {
    exit(0);
  }

  if (command.args[0] == "setenv") {
    setenv(command.args[1].c_str(), command.args[2].c_str(), 1);
    return;
  }

  if (command.args[0] == "printenv") {
    if (const char *env = getenv(command.args[1].c_str())) {
      cout << env << '\n';
    }
    return;
  }

  pid_t child;
  while ((child = fork()) == -1) {
    if (errno == EAGAIN) {
      wait(nullptr); // wait for any child process to release resource
    }
  }

  if (child != 0) { // parent process
    // close pipe
    if (command.pipe[0] != STDIN_FILENO) {
      close(command.pipe[0]);
      close(command.pipe[1]);
    }
    struct stat fd_stat;
    fstat(command.fd_out, &fd_stat);
    // close file if fd_out isn't STDOUT_FILENO
    if (command.fd_out != STDOUT_FILENO && S_ISREG(fd_stat.st_mode)) {
      close(command.fd_out);
    }
    // wait for child when fd_out isn't pipe
    if (!S_ISFIFO(fd_stat.st_mode)) {
      waitpid(child, nullptr, 0);
    }
    return;
  }

  // child process
  dup2(command.pipe[0], STDIN_FILENO);
  dup2(command.fd_out, STDOUT_FILENO);
  dup2(command.fd_err, STDERR_FILENO);

  auto args = make_unique<char *[]>(command.args.size() + 1);
  for (size_t i = 0; i < command.args.size(); i++) {
    args[i] = strdup(command.args[i].c_str());
  }
  args[command.args.size()] = nullptr;

  if (execvp(args[0], args.get()) == -1 && errno == ENOENT) {
    cerr << "Unknown command: [" << args[0] << "].\n";
    exit(0);
  }
}

void update_pipeMap(unordered_map<int, array<int, 2>> &pipeMap) {
  unordered_map<int, array<int, 2>> new_map;
  for (const auto[key, value] : pipeMap) {
    new_map.emplace(key - 1, value); // reduce pipeNum
  }

  pipeMap = move(new_map);
}

int main() {
  signal(SIGCHLD, SIG_IGN);
  setenv("PATH", "bin:.", 1);

  unordered_map<int, array<int, 2>> pipeMap; // 0: read, 1: write
  while (true) {
    cout << "% ";

    string line;
    getline(cin, line);
    if (line.empty()) {
      continue;
    }

    stringstream ss(line);
    string arg;
    vector<string> command_args;
    while (getline(ss, arg, ' ')) {
      if (arg[0] == '|' || arg[0] == '!' || arg[0] == '>') {
        Command command;
        command.args = move(command_args);
        command_args.clear();

        if (pipeMap.count(0)) { // pipe fd_in
          command.pipe = pipeMap[0];
          pipeMap.erase(0);
        }

        if (arg[0] == '>') {
          string filename;
          getline(ss, filename, ' ');
          command.fd_out = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0664);
        } else { // command with piping
          int pipeNum = 0; // "|" piping without updating pipeMap
          if (arg.size() > 1) {
            pipeNum = stoi(arg.substr(1));
          }

          if (pipeMap.count(pipeNum) == 0) { //create pipe
            array<int, 2> pipe_fd;
            while (pipe(pipe_fd.data()) == -1) {
              if (errno == EMFILE || errno == ENFILE) {
                wait(nullptr); // wait for any child process to release resource
              }
            }
            fcntl(pipe_fd[0], F_SETFD, FD_CLOEXEC);
            fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);
            pipeMap.emplace(pipeNum, pipe_fd);
          }

          if (arg[0] == '|') { // pipe fd_out
            command.fd_out = pipeMap[pipeNum][1];
          }

          if (arg[0] == '!') { // pipe fd_out, fd_err
            command.fd_out = pipeMap[pipeNum][1];
            command.fd_err = pipeMap[pipeNum][1];
          }
        }
        executeCommand(command);
        if (arg != "|") { // "|" piping without updating pipeMap
          update_pipeMap(pipeMap);
        }
      } else {
        command_args.emplace_back(arg);
      }
    }

    if (!command_args.empty()) { // parse last command
      Command command;
      command.args = move(command_args);

      if (pipeMap.count(0)) { // pipe fd_in
        command.pipe = pipeMap[0];
        pipeMap.erase(0);
      }
      executeCommand(command);
      update_pipeMap(pipeMap);
    }
  }

  return 0;
}