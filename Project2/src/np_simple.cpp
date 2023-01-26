#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_USER_ID 30
#define DEFAULT_USER_NAME "(no name)"

using namespace std;

const char kWelcome_Message[] =
    "****************************************\n"
    "** Welcome to the information server. **\n"
    "****************************************\n";
const char kBash[] = "% ";

int null_fd;

set<int> ids_available;
map<int, int> id_fd;
unordered_map<int, int> fd_id;
unordered_map<int, string> id_name;
set<string> names;
unordered_map<int, unordered_map<int, array<int, 2>>> id_pipeMap; // 0: read, 1: write
unordered_map<int, unordered_map<string, string>> id_env;
map<pair<int, int>, int> user_pipe; // <receiver, sender> as key

static string TcpIpPort(int tcp_fd) {
  sockaddr_in src_addr;
  socklen_t src_len = sizeof(src_addr);
  getpeername(tcp_fd, (sockaddr *) &src_addr, &src_len); //tcp

  stringstream ss;
  ss << inet_ntoa(src_addr.sin_addr) << ":" << ntohs(src_addr.sin_port);
  return ss.str();
}

struct Command {
  int sender_id;
  vector<string> args;
  array<int, 2> pipe = {STDIN_FILENO, STDOUT_FILENO}; // 0: fd_in, 1: fd write to fd_in
  int fd_out = STDOUT_FILENO;
  int fd_err = STDERR_FILENO;
};

