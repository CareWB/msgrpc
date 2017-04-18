#ifndef MSGRPC_SET_TIMER_HANDLER_H
#define MSGRPC_SET_TIMER_HANDLER_H

#include <test/core/adapter/simple_timer_adapter.h>

#include <cassert>
#include <thread>

#include <msgrpc/util/singleton.h>
#include <test/test_util/UdpChannel.h>
#include <test/core/adapter/udp_msg_channel.h>

namespace demo {
    struct SetTimerHandler : msgrpc::ThreadLocalSingleton<SetTimerHandler> {
        void set_timer(const char* msg, size_t len) {
            static unsigned short entry_times = 0;
            ++entry_times;

            assert(msg != nullptr && len == sizeof(timer_info));
            timer_info& ti = *(timer_info*)msg;

            unsigned short temp_udp_port = timer_service_id + entry_times;

            std::thread timer_thread([ti, temp_udp_port]{
                msgrpc::Config::instance().init_with( &UdpMsgChannel::instance()
                                                    , &SimpleTimerAdapter::instance()
                                                    , k_msgrpc_request_msg_id
                                                    , k_msgrpc_response_msg_id
                                                    , k_msgrpc_set_timer_msg
                                                    , k_msgrpc_timeout_msg);

                UdpChannel channel(temp_udp_port,
                                   [ti](msgrpc::msg_id_t msg_id, const char* msg, size_t len) {
                                       if (0 == strcmp(msg, "init")) {
                                           std::this_thread::sleep_for(std::chrono::milliseconds(ti.millionseconds_));
                                           msgrpc::Config::instance().msg_channel_->send_msg(ti.service_id_, k_msgrpc_timeout_msg, (const char*)&ti, sizeof(ti));
                                       }
                                   }
                );
            });
            timer_thread.detach();
        }
    };
}


#endif //MSGRPC_SET_TIMER_HANDLER_H
