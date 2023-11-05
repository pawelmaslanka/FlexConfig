#pragma once

#include "config.hpp"
#include "node/node.hpp"

class NodeDependencyManager {
public:
    NodeDependencyManager() = delete;
    NodeDependencyManager(SharedPtr<Config::Manager>& config_mngr);
    bool resolve(const SharedPtr<Node>& config, List<String>& ordered_nodes_by_xpath);

private:
    SharedPtr<Config::Manager> m_config_mngr;
};
