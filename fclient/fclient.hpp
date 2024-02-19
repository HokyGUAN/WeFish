#ifndef CLIENT_H
#define CLIENT_H
#include <iostream>
#include <fstream>
#include <string>
#include <deque>
#include <mutex>
#include <map>
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
    std::string key_;
    boost::asio::io_context::strand strand_;
    boost::asio::streambuf streambuf_;
    std::deque<std::string> messages_;
};

class FileStream
{
public:
    FileStream(std::string& name);
    ~FileStream() = default;

    void Push(std::string& content);
    std::string Pop(void);
    int Size(void);
    std::string Name(void);

private:
    std::string name_;
    std::string contents_;
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

    void doFileSection(std::string filename, int sequence, std::streampos start, std::streampos sectionSize);

private:
    std::shared_ptr<Connection> connection_;
    std::map<std::string, std::shared_ptr<FileStream>> container_;
    std::shared_ptr<FileStream> upgrade_stream_ = nullptr;
    std::mutex mutex_;
    std::vector<std::string> v_section_;
};


#endif

