#include <bits/stdc++.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

static std::string VersionString(unsigned int version) {
  unsigned int major = version / 10;
  unsigned int minor = version % 10;
  return "HTTP/" + std::to_string(major) + '.' + std::to_string(minor);
}

static std::string HtmlEscape(std::string content) {
  // List of predefined entities in XML
  boost::replace_all(content, "\"", "&quot;");
  boost::replace_all(content, "&", "&amp;");
  boost::replace_all(content, "\'", "&apos;");
  boost::replace_all(content, "<", "&lt;");
  boost::replace_all(content, ">", "&gt;");

  // character entity references in HTML
  boost::replace_all(content, "\n", "&NewLine;");
  return content;
}

struct Console_ARG {
  std::string host;
  std::string port;
  std::string file;
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
  }

 private:
  boost::asio::io_context &io_context_;

  // The socket for the currently connected client.
  tcp::socket socket_;

  // The buffer for performing reads.
  beast::flat_buffer buffer_{8192};

  // The request message.
  http::request<http::dynamic_body> request_;

  // The response message.
  http::response<http::dynamic_body> response_;

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
  void process_request();
};

class TcpClient : public std::enable_shared_from_this<TcpClient> {
 public:
  typedef std::shared_ptr<TcpClient> pointer;

  static pointer create(boost::asio::io_context &io_context, TcpConnection::pointer ptr) {
    return pointer(new TcpClient(io_context, std::move(ptr)));
  }

  void async_connect(const Console_ARG &console_arg) {
    auto self = shared_from_this();
    resolver_.async_resolve(console_arg.host,
                            console_arg.port,
                            [self, console_arg](beast::error_code, const tcp::resolver::iterator &it) {
                              self->socket_.async_connect(it->endpoint(),
                                                          [self, console_arg](beast::error_code) {
                                                            self->ip_port_ =
                                                                console_arg.host + ':' + console_arg.port;
                                                            self->file_.open("test_case/" + console_arg.file,
                                                                             std::fstream::in);
                                                            self->async_read();
                                                          });
                            });
  }

 private:
  tcp::resolver resolver_;
  tcp::socket socket_;
  boost::asio::streambuf streambuf_;
  std::string ip_port_;
  std::fstream file_;

  TcpConnection::pointer tcp_connection_;

  explicit TcpClient(boost::asio::io_context &io_context, TcpConnection::pointer ptr)
      : resolver_(io_context), socket_(io_context), tcp_connection_(std::move(ptr)) {
  }

  void async_read() {
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
                            streambuf_,
                            boost::asio::transfer_at_least(1),
                            [self](beast::error_code, std::size_t) {
                              self->process_response();
                            });
  }

  void process_response() {
    // read streambuf_
    std::istream input(&streambuf_);
    std::stringstream ss;
    ss << input.rdbuf() << std::flush;

    std::string buffer_content =
        R"(<script>document.all(')" + ip_port_ + R"(').innerHTML += ')" + HtmlEscape(ss.str()) + R"(';</script>)";
    boost::asio::async_write(tcp_connection_->socket(),
                             boost::asio::buffer(buffer_content),
                             [](beast::error_code, std::size_t) {});

    if (ss.str().find('%') == std::string::npos) {
      async_read();
      return;
    }

    std::string cmd;
    std::getline(file_, cmd);
    if (cmd.back() == '\r') {
      cmd.pop_back();
    }
    cmd.append(1, '\n');
    buffer_content =
        R"(<script>document.all(')" + ip_port_ + R"(').innerHTML += '<b>)" + HtmlEscape(cmd) + R"(</b>';</script>)";
    boost::asio::async_write(tcp_connection_->socket(),
                             boost::asio::buffer(buffer_content),
                             [](beast::error_code, std::size_t) {});

    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(cmd), [self, cmd](beast::error_code, std::size_t) {
      if (cmd == "exit\n") {
        self->socket_.close();
      } else {
        self->async_read();
      }
    });
  }
};

