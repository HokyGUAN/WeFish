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
#include <list>
#include <utility>

#include "jsonrpcpp.hpp"
#include "message.hpp"
#include "cryptor.h"



using boost::asio::ip::tcp;

typedef std::deque<std::string> MessageQueue;

using ResponseHandler = std::function<void(const boost::system::error_code&, std::string&)>;

class Participant
{
public:
    virtual ~Participant() {}
    virtual void Deliver(const std::string& msg) = 0;
    int IDinGroup_;
    std::string name_;
    std::string icon_;
};

typedef std::shared_ptr<Participant> Participant_ptr;

class Group
{
public:
    Group():available_id_(0){}

    void Join(Participant_ptr participant)
    {
        participants_.push_back(participant);
    }

    void Flush(Participant_ptr participant)
    {
        for (auto msg: recentMessages_)
            participant->Deliver(msg);
    }

    void Leave(Participant_ptr participant)
    {
        participants_.remove(participant);
    }

    //Include Self
    void Deliver(const std::string& msg)
    {
        recentMessages_.push_back(msg);
        while (recentMessages_.size() > max_recent_msgs)
            recentMessages_.pop_front();

        for (auto participant: participants_)
            participant->Deliver(msg);
    }
    //Exclude Self
    void Deliver(Participant_ptr self_ptr, const std::string& msg, NotificationType type)
    {
        if (type == NOTIFICATION_TYPE_HIS)
            recentMessages_.push_back(msg);
        while (recentMessages_.size() > max_recent_msgs)
            recentMessages_.pop_front();

        for (auto participant: participants_)
            if (self_ptr != participant)
                participant->Deliver(msg);
    }
    //Specify
    void Deliver(int to_id, const std::string& msg)
    {
        for (auto participant: participants_)
            if (to_id == participant->IDinGroup_)
                participant->Deliver(msg);
    }

    int GetAvailableID()
    {
        return ++available_id_;
    }

    std::string GetActiveList()
    {
        std::string liststr;
        for (auto participant: participants_)
            liststr += "#W#F#" + std::to_string(participant->IDinGroup_) + "-W-F-" + participant->name_ + "-W-F-" + participant->icon_ + "#W#F#";
        return liststr;
    }
private:
    std::list<Participant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    MessageQueue recentMessages_;
    int available_id_;
};

class Session : public Participant, public std::enable_shared_from_this<Session>
{
public:
    Session(boost::asio::io_context& io_context, tcp::socket socket, Group& group);

    void Start();
    void Deliver(const std::string& msg);
    void processRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification);
    std::string doMessageReceived(const std::string& message);
    void doRead();
    void doWrite();

    //int IDinGroup_;
    //std::string name_;
private:
    tcp::socket socket_;
    boost::asio::streambuf streambuf_;
    boost::asio::io_context::strand strand_;
    Group& group_;
    MessageQueue messages_;
    std::string key_;
    //std::shared_ptr<Cryptor> cryptor_;
};

class Server
{
public:
    Server(boost::asio::io_context& io_context, const tcp::endpoint& endpoint);
    ~Server() = default;

    void doAccept();

private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    Group group_;
};

#endif
