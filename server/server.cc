/*
 *         WeFish Server
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#include "server.hpp"

#include <csignal>
#include <chrono>
#include <exception>
#include <system_error>


Session::Session(boost::asio::io_context& io_context, tcp :: socket socket, Group & group)
    : socket_(std::move(socket)), group_(group), strand_(io_context)
{
}

void Session::processRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification)
{
    try
    {
        Json result;
        if (request->id().int_id() == MESSAGE_TYPE_CONTENT) {
            if (request->method() == "Content") {
                notification.reset(new jsonrpcpp::Notification("ContentNotification", jsonrpcpp::Parameter("Who", request->params().get("Who"), "Content", request->params().get("Content"))));
            } else if (request->method() == "SayHello") {
                notification.reset(new jsonrpcpp::Notification("OnlineNotification", jsonrpcpp::Parameter("Who", request->params().get("Who"))));
                name_ = request->params().get("Who");
            }
        } else if (request->id().int_id() == MESSAGE_TYPE_SETTING) {
            result["Status"] = "isSuccessful?";
            response.reset(new jsonrpcpp::Response(*request, result));
        } else {
            std::cout << "NULL Process Request\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Server::onMessageReceived exception: " << e.what() << ", message: " << request->to_json().dump() << "\n";
        response.reset(new jsonrpcpp::InternalErrorException(e.what(), request->id()));
    }
}


std::string Session::doMessageReceived(const std::string& message)
{
    jsonrpcpp::entity_ptr entity(nullptr);
    entity = jsonrpcpp::Parser::do_parse(message);
    try
    {
        entity = jsonrpcpp::Parser::do_parse(message);
        if (!entity)
            return "";
    }
    catch (const jsonrpcpp::ParseErrorException& e)
    {
        return e.to_json().dump();
    }
    catch (const std::exception& e)
    {
        return jsonrpcpp::ParseErrorException(e.what()).to_json().dump();
    }
    jsonrpcpp::entity_ptr response(nullptr);
    jsonrpcpp::notification_ptr notification(nullptr);
    if (entity->is_request())
    {
        auto self(shared_from_this());
        jsonrpcpp::request_ptr request = std::dynamic_pointer_cast<jsonrpcpp::Request>(entity);
        processRequest(request, response, notification);
        if (notification) {
            if (notification->method() == "OnlineNotification")
                group_.Deliver(self, notification->to_json().dump(), NOTIFICATION_TYPE_NO_HIS);
            else if (notification->method() == "ContentNotification")
                group_.Deliver(self, notification->to_json().dump(), NOTIFICATION_TYPE_HIS);
        }
        if (response) {
            std::cout << "Response: " << response->to_json().dump() << "\n";
            return response->to_json().dump();
        }
        return "";
    }
}

void Session::Start()
{
    group_.Join(shared_from_this());
    doRead();
}

void Session::doRead()
{
    auto self(shared_from_this());
    const std::string delimiter = "\n";
    boost::asio::async_read_until(
        socket_, streambuf_, delimiter,
        boost::asio::bind_executor(strand_, [this, self, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                auto self = shared_from_this();
                jsonrpcpp::notification_ptr notification(nullptr);
                notification.reset(new jsonrpcpp::Notification("OfflineNotification", jsonrpcpp::Parameter("Who", self->name_)));
                group_.Deliver(self, notification->to_json().dump(), NOTIFICATION_TYPE_NO_HIS);
                group_.Leave(self);
                return;
            }
            std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
            if (!line.empty()) {
                if (line.back() == '\r')
                    line.resize(line.size() - 1);
                if (!line.empty()) {
                    //std::cout << "Received: " << line <<"\n";
                    std::string response = doMessageReceived(line);
                    //For response to client
                    if (!response.empty())
                        Deliver(response);
                }
            }
            streambuf_.consume(bytes_transferred);
            doRead();
        }));
}

void Session::doWrite()
{
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(messages_.front().data(), messages_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                messages_.pop_front();
                if (!messages_.empty())
                    doWrite();
            } else {
                group_.Leave(shared_from_this());
            }
        });
}


void Session::Deliver(const std::string& msg)
{
    bool write_in_progress = !messages_.empty();
    messages_.push_back(msg + "\r\n");
    if (!write_in_progress) {
        doWrite();
    }
}

Server::Server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint)
    : io_context_(io_context), acceptor_(io_context, endpoint)
{
    doAccept();
}

void Server::doAccept()
{
    acceptor_.async_accept(
        [this] (boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(io_context_, std::move(socket), group_)->Start();
            } else {
                std::cout << "Accept error: " << ec.message() << "\n";
            }
            doAccept();
        });
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: ./server <Group port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        std::list<Server> servers;
        for (int i = 1; i < argc; ++i)
        {
            tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
            servers.emplace_back(io_context, endpoint);
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
            io_context.stop();
        });

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "\nException Occurred\n";
    }
    std::cout << "\nWeFish Server terminated.\n";
    return 0;
}


