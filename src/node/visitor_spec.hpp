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
        : _parent_name { parent_name } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (node->Parent()->Name() == _parent_name)) {
            // TODO: Extend it to check full xpath!
            _child_subnode_names.emplace_front(node->Name());
        }

        return true;
    }

    ForwardList<String> getAllSubnodes() { return _child_subnode_names; }

private:
    String _parent_name;
    ForwardList<String> _child_subnode_names;
};

class NodeChildsOnlyVisitor : public Visitor {
public:
    virtual ~NodeChildsOnlyVisitor() = default;
    NodeChildsOnlyVisitor(SharedPtr<Node>& parent_node)
        : _parent_name { XPath::toString(parent_node) } {}

    virtual bool visit(SharedPtr<Node> node) override {
        if (node &&
            (XPath::toString(node->Parent()) == _parent_name)) {
            // TODO: Extend it to check full xpath!
            _child_subnode_names.emplace_front(_parent_name + "/" + node->Name());
        }

        return true;
    }

    ForwardList<String> getAllSubnodes() { return _child_subnode_names; }

private:
    String _parent_name;
    ForwardList<String> _child_subnode_names;
};
