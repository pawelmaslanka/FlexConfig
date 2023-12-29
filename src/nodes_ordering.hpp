/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
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
