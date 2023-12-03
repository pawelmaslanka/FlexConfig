#include "session_management.hpp"

SessionManagement::SessionManagement(SharedPtr<Config::Manager> config_mngr, SharedPtr<ConnectionManagement::Server> conn_mngt)
: m_config_mngr { config_mngr }, m_conn_mngt { conn_mngt } {

}