#pragma

#include "config.hpp"
#include "connection_management.hpp"

/*
    Manages client session and transaction.
    Client start a session and SessionManagement start counter. If client won't confirm its changes in 
    5 minutes then his changes are withdrawing.
*/
class SessionManagement {
public:
    SessionManagement(SharedPtr<Config::Manager> config_mngr, SharedPtr<ConnectionManagement::Server> conn_mngt);

private:
    SharedPtr<Config::Manager> m_config_mngr;
    SharedPtr<ConnectionManagement::Server> m_conn_mngt;
};