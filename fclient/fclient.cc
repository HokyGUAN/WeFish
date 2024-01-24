/*
 *       WeFish File Client
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#include <cstdlib>
#include <deque>
#include <thread>
#include <boost/asio.hpp>
#include "fclient.hpp"
#include <stdio.h>
#include <utility>


Connection::Connection(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints)
    : io_context_(io_context), strand_(io_context), socket_(io_context), endpoints_(endpoints) {
}

void Connection::doConnect(const ResponseHandler& handler)
{
    boost::asio::async_connect(socket_, endpoints_,
        [this, self = shared_from_this(), handler](boost::system::error_code ec, tcp::endpoint) {
            std::string ret;
            if (!ec && handler) {
                ret = "Connected";
                handler(ec, ret);
            }
        });
}

void Connection::doDisconnect()
{
    std::cout << "Disconnecting\n";
    if (!socket_.is_open()) {
        std::cerr << "Not connected\n";
        return;
    }
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec) {
        std::cout << " Error in socket shutdown: " << ec.message() << "\n";
    }
    socket_.close(ec);
    if (ec)
        std::cout << "Error in socket close: " << ec.message() << "\n";
    std::cout << "Disconnected\n";
    std::cout << "\n * Enter to quit * \n";
    io_context_.stop();
}

void Connection::doRead(const ResponseHandler& handler)
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(
        socket_, streambuf_, delimiter,
        boost::asio::bind_executor(strand_, [this, self = shared_from_this(), delimiter, handler](const std::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                if (handler) {
                    std::string ret = "Socket Error Occurred";
                    handler(ec, ret);
                }
                return;
            }
            std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
            if (!line.empty()) {
                if (line.back() == '\r')
                    line.resize(line.size() - 1);
                if (!line.empty()) {
                    if (handler)
                        handler(ec, line);
                }
            }
            streambuf_.consume(bytes_transferred);
            doRead(handler);
        }));
}

void Connection::SendAsync(const std::string& msg)
{
    strand_.post([this, self = shared_from_this(), msg]() {
        messages_.emplace_back(msg + "\r\n");
        if (messages_.size() > 1) {
            // std::cout << "TCP session async_writes: " << messages_.size() << "\n";
            return;
        }
        doWrite();
    });
}

void Connection::doWrite()
{
    boost::asio::async_write(socket_, boost::asio::buffer(messages_.front()),
         boost::asio::bind_executor(strand_, [this, self = shared_from_this()] (std::error_code ec, std::size_t length) {
             messages_.pop_front();
             if (ec) {
                 std::cout << "Client: Error while writing to socket: " << ec.message() << "\n";
                 socket_.close();
             }
             if (!messages_.empty())
                 doWrite();
         }));
}

FileStream::FileStream(std::string& name) : name_(name)
{
}

void FileStream::Push(std::string& content)
{
    contents_.append(content);
}

std::string FileStream::Pop(void)
{
    return contents_;
}

int FileStream::Size(void)
{
    return contents_.size();
}

FClient::FClient(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints)
{
    connection_ = std::make_shared<Connection>(io_context, endpoints);
    Start();
}

void FClient::Start()
{
    connection_->doConnect([this] (const boost::system::error_code& ec, std::string& response) {
        if (!ec) {
            if (response == "Connected") {
                doMessageReceived();
            }
        }
    });
}

void FClient::doMessageReceived()
{
    connection_->doRead([this] (const boost::system::error_code& ec, std::string& response) {
        if (ec) {
            std::cerr << "Server Lost\n";
            Stop();
        } else {
            //std::cout << "Response: " << response << " , Length: " << response.size() << "\n";
            jsonrpcpp::entity_ptr entity(nullptr);
            entity = jsonrpcpp::Parser::do_parse(response);
            if (entity->is_response()) {
                jsonrpcpp::response_ptr response = std::dynamic_pointer_cast<jsonrpcpp::Response>(entity);
                jsonrpcpp::parameter_ptr param = std::make_shared<jsonrpcpp::Parameter>(response->result());
                if (param->get("Method") == "SayHello") {
                    int Expired = param->get("Expired");
                    if (Expired) {
                        jsonrpcpp::request_ptr request(nullptr);
                        request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_SETTING), "UpgradeRequest",
                            jsonrpcpp::Parameter("Account", 522001)));
                        connection_->SendAsync(request->to_json().dump());
                    }
                } else if (param->get("Method") == "UpgradeReply") {
                    size_t file_size = param->get("Filesize");
                    std::cout << "Upgrade File Size: " << std::to_string(file_size) << std::endl;

                    std::string file_name = "upgrade.exe";
                    upgrade_stream_.reset(new FileStream(file_name));

                    jsonrpcpp::request_ptr request(nullptr);
                    request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "UpgradeFileTransfer",
                        jsonrpcpp::Parameter("Account", 522001)));
                    connection_->SendAsync(request->to_json().dump());

                } else if (param->get("Method") == "UpgradeProcessing") {
                    int process = param->get("Process");
                    std::cout << "Process: " << std::to_string(process) << std::endl;
                    std::string content = param->get("Content");
                    upgrade_stream_->Push(content);

                    if (process < 100) {
                        jsonrpcpp::request_ptr request(nullptr);
                        request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "UpgradeFileTransfer",
                            jsonrpcpp::Parameter("Account", 522001)));
                        connection_->SendAsync(request->to_json().dump());
                    } else {
                        std::fstream file_stream;
                        file_stream.open("upgrade2.exe", std::ios::out | std::ios::binary);
                        file_stream.write(upgrade_stream_->Pop().c_str(), upgrade_stream_->Size());
                        file_stream.close();
                    }
                }
            } else if (entity->is_notification()) {
                jsonrpcpp::notification_ptr notification = std::dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
                if (notification->method() == "FileTransferNotification") {
                    int from_account = notification->params().get("Account");
                    int to_account = notification->params().get("ToAccount");
                    std::string file_name = notification->params().get("Filename");
                    std::string checksum = notification->params().get("Checksum");
                    int file_size = notification->params().get("Filesize");
                    std::string content = notification->params().get("Content");
                    int status = notification->params().get("Status");

                    std::cout << "Account: " << std::to_string(from_account) << std::endl;
                    std::cout << "ToAccount: " << std::to_string(to_account) << std::endl;
                    std::cout << "Filename: " << file_name << std::endl;
                    std::cout << "Filesize: " << std::to_string(file_size) << std::endl;
                    std::cout << "Checksum: " << checksum << std::endl;
                    std::cout << "Content: " << content << std::endl;
                    std::cout << "Status: " << std::to_string(status) << std::endl;

                    auto it = container_.find(checksum);
                    if (status == 1 && it == container_.end()) {
                        std::shared_ptr<FileStream> file_stream = std::make_shared<FileStream>(file_name);
                        // file_stream->Push(content);
                        container_.insert(std::pair<std::string, std::shared_ptr<FileStream>>(checksum, file_stream));
                    } else if (status == 2 && it != container_.end()) {
                        (it->second)->Push(content);
                    } else if (status == 0 && it != container_.end()) {
                        (it->second)->Push(content);
                        std::fstream file_stream;
                        file_stream.open(file_name, std::ios::out | std::ios::binary);
                        file_stream.write((it->second)->Pop().c_str(), (it->second)->Size());
                        file_stream.close();
                        container_.erase(it);
                    }
				}
            }
        }
    });
}

void FClient::Send(const std::string& msg)
{
    connection_->SendAsync(msg);
}

void FClient::Stop()
{
    connection_->doDisconnect();
}

void FClient::SendFile(std::string& file_name)
{
    bool head = true;
    char buffer[1025] = {0};
    std::fstream file_stream;
    file_stream.open(file_name, std::ios::in | std::ios::binary);
    if (!file_stream.is_open()) {
        std::cerr << "Could not create file" << std::endl;
        return;
    }
    file_stream.seekg(0, std::ios::end);
    size_t file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    jsonrpcpp::request_ptr request(nullptr);
    std::string checksum = "36086161706636fbf413b2344fd65e0c";
    int account = 522001;
    int to_account = 522002;

    while (file_stream.eof() == false) {
        if (head) {
            head = false;
            request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "FileTransfer",
                jsonrpcpp::Parameter("Account", account, "ToAccount", to_account, "Status", 1,
                                    "Filename", file_name, "Filesize", file_size, "Checksum", checksum,
                                    "Content", "")));
        } else {
            file_stream.read(buffer, 1024);
            std::cout << "(-): " << std::to_string(file_stream.gcount()) << std::endl;
            size_t real_read = file_stream.gcount();
            if (real_read < 1024) {
                std::string content = buffer;
                content.erase(real_read, 1024 - real_read);
                request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "FileTransfer",
                    jsonrpcpp::Parameter("Account", account, "ToAccount", to_account, "Status", 0,
                                        "Filename", file_name, "Filesize", file_size, "Checksum", checksum,
                                        "Content", content)));
            } else {
                request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "FileTransfer",
                    jsonrpcpp::Parameter("Account", account, "ToAccount", to_account, "Status", 2,
                                        "Filename", file_name, "Filesize", file_size, "Checksum", checksum,
                                        "Content", buffer)));
            }
        }
        Send(request->to_json().dump());
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) return -1;

    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("10.13.3.23", "6688");
        FClient fclient(io_context, endpoints);

        jsonrpcpp::request_ptr request(nullptr);
        request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_SETTING), "SayHello",
            jsonrpcpp::Parameter("Account", atoi(argv[2]), "Clientversion", "1.0")));
        fclient.Send(request->to_json().dump());
        std::cout << request->to_json().dump() << std::endl;

        if (atoi(argv[1]) == 1) {
            std::string filename = "send.txt";
            fclient.SendFile(filename);
        }

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM, SIGPIPE);
        signals.async_wait([&](const boost::system::error_code& ec, int signal) {
            if (!ec) {
                switch (signal) {
                case SIGINT:
                case SIGTERM:
                    throw std::system_error(); 
                    break;
                case SIGPIPE:
                    break;
                default:
                    throw std::system_error();
                    break;
                }
            } else {
                std::cout << "Failed to wait for signal, error: " << ec.message() << "\n";
            }
            fclient.Stop();
        });

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "\nException Occurred\n";
    }
    std::cout << "\nWeFish FClient terminated.\n";
    return 0;
}