void ExecuteCommand(const Command &command) {
  if (command.args[0] == "exit") {
    ids_available.emplace(command.sender_id);
    fd_id.erase(id_fd[command.sender_id]);
    id_fd.erase(command.sender_id);

    // broadcast message after id is erased, before name is erased
    for (const auto &[key, value] : id_fd) {
      dprintf(value, "*** User \'%s\' left. ***\n", id_name[command.sender_id].c_str());
    }

    names.erase(id_name[command.sender_id]);
    id_name.erase(command.sender_id);
    // close pipes in pipeMap
    for (const auto &[key, value] : id_pipeMap[command.sender_id]) {
      close(value[0]);
      close(value[1]);
    }
    id_pipeMap.erase(command.sender_id);
    id_env.erase(command.sender_id);

    // close and erase pipes in user_pipe
    for (auto it = user_pipe.cbegin(); it != user_pipe.cend(); it++) {
      auto[key, value] = *it;
      auto[receiver, sender] = key;

      if (receiver == command.sender_id || sender == command.sender_id) {
        close(value);
        user_pipe.erase(it--);
      }
    }
    return;
  }

  if (command.args[0] == "setenv") {
    id_env[command.sender_id][command.args[1]] = command.args[2];
    return;
  }

  if (command.args[0] == "printenv") {
    if (id_env[command.sender_id].count(command.args[1])) {
      dprintf(command.fd_out, "%s\n", id_env[command.sender_id][command.args[1]].c_str());
    }
    return;
  }

  if (command.args[0] == "yell") {
    for (const auto &[key, value] : fd_id) {
      dprintf(key, "*** %s yelled ***: %s\n", id_name[command.sender_id].c_str(), command.args[1].c_str());
    }
    return;
  }

  if (command.args[0] == "tell") {
    int recv_id = stoi(command.args[1]);
    if (id_fd.count(recv_id) == 1) {
      dprintf(id_fd[recv_id], "*** %s told you ***: %s\n", id_name[command.sender_id].c_str(), command.args[2].c_str());
    } else {
      dprintf(command.fd_out, "*** Error: user #%d does not exist yet. ***\n", recv_id);
    }
    return;
  }

  if (command.args[0] == "who") {
    stringstream output;
    output << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for (const auto &[key, value] : id_fd) {
      output << key << '\t' << id_name[key] << '\t' << TcpIpPort(value);
      if (key == command.sender_id) {
        output << "\t<-me";
      }
      output << '\n';
    }
    dprintf(command.fd_out, output.str().c_str(), output.str().size(), 0);
    return;
  }

  if (command.args[0] == "name") {
    if (names.count(command.args[1])) { // new_name already exists
      dprintf(command.fd_out, "*** User \'%s\' already exists. ***\n", command.args[1].c_str());
    } else {
      // remove old name from names
      names.erase(id_name[command.sender_id]);
      names.emplace(command.args[1]);
      id_name[command.sender_id] = command.args[1];
      for (const auto &[key, value] : fd_id) {
        dprintf(key,
                "*** User from %s is named \'%s\'. ***\n",
                TcpIpPort(id_fd[command.sender_id]).c_str(),
                command.args[1].c_str());
      }
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
    if (command.pipe[0] != id_fd[command.sender_id] && command.pipe[0] != null_fd) {
      close(command.pipe[0]);

    }
    if (command.pipe[1] != id_fd[command.sender_id] && command.pipe[0] != null_fd) {
      close(command.pipe[1]);
    }

    struct stat fd_stat;
    fstat(command.fd_out, &fd_stat);
    // close file if fd_out isn't client_fd and is file or FIFO write
    if (command.fd_out != id_fd[command.sender_id]) {
      if (S_ISREG(fd_stat.st_mode)) {
        close(command.fd_out);
      }
      if (S_ISFIFO(fd_stat.st_mode) && fcntl(command.fd_out, F_GETFD) == 0) {
        // FIFO write doesn't have FD_CLOEXEC since it's closed right away!
        close(command.fd_out);
      }
    }
    // wait for child when fd_out is client_fd or file
    if (command.fd_out == id_fd[command.sender_id] || S_ISREG(fd_stat.st_mode)) {
      waitpid(child, nullptr, 0);
    }
    return;
  }

  // child process
  dup2(command.pipe[0], STDIN_FILENO);
  dup2(command.fd_out, STDOUT_FILENO);
  dup2(command.fd_err, STDERR_FILENO);

  // setup env
  for (const auto &[key, value] : id_env[command.sender_id]) {
    setenv(key.c_str(), value.c_str(), 1);
  }

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

void UpdatePipeMap(unordered_map<int, array<int, 2>> &pipeMap) {
  unordered_map<int, array<int, 2>> new_map;
  for (const auto[key, value] : pipeMap) {
    new_map.emplace(key - 1, value); // reduce pipeNum
  }

  pipeMap = move(new_map);
}

bool Shell(int fd) {
  const int user_id = fd_id[fd];

  string line;
  char recv_buf[1];
  while (recv(fd, recv_buf, 1, 0)) {
    if (recv_buf[0] == '\r') {
      continue;
    }
    if (recv_buf[0] == '\n') {
      break;
    }
    line.append(recv_buf, 1);
  }

  if (line.empty()) {
    dprintf(fd, "%s", kBash);
    return false;
  }

  stringstream ss(line);
  string arg;
  vector<string> command_args;
  while (getline(ss, arg, ' ')) {
    if (arg[0] == '|' || arg[0] == '!' || arg[0] == '>' || arg[0] == '<') {
      Command command{.sender_id = user_id, .args = move(command_args), .pipe = {fd, fd}, .fd_out = fd, .fd_err = fd};
      command_args.clear();

      if (id_pipeMap[user_id].count(0)) { // pipe fd_in
        command.pipe = id_pipeMap[user_id][0];
        id_pipeMap[user_id].erase(0);
      }

      if (arg[0] == '<') { // pipe from user_pipe
        command.pipe[0] = null_fd;
        int sender_id = stoi(arg.substr(1));
        if (id_fd.count(sender_id) == 0) { // sender doesn't exist
          dprintf(id_fd[user_id], "*** Error: user #%d does not exist yet. ***\n", sender_id);
        } else {
          if (user_pipe.count(make_pair(user_id, sender_id)) == 0) { // user pipe doesn't exist
            dprintf(id_fd[user_id],
                    "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
                    sender_id,
                    user_id);
          } else {
            command.pipe[0] = user_pipe[make_pair(user_id, sender_id)];
            user_pipe.erase(make_pair(user_id, sender_id));

            for (const auto &[key, value] : fd_id) {
              dprintf(key,
                      "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
                      id_name[user_id].c_str(),
                      user_id,
                      id_name[sender_id].c_str(),
                      sender_id,
                      ss.str().c_str());
            }
          }
        }
        if (!getline(ss, arg, ' ')) { // get next arg for output
          arg.clear();
        }
      }

      string user_pipe_out_msg;
      if (arg[0] == '>') {
        if (arg.size() == 1) { // pipe to file
          string filename;
          getline(ss, filename, ' ');
          command.fd_out = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0664);
        } else { // pipe to user_pipe
          command.fd_out = null_fd;
          int receiver_id = stoi(arg.substr(1));
          if (id_fd.count(receiver_id) == 0) { // receiver doesn't exist
            user_pipe_out_msg = " *** Error: user #" + arg.substr(1) + " does not exist yet. ***\n";
          } else {
            if (user_pipe.count(make_pair(receiver_id, user_id)) == 1) { // user pipe already exist
              user_pipe_out_msg =
                  " *** Error: the pipe #" + to_string(user_id) + "->#" + arg.substr(1) + " already exists. ***\n";
            } else {
              array<int, 2> pipe_fd;
              while (pipe(pipe_fd.data()) == -1) {
                if (errno == EMFILE || errno == ENFILE) {
                  wait(nullptr); // wait for any child process to release resource
                }
              }
              fcntl(pipe_fd[0], F_SETFD, FD_CLOEXEC);

              command.fd_out = pipe_fd[1];
              user_pipe[make_pair(receiver_id, user_id)] = pipe_fd[0];
              user_pipe_out_msg =
                  "*** " + id_name[user_id] + " (#" + to_string(user_id) + ") just piped \'" + ss.str() + "\' to "
                      + id_name[stoi(arg.substr(1))] + " (#" + arg.substr(1) + ") ***\n";
            }
          }
        }
      } else { // command with piping
        int pipeNum = 0; // "|" piping without updating pipeMap
        if (arg.size() > 1) {
          pipeNum = stoi(arg.substr(1));
        }

        if (id_pipeMap[user_id].count(pipeNum) == 0) { //create pipe
          array<int, 2> pipe_fd;
          while (pipe2(pipe_fd.data(), O_CLOEXEC) == -1) {
            if (errno == EMFILE || errno == ENFILE) {
              wait(nullptr); // wait for any child process to release resource
            }
          }
          id_pipeMap[user_id].emplace(pipeNum, pipe_fd);
        }

        if (arg[0] == '|') { // pipe fd_out
          command.fd_out = id_pipeMap[user_id][pipeNum][1];
        }

        if (arg[0] == '!') { // pipe fd_out, fd_err
          command.fd_out = id_pipeMap[user_id][pipeNum][1];
          command.fd_err = id_pipeMap[user_id][pipeNum][1];
        }
      }

      if (arg != "|") { // "|" piping without updating pipeMap
        UpdatePipeMap(id_pipeMap[user_id]);
      }

      // pipe from user_pipe if possible
      streampos position = ss.tellg();
      if (getline(ss, arg, ' ') && arg[0] == '<') {
        command.pipe[0] = null_fd;
        int sender_id = stoi(arg.substr(1));
        if (id_fd.count(sender_id) == 0) { // sender doesn't exist
          dprintf(id_fd[user_id], "*** Error: user #%d does not exist yet. ***\n", sender_id);
        } else {
          if (user_pipe.count(make_pair(user_id, sender_id)) == 0) { // user pipe doesn't exist
            dprintf(id_fd[user_id],
                    "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
                    sender_id,
                    user_id);
          } else {
            command.pipe[0] = user_pipe[make_pair(user_id, sender_id)];
            user_pipe.erase(make_pair(user_id, sender_id));

            for (const auto &[key, value] : fd_id) {
              dprintf(key,
                      "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
                      id_name[user_id].c_str(),
                      user_id,
                      id_name[sender_id].c_str(),
                      sender_id,
                      ss.str().c_str());
            }
          }
        }
      } else {
        ss.seekg(position);
      }

      // send user pipe out msg
      if (!user_pipe_out_msg.empty()) {
        if (user_pipe_out_msg[user_pipe_out_msg.size() - 6] == ')') { // if user pipe out success
          for (const auto &[key, value] : fd_id) {
            dprintf(key, "%s", user_pipe_out_msg.c_str());
          }
        } else {
          dprintf(id_fd[user_id], "%s", user_pipe_out_msg.c_str());
        }
      }

      ExecuteCommand(command);
    } else {
      command_args.emplace_back(arg);
    }

    // special commands
    if (arg == "yell" || arg == "tell") {
      if (arg == "yell") {
        // take everything as second argument including space
        getline(ss, arg);
        command_args.emplace_back(arg);
      }

      if (arg == "tell") {
        // take receiver id as second argument
        getline(ss, arg, ' ');
        command_args.emplace_back(arg);
        // take everything as third argument including space
        getline(ss, arg);
        command_args.emplace_back(arg);
      }
      break;
    }
  }

  if (!command_args.empty()) { // parse last command
    Command command{.sender_id = user_id, .args = move(command_args), .pipe = {fd, fd}, .fd_out = fd, .fd_err = fd};

    if (id_pipeMap[user_id].count(0)) { // pipe fd_in
      command.pipe = id_pipeMap[user_id][0];
      id_pipeMap[user_id].erase(0);
    }
    UpdatePipeMap(id_pipeMap[user_id]);
    ExecuteCommand(command);
  }

  if (line == "exit") {
    return true;
  } else {
    dprintf(fd, "%s", kBash);
    return false;
  }
}

class Server {
 private:
  int tcp_fd_;
  sockaddr_in tcp_addr_;

  bool listening_ = false;
  fd_set read_fds_;
  int max_fd_num;

  // return true to disconnect
  bool (*service_function_)(int fd);

  void handleConnection() {
    sockaddr src_addr;
    socklen_t src_len = sizeof(src_addr);

    int client_fd = accept4(tcp_fd_, &src_addr, &src_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    string ip_port = TcpIpPort(client_fd);
    cout << "[Server] Client " << ip_port << " connected using TCP" << '\n';

    FD_SET(client_fd, &read_fds_);
    max_fd_num = max(max_fd_num, client_fd);

    int user_id = *ids_available.cbegin();
    ids_available.erase(user_id);
    id_fd[user_id] = client_fd;
    fd_id[client_fd] = user_id;
    id_name[user_id] = DEFAULT_USER_NAME;
    id_env[user_id]["PATH"] = "bin:.";

    /* np_single_proc
    dprintf(client_fd, "%s", kWelcome_Message);

    for (const auto &[key, value] : id_fd) {
      dprintf(value, "*** User \'%s\' entered from %s. ***\n", DEFAULT_USER_NAME, TcpIpPort(client_fd).c_str());
    }
    */

    dprintf(client_fd, "%s", kBash);
  }

 public:
  Server(uint16_t port, bool(*service)(int)) {
    tcp_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    tcp_addr_.sin_family = AF_INET;
    tcp_addr_.sin_addr.s_addr = INADDR_ANY;
    tcp_addr_.sin_port = htons(port);

    int enable = 1;
    setsockopt(tcp_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    bind(tcp_fd_, (sockaddr *) &tcp_addr_, sizeof(tcp_addr_));
    cout << "[Server] Hosting on " << inet_ntoa(tcp_addr_.sin_addr) << ", port " << ntohs(tcp_addr_.sin_port) << '\n';

    FD_ZERO(&read_fds_);
    FD_SET(tcp_fd_, &read_fds_);
    max_fd_num = tcp_fd_;

    service_function_ = service;
  }
  ~Server() {
    shutdown(tcp_fd_, SHUT_RDWR);
    close(tcp_fd_);
    cout << "[Server] Shutdown\n";
  }

  void listen_for_message() {
    listening_ = true;
    listen(tcp_fd_, 1);

    while (listening_) {
      fd_set select_fd_set = read_fds_;
      select(max_fd_num + 1, &select_fd_set, nullptr, nullptr, nullptr);

      for (int fd = 0; fd <= max_fd_num; fd++) {
        if (FD_ISSET(fd, &select_fd_set)) {
          if (fd == tcp_fd_) {
            handleConnection();
          } else {
            if (service_function_(fd)) {
              cout << "[Server] Exiting service for client " << TcpIpPort(fd) << '\n';
              shutdown(fd, SHUT_RDWR);
              close(fd);
              FD_CLR(fd, &read_fds_);
            }
          }
        }
      }

    }
  }
};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, SIG_IGN);
  setenv("PATH", "bin:.", 1);
  null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);

  // fill available user ids
  for (int i = 1; i <= MAX_USER_ID; i++) {
    ids_available.emplace(i);
  }

  if (argc == 2) {
    Server server(stol(argv[1]), Shell);
    server.listen_for_message();
  }

  return 0;
}