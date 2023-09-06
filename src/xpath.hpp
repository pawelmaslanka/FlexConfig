#pragma once

#include "types.hpp"

#include "node.hpp"

namespace XPath {
    static auto constexpr KEY_NAME = "@key";
    static auto constexpr KEY_NAME_SUBSCRIPT = "[@key]";
    static auto constexpr SUBSCRIPT_LEFT_PARENTHESIS = "[";
    static auto constexpr SUBSCRIPT_RIGHT_PARENTHESIS = "]";
    static auto constexpr SEPARATOR = "/";

    class NodeFinder : public Visitor {
      public:
        virtual ~NodeFinder() = default;
        virtual bool visit(SharedPtr<Node> node) override;

        bool init(const String wanted_node_name);
        SharedPtr<Node> get_result();

      private:
        SharedPtr<Node> _wanted_node;
        String _wanted_node_name;
    };

    SharedPtr<Queue<String>> parse(const String xpath);
    SharedPtr<Node> select(SharedPtr<Node> root_node, const String xpath);
    String to_string(SharedPtr<Node> node);
    String to_string2(SharedPtr<Node> node);
    size_t count_members(SharedPtr<Node> root_node, const String xpath);
    SharedPtr<Node> get_root(SharedPtr<Node> start_node);
    String evaluate_xpath(SharedPtr<Node> start_node, String xpath);
    String evaluate_xpath2(SharedPtr<Node> start_node, String xpath);
};
