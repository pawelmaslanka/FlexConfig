#include "node.hpp"

#include <iostream>

const Set<String> SchemaNode::ATTR_NAME = {
    "update-depends",
    "delete-depends",
    "update-constraints",
    "delete-constraints",
    "type",
    "@item",
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

Node::Node(const String& name, SharedPtr<Node> parent)
 : m_name(name), m_parent(parent) {

}

Node::~Node() {
    
}

String Node::getName() {
    return m_name;
}

void Node::setParent(SharedPtr<Node> parent) {
    m_parent = parent;
}

SharedPtr<Node> Node::getParent() {
    return m_parent;
}

void Node::accept(Visitor& visitor) {
    return;
}

Leaf::Leaf(const String& name, SharedPtr<Node> parent, const String value)
: Node(name, parent), m_value { value } {

}

void Leaf::setValue(const String value) {
    m_value = value;
}

String Leaf::getValue() const {
    return m_value;
}

SchemaNode::SchemaNode(const String& name, SharedPtr<Node> parent)
 : Node(name, parent) {

}

SchemaNode::~SchemaNode() {

}

void SchemaNode::addAttr(const String attr_name, const String attr_val) {
    m_attr_by_name[attr_name] = attr_val;
}

String SchemaNode::findAttr(const String attr_name) {
    auto attr_it = m_attr_by_name.find(attr_name);
    if (attr_it != m_attr_by_name.end()) {
        return attr_it->second;
    }

    return {};
}

void SchemaNode::accept(Visitor& visitor) {
    std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Members for node " << getName() << ":\n";
    for (auto& [attr, value] : m_attr_by_name) {
        std::clog << attr << " -> " << value << std::endl;
    }
}
