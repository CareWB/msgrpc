#ifndef MSGRPC_UDPCHANNEL_H
#define MSGRPC_UDPCHANNEL_H
#include <ctime>
#include <iostream>
#include <string>
#include <list>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <mutex>

#include <msgrpc/core/typedefs.h>

using boost::asio::ip::udp;

typedef std::function<void(msgrpc::msg_id_t msg_id, const char* msg, size_t len, udp::endpoint sender)> OnMsgFunc;

struct UdpChannel;


#ifdef USE_THREAD_SIMULATE_MSGRPC_PROCESS
thread_local
#endif
UdpChannel* g_msg_channel;

struct UdpChannel {
    UdpChannel(const msgrpc::service_id_t& service_id, OnMsgFunc on_msg_func)
            : service_id_(service_id)
            , io_service_()
            , socket_(io_service_, service_id)
            , on_msg_func_(on_msg_func) {

        start_receive();
        this->send_msg_to_remote("00init", service_id); //00 means leading msgrpc::msg_id_t

        if (g_msg_channel == nullptr) {
            g_msg_channel = this;
        }

        mutex_.lock();
        io_services_.push_back(&io_service_);
        mutex_.unlock();

        io_service_.run();
    }

    void start_receive() {
        socket_.async_receive_from( boost::asio::buffer(recv_buffer_)
                                  , remote_endpoint_
                                  , boost::bind(&UdpChannel::handle_receive
                                  , this, boost::asio::placeholders::error
                                  , boost::asio::placeholders::bytes_transferred));
    }

    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (!error || error == boost::asio::error::message_size) {
            msgrpc::msg_id_t* msg_id = (msgrpc::msg_id_t*)recv_buffer_.data();
            on_msg_func_(*msg_id, (const char*)(msg_id + 1), bytes_transferred - sizeof(msgrpc::msg_id_t), remote_endpoint_);

            start_receive();
        }
    }

    void send_msg_to_sender(const std::string& msg) {
        return send_msg_to_remote(msg, remote_endpoint_);
    }

    void send_msg_to_remote(const std::string& msg, const udp::endpoint& endpoint) {
        socket_.async_send_to(boost::asio::buffer(msg), endpoint,
                              boost::bind( &UdpChannel::handle_send, this, msg
                                      , boost::asio::placeholders::error
                                      , boost::asio::placeholders::bytes_transferred));
    }

    void handle_send(const std::string& /*message*/, const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) { std::cout << "msg send failed." << std::endl; }
    }

    static void close_all_channels() {
        for (auto* s: io_services_) {
            s->stop();
        }
        io_services_.clear();
    }

    static std::list<boost::asio::io_service*> io_services_;
    static std::mutex mutex_;

    const msgrpc::service_id_t& service_id_;
    boost::asio::io_service io_service_;
    udp::socket socket_;
    OnMsgFunc on_msg_func_;

    udp::endpoint remote_endpoint_;
    boost::array<char, 10240> recv_buffer_;
};

std::list<boost::asio::io_service*> UdpChannel::io_services_;
std::mutex UdpChannel::mutex_;

#endif //MSGRPC_UDPCHANNEL_H
