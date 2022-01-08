#ifndef CLIENT_H
#define CLIENT_H
#include <iostream>
#include <string>
#include <deque>
#include <boost/asio.hpp>


using boost::asio::ip::tcp;

using ResponseHandler = std::function<void(const boost::system::error_code&, std::string&)>;

class Connection {
public: 
    using ResultHandler = std::function<void(const boost::system::error_code&)>;

    Connection(boost::asio::io_context& io_context);
    virtual ~Connection();

    void restart();
    void connect(const ResultHandler& handler);
    void disconnect();
    void send(const std::string message, const ResultHandler& handler);
    void getResponse(const ResponseHandler& handler);

private:
    boost::asio::io_context& io_context_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::io_context::strand strand_;
    boost::asio::streambuf streambuf_;
};

class Client {
public:
    Client(boost::asio::io_context& io_context);
    ~Client();

    void Start();
    void Restart();
    void Stop();
    void Send(std::string json_str);

private:
    void worker();
    void reconnect();
    void getResponseMessage();

    std::unique_ptr<Connection> Connection_;
    boost::asio::io_context& io_context_;
    boost::asio::steady_timer timer_;
};

#endif
