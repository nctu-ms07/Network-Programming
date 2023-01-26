#include <bits/stdc++.h>
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

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

std::vector<Console_ARG> console_args;

void ParseQuery() {
  std::stringstream ss(getenv("QUERY_STRING"));

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
    }
  }
}

void PreprocessResponse() {
  std::cout << "Content-type: text/html\r\n\r\n";

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

  std::string th, td;
  for (const Console_ARG &arg : console_args) {
    std::string ip_port = arg.host + ':' + arg.port;
    th += R"(<th scope="col">)" + ip_port + R"(</th>)";
    td += R"(<td><pre id=")" + ip_port + R"(" class="mb-0"></pre></td>)";
  }

  std::cout << fmt % th % td;
}

class TcpClient : public std::enable_shared_from_this<TcpClient> {
 public:
  typedef std::shared_ptr<TcpClient> pointer;

  static pointer create(boost::asio::io_context &io_context) {
    return pointer(new TcpClient(io_context));
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

  explicit TcpClient(boost::asio::io_context &io_context) : resolver_(io_context), socket_(io_context) {
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
    std::cout << R"(<script>document.all(')" << ip_port_ << R"(').innerHTML += ')" << HtmlEscape(ss.str())
              << R"(';</script>)" << std::flush;

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
    std::cout << R"(<script>document.all(')" << ip_port_ << R"(').innerHTML += '<b>)" << HtmlEscape(cmd)
              << R"(</b>';</script>)" << std::flush;

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

int main() {
  ParseQuery();
  PreprocessResponse();

  try {
    boost::asio::io_context io_context;
    for (const Console_ARG &arg : console_args) {
      TcpClient::pointer client = TcpClient::create(io_context);
      client->async_connect(arg);
    }
    io_context.run();
  }
  catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}