/*
 *         WeFish Client
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "client.hpp"


// Client implementation
Client::Client(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints, std::string self_name)
    : self_name_(self_name), isProcessing_(true)
{
    connection_ = std::make_shared<Connection>(io_context, endpoints);
    Start();
}

void Client::Start()
{
    connection_->doConnect([this] (const boost::system::error_code& ec, std::string& response) {
        if (!ec) {
            if (response == "Connected") {
                doMessageReceived();
                jsonrpcpp::request_ptr request(nullptr);
                request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_CONTENT), "SayHello", jsonrpcpp::Parameter("Who", self_name_)));
                Send(request->to_json().dump());
            }
        }
    });
}

std::string Client::GetName()
{
    return self_name_;
}

void Client::doMessageReceived()
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
                //TODO: Something about Setting message
                jsonrpcpp::response_ptr response = std::dynamic_pointer_cast<jsonrpcpp::Response>(entity);
                std::string result_str = response->to_json().dump();
                std::cout << "Response From Server: " << result_str << "\n";
            } else if (entity->is_notification()) {
                jsonrpcpp::notification_ptr notification = std::dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
                if (notification->method() == "ContentNotification") {
                    std::string who_str = notification->params().get("Who");
                    std::string content_str = notification->params().get("Content");
                    std::cout << "\t\t\t\t" << who_str << ": " << content_str << "\n";
                } else if (notification->method() == "OnlineNotification") {
                    std::string who_str = notification->params().get("Who");
                    std::cout << "\t\t============ " << who_str << " Online ============\n";
                } else if (notification->method() == "OfflineNotification") {
                    std::string who_str = notification->params().get("Who");
                    std::cout << "\t\t============ " << who_str << " Offline ============\n";
                }
            } else {
                std::cout << "Not registered feedback\n";
            }
        }
    });
}

void Client::Send(const std::string& msg)
{
    connection_->SendAsync(msg);
}

void Client::Stop()
{
    connection_->doDisconnect();
    isProcessing_ = false;
}


// Connection implementation
Connection::Connection(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints)
    : io_context_(io_context), strand_(io_context), socket_(io_context), endpoints_(endpoints) {
}

void Connection::doConnect(const ResponseHandler& handler)
{
    boost::asio::async_connect(socket_, endpoints_,
        [this, self = shared_from_this(), handler](boost::system::error_code ec, tcp::endpoint)
        {
            std::string ret;
            if (!ec && handler)
            {
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
            if (ec)
            {
                if (handler) {
                    std::string ret = "Socket Error Occurred";
                    handler(ec, ret);
                }
                return;
            }
            std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
            if (!line.empty())
            {
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
        if (messages_.size() > 1)
        {
            std::cout << "TCP session async_writes: " << messages_.size() << "\n";
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

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 4) {
            std::cerr << "Usage: ./client <IP Address> <Group port> <Your chat name>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(argv[1], argv[2]);
        Client client(io_context, endpoints, argv[3]);

        std::thread client_thread([&io_context](){ io_context.run(); });

        std::string line;
        while (client.GetProcessStatus()) {
            std::getline(std::cin, line);
            if (!line.empty()) {
                jsonrpcpp::request_ptr request(nullptr);
                request.reset(new jsonrpcpp::Request(jsonrpcpp::Id(MESSAGE_TYPE_CONTENT), "Content", jsonrpcpp::Parameter("Who", client.GetName(), "Content", line)));
                client.Send(request->to_json().dump());
            }
        }

        client_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "\nException Occurred\n";
    }
    std::cout << "\nWeFish Client terminated.\n";
    return 0;
}


