#ifndef MSGRPC_TYPEDEFS_H
#define MSGRPC_TYPEDEFS_H

#include <boost/asio/ip/udp.hpp>

namespace msgrpc {
    typedef unsigned short msg_id_t;
    typedef long long timeout_len_t;

    typedef boost::asio::ip::udp::endpoint service_id_t;
    //typedef unsigned short service_id_t; //TODO: how to deal with different service id types
}

#endif //MSGRPC_TYPEDEFS_H
