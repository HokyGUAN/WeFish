#ifndef CLIENT_H
#define CLIENT_H
#include <iostream>
#include <string>
#include <deque>
#include <boost/asio.hpp>

#include "jsonrpcpp.hpp"
#include "message.hpp"

using boost::asio::ip::tcp;

using ResponseHandler = std::function<void(const boost::system::error_code&, std::string&)>;


class Connection : public std::enable_shared_from_this<Connection>
{
public: 
    Connection(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints);
    ~Connection() = default;

    void doConnect(const ResponseHandler& handler);
    void doDisconnect();
    void doRead(const ResponseHandler& handler);
    void SendAsync(const std::string& msg);
    void doWrite();

private:
    boost::asio::io_context& io_context_;
    tcp::resolver::results_type& endpoints_;
    tcp::socket socket_;
    boost::asio::io_context::strand strand_;
    boost::asio::streambuf streambuf_;
    std::deque<std::string> messages_;
};


class Client
{
public:
    Client(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints, std::string self_name);
    ~Client() = default;

    void Start();
    void Stop();
    void doMessageReceived();
    void Send(const std::string& msg);
    std::string GetName();
    bool GetProcessStatus() { return isProcessing_; }
    
private:
    bool isProcessing_;
    std::shared_ptr<Connection> connection_;
    std::string self_name_;
};


#endif
