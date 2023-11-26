#include "node.hpp"
#include "leaf.hpp"

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

Node::Node(const String& name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : m_name { name }, m_parent { parent }, m_schema_node { schema_node } {

}

Node::~Node() {
    
}

String Node::Name() const {
    return m_name;
}

void Node::SetParent(SharedPtr<Node> parent) {
    m_parent = parent;
}

SharedPtr<Node> Node::Parent() const {
    return m_parent;
}

void Node::SetSchemaNode(SharedPtr<Node> schema_node) {
    m_schema_node = schema_node;
}

SharedPtr<Node> Node::SchemaNode() const {
    return m_schema_node;
}

SharedPtr<Node> Node::MakeCopy(SharedPtr<Node> parent) const {
    auto copy_node = std::make_shared<Node>(m_name);
    copy_node->m_parent = parent ? parent : m_parent;
    copy_node->m_schema_node = m_schema_node;
    return copy_node;
}

void Node::Accept(Visitor& visitor) {
    visitor.visit(shared_from_this());
}

SchemaNode::SchemaNode(const String& name, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
 : Node(name, parent, schema_node) {

}

SchemaNode::~SchemaNode() {

}

void SchemaNode::AddAttr(const String& attr_name, const String& attr_val) {
    m_attr_by_name[attr_name].emplace_front(attr_val);
}

ForwardList<String> SchemaNode::FindAttr(const String& attr_name) {
    auto attr_it = m_attr_by_name.find(attr_name);
    if (attr_it != m_attr_by_name.end()) {
        return attr_it->second;
    }

    return {};
}

void SchemaNode::Accept(Visitor& visitor) {
    for (auto& [attr, value] : m_attr_by_name) {
        for (auto& val : value) {
            std::clog << attr << " -> " << val << std::endl;
        }
    }

    Node::Accept(visitor);
}
