#ifndef PROJECT_ZK_SERVICE_REGISTER_H
#define PROJECT_ZK_SERVICE_REGISTER_H

#include <zookeeper/zookeeper.h>
#include <msgrpc/core/adapter/service_register.h>
#include <cstdlib>
#include <iostream>
#include <msgrpc/util/singleton.h>
#include <conservator/ConservatorFrameworkFactory.h>

namespace demo {
    void close_zk_connection_at_exit();

    struct ZkServiceRegister : msgrpc::ServiceRegister, msgrpc::Singleton<ZkServiceRegister> {

        bool zk_is_connected(const unique_ptr<ConservatorFramework>& zk) const {
            return zk && (zk->getState() == ZOO_CONNECTED_STATE);
        }

        bool assure_zk_is_connected() {
            if (zk_is_connected(zk_)) {
                return true;
            }

            if ( ! zk_) {
                std::atexit(close_zk_connection_at_exit);
            }

            //TODO: read zk server address from configuration
            auto zk = ConservatorFrameworkFactory().newClient("localhost:2181");
            zk->start();

            bool is_connected = zk_is_connected(zk);
            if (is_connected) {
                zk_ = std::move(zk);
            }

            std::cout << "(zk_->getState() ==  ZOO_CONNECTED_STATE) ? " << is_connected << std::endl;

            return is_connected;
        }

        virtual bool register_service(const char* service_name, const char *end_point) override {
            if (service_name == nullptr || end_point == nullptr) {
                return false;
            }

            if ( ! assure_zk_is_connected()) {
                std::cout << "[ERROR] connection to zk failed" << std::endl;
                return false;
            }

            string services = "/services";

            int ret;
            ret = zk_->create()->forPath(services);
            ret = zk_->create()->forPath(services + "/" + service_name);

            string result_path;
            ret = zk_->create()->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)->forPath(services + "/" + service_name + "/instance-", end_point, result_path);

            cout << "register result ZOK == ret: " << (ZOK == ret) << endl;
            cout << "result_path: " << result_path << endl;

            return ZOK == ret;
        }

        unique_ptr<ConservatorFramework> zk_;
    };

    void close_zk_connection_at_exit() {
        auto& zk = ZkServiceRegister::instance().zk_;

        if (zk)
            zk->close();
    }
}

#endif //PROJECT_ZK_SERVICE_REGISTER_H