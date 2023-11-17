#pragma once

#include "node.hpp"
#include "visitor.hpp"
#include "xpath.hpp"

#include "lib/types.hpp"

class SubnodeChildsVisitor : public Visitor {
public:
    virtual ~SubnodeChildsVisitor() = default;
    SubnodeChildsVisitor(const String& parent_name)
        : m_parent_name { parent_name } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (node->getParent()->getName() == m_parent_name)) {
            // TODO: Extend it to check full xpath!
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

class NodeChildsOnlyVisitor : public Visitor {
public:
    virtual ~NodeChildsOnlyVisitor() = default;
    NodeChildsOnlyVisitor(SharedPtr<Node>& parent_node)
        : m_parent_name { XPath::to_string2(parent_node) } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (XPath::to_string2(node->getParent()) == m_parent_name)) {
            // TODO: Extend it to check full xpath!
            // spdlog::info("{} is child of {}", node->getName(), m_parent_name);
            m_child_subnode_names.emplace_front(m_parent_name + "/" + node->getName());
        }

        return true;
    }

    ForwardList<String> getAllSubnodes() { return m_child_subnode_names; }

private:
    String m_parent_name;
    ForwardList<String> m_child_subnode_names;
};