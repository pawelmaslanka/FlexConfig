#pragma once

#include "config.hpp"
#include "node/node.hpp"

#include <memory>

class ConstraintChecker : public std::enable_shared_from_this<ConstraintChecker> {
public:
    ConstraintChecker() = delete;
    ConstraintChecker(SharedPtr<Config::Manager>& config_mngr, SharedPtr<Node>& root_config);

    bool validate(SharedPtr<Node>& node_to_validate, const String& constraint_definition);

private:
    SharedPtr<Config::Manager>& m_config_mngr;
    SharedPtr<Node> m_root_config;
};