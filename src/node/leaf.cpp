/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "leaf.hpp"

#include <iostream>

Leaf::Leaf(const String& name, const Value value, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
: Node(name, parent, schema_node), m_value(value) {

}

void Leaf::SetValue(const Value value) {
    m_value = value;
}

Value Leaf::getValue() const {
    return m_value;
}

SharedPtr<Node> Leaf::MakeCopy(SharedPtr<Node> parent) const {
    auto copy_node = std::make_shared<Leaf>(Name(), m_value);
    copy_node->SetParent(parent ? parent : Parent());
    copy_node->SetSchemaNode(SchemaNode());

    return copy_node;
}

void Leaf::Accept(Visitor& visitor) {

}