void TcpConnection::process_request() {
  std::string target = request_.target().to_string();
  std::string QUERY_STRING;

  std::size_t query_position = target.find_first_of('?');
  if (query_position != std::string::npos) {
    QUERY_STRING = target.substr(query_position + 1);
    target = target.substr(1, query_position - 1);
  } else {
    target = target.substr(1);
  }

  std::cout << target << '\n';
  std::cout << QUERY_STRING << '\n';

  if (target == "panel.cgi") {
    boost::asio::streambuf response;
    std::ostream response_stream(&response);

    // Http Response Status-Line
    response_stream << VersionString(request_.version()) << " "
                    << static_cast<std::underlying_type_t<http::status>>(http::status::ok) << " "
                    << http::status::ok << "\r\n";

    response_stream << "Content-type: text/html\r\n\r\n";

    response_stream << "<!DOCTYPE html>"
                       "<html lang=\"en\">"
                       "  <head>"
                       "    <title>NP Project 3 Panel</title>"
                       "    <link"
                       "      rel=\"stylesheet\""
                       "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""
                       "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""
                       "      crossorigin=\"anonymous\""
                       "    />"
                       "    <link"
                       "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""
                       "      rel=\"stylesheet\""
                       "    />"
                       "    <link"
                       "      rel=\"icon\""
                       "      type=\"image/png\""
                       "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\""
                       "    />"
                       "    <style>"
                       "      * {"
                       "        font-family: 'Source Code Pro', monospace;"
                       "      }"
                       "    </style>"
                       "  </head>"
                       "  <body class=\"bg-secondary pt-5\">"
                       "    <form action=\"console.cgi\" method=\"GET\">"
                       "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">"
                       "        <thead class=\"thead-dark\">"
                       "          <tr>"
                       "            <th scope=\"col\">#</th>"
                       "            <th scope=\"col\">Host</th>"
                       "            <th scope=\"col\">Port</th>"
                       "            <th scope=\"col\">Input File</th>"
                       "          </tr>"
                       "        </thead>"
                       "        <tbody>"
                       "          <tr>"
                       "            <th scope=\"row\" class=\"align-middle\">Session 1</th>"
                       "            <td>"
                       "              <div class=\"input-group\">"
                       "                <select name=\"h0\" class=\"custom-select\">"
                       "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"
                       "                </select>"
                       "                <div class=\"input-group-append\">"
                       "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>"
                       "                </div>"
                       "              </div>"
                       "            </td>"
                       "            <td>"
                       "              <input name=\"p0\" type=\"text\" class=\"form-control\" size=\"5\" />"
                       "            </td>"
                       "            <td>"
                       "              <select name=\"f0\" class=\"custom-select\">"
                       "                <option></option>"
                       "                <option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>"
                       "              </select>"
                       "            </td>"
                       "          </tr>"
                       "          <tr>"
                       "            <th scope=\"row\" class=\"align-middle\">Session 2</th>"
                       "            <td>"
                       "              <div class=\"input-group\">"
                       "                <select name=\"h1\" class=\"custom-select\">"
                       "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"
                       "                </select>"
                       "                <div class=\"input-group-append\">"
                       "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>"
                       "                </div>"
                       "              </div>"
                       "            </td>"
                       "            <td>"
                       "              <input name=\"p1\" type=\"text\" class=\"form-control\" size=\"5\" />"
                       "            </td>"
                       "            <td>"
                       "              <select name=\"f1\" class=\"custom-select\">"
                       "                <option></option>"
                       "                <option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>"
                       "              </select>"
                       "            </td>"
                       "          </tr>"
                       "          <tr>"
                       "            <th scope=\"row\" class=\"align-middle\">Session 3</th>"
                       "            <td>"
                       "              <div class=\"input-group\">"
                       "                <select name=\"h2\" class=\"custom-select\">"
                       "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"
                       "                </select>"
                       "                <div class=\"input-group-append\">"
                       "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>"
                       "                </div>"
                       "              </div>"
                       "            </td>"
                       "            <td>"
                       "              <input name=\"p2\" type=\"text\" class=\"form-control\" size=\"5\" />"
                       "            </td>"
                       "            <td>"
                       "              <select name=\"f2\" class=\"custom-select\">"
                       "                <option></option>"
                       "                <option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>"
                       "              </select>"
                       "            </td>"
                       "          </tr>"
                       "          <tr>"
                       "            <th scope=\"row\" class=\"align-middle\">Session 4</th>"
                       "            <td>"
                       "              <div class=\"input-group\">"
                       "                <select name=\"h3\" class=\"custom-select\">"
                       "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"
                       "                </select>"
                       "                <div class=\"input-group-append\">"
                       "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>"
                       "                </div>"
                       "              </div>"
                       "            </td>"
                       "            <td>"
                       "              <input name=\"p3\" type=\"text\" class=\"form-control\" size=\"5\" />"
                       "            </td>"
                       "            <td>"
                       "              <select name=\"f3\" class=\"custom-select\">"
                       "                <option></option>"
                       "                <option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>"
                       "              </select>"
                       "            </td>"
                       "          </tr>"
                       "          <tr>"
                       "            <th scope=\"row\" class=\"align-middle\">Session 5</th>"
                       "            <td>"
                       "              <div class=\"input-group\">"
                       "                <select name=\"h4\" class=\"custom-select\">"
                       "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>"
                       "                </select>"
                       "                <div class=\"input-group-append\">"
                       "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>"
                       "                </div>"
                       "              </div>"
                       "            </td>"
                       "            <td>"
                       "              <input name=\"p4\" type=\"text\" class=\"form-control\" size=\"5\" />"
                       "            </td>"
                       "            <td>"
                       "              <select name=\"f4\" class=\"custom-select\">"
                       "                <option></option>"
                       "                <option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>"
                       "              </select>"
                       "            </td>"
                       "          </tr>"
                       "          <tr>"
                       "            <td colspan=\"3\"></td>"
                       "            <td>"
                       "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>"
                       "            </td>"
                       "          </tr>"
                       "        </tbody>"
                       "      </table>"
                       "    </form>"
                       "  </body>"
                       "</html>";

    auto self = shared_from_this();
    boost::asio::async_write(socket_, response, [](beast::error_code, std::size_t) {});
    return;
  }

  if (target == "console.cgi") {
    boost::asio::streambuf response;
    std::ostream response_stream(&response);

    // Http Response Status-Line
    response_stream << VersionString(request_.version()) << " "
                    << static_cast<std::underlying_type_t<http::status>>(http::status::ok) << " "
                    << http::status::ok << "\r\n";

    response_stream << "Content-type: text/html\r\n\r\n";

    // parse query
    std::vector<Console_ARG> console_args;
    std::string th, td;
    std::stringstream ss(QUERY_STRING);

    std::string arg;
    std::size_t position;
    while (std::getline(ss, arg, '&')) {
      // host
      position = arg.find_first_of('=');
      std::string host = arg.substr(position + 1);
      // port
      std::getline(ss, arg, '&');
      position = arg.find_first_of('=');
      std::string port = arg.substr(position + 1);
      // file
      std::getline(ss, arg, '&');
      position = arg.find_first_of('=');
      std::string file = arg.substr(position + 1);

      if (!host.empty()) {
        console_args.emplace_back(Console_ARG{.host=host, .port=port, .file=file});
        host += ':' + port;
        th += R"(<th scope="col">)" + host + R"(</th>)";
        td += R"(<td><pre id=")" + host + R"(" class="mb-0"></pre></td>)";
      }
    }

    boost::format fmt("<!DOCTYPE html>"
                      "<html lang=\"en\">"
                      "  <head>"
                      "    <meta charset=\"UTF-8\" />"
                      "    <title>NP Project 3 Sample Console</title>"
                      "    <link"
                      "      rel=\"stylesheet\""
                      "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""
                      "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""
                      "      crossorigin=\"anonymous\""
                      "    />"
                      "    <link"
                      "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""
                      "      rel=\"stylesheet\""
                      "    />"
                      "    <link"
                      "      rel=\"icon\""
                      "      type=\"image/png\""
                      "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""
                      "    />"
                      "    <style>"
                      "      * {"
                      "        font-family: 'Source Code Pro', monospace;"
                      "        font-size: 1rem !important;"
                      "      }"
                      "      body {"
                      "        background-color: #212529;"
                      "      }"
                      "      pre {"
                      "        color: #cccccc;"
                      "      }"
                      "      b {"
                      "        color: #01b468;"
                      "      }"
                      "    </style>"
                      "  </head>"
                      "  <body>"
                      "    <table class=\"table table-dark table-bordered\">"
                      "      <thead>"
                      "        <tr>"
                      "          %1%"
                      "        </tr>"
                      "      </thead>"
                      "      <tbody>"
                      "        <tr>"
                      "          %2%"
                      "        </tr>"
                      "      </tbody>"
                      "    </table>"
                      "  </body>"
                      "</html>");

    response_stream << fmt % th % td;

    auto self = shared_from_this();
    boost::asio::async_write(socket_, response, [self, console_args](beast::error_code, std::size_t) {
      for (const Console_ARG &arg : console_args) {
        TcpClient::pointer client = TcpClient::create(self->io_context_, self);
        client->async_connect(arg);
      }
    });
    return;
  }

  response_.version(request_.version());
  response_.result(http::status::not_found);
  response_.set(http::field::content_type, "text/plain");
  beast::ostream(response_.body()) << "File not found\r\n";
  response_.prepare_payload();

  auto self = shared_from_this();
  http::async_write(socket_, response_, [self](beast::error_code error, std::size_t) {
    self->socket_.shutdown(tcp::socket::shutdown_send, error);
  });
}

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