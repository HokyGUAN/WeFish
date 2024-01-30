/*
 *      WeFish File Server
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#ifndef FSERVER_H
#define FSERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <memory>
#include <unistd.h>
#include <thread>
#include <cstdlib>
#include <deque>
#include <list>
#include <utility>

#include "jsonrpcpp.hpp"
#include "message.hpp"


using boost::asio::ip::tcp;

typedef std::deque<std::string> MessageQueue;

using ResponseHandler = std::function<void(const boost::system::error_code&, std::string&)>;

class Participant
{
public:
    virtual ~Participant() {}
    virtual void Deliver(const std::string& msg) = 0;
    int account_;
};

typedef std::shared_ptr<Participant> Participant_ptr;

class Group
{
public:
    Group(){}

    void Join(Participant_ptr participant)
    {
        participants_.push_back(participant);
    }

    void Leave(Participant_ptr participant)
    {
        participants_.remove(participant);
    }

    void Deliver(Participant_ptr self_ptr, const std::string& msg)
    {
        for (auto participant: participants_)
            if (self_ptr != participant)
                participant->Deliver(msg);
    }

    void Deliver(int to_account, const std::string& msg)
    {
        for (auto participant: participants_)
            if (to_account == participant->account_)
                participant->Deliver(msg);
    }

private:
    std::list<Participant_ptr> participants_;
};

class FSession : public Participant, public std::enable_shared_from_this<FSession>
{
public:
    FSession(boost::asio::io_context& io_context, tcp::socket socket, Group& group);

    void Start();
    void Deliver(const std::string& msg);
    void processRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification);
    std::string doMessageReceived(const std::string& message);
    void doRead();
    void doWrite();

private:
    tcp::socket socket_;
    boost::asio::streambuf streambuf_;
    boost::asio::io_context::strand strand_;
    Group& group_;
    MessageQueue messages_;
    std::string key_;
    std::vector<char> file_content_;
    std::string encoded_content_;
    size_t encoded_size_ = 0;
    size_t encoded_consume_ = 0;
};

class FServer
{
public:
    FServer(boost::asio::io_context& io_context, const tcp::endpoint& endpoint);
    ~FServer() = default;

    void doAccept();

private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    Group group_;
};

#endif
