#include "leaf.hpp"

#include <iostream>

Leaf::Leaf(const String& name, const Value value, SharedPtr<Node> parent, SharedPtr<Node> schema_node)
: Node(name, parent, schema_node), m_value(value) {

}

void Leaf::setValue(const Value value) {
    m_value = value;
}

Value Leaf::getValue() const {
    return m_value;
}

SharedPtr<Node> Leaf::makeCopy(SharedPtr<Node> parent) const {
    auto copy_node = std::make_shared<Leaf>(getName(), m_value);
    copy_node->setParent(parent ? parent : getParent());
    copy_node->setSchemaNode(getSchemaNode());

    return copy_node;
}

void Leaf::accept(Visitor& visitor) {
    std::clog << getName() << " is a leaf so skip visiting" << std::endl;
}