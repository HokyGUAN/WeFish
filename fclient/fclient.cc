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

void FContainer::Push(std::string& content)
{
    container_.append(content);
}

std::string FContainer::Pop(void)
{
    return container_;
}

int FContainer::Size(void)
{
    return container_.size();
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
                
            } else if (entity->is_notification()) {
                jsonrpcpp::notification_ptr notification = std::dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
                if (notification->method() == "FileTransferNotification") {
                    std::string from_account = notification->params().get("Account");
                    std::string to_account = notification->params().get("ToAccount");
                    std::string file_name = notification->params().get("Filename");
                    std::string checksum = notification->params().get("Checksum");
                    int file_size = notification->params().get("Filesize");
                    std::string content = notification->params().get("Content");
                    int status = notification->params().get("Status");

                    if (status == 1) {
                        auto it = m_file_container_.find(checksum);
                        if (it == m_file_container_.end()) {
                            std::shared_ptr<FContainer> f(new std::shared_ptr<FContainer>);
                            f->Push(content);
                            m_file_container_.insert(std::make_pair<checksum, f>);
                        } else {
                            (it->second)->Push(content);
                        }
                    } else if (status == 2) {
                        auto it = m_file_container_.find(checksum);
                        if (it == m_file_container_.end()) {
                            std::shared_ptr<FContainer> f(new std::shared_ptr<FContainer>);
                            f->Push(content);
                            m_file_container_.insert(std::make_pair<checksum, f>);
                        } else {
                            (it->second)->Push(content);
                        }
                    } else if (status == 0) {
                        auto it = m_file_container_.find(checksum);
                        (it->second)->Push(content);
                        std::fstream file_stream;
                        file_stream.open(file_name, std::ios::out | std::ios::binary);
                        file_stream.write((it->second)->Pop(), (it->second)->Size());
                        file_stream.close();
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
    std::fstream file_stream;
    file_stream.open(file_name, std::ios::in | std::ios::binary);
    size_t file_size = 0;

    if (!file_stream.is_open()) {
        std::cerr << "Could not create file" << std::endl;
        return;
    }

    file_stream.seekg(0, std::ios::end);
    file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);
    // char buf[100] = {0};
    // while(file_stream.eof() == false) {
    //     file_stream.read(buf, 100);
    //     std::cout << buf;
    //     std::cout << "Gcount: " << file_stream.gcount() << std::endl;
    //     fclient.Send(buf);
    // }

    std::string checksum = "36086161706636fbf413b2344fd65e0c";
    int account = 522001;
    int to_account = 522002;

    jsonrpcpp::request_ptr request(nullptr);
    request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_FILE), "FileTransfer",
        jsonrpcpp::Parameter("Account", account, "ToAccount", to_account, "Filename", file_name, "Filesize", file_size, "Checksum", checksum,
                            "Content", "456")));
    Send(request->to_json().dump());
}

int main(int argc, char* argv[])
{
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("10.13.3.23", "6688");
        FClient fclient(io_context, endpoints);

        jsonrpcpp::request_ptr request(nullptr);
        request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_SETTING), "SayHello",
            jsonrpcpp::Parameter("Account", atoi(argv[1]))));
        fclient.Send(request->to_json().dump());
        std::cout << request->to_json().dump() << std::endl;


        std::string filename = "send.txt";
        fclient.SendFile(filename);

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


