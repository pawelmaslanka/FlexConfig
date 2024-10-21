/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "node.hpp"
#include "value.hpp"
#include "visitor.hpp"

#include <lib/multiinherit_shared.hpp>
#include <lib/std_types.hpp>

class Leaf : virtual public Node, public inheritable_enable_shared_from_this<Leaf> {
public:
    virtual ~Leaf() = default;
    Leaf(const String& name, const Value value, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    void SetValue(const Value value);
    Value getValue() const;
    virtual SharedPtr<Node> MakeCopy(SharedPtr<Node> parent = nullptr) const override;
    virtual void Accept(Visitor& visitor) override;

private:
    Value _value;
};
