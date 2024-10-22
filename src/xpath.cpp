/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "xpath.hpp"
#include "node/composite.hpp"
#include "utils.hpp"

#include <iostream>
#include <cstring>

bool XPath::NodeFinder::init(const String wanted_node_name) {
    _wanted_node_name = wanted_node_name;
    _wanted_node = nullptr;
    return true;
}

bool XPath::NodeFinder::visit(SharedPtr<Node> node) {
    if (node->Name() == _wanted_node_name) {
        _wanted_node = node;
        return false; // Stop visitng another nodes
    }

    return true; // Continue visiting other nodes
}

SharedPtr<Node> XPath::NodeFinder::getResult() {
    return _wanted_node;
}

Deque<String> XPath::parse(const String xpath) {
    Deque<String> xpath_items;
    // Returns first token
    char* token = std::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    while (token != nullptr) {
        xpath_items.push_back(token);
        token = std::strtok(nullptr, XPath::SEPARATOR);
    }

    return xpath_items;
}

SharedPtr<Node> XPath::select(SharedPtr<Node> root_node, const String xpath) {
    if (!root_node || xpath.empty()) {
        return nullptr;
    }

    auto xpath_items = parse(xpath);
    auto node_finder = MakeSharedPtr<XPath::NodeFinder>();
    auto visiting_node = root_node;
    while (!xpath_items.empty()) { 
        auto item = xpath_items.front();
        if ((item == "value") && (xpath_items.size() == 1)) {
            return visiting_node;
        }

        auto left_pos = item.find(XPath::SUBSCRIPT_LEFT_PARENTHESIS);
        auto right_pos = item.find(XPath::SUBSCRIPT_RIGHT_PARENTHESIS);
        if (left_pos != StringEnd()
            && right_pos != StringEnd()) {
            xpath_items.pop_front();
            for (auto new_item : { item.substr(0, left_pos), item.substr(left_pos + 1, (right_pos - left_pos - 1)) }) {
                node_finder->init(new_item);
                visiting_node->Accept(*node_finder);
                visiting_node = node_finder->getResult();
                if (!visiting_node) {
                    return nullptr;
                }
            }

            continue;
        }

        node_finder->init(xpath_items.front());
        visiting_node->Accept(*node_finder);
        visiting_node = node_finder->getResult();
        if (!visiting_node) {
            return nullptr;
        }

        xpath_items.pop_front();
    }

    return visiting_node;
}

String XPath::mergeTokens(const Deque<String>& xpath_tokens) {
    String xpath = "/";
    for (auto& token : xpath_tokens) {
        if (xpath.at(xpath.size() - 1) != '/') {
            xpath += "/";
        }

        xpath += token;
    }

    return xpath;
}

String XPath::toString(SharedPtr<Node> node) {
    String xpath;
    Stack<String> xpath_stack;
    auto processing_node = node;

    while (processing_node) {
        xpath_stack.push(processing_node->Name());
        processing_node = processing_node->Parent();
    }

    while (!xpath_stack.empty()) {
        if (xpath_stack.top() != XPath::SEPARATOR) {
            if (xpath_stack.top()[0] != '[') {
                xpath += XPath::SEPARATOR;
            }

            xpath += xpath_stack.top();
        }

        xpath_stack.pop();
    }

    return xpath;
}

struct NodeCounter : public Visitor {
    size_t node_cnt;
    NodeCounter() : node_cnt(0) {}
    virtual ~NodeCounter() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        ++node_cnt;
        return true;
    }
};

// Resolves the xpath based on the node from the same xpath,
// i.e. xpath = '/platform/port/[@item]/breakout-mode' then node
// must be "breakout-mode" to successfully evaluate the node - it
// cannot be i.e. '/interface/ethernet[@item]/auto-negotiation'
String XPath::evaluateXPath(SharedPtr<Node> start_node, String xpath) {
    if (xpath.at(0) != XPath::SEPARATOR[0]) {
        return {};
    }

    Utils::find_and_replace_all(xpath, XPath::ITEM_NAME_SUBSCRIPT, String(XPath::SEPARATOR) + XPath::ITEM_NAME_SUBSCRIPT);
    MultiMap<String, std::size_t> idx_by_xpath_item;
    Vector<String> xpath_items;
    // Returns first token
    char* token = std::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    size_t idx = 0;
    while (token != nullptr) {
        xpath_items.push_back(token);
        idx_by_xpath_item.emplace(token, idx);
        token = strtok(NULL, XPath::SEPARATOR);
        ++idx;
    }

    // Resolve @item key
    auto key_name_pos = idx_by_xpath_item.find(XPath::ITEM_NAME_SUBSCRIPT);
    if (key_name_pos != std::end(idx_by_xpath_item)) {
        // Find last idx
        idx = 0; 
        auto range = idx_by_xpath_item.equal_range(XPath::ITEM_NAME_SUBSCRIPT);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second > idx) {
                idx = it->second;
            }
        }

        String wanted_child = xpath_items.at(idx);
        if (xpath_items.size() > (idx + 1)) {
            wanted_child = xpath_items.at(idx + 1);
        }

        auto curr_node = start_node;
        while ((curr_node->Name() != wanted_child)
                && (curr_node->Parent() != nullptr)) {
            curr_node = curr_node->Parent();
        }

        String resolved_key_name;
        if (curr_node->Name() != wanted_child) {
            curr_node = start_node;
            do {
                if (auto list = std::dynamic_pointer_cast<Composite>(curr_node->Parent())) {
                    resolved_key_name = curr_node->Name();
                    break;
                }

                curr_node = curr_node->Parent();
            } while (curr_node);
        }
        else {
            resolved_key_name = curr_node->Parent()->Name(); // We want to resolve name of parent node based on its child node
        }

        // Resolve all occurences of XPath::ITEM_NAME_SUBSCRIPT
        for (auto it = range.first; it != range.second; ++it) {
            xpath_items.at(it->second) = resolved_key_name;
        }
    }

    String evaluated_xpath;
    for (auto item : xpath_items) {
        evaluated_xpath += XPath::SEPARATOR;
        evaluated_xpath += item;
    }

    return evaluated_xpath;
}
