#include "client.h"

#include <boost/array.hpp>
#include <csignal>
#include <chrono>
#include <exception>
#include <system_error>

Connection::Connection(boost::asio::io_context& io_context)
    : io_context_(io_context), resolver_(io_context_), socket_(io_context_), strand_(io_context_) {
}

Connection::~Connection()
{
    disconnect();
}

void Connection::restart() {
    std::cout << "Disconnecting\n";
    if (!socket_.is_open())
    {
        std::cout << "Not connected\n";
        return;
    }
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_.cancel();
    socket_.close(ignored_ec);
    std::cout << "Disconnected\n";
}

void Connection::connect(const ResultHandler& handler)
{
    tcp::resolver::query query("127.0.0.1", std::to_string(52323), boost::asio::ip::resolver_query_base::numeric_service);
    boost::system::error_code ec;
    auto iterator = resolver_.resolve(query, ec);
    if (ec)
    {
        std::cout << "Failed to resolve host '" << "127.0.0.1" << "', error: " << ec.message() << "\n";
        handler(ec);
        return;
    }

    std::cout << "Connecting\n";
    socket_.connect(*iterator, ec);
    if (ec)
    {
        std::cout << "Failed to connect to host '" << "127.0.0.1" << "', error: " << ec.message() << "\n";
        handler(ec);
        return;
    }
    std::cout << "Connected to " << socket_.remote_endpoint().address().to_string() << "\n";
    handler(ec);
}

void Connection::disconnect()
{
    std::cout << "Disconnecting\n";
    if (!socket_.is_open())
    {
        std::cout << "Not connected\n";
        return;
    }
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
    {
        std::cout << " Error in socket shutdown: " << ec.message() << std::endl;
    }
    socket_.close(ec);
    if (ec)
        std::cout << "Error in socket close: " << ec.message() << std::endl;
    std::cout << "Disconnected\n";
}

void Connection::send(const std::string message, const ResultHandler& handler)
{
    boost::asio::async_write(socket_, boost::asio::buffer(message), boost::asio::bind_executor(strand_, [this, handler](boost::system::error_code ec, std::size_t length) {
                                if (ec)
                                    std::cout << "Failed to send message, error: " << ec.message() << "\n";
                                else
                                    std::cout << "Wrote " << length << " bytes to socket\n";

                                if (handler)
                                    handler(ec);
                             }));
}

void Connection::getResponse(const ResponseHandler& handler)
{
    const std::string delimiter = "\n";
        boost::asio::async_read_until(
            socket_, streambuf_, delimiter,
            boost::asio::bind_executor(strand_, [this, handler, delimiter](const boost::system::error_code& ec, std::size_t length) {
                if (ec)
                {
                    std::cout << "Error while reading from control socket: " << ec.message() << "\n";
                    if (handler) {
                        std::string ret = "Socket Error Occurred";
                        handler(ec, ret);
                    }
                    return;
                }
                // Extract up to the first delimiter.
                std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + length - delimiter.length()};
                if (!line.empty())
                {
                    if (line.back() == '\r')
                        line.resize(line.size() - 1);
                    if (!line.empty())
                    {
                        //std::cout << "GetNextMsg read " << "received: " << line << "\n";
                        if (handler)
                            handler(ec, line);
                    }
                }
                streambuf_.consume(length);
                getResponse(handler);
        }));

}


Client::Client(boost::asio::io_context& io_context)
    : io_context_(io_context), timer_(io_context) {
}

Client::~Client() = default;

void Client::getResponseMessage()
{
    Connection_->getResponse([this](const boost::system::error_code& ec, std::string &response) {
        if (ec)
        {
            std::cout << "Failed to getNextMessage: " << response << "\n";
            return;
        } else {
            std::cout << "GetResponseMsg: " << response << "\n";
            
        }
    });
}


void Client::Send(std::string json_str)
{
    Connection_->send(json_str, [this](const boost::system::error_code& ec) mutable {
        if (ec)
        {
            std::cout << "Failed to send hello request, error: " << ec.message() << "\n";
            return;
        }
    });
}

void Client::Start()
{
    Connection_ = std::make_unique<Connection>(io_context_);
    worker();
}

void Client::Restart()
{
    Stop();
    worker();
}

void Client::Stop()
{
    timer_.cancel();
    Connection_->disconnect();
}

void Client::reconnect()
{
    Stop();
    timer_.expires_after(std::chrono::seconds(2));
    timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec)
        {
            worker();
        }
    });
}

void Client::worker()
{
    Connection_->connect([this](const boost::system::error_code& ec) {
        if (!ec)
        {
            std::cout << "Connected!\n";

            // Start receiver loop
            getResponseMessage();
        } else {
            std::cout << "Error: " << ec.message() << "\n";
            reconnect();
        }
    });
}


int main(int argc, char* argv[])
{
    bool success = true;
    try {
        boost::asio::io_context io_context;
        Client client(io_context);

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

        client.Start();

        io_context.run();
    }
    catch (const std::exception& e)
    {
        success = false;
    }
    std::cout << "Client terminated.\n";
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
