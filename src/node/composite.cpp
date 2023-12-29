/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "composite.hpp"

#include <iostream>

Composite::Composite(const String name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node) {

}

Composite::~Composite() {
    
}

bool Composite::Add(SharedPtr<Node> node) {
    // if (!m_node_by_name.insert({ node->Name(), node }).second) {
    //     // std::cerr << "Node '" << node->Name() << "' already exists in set of '" << Name() << "'\n";
    //     return false;
    // }

    m_node_by_name[node->Name()] = node;
    node->SetParent(Node::downcasted_shared_from_this<Composite>());
    return true;
}

bool Composite::Remove(const String node_name) {
    auto node_it = m_node_by_name.find(node_name);
    if (node_it == m_node_by_name.end()) {
        return false;
    }

    node_it->second->SetParent(nullptr);
    m_node_by_name.erase(node_it);
    return true;
}

SharedPtr<Node> Composite::MakeCopy(SharedPtr<Node> parent) const {
    auto copy_node = std::make_shared<Composite>(Name());
    copy_node->SetParent(parent ? parent : Parent());
    copy_node->SetSchemaNode(SchemaNode());
    for (auto& [name, node] : m_node_by_name) {
        copy_node->m_node_by_name.emplace(name, node->MakeCopy(copy_node));
    }

    return copy_node;
}

// TODO: Get value from Visitor if it prefere DFS or BFS
void Composite::Accept(Visitor& visitor) {
    for (auto node : m_node_by_name) {
        if (!visitor.visit(node.second)) {
            break;
        }

        node.second->Accept(visitor); // TODO: Czy to dobre? Nie przerwiemy rekurencji, bez mozliwosci zwrocenia true/false
    }
}

SharedPtr<Node> Composite::FindNode(const String node_name) {
    auto node_it = m_node_by_name.find(node_name);
    if (node_it != m_node_by_name.end()) {
        return node_it->second;
    }

    return nullptr;
}

size_t Composite::Count() const {
    return m_node_by_name.size();
}

SchemaComposite::SchemaComposite(const String name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node), SchemaNode(name, parent, schema_node), Composite(name, parent, schema_node) {

}

SchemaComposite::~SchemaComposite() {
    // TODO: Remove reference to this node from childs
}

void SchemaComposite::Accept(Visitor& visitor) {
    // Just for reference, not really needed
    Composite::Accept(visitor);
    SchemaNode::Accept(visitor);
}
