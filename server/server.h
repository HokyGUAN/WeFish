/*
 *         WeFish Server
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <unistd.h>
#include <thread>
#include <cstdlib>
#include <deque>
#include <list>
#include <memory>
#include <set>
#include <utility>

#include "message.h"


using boost::asio::ip::tcp;

typedef std::deque<Message> message_queue;

class Participant
{
public:
    virtual ~Participant() {}
    virtual void Deliver(const Message& msg) = 0;
};

typedef std::shared_ptr<Participant> Participant_ptr;

class Group
{
public:
    void Join(Participant_ptr participant)
    {
        participants_.insert(participant);
        for (auto msg: recent_msgs_)
            participant->Deliver(msg);
    }

    void Leave(Participant_ptr participant)
    {
        participants_.erase(participant);
    }

    void Deliver(const Message& msg)
    {
        recent_msgs_.push_back(msg);
        while (recent_msgs_.size() > max_recent_msgs)
            recent_msgs_.pop_front();

        for (auto participant: participants_)
            participant->Deliver(msg);
    }

private:
    std::set<Participant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    message_queue recent_msgs_;
};


class Session : public Participant, public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Group& group);

    void Start();
    void Deliver(const Message& msg);

private:
    void doReadHeader()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), Message::header_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec && read_msg_.decode_header())
                {
                    doReadName();
                } else {
                    group_.Leave(shared_from_this());
                }
            });
    }

    void doReadName()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.name(), Message::max_name_length),
            [this, self](boost::system::error_code ec, std::size_t length)
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
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    group_.Deliver(read_msg_);
                    doReadHeader();
                }
                else
                {
                    group_.Leave(shared_from_this());
                }
            });
    }

    void doWrite()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty())
                    {
                      doWrite();
                    }
                }
                else
                {
                    group_.Leave(shared_from_this());
                }
            });
    }

    tcp::socket socket_;
    Group& group_;
    Message read_msg_;
    message_queue write_msgs_;
};

class Server
{
public:
	Server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint);

private:
    void doAccept()
    {
        acceptor_.async_accept(
            [this] (boost::system::error_code ec, tcp::socket socket) {
                if (!ec)
                {
                    std::make_shared<Session>(std::move(socket), group_)->Start();
                } else {
                    std::cout << "Accept error: " << ec.message() << "\n";
                }

                doAccept();
            });
    }

    tcp::acceptor acceptor_;
    Group group_;
};

#endif
