/*
 *      WeFish File Server
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#include "fserver.hpp"

#include <csignal>
#include <chrono>
#include <exception>
#include <system_error>
#include <boost/algorithm/string.hpp>


FSession::FSession(boost::asio::io_context& io_context, tcp::socket socket, Group& group)
    : socket_(std::move(socket)), strand_(io_context), group_(group)
{
}

void FSession::processRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification)
{
    try {
        Json result;
        if (request->id().int_id() == MESSAGE_TYPE_SETTING) {
            if (request->method() == "SayHello") {
                group_.Join(shared_from_this());
                account_ = request->params().get("Account");
                result["Method"] = "SayHello";
                result["Account"] = account_;
                response.reset(new jsonrpcpp::Response(*request, result));
            }
        } else if (request->id().int_id() == MESSAGE_TYPE_FILE) {
            if (request->method() == "FileTransfer") {
                int account = request->params().get("Account");
                int to_account = request->params().get("ToAccount");
                std::string filename = request->params().get("Filename");
                int filesize = request->params().get("Filesize");
                std::string checksum = request->params().get("Checksum");
                std::string content = request->params().get("Content");
                int status = request->params().get("Status");

                std::cout << "Account: " << std::to_string(account) << std::endl;
                std::cout << "ToAccount: " << std::to_string(to_account) << std::endl;
                std::cout << "Filename: " << filename << std::endl;
                std::cout << "Filesize: " << std::to_string(filesize) << std::endl;
                std::cout << "Checksum: " << checksum << std::endl;
                std::cout << "Content: " << content << std::endl;
                std::cout << "Status: " << std::to_string(status) << std::endl;
                notification.reset(new jsonrpcpp::Notification("FileTransferNotification",
                    jsonrpcpp::Parameter("Account", request->params().get("Account"), "ToAccount", request->params().get("ToAccount"),
                                        "Status", request->params().get("Status"), "Filename", request->params().get("Filename"),
                                        "Filesize", request->params().get("Filesize"), "Checksum", request->params().get("Checksum"), 
                                        "Content", request->params().get("Content"))));
            }
        } else {
            std::cout << "NULL Process Request\n";
        }
    } catch (const std::exception& e) {
        std::cout << "FServer::onMessageReceived exception: " << e.what() << ", message: " << request->to_json().dump() << "\n";
        response.reset(new jsonrpcpp::InternalErrorException(e.what(), request->id()));
    }
}


std::string FSession::doMessageReceived(const std::string& message)
{
    jsonrpcpp::entity_ptr entity(nullptr);
    try {
        entity = jsonrpcpp::Parser::do_parse(message);
        if (!entity)
            return "";
    } catch (const jsonrpcpp::ParseErrorException& e) {
        std::cout << e.to_json().dump() << "\nCaused by:" << message << "\n";
        return e.to_json().dump();
    } catch (const std::exception& e) {
        std::cout << e.what() << "\nCaused by:" << message << "\n";
        return jsonrpcpp::ParseErrorException(e.what()).to_json().dump();
    }
    jsonrpcpp::entity_ptr response(nullptr);
    jsonrpcpp::notification_ptr notification(nullptr);
    if (entity->is_request()) {
        auto self(shared_from_this());
        jsonrpcpp::request_ptr request = std::dynamic_pointer_cast<jsonrpcpp::Request>(entity);
        processRequest(request, response, notification);
        if (notification) {
            if (notification->method() == "FileTransferNotification") {
                int to_account = notification->params().get("ToAccount");
                
                group_.Deliver(to_account, notification->to_json().dump());
                std::cout << notification->to_json().dump() << std::endl;
            }
        }
        if (response) {
            //std::cout << "Response: " << response->to_json().dump() << "\n";
            return response->to_json().dump();
        }
        return "";
    }
}

void FSession::Start()
{
    file_stream_.open("example.txt", std::ios::out | std::ios::binary);
    if (file_stream_.is_open()) {
        doRead();
    } else {
        std::cerr << "Unable to open file" << std::endl;
    }
}

void FSession::doRead()
{
    auto self(shared_from_this());
    const std::string delimiter = "\n";
    boost::asio::async_read_until(
        socket_, streambuf_, delimiter,
        boost::asio::bind_executor(strand_, [this, self, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Failed to do read\n";
                return;
            }
            std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
            if (!line.empty()) {
                if (line.back() == '\r') {
                    line.resize(line.size() - 1);
                }
                std::string response = doMessageReceived(line);
                if (!response.empty()) {
                    Deliver(response);
                }
            }
            streambuf_.consume(bytes_transferred);
            doRead();
        }));
}

void FSession::doWrite()
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

void FSession::Deliver(const std::string& msg)
{
    bool write_in_progress = !messages_.empty();

    messages_.push_back(msg + "\r\n");
    if (!write_in_progress) {
        doWrite();
    }
}

FServer::FServer(boost::asio::io_context& io_context, const tcp::endpoint& endpoint)
    : io_context_(io_context), acceptor_(io_context, endpoint)
{
    //group_.Join(shared_from_this());
    doAccept();
}

void FServer::doAccept()
{
    acceptor_.async_accept(
        [this] (boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<FSession>(io_context_, std::move(socket), group_)->Start();
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
            std::cerr << "Usage: ./fserver <Group port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        std::list<FServer> fservers;
        for (int i = 1; i < argc; ++i)
        {
            tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
            fservers.emplace_back(io_context, endpoint);
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
        std::cout << e.what();
    }
    std::cout << "\nWeFish File Server terminated.\n";
    return 0;
}


