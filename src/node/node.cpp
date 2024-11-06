/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "node.hpp"
#include "leaf.hpp"

#include <iostream>

const Set<String> SchemaNode::ATTR_NAME = {
    "update-depends",
    "delete-depends",
    "update-constraints",
    "delete-constraints",
    "type",
    "@key",
    "array",
    "min",
    "max",
    "description",
    "pattern-name",
    "pattern-value",
    "default"
};

const Set<String> SchemaNode::TYPE_NAME = {
    "bool",
    "number",
    "string",
    "array",
    "container",
    "object",
    "leaf"
};

Node::Node(const String& name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : _name { name }, _parent { parent }, _schema_node { schema_node } {

}

Node::~Node() {
    _parent.reset();
    _schema_node.reset();
}

String Node::Name() const {
    return _name;
}

void Node::SetParent(SharedPtr<Node> parent) {
    _parent = parent;
}

SharedPtr<Node> Node::Parent() const {
    return _parent;
}

void Node::SetSchemaNode(SharedPtr<Node> schema_node) {
    _schema_node = schema_node;
}

SharedPtr<Node> Node::SchemaNode() const {
    return _schema_node;
}

SharedPtr<Node> Node::MakeCopy(SharedPtr<Node> parent) const {
    auto copy_node = MakeSharedPtr<Node>(_name);
    copy_node->_parent = parent ? parent : _parent;
    copy_node->_schema_node = _schema_node;
    return copy_node;
}

void Node::Accept(Visitor& visitor) {
    visitor.visit(shared_from_this());
}

SchemaNode::SchemaNode(const String& name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node) {

}

SchemaNode::~SchemaNode() {
    // ~Node() will remove all references 
}

void SchemaNode::AddAttr(const String& attr_name, const String& attr_val) {
    _attr_by_name[attr_name].emplace_front(attr_val);
}

ForwardList<String> SchemaNode::FindAttr(const String& attr_name) {
    auto attr_it = _attr_by_name.find(attr_name);
    if (attr_it != _attr_by_name.end()) {
        return attr_it->second;
    }

    return {};
}

void SchemaNode::Accept(Visitor& visitor) {
    for (auto& [attr, value] : _attr_by_name) {
        for (auto& val : value) {
            std::clog << attr << " -> " << val << std::endl;
        }
    }

    Node::Accept(visitor);
}
