#ifndef PROJECT_ZK_SERVICE_REGISTER_H
#define PROJECT_ZK_SERVICE_REGISTER_H

#include <zookeeper/zookeeper.h>
#include <msgrpc/core/adapter/service_register.h>
#include <msgrpc/core/schedule/task_run_on_main_queue.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

#include <msgrpc/util/singleton.h>
#include <conservator/ConservatorFrameworkFactory.h>

//TODO: split into .h and .cpp
namespace demo {
    void close_zk_connection_at_exit();

    const string k_services_root = "/services";

    /////////////////////////////////////////////////////////////////////
    struct InstanceInfo {
        msgrpc::service_id_t service_id_;
    };

    typedef std::vector<InstanceInfo> instance_vector_t;
    typedef ConservatorFramework ZKHandle;

    /////////////////////////////////////////////////////////////////////
    namespace {
        boost::optional<msgrpc::service_id_t> str_to_service_id(const string& endpoint) {
            size_t sep = endpoint.find(":");
            if (sep == string::npos) {
                return boost::none;
            }

            string ip = string(endpoint, 0, sep);
            unsigned short port = (unsigned short)strtoul(endpoint.c_str() + sep + 1, NULL, 0);

            return msgrpc::service_id_t(boost::asio::ip::address::from_string(ip), port);
        }

        instance_vector_t instance_vector_from(const vector<string>& service_instances) {
            instance_vector_t iv;
            for (auto& si : service_instances) {
                boost::optional<msgrpc::service_id_t> service_id = str_to_service_id(si);

                if (service_id) {
                    InstanceInfo ii;
                    ii.service_id_ = service_id.value();

                    iv.push_back(ii);
                }
            }

            return iv;
        }
    }

    struct ZkServiceRegister : msgrpc::ServiceRegister, msgrpc::Singleton<ZkServiceRegister> {
        bool is_zk_connected(const unique_ptr<ZKHandle> &zk) const {
            return zk && (zk->getState() == ZOO_CONNECTED_STATE);
        }

        bool try_connect_zk() {
            if (is_zk_connected(zk_)) {
                return true;
            }

            if ( ! zk_) {
                std::atexit(close_zk_connection_at_exit);
            }

            //TODO: read zk server address from configuration
            auto zk = ConservatorFrameworkFactory().newClient("localhost:2181");
            try {
                zk->start();
            } catch (const char* msg) {
                zk->close();
                cout << "[ERROR] catched exception: " << msg << endl;
                return false;
            }

            bool is_connected = is_zk_connected(zk);
            if (is_connected) {
                zk_ = std::move(zk);
            }

            return is_connected;
        }

        void wait_zk_is_connected() {
            bool connected;

            do {
                if ( ! (connected = try_connect_zk())) {
                    std::cout << "[ERROR] connection to zk failed" << std::endl;
                }
            } while( ! connected);
        }

        //TODO: peroidically test existence of ephemeral node of service, and create the ephemeral node if need.
        //      after macbook sleep, and after sleep the ephemeral nodes were disappeared.
        //      should re-register into zk, if zk's data was accidentally deleted.

        //TODO: should periodically fetch services/instances info from zk,
        //      incase miss notifications between handling watch notification and set watch again.

        //TODO: when connected to zk, and restart zk server, then we should able to reconnect to zk,
        //      instead of continuous print out "ZOO_ERROR@handle_socket_error_msg@1746: \
        //                                       Socket [::1:2181] zk retcode=-4, errno=64(Host is down): \
        //                                       failed while receiving a server response"
        //      test shutdown zk, wait 30 minutes, and restart zk, see if we'll connected again.


