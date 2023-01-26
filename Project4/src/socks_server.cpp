#include <bits/stdc++.h>
#include <wait.h>
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class SOCK4_Header {
 public:

  std::array<boost::asio::mutable_buffer, 4> request_buffer() {
    return std::array<boost::asio::mutable_buffer, 4>{
        boost::asio::buffer(&version, 1),
        boost::asio::buffer(&code, 1),
        boost::asio::buffer(&destPort, 2),
        boost::asio::buffer(destIP),
    };
  }

  std::array<boost::asio::const_buffer, 4> respond_buffer(uint8_t reply_code, uint16_t port = 0) {
    version = 0;
    code = reply_code;
    destPort = port;
    destIP = {0, 0, 0, 0};
    return std::array<boost::asio::const_buffer, 4>{
        boost::asio::buffer(&version, 1),
        boost::asio::buffer(&code, 1),
        boost::asio::buffer(&destPort, 2),
        boost::asio::buffer(destIP),
    };
  }

  uint8_t getCode() const {
    return code;
  }

  std::string getIP() {
    return boost::asio::ip::address_v4(destIP).to_string();
  }

  uint16_t getPort() const {
    return (destPort << 8) | (destPort >> 8);
  }

  bool isSOCK4A() {
    return destIP[0] == 0 && destIP[1] == 0 && destIP[2] == 0 && destIP[3] != 0;
  }

 private:
  uint8_t version;
  uint8_t code;
  uint16_t destPort;
  std::array<uint8_t, 4> destIP;
};

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
    std::string command = request_.getCode() == 1 ? "CONNECT" : "BIND";

    if (passFirewall()) {
      if (command == "CONNECT") {
        connect();
      } else {
        bind();
      }
    } else {
      std::cout << "<S_IP>: " << socket_.remote_endpoint().address().to_string() << '\n';
      std::cout << "<S_PORT>: " << std::to_string(socket_.remote_endpoint().port()) << '\n';
      std::cout << "<D_IP>: " << it_->endpoint().address().to_string() << '\n';
      std::cout << "<D_PORT>: " << request_.getPort() << '\n';
      std::cout << "<Command>: " << command << '\n';
      std::cout << "<Reply>: Reject\n\n";
      boost::asio::write(socket_, request_.respond_buffer(0x5b));
    }
  }

 private:
  boost::asio::io_context &io_context_;

  // The socket for the currently connected client.
  tcp::socket socket_;
  boost::asio::streambuf clientBuf_;

  SOCK4_Header request_;
  tcp::resolver::iterator it_;

  // The socket for the SOCK4 destination.
  tcp::socket dest_socket_;
  boost::asio::streambuf destBuf_;

  explicit TcpConnection(boost::asio::io_context &io_context)
      : io_context_(io_context), socket_(io_context), dest_socket_(io_context) {
  }

  void read_request() {
    beast::error_code error;
    boost::asio::read(socket_, request_.request_buffer(), error);

    if (error == boost::asio::error::eof) {
      return;
    }

    std::istream input(&clientBuf_);

    boost::asio::read_until(socket_, clientBuf_, '\0');
    std::string userID;
    std::getline(input, userID, '\0');

    if (request_.isSOCK4A()) {
      std::string domain;
      boost::asio::read_until(socket_, clientBuf_, '\0');
      std::getline(input, domain, '\0');
      it_ = tcp::resolver(io_context_).resolve(domain, std::to_string(request_.getPort()));
    } else {
      it_ = tcp::resolver(io_context_).resolve(request_.getIP(), std::to_string(request_.getPort()));
    }
  }

  bool passFirewall() {
    std::string target = request_.getCode() == 1 ? "permit c " + it_->endpoint().address().to_string() :
                         "permit b " + it_->endpoint().address().to_string();

    std::ifstream input("./socks.conf");
    std::string line;
    while (getline(input, line)) {
      boost::replace_all(line, "*", ".+");
      std::regex reg(line);
      if (std::regex_match(target, reg)) {
        return true;
      }
    }
    return false;
  }

  void connect() {
    dest_socket_.connect(it_->endpoint());

    std::cout << "<S_IP>: " << socket_.remote_endpoint().address().to_string() << '\n';
    std::cout << "<S_PORT>: " << std::to_string(socket_.remote_endpoint().port()) << '\n';
    std::cout << "<D_IP>: " << it_->endpoint().address().to_string() << '\n';
    std::cout << "<D_PORT>: " << request_.getPort() << '\n';
    std::cout << "<Command>: CONNECT\n";
    std::cout << "<Reply>: Accept\n\n";
    boost::asio::write(socket_, request_.respond_buffer(0x5a));

    clientToDest();
    destToClient();
  }

  void bind() {
    tcp::acceptor bindAcceptor(io_context_, tcp::endpoint(tcp::v4(), 0));

    uint16_t port = (bindAcceptor.local_endpoint().port() >> 8 | bindAcceptor.local_endpoint().port() << 8);
    boost::asio::write(socket_, request_.respond_buffer(0x5a, port));

    bindAcceptor.accept(dest_socket_);
    bindAcceptor.close();

    boost::asio::write(socket_, request_.respond_buffer(0x5a, port));
    std::cout << "<S_IP>: " << socket_.remote_endpoint().address().to_string() << '\n';
    std::cout << "<S_PORT>: " << std::to_string(socket_.remote_endpoint().port()) << '\n';
    std::cout << "<D_IP>: " << it_->endpoint().address().to_string() << '\n';
    std::cout << "<D_PORT>: " << request_.getPort() << '\n';
    std::cout << "<Command>: BIND\n";
    std::cout << "<Reply>: Accept\n\n";

    clientToDest();
    destToClient();
  }

  void clientToDest() {
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
                            clientBuf_,
                            boost::asio::transfer_at_least(1),
                            [self](beast::error_code, std::size_t) {
                              boost::asio::write(self->dest_socket_, self->clientBuf_);
                              self->clientToDest();
                            });
  }

  void destToClient() {
    auto self = shared_from_this();
    boost::asio::async_read(dest_socket_,
                            destBuf_,
                            boost::asio::transfer_at_least(1),
                            [self](beast::error_code, std::size_t) {
                              boost::asio::write(self->socket_, self->destBuf_);
                              self->destToClient();
                            });
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
                               start_accept();
                               return;
                             }

                             // Inform the io_context that the fork is finished and that this is the
                             // child process. The io_context uses this opportunity to create any
                             // internal file descriptors that must be private to the new process.
                             io_context_.notify_fork(boost::asio::io_context::fork_child);
                             acceptor_.close();
                             new_connection->start();
                           });
  }
};

int main(int argc, char *argv[]) {
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
