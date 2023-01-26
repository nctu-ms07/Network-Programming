#include <bits/stdc++.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <wait.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

static std::string VersionString(unsigned int version) {
  unsigned int major = version / 10;
  unsigned int minor = version % 10;
  return "HTTP/" + std::to_string(major) + '.' + std::to_string(minor);
}

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
 public:
  typedef std::shared_ptr<TcpConnection> pointer;

  static pointer create(boost::asio::io_context &io_context) {
    return pointer(new TcpConnection(io_context));
  }

  tcp::socket &socket() {
    return socket_;
  }

  void start() {
    read_request();
  }

 private:
  boost::asio::io_context &io_context_;

  // The socket for the currently connected client.
  tcp::socket socket_;

  // The buffer for performing reads.
  beast::flat_buffer buffer_{8192};

  // The request message.
  http::request<http::dynamic_body> request_;

  explicit TcpConnection(boost::asio::io_context &io_context) : io_context_(io_context), socket_(io_context) {
  }

  // Asynchronously receive a complete request message.
  void read_request() {
    auto self = shared_from_this();
    http::async_read(
        socket_,
        buffer_,
        request_,
        [self](beast::error_code, std::size_t bytes_transferred) {
          boost::ignore_unused(bytes_transferred);
          self->process_request();
        });
  }

  // Determine what needs to be done with the request message.
  void process_request() {
    // Inform the io_context that we are about to fork. The io_context cleans
    // up any internal resources, such as threads, that may interfere with
    // forking.
    io_context_.notify_fork(boost::asio::io_context::fork_prepare);

    pid_t child;
    while ((child = fork()) == -1) {
      if (errno == EAGAIN) {
        wait(nullptr); // wait for any child process to release resource
      }
    }

    if (child != 0) { // parent process
      // Inform the io_context that the fork is finished (or failed) and that
      // this is the parent process. The io_context uses this opportunity to
      // recreate any internal resources that were cleaned up during
      // preparation for the fork.
      io_context_.notify_fork(boost::asio::io_context::fork_parent);
      return;
    }

    // Inform the io_context that the fork is finished and that this is the
    // child process. The io_context uses this opportunity to create any
    // internal file descriptors that must be private to the new process.
    io_context_.notify_fork(boost::asio::io_context::fork_child);

    auto args = std::make_unique<char *[]>(2);
    args[1] = nullptr;

    setenv("REQUEST_METHOD", request_.method_string().to_string().c_str(), 1);
    setenv("REQUEST_URI", request_.target().to_string().c_str(), 1);

    std::string target;
    std::size_t query_position = request_.target().find_first_of('?');
    if (query_position != std::string::npos) {
      setenv("QUERY_STRING", request_.target().substr(query_position + 1).to_string().c_str(), 1);
      target = "./" + std::string(request_.target().substr(1).data(), query_position - 1);
      args[0] = target.data();
    } else {
      setenv("QUERY_STRING", "", 1);
      target = "./" + std::string(request_.target().substr(1).data(), request_.target().size() - 1);
      args[0] = target.data();
    }

    setenv("SERVER_PROTOCOL", VersionString(request_.version()).c_str(), 1);
    setenv("HTTP_HOST", request_.at(http::field::host).to_string().c_str(), 1);
    setenv("SERVER_ADDR", socket_.local_endpoint().address().to_string().c_str(), 1);
    setenv("SERVER_PORT", std::to_string(socket_.local_endpoint().port()).c_str(), 1);
    setenv("REMOTE_ADDR", socket_.remote_endpoint().address().to_string().c_str(), 1);
    setenv("REMOTE_PORT", std::to_string(socket_.remote_endpoint().port()).c_str(), 1);

    show_env();

    // child process
    dup2(socket_.native_handle(), STDIN_FILENO);
    dup2(socket_.native_handle(), STDOUT_FILENO);
    close(socket_.native_handle());

    // Http Response Status-Line
    std::cout << VersionString(request_.version()) << " "
              << static_cast<std::underlying_type_t<http::status>>(http::status::ok) << " "
              << http::status::ok << "\r\n";

    if (execv(args[0], args.get()) == -1) {
      perror(args[0]);
      exit(0);
    }
  }

  void show_env() {
    printf("\n");
    printf("REQUEST_METHOD %s\n", getenv("REQUEST_METHOD"));
    printf("REQUEST_URI %s\n", getenv("REQUEST_URI"));
    printf("QUERY_STRING %s\n", getenv("QUERY_STRING"));
    printf("SERVER_PROTOCOL %s\n", getenv("SERVER_PROTOCOL"));
    printf("HTTP_HOST %s\n", getenv("HTTP_HOST"));
    printf("SERVER_ADDR %s\n", getenv("SERVER_ADDR"));
    printf("SERVER_PORT %s\n", getenv("SERVER_PORT"));
    printf("REMOTE_ADDR %s\n", getenv("REMOTE_ADDR"));
    printf("REMOTE_PORT %s\n", getenv("REMOTE_PORT"));
    printf("\n");
  }
};

class TcpServer {
 public:
  TcpServer(boost::asio::io_context &io_context, uint16_t port)
      : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
  }

 private:
  boost::asio::io_context &io_context_;
  tcp::acceptor acceptor_;

  void start_accept() {
    TcpConnection::pointer new_connection = TcpConnection::create(io_context_);

    acceptor_.async_accept(new_connection->socket(),
                           [this, new_connection](beast::error_code) {
                             new_connection->start();
                             start_accept();
                           });
  }
};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, SIG_IGN);

  if (argc == 2) {
    try {
      boost::asio::io_context io_context;
      TcpServer server(io_context, std::stol(argv[1]));
      io_context.run();
    }
    catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }

  return 0;
}