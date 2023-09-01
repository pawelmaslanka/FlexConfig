#include "composite.hpp"

#include <iostream>

Composite::Composite(const String name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node) {

}

Composite::~Composite() {
    
}

bool Composite::add(SharedPtr<Node> node) {
    if (!m_node_by_name.insert({ node->getName(), node }).second) {
        return false;
    }

    node->setParent(Node::downcasted_shared_from_this<Composite>());
    return true;
}

bool Composite::remove(const String node_name) {
    auto node_it = m_node_by_name.find(node_name);
    if (node_it == m_node_by_name.end()) {
        return false;
    }

    node_it->second->setParent(nullptr);
    m_node_by_name.erase(node_it);
    return true;
}

void Composite::accept(Visitor& visitor) {
    for (auto node : m_node_by_name) {
        if (!visitor.visit(node.second)) {
            break;
        }
    }
}

SharedPtr<Node> Composite::findNode(const String node_name) {
    auto node_it = m_node_by_name.find(node_name);
    if (node_it != m_node_by_name.end()) {
        return node_it->second;
    }

    return nullptr;
}

size_t Composite::count() const {
    return m_node_by_name.size();
}

SchemaComposite::SchemaComposite(const String name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node), SchemaNode(name, parent, schema_node), Composite(name, parent, schema_node) {

}

SchemaComposite::~SchemaComposite() {
    // TODO: Remove reference to this node from childs
}

void SchemaComposite::accept(Visitor& visitor) {
    // Just for reference, not really needed
    Composite::accept(visitor);
    SchemaNode::accept(visitor);
}