        bool create_ephemeral_node_for_service_instance(const char* service_name, const char* version, const char *end_point) {
            int ret;
            ret = zk_->create()->forPath(k_services_root);
            ret = zk_->create()->forPath(k_services_root + "/" + service_name);

            const clientid_t* session_id = zoo_client_id(zk_->handle());
            if (session_id == nullptr) {
                return false;
            }

            string path = k_services_root + "/" + service_name + "/" + end_point + "@" + std::to_string(session_id->client_id) + "@" + version;
            ret = zk_->create()->withFlags(ZOO_EPHEMERAL)->forPath(path, end_point);

            bool result = (ZOK == ret || ZNODEEXISTS == ret);
            if (!result) {
                cout << "register service on zk failed: zk_reuslt: " << ret << " for path:" << path << endl;
            }

            cout << "result_path: " << path << endl;
            return result;
        }

        static void service_child_watcher_fn(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
            if (type == ZOO_SESSION_EVENT) {
                cout << "got ZOO_SESSION_EVENT" << endl;
                //TODO: handle loss connection,  set state to loss_connection
                //TODO: handle reconnected to zk event, and reset watcher on services/instances znodes.
                return;
            }

            cout << "get child watcher function called, state: " << state << endl;

            ZKHandle* zk = (ZKHandle *)watcherCtx;

            if (type == ZOO_CHILD_EVENT) {
                cout << "got ZOO_CHILD_EVENT: path: " << path << endl;
                if (k_services_root == path) {
                    //TODO: get list of /services and update services_cache_
                }
            }

            zk->getChildren()->withWatcher(service_child_watcher_fn, zk)->forPath(path);
        }


        bool fetch_services_from_zk() {
            wait_zk_is_connected();

            vector<string> services = zk_->getChildren()->withWatcher(service_child_watcher_fn, zk_.get())->forPath(k_services_root);

            for (auto& service : services) {
                std::cout << "service list: " << service << std::endl;

                vector<string> service_instances = zk_->getChildren()->withWatcher(service_child_watcher_fn, zk_.get())->forPath(k_services_root + "/" + service);

                for (auto& service_instance : service_instances) {
                    std::cout << "    instance list: " << service_instance << std::endl;
                }

                services_cache_[service] = instance_vector_from(service_instances);
            }

            return true;
        }

        virtual bool init() override {
            //TODO: fetch server list for first time and start watch /services node
            return fetch_services_from_zk();
            //            msgrpc::Task::schedule_run_on_main_queue(
            //                    []{
            //                        std::cout << "get init service and instances from zk, and set watcher for changes." << std::endl;
            //                    }
            //            );
        }

        virtual bool register_service(const char* service_name, const char* version, const char *end_point) override {
            if (service_name == nullptr || end_point == nullptr) {
                return false;
            }

            wait_zk_is_connected();

            return create_ephemeral_node_for_service_instance(service_name, version, end_point);
        }


        virtual boost::optional<msgrpc::service_id_t> service_name_to_id(const char* service_name, const char* req, size_t req_len) override {
            //TODO: refactor to only read cached_ services
            wait_zk_is_connected();

            vector<string> children = zk_->getChildren()->forPath(k_services_root + "/" + service_name);
            for (auto child : children) {
                std::cout << "service instance: " << child << std::endl;
            }

            //TODO: save fetched service list into local service_discovery cache
            //TODO: first add auto register msg handler then schedule msg to self's msg queue.
            //TODO: 1. read services_cache_ and answer request of service_name_to_id directly
            //TODO: 2. populate services_cache_ at process init, and during handling of zookeeper watcher notifications.
            msgrpc::Task::schedule_run_on_main_queue(
                    []{
                        std::cout << "struct HandleZkNotification : TaskRunOnMainQueue, run_task()." << std::endl;
                      }
            );

            if (children.empty()) {
                return boost::none;
            }

            //TODO: select which service to route
            return str_to_service_id(children[0]);
        }

        ////////////////////////////////////////////////////////////////////////
        std::map<string, instance_vector_t> services_cache_;

        ////////////////////////////////////////////////////////////////////////
        unique_ptr<ZKHandle> zk_;
    };

    void close_zk_connection_at_exit() {
        if (ZkServiceRegister::instance().zk_)
            ZkServiceRegister::instance().zk_->close();
    }
}

#endif //PROJECT_ZK_SERVICE_REGISTER_H
