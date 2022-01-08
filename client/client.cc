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
#include "message.h"

using boost::asio::ip::tcp;

typedef std::deque<Message> message_queue;

class Client
{
public:
    Client(boost::asio::io_context& io_context, const tcp::resolver::results_type& endpoints, std::string self_name)
    : io_context_(io_context), socket_(io_context), self_name_(self_name)
    {
        doConnect(endpoints);
    }

    void Write(const Message& msg)
    {
        boost::asio::post(io_context_,
            [this, msg]() {
                bool write_in_progress = !write_msgs_.empty();
                write_msgs_.push_back(msg);
                if (!write_in_progress)
                {
                    doWrite();
                }
            });
    }

    void Close()
    {
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }

private:
    void doConnect(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(socket_, endpoints,
            [this](boost::system::error_code ec, tcp::endpoint)
            {
                if (!ec)
                {
                    doReadHeader();
                }
            });
    }

    void doReadHeader()
    {
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), Message::header_length),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (!ec && read_msg_.decode_header())
                {
                    doReadName();
                } else {
                    socket_.close();
                }
            });
    }

    void doReadName()
    {
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.name(), Message::max_name_length),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    doReadBody();
                } else {
                    socket_.close();
                }
            });
    }

    void doReadBody()
    {
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    char name[Message::max_name_length + 1] = "";
                    memcpy(name, read_msg_.name(), Message::max_name_length);
                    if (std::strstr(name, self_name_.c_str()) == NULL) {
                        std::cout << "\t\t\t";
                        std::cout.write(read_msg_.name(), Message::max_name_length);
                        std::cout << ": ";
                        std::cout.write(read_msg_.body(), read_msg_.body_length());
                        std::cout << "\n";
                    }
                    doReadHeader();
                } else {
                    socket_.close();
                }
            });
    }

    void doWrite()
    {
        boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty())
                    {
                        doWrite();
                    }
                } else {
                    socket_.close();
                }
            });
    }

private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    Message read_msg_;
    message_queue write_msgs_;
    std::string self_name_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: ./client <Group port> <Your chat name>\n";
            return 1;
        }

        char name[Message::max_name_length + 1];
        std::sprintf(name, "%8s", argv[2]);

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", argv[1]);
        Client client(io_context, endpoints, argv[2]);

        std::thread client_thread([&io_context](){ io_context.run(); });

        char line[Message::max_body_length + 1];
        while (std::cin.getline(line, Message::max_body_length + 1))
        {
            Message msg;
            msg.body_length(std::strlen(line));
            std::memcpy(msg.name(), name, Message::max_name_length);
            std::memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            client.Write(msg);
        }

        client.Close();
        client_thread.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}


