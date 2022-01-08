/*
 *         WeFish Server
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#include "server.h"

#include <csignal>
#include <chrono>
#include <exception>
#include <system_error>


Session::Session(tcp :: socket socket, Group & group) : socket_(std::move(socket)), group_(group)
{
}

void Session::Start()
{
    group_.Join(shared_from_this());
    doReadHeader();
}

void Session::Deliver(const Message& msg)
{
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
        doWrite();
    }
}

Server::Server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint) : acceptor_(io_context, endpoint)
{
    doAccept();
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
        std::cerr << "Exception: " << e.what() << "\n";
    }
    std::cout << "WeFish Server terminated.\n";
    return 0;
}


