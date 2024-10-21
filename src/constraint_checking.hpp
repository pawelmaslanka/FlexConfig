/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
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
    SharedPtr<Config::Manager>& _config_mngr;
    SharedPtr<Node> _root_config;
};
