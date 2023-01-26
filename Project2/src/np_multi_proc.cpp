#include <bits/stdc++.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
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

// shared-pipe for client-to-server communication
array<int, 2> shared_pipe;
sem_t *read_lock;
sem_t *write_lock;

// pipes for server-to-client communication
map<int, int> id_pipe;

set<int> ids_available;
map<int, string> id_tcpinfo;
unordered_map<int, string> id_name;
set<string> names;

map<int, int> user_pipe_fds;

static string TcpIpPort(int tcp_fd) {
  sockaddr_in src_addr;
  socklen_t src_len = sizeof(src_addr);
  getpeername(tcp_fd, (sockaddr *) &src_addr, &src_len); //tcp

  stringstream ss;
  ss << inet_ntoa(src_addr.sin_addr) << ":" << ntohs(src_addr.sin_port);
  return ss.str();
}

void HandleInternalMsg(int fd_in, int user_id) {
  int sender_id;
  size_t msg_len;

  // received msg from pipe
  read(fd_in, &sender_id, sizeof(sender_id));
  read(fd_in, &msg_len, sizeof(msg_len));
  char msg_buf[msg_len + 1];
  msg_buf[msg_len] = '\0';
  read(fd_in, msg_buf, msg_len);
  string msg(msg_buf, msg_len);

  // parse message
  stringstream ss(msg);
  string arg;
  // first argument
  getline(ss, arg, ' ');

  if (arg == "login") {
    ids_available.erase(sender_id);
    getline(ss, arg, ' '); // tcpinfo
    id_tcpinfo[sender_id] = arg;
    id_name[sender_id] = DEFAULT_USER_NAME;
    getline(ss, arg); // msg
    printf("%s\n", arg.c_str()); // send message to client
  }

  if (arg == "exit") {
    // server clean up
    if (user_id == -1) {
      close(id_pipe[sender_id]);
      id_pipe.erase(sender_id);
    }

    if (user_id == sender_id) {
      // delete user_pipe dir
      string dir = "./user_pipe/" + to_string(user_id) + '/';
      for (const auto &[key, value] : id_tcpinfo) {
        string file = dir + to_string(key);
        unlinkat(AT_FDCWD, file.c_str(), 0);
      }
      unlinkat(AT_FDCWD, dir.c_str(), AT_REMOVEDIR);
      exit(0);
    }

    // delete user_pipe files
    string file = "./user_pipe/" + to_string(user_id) + '/' + to_string(sender_id);
    unlinkat(AT_FDCWD, file.c_str(), 0);
    if (user_pipe_fds.count(sender_id)) {
      close(user_pipe_fds[sender_id]);
    }
    user_pipe_fds.erase(sender_id);

    ids_available.emplace(sender_id);
    id_tcpinfo.erase(sender_id);
    names.erase(id_name[sender_id]);
    id_name.erase(sender_id);
    getline(ss, arg); // msg
    printf("%s\n", arg.c_str()); // send message to client
  }

  if (arg == "yell") {
    getline(ss, arg); // msg
    printf("%s\n", arg.c_str()); // send message to client
  }

  if (arg == "tell") {
    getline(ss, arg, ' '); // receiver_id
    if (user_id == stoi(arg)) { // we are the receiver
      getline(ss, arg); // msg
      printf("%s\n", arg.c_str()); // send message to client
    }
  }

  if (arg == "name") {
    getline(ss, arg, ' '); // new_name
    // remove old name from names
    names.erase(id_name[sender_id]);
    names.emplace(arg);
    id_name[sender_id] = arg;
    getline(ss, arg); // msg
    printf("%s\n", arg.c_str()); // send message to client
  }

  if (arg == ">") {
    getline(ss, arg, ' '); // receiver_id
    int receiver_id = stoi(arg);
    if (user_id == receiver_id && sender_id != user_id) { // if it's piped to us
      string file = "./user_pipe/" + to_string(user_id) + '/' + to_string(sender_id);
      user_pipe_fds[sender_id] = openat(AT_FDCWD, file.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
    // avoid printing kBash
    return;
  }

  if (arg == "user_pipe") {
    if (sender_id == user_id) {
      // avoid printing msg and kBash
      return;
    }
    while (getline(ss, arg)) { // msg (maybe more than one)
      printf("%s\n", arg.c_str()); // send message to client
    }
  }

  if (sender_id == user_id) {
    printf("%s", kBash);
    fflush(stdout);
  }
}

struct Command {
  vector<string> args;
  array<int, 2> pipe = {STDIN_FILENO, STDOUT_FILENO}; // 0: fd_in, 1: fd write to fd_in
  int fd_out = STDOUT_FILENO;
  int fd_err = STDERR_FILENO;
};

bool ExecuteCommand(const int &user_id, const Command &command) {
  if (command.args[0] == "exit") {
    sem_wait(write_lock);
    write(shared_pipe[1], &user_id, sizeof(user_id));
    string msg = "exit *** User \'" + id_name[user_id] + "\' left. ***";
    size_t size = msg.size();
    write(shared_pipe[1], &size, sizeof(size));
    write(shared_pipe[1], msg.c_str(), size);
    sem_post(read_lock);
    return false;
  }

  if (command.args[0] == "setenv") {
    setenv(command.args[1].c_str(), command.args[2].c_str(), 1);
    return true;
  }

  if (command.args[0] == "printenv") {
    if (const char *env = getenv(command.args[1].c_str())) {
      printf("%s\n", env);
    }
    return true;
  }

  if (command.args[0] == "yell") {
    sem_wait(write_lock);
    write(shared_pipe[1], &user_id, sizeof(user_id));
    string msg = "yell *** " + id_name[user_id] + " yelled ***: " + command.args[1];
    size_t size = msg.size();
    write(shared_pipe[1], &size, sizeof(size));
    write(shared_pipe[1], msg.c_str(), size);
    sem_post(read_lock);
    return false;
  }

  if (command.args[0] == "tell") {
    int receiver_id = stoi(command.args[1]);
    if (ids_available.count(receiver_id)) { // receiver doesn't exist
      printf("*** Error: user #%d does not exist yet. ***\n", receiver_id);
      return true;
    }

    sem_wait(write_lock);
    write(shared_pipe[1], &user_id, sizeof(user_id));
    string msg = "tell " + command.args[1] + " *** " + id_name[user_id] + " told you ***: " + command.args[2];
    size_t size = msg.size();
    write(shared_pipe[1], &size, sizeof(size));
    write(shared_pipe[1], msg.c_str(), size);
    sem_post(read_lock);
    return false;
  }

  if (command.args[0] == "who") {
    stringstream output;
    output << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for (const auto &[key, value] : id_tcpinfo) {
      output << key << '\t' << id_name[key] << '\t' << value;
      if (key == user_id) {
        output << "\t<-me";
      }
      output << '\n';
    }
    printf("%s", output.str().c_str());
    return true;
  }

  if (command.args[0] == "name") {
    if (names.count(command.args[1])) { // new_name already exists
      printf("*** User \'%s\' already exists. ***\n", command.args[1].c_str());
      return true;
    }

    sem_wait(write_lock);
    write(shared_pipe[1], &user_id, sizeof(user_id));
    string msg = "name " + command.args[1] + " *** User from " + id_tcpinfo[user_id] + " is named \'" + command.args[1]
        + "\'. ***";
    size_t size = msg.size();
    write(shared_pipe[1], &size, sizeof(size));
    write(shared_pipe[1], msg.c_str(), size);
    sem_post(read_lock);
    return false;
  }

  pid_t child;
  while ((child = fork()) == -1) {
    if (errno == EAGAIN) {
      wait(nullptr); // wait for any child process to release resource
    }
  }

  if (child != 0) { // parent process
    // close pipe
    if (command.pipe[0] != STDIN_FILENO && command.pipe[0] != null_fd) {
      close(command.pipe[0]);

    }
    if (command.pipe[1] != STDOUT_FILENO && command.pipe[1] != null_fd) {
      close(command.pipe[1]);
    }

    struct stat fd_stat;
    fstat(command.fd_out, &fd_stat);
    // close file if fd_out isn't STDOUT_FILENO or FIFO read
    if (command.fd_out != STDOUT_FILENO) {
      if (S_ISREG(fd_stat.st_mode)) {
        close(command.fd_out);
      }

      if (S_ISFIFO(fd_stat.st_mode) && fcntl(command.fd_out, F_GETFD) == 0) {
        // FIFO write doesn't have FD_CLOEXEC since it's closed right away!
        close(command.fd_out);
      }
    }
    // wait for child when fd_out is STDOUT_FILENO or file
    if (command.fd_out == STDOUT_FILENO || S_ISREG(fd_stat.st_mode)) {
      waitpid(child, nullptr, 0);
    }
    return true;
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

void UpdatePipeMap(unordered_map<int, array<int, 2>> &pipeMap) {
  unordered_map<int, array<int, 2>> new_map;
  for (const auto[key, value] : pipeMap) {
    new_map.emplace(key - 1, value); // reduce pipeNum
  }

  pipeMap = move(new_map);
}

void Shell(int user_id, unordered_map<int, array<int, 2>> &pipeMap) {
  string line;
  getline(cin, line);

  if (line.back() == '\r') {
    line.pop_back();
  }

  if (line.empty()) {
    printf("%s", kBash);
    fflush(stdout);
    return;
  }

  bool printBash;
  stringstream ss(line);
  string arg;
  vector<string> command_args;
  while (getline(ss, arg, ' ')) {
    if (arg[0] == '|' || arg[0] == '!' || arg[0] == '>' || arg[0] == '<') {
      Command command{.args = move(command_args)};
      command_args.clear();

      if (pipeMap.count(0)) { // pipe fd_in
        command.pipe = pipeMap[0];
        pipeMap.erase(0);
      }

      string user_pipe_msg;
      if (arg[0] == '<') { // pipe from FIFO
        command.pipe[0] = null_fd;
        int sender_id = stoi(arg.substr(1));
        if (ids_available.count(sender_id)) { // sender doesn't exist
          printf("*** Error: user #%d does not exist yet. ***\n", sender_id);
        } else {
          if (user_pipe_fds.count(sender_id) == 0) { // user pipe doesn't exist
            printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", sender_id, user_id);
          } else {
            command.pipe[0] = user_pipe_fds[sender_id];
            user_pipe_fds.erase(sender_id);
            string file = "./user_pipe/" + to_string(user_id) + '/' + arg.substr(1);
            unlinkat(AT_FDCWD, file.c_str(), 0);

            user_pipe_msg =
                "*** " + id_name[user_id] + " (#" + to_string(user_id) + ") just received from " + id_name[sender_id]
                    + " (#" + arg.substr(1) + ") by \'" + ss.str() + "\' ***\n";
          }
        }
        if (!getline(ss, arg, ' ')) { // get next arg for output
          arg.clear();
        }
      }

      string user_pipe_err;
      if (arg[0] == '>') {
        if (arg.size() == 1) { // pipe to file
          string filename;
          getline(ss, filename, ' ');
          command.fd_out = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0664);
        } else { // pipe to FIFO
          command.fd_out = null_fd;
          int receiver_id = stoi(arg.substr(1));
          if (ids_available.count(receiver_id)) { // receiver doesn't exist
            user_pipe_err = "*** Error: user #" + arg.substr(1) + " does not exist yet. ***\n";
          } else {
            string file = "./user_pipe/" + arg.substr(1) + '/' + to_string(user_id);
            if (faccessat(AT_FDCWD, file.c_str(), W_OK, 0) == 0) { // user pipe already exists
              user_pipe_err =
                  "*** Error: the pipe #" + to_string(user_id) + "->#" + arg.substr(1) + " already exists. ***\n";
            } else {
              mkfifoat(AT_FDCWD, file.c_str(), 0660);

              if (receiver_id == user_id) {
                user_pipe_fds[user_id] = openat(AT_FDCWD, file.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
              } else {
                sem_wait(write_lock);
                write(shared_pipe[1], &user_id, sizeof(user_id));
                string msg = "> " + arg.substr(1);
                size_t size = msg.size();
                write(shared_pipe[1], &size, sizeof(size));
                write(shared_pipe[1], msg.c_str(), size);
                sem_post(read_lock);
              }
              command.fd_out = openat(AT_FDCWD, file.c_str(), O_WRONLY);
              string msg =
                  "*** " + id_name[user_id] + " (#" + to_string(user_id) + ") just piped \'" + ss.str() + "\' to "
                      + id_name[receiver_id] + " (#" + arg.substr(1) + ") ***\n";
              user_pipe_msg.append(msg);
            }
          }
        }
      } else { // command with piping
        int pipeNum = 0; // "|" piping without updating pipeMap
        if (arg.size() > 1) {
          pipeNum = stoi(arg.substr(1));
        }

        if (pipeMap.count(pipeNum) == 0) { //create pipe
          array<int, 2> pipe_fd;
          while (pipe2(pipe_fd.data(), O_CLOEXEC) == -1) {
            if (errno == EMFILE || errno == ENFILE) {
              wait(nullptr); // wait for any child process to release resource
            }
          }
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

      if (arg != "|") { // "|" piping without updating pipeMap
        UpdatePipeMap(pipeMap);
      }

      // pipe from FIFO if possible
      streampos position = ss.tellg();
      if (getline(ss, arg, ' ') && arg[0] == '<') {
        command.pipe[0] = null_fd;
        int sender_id = stoi(arg.substr(1));
        if (ids_available.count(sender_id)) { // sender doesn't exist
          printf("*** Error: user #%d does not exist yet. ***\n", sender_id);
        } else {
          if (user_pipe_fds.count(sender_id) == 0) { // user pipe doesn't exist
            printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n", sender_id, user_id);
          } else {
            command.pipe[0] = user_pipe_fds[sender_id];
            user_pipe_fds.erase(sender_id);
            string file = "./user_pipe/" + to_string(user_id) + '/' + arg.substr(1);
            unlinkat(AT_FDCWD, file.c_str(), 0);

            string msg =
                "*** " + id_name[user_id] + " (#" + to_string(user_id) + ") just received from " + id_name[sender_id]
                    + " (#" + arg.substr(1) + ") by \'" + ss.str() + "\' ***\n";
            user_pipe_msg.insert(0, msg);
          }
        }
      } else {
        ss.seekg(position);
      }

      if (!user_pipe_msg.empty()) {
        printf("%s", user_pipe_msg.c_str());
        user_pipe_msg.insert(0, "user_pipe "s);
        sem_wait(write_lock);
        write(shared_pipe[1], &user_id, sizeof(user_id));
        size_t size = user_pipe_msg.size();
        write(shared_pipe[1], &size, sizeof(size));
        write(shared_pipe[1], user_pipe_msg.c_str(), size);
        sem_post(read_lock);
      }

      if (!user_pipe_err.empty()) {
        printf("%s", user_pipe_err.c_str());
      }

      printBash = ExecuteCommand(user_id, command);
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
    Command command{.args = move(command_args)};

    if (pipeMap.count(0)) { // pipe fd_in
      command.pipe = pipeMap[0];
      pipeMap.erase(0);
    }
    UpdatePipeMap(pipeMap);
    printBash = ExecuteCommand(user_id, command);
  }

  if (printBash) {
    printf("%s", kBash);
    fflush(stdout);
  }
}

class Server {
 private:
  int tcp_fd_;
  sockaddr_in tcp_addr_;

  bool listening_ = false;
  array<pollfd, 2> fds_;

  void (*service_function_)(int user_id, unordered_map<int, array<int, 2>> &pipeMap);

  void handleConnection() {
    sockaddr src_addr;
    socklen_t src_len = sizeof(src_addr);

    int client_fd = accept(tcp_fd_, &src_addr, &src_len);
    string ip_port = TcpIpPort(client_fd);

    // server assign an user_id for the client
    const int user_id = *ids_available.cbegin();
    ids_available.erase(user_id);

    // create pipe for server-client communication
    array<int, 2> pipe_fd;
    while (pipe2(pipe_fd.data(), O_CLOEXEC) == -1) {
      if (errno == EMFILE || errno == ENFILE) {
        wait(nullptr); // wait for any child process to release resource
      }
    }
    id_pipe.emplace(user_id, pipe_fd[1]);

    pid_t child;
    while ((child = fork()) == -1) {
      if (errno == EAGAIN) {
        wait(nullptr); // wait for any child process to release resource
      }
    }

    if (child != 0) { // parent process
      close(client_fd);
      close(pipe_fd[0]);
      return;
    }

    // child process
    close(tcp_fd_);
    close(shared_pipe[0]);
    for (const auto[key, value] : id_pipe) {
      close(value);
    }
    id_pipe.clear();

    // create user_pipe dir
    string dir = "./user_pipe/" + to_string(user_id) + '/';
    mkdirat(AT_FDCWD, dir.c_str(), 0770);

    dup2(client_fd, STDIN_FILENO);
    dup2(client_fd, STDOUT_FILENO);
    dup2(client_fd, STDERR_FILENO);
    close(client_fd);

    printf("%s", kWelcome_Message);

    sem_wait(write_lock);
    write(shared_pipe[1], &user_id, sizeof(user_id));
    string msg = "login " + ip_port + " *** User \'" + DEFAULT_USER_NAME + "\' entered from " + ip_port + ". ***";
    size_t size = msg.size();
    write(shared_pipe[1], &size, sizeof(size));
    write(shared_pipe[1], msg.c_str(), size);
    sem_post(read_lock);

    unordered_map<int, array<int, 2>> pipeMap; // 0: read, 1: write

    fds_[0] = {.fd = pipe_fd[0], .events = POLL_IN, .revents = 0};
    fds_[1] = {.fd = STDIN_FILENO, .events = POLL_IN, .revents = 0};

    while (true) {
      poll(fds_.data(), fds_.size(), -1);

      for (pollfd pfd : fds_) {
        if (pfd.revents & POLL_IN) {
          // handle internal message
          if (pfd.fd == pipe_fd[0]) {
            HandleInternalMsg(pipe_fd[0], user_id);
          }

          // handle client message
          if (pfd.fd == STDIN_FILENO) {
            service_function_(user_id, pipeMap);
          }
        }
      }
    }
  }

 public:
  Server(uint16_t port, void(*service)(int, unordered_map<int, array<int, 2>> &)) {
    tcp_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    tcp_addr_.sin_family = AF_INET;
    tcp_addr_.sin_addr.s_addr = INADDR_ANY;
    tcp_addr_.sin_port = htons(port);

    int enable = 1;
    setsockopt(tcp_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    bind(tcp_fd_, (sockaddr *) &tcp_addr_, sizeof(tcp_addr_));
    cout << "[Server] Hosting on " << inet_ntoa(tcp_addr_.sin_addr) << ", port " << ntohs(tcp_addr_.sin_port) << '\n';

    fds_[0] = {.fd = shared_pipe[0], .events = POLL_IN, .revents = 0};
    fds_[1] = {.fd = tcp_fd_, .events = POLL_IN, .revents = 0};

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
      poll(fds_.data(), fds_.size(), -1);

      for (pollfd pfd : fds_) {
        if (pfd.revents & POLL_IN) {
          if (pfd.fd == shared_pipe[0]) {
            // broadcast internal message to every client
            sem_wait(read_lock);
            for (const auto[key, value] : id_pipe) {
              tee(shared_pipe[0], value, INT_MAX, 0);
            }
            // handle internal message
            HandleInternalMsg(shared_pipe[0], -1);
            sem_post(write_lock);
          }

          // new tcp connection
          if (pfd.fd == tcp_fd_) {
            handleConnection();
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

  // set up shared_pipe
  pipe2(shared_pipe.data(), O_CLOEXEC);

  // init read write lock from memory file
  int mem_fd = shm_open("share-mem", O_CREAT | O_RDWR, 0664);
  ftruncate(mem_fd, sizeof(sem_t) * 2);
  read_lock = (sem_t *) mmap(nullptr, sizeof(sem_t) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
  write_lock = read_lock + 1;
  sem_init(read_lock, 1, 0);
  sem_init(write_lock, 1, 1);
  close(mem_fd);
  shm_unlink("share-mem");

  // fill available user ids
  for (int i = 1; i <= MAX_USER_ID; i++) {
    ids_available.emplace(i);
  }

  if (argc == 2) {
    Server server(stol(argv[1]), Shell);
    server.listen_for_message();
  }

  // destroy semaphores
  sem_destroy(read_lock);
  sem_destroy(write_lock);
  return 0;
}