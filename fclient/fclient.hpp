#ifndef CLIENT_H
#define CLIENT_H
#include <iostream>
#include <fstream>
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

class FContainer
{
public:
    FContainer() {};
    ~FContainer() = default;

    void Push(std::string& content);
    std::string Pop(void);
    int Size(void);

private:
    std::string name_;
    std::string container_;
};

class FClient
{
public:
    FClient(boost::asio::io_context& io_context, tcp::resolver::results_type& endpoints);
    ~FClient() = default;

    void Start();
    void Stop();
    void doMessageReceived();
    void Send(const std::string& msg);
    void SendFile(std::string& filename);

private:
    std::shared_ptr<Connection> connection_;
    std::ofstream file_stream_;
    std::map<std::string, std::shared_ptr<FContainer>> m_file_container_;
};


#endif

