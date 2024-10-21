/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "lib/std_types.hpp"

#include "node.hpp"

namespace XPath {
    static auto constexpr ITEM_NAME = "@item";
    static auto constexpr ITEM_NAME_SUBSCRIPT = "[@item]";
    static auto constexpr REFERENCE_TOKEN = "@";
    static auto constexpr SUBSCRIPT_LEFT_PARENTHESIS = "[";
    static auto constexpr SUBSCRIPT_RIGHT_PARENTHESIS = "]";
    static auto constexpr SEPARATOR = "/";

    class NodeFinder : public Visitor {
      public:
        virtual ~NodeFinder() = default;
        virtual bool visit(SharedPtr<Node> node) override;

        bool init(const String wanted_node_name);
        SharedPtr<Node> getResult();

      private:
        SharedPtr<Node> _wanted_node;
        String _wanted_node_name;
    };

    Deque<String> parse(const String xpath);
    SharedPtr<Node> select(SharedPtr<Node> root_node, const String xpath);
    String mergeTokens(const Deque<String>& xpath_tokens);
    String toString(SharedPtr<Node> node);
    String evaluateXPath(SharedPtr<Node> start_node, String xpath);
};
