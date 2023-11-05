#pragma once

#include "node.hpp"
#include "visitor.hpp"

#include "lib/types.hpp"

class SubnodeChildsVisitor : public Visitor {
public:
    virtual ~SubnodeChildsVisitor() = default;
    SubnodeChildsVisitor(const String& parent_name)
        : m_parent_name { parent_name } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (node->getParent()->getName() == m_parent_name)) {
            // spdlog::info("{} is child of {}", node->getName(), m_parent_name);
            m_child_subnode_names.emplace_front(node->getName());
        }

        return true;
    }

    ForwardList<String> getAllSubnodes() { return m_child_subnode_names; }

private:
    String m_parent_name;
    ForwardList<String> m_child_subnode_names;
};