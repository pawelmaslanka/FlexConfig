/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "node.hpp"
#include "visitor.hpp"
#include "xpath.hpp"

#include "lib/std_types.hpp"

class SubnodeChildsVisitor : public Visitor {
public:
    virtual ~SubnodeChildsVisitor() = default;
    SubnodeChildsVisitor(const String& parent_name)
        : m_parent_name { parent_name } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (node->Parent()->Name() == m_parent_name)) {
            // TODO: Extend it to check full xpath!
            m_child_subnode_names.emplace_front(node->Name());
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
        : m_parent_name { XPath::to_string(parent_node) } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (XPath::to_string(node->Parent()) == m_parent_name)) {
            // TODO: Extend it to check full xpath!
            m_child_subnode_names.emplace_front(m_parent_name + "/" + node->Name());
        }

        return true;
    }

    ForwardList<String> getAllSubnodes() { return m_child_subnode_names; }

private:
    String m_parent_name;
    ForwardList<String> m_child_subnode_names;
};
