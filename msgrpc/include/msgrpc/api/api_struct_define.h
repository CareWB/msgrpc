//#ifndef MSGRPC_SERVICE_API_DEFINE_H_H
//#define MSGRPC_SERVICE_API_DEFINE_H_H

#include <msgrpc/thrift/thrift_struct_define.h>
#include <msgrpc/core/adapter/config.h>
#include <msgrpc/core/adapter/service_register.h>
#include <cassert>

////////////////////////////////////////////////////////////////////////////////
#ifdef ___def_service
#undef ___def_service
#endif

#define ___def_service(service_name_, version_)                                                              \
namespace service_name_ {                                                                                    \
    const char k_name[] = #service_name_;                                                                    \
    const char k_version[] = #version_;                                                                      \
    void msgrpc_register_service(const char* endpoint) {                                                     \
        assert(msgrpc::Config::instance().service_register_ != nullptr);                                     \
        msgrpc::Config::instance().service_register_->register_service(#service_name_, #version_, endpoint); \
    }                                                                                                        \
}                                                                                                            \
namespace service_name_

//#endif //MSGRPC_SERVICE_API_DEFINE_H_H

////////////////////////////////////////////////////////////////////////////////
//during struct_declaration, do_nothing for interface related macros.
#ifdef ___as_interface
#undef ___as_interface
#endif

#define ___as_interface(...)
////////////////////////////////////////////////////////////////////////////////