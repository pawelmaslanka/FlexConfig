/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "nodes_ordering.hpp"

#include "config.hpp"
#include "visitor.hpp"
#include "node/visitor_spec.hpp"
#include "xpath.hpp"
#include "lib/topo_sort.hpp"
#include "lib/std_types.hpp"

#include <spdlog/spdlog.h>

#include <iostream>

static const auto REFERENCE_XPATH_NODE = String(XPath::SEPARATOR) + XPath::REFERENCE_TOKEN;
static const auto WILDCARD_XPATH_NODE = String(XPath::SEPARATOR) + XPath::WILDCARD_TOKEN;

class NodeReferenceResolverVisitior : public Visitor {
public:
    virtual ~NodeReferenceResolverVisitior() = default;
    NodeReferenceResolverVisitior(const String& xpath_to_resolve) {
        _pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find(REFERENCE_XPATH_NODE));
        if ((xpath_to_resolve.find(REFERENCE_XPATH_NODE) + 1) < xpath_to_resolve.size()) {
            _post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find(REFERENCE_XPATH_NODE) + 2);
        }

        spdlog::debug("Fillout all subnodes for xpath {}", _pre_wildcard);
        spdlog::debug("Left xpath {}", _post_wildcard);
        _pre_wildcard_tokens = XPath::parse(_pre_wildcard);
    }

    Optional<String> getEvaluatedXpath() { return _result; }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        if (_pre_wildcard_tokens.empty()
            || (node->Name() != _pre_wildcard_tokens.front())) {
            spdlog::debug("Failed to find key selector");
            return true;
        }

        _pre_wildcard_tokens.pop_front();
        if (_pre_wildcard_tokens.empty()) {
            spdlog::debug("We reached leaf token at node {}", node->Name());
            SubnodeChildsVisitor subnode_visitor(node->Name());
            node->Accept(subnode_visitor);
            for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                String resolved_wildcard_xpath = _pre_wildcard + XPath::SEPARATOR + subnode;
                if (!_post_wildcard.empty()) {
                    if ((resolved_wildcard_xpath.size() > 1)
                        && (resolved_wildcard_xpath.at(resolved_wildcard_xpath.size() - 1) != Config::XPATH_NODE_SEPARATOR[0])) {
                        if (_post_wildcard.at(0) != Config::XPATH_NODE_SEPARATOR[0]) {
                            resolved_wildcard_xpath += Config::XPATH_NODE_SEPARATOR;
                        }
                    }

                    resolved_wildcard_xpath += _post_wildcard;
                }

                spdlog::debug("Resolved node reference {}", resolved_wildcard_xpath);
                _result = resolved_wildcard_xpath;
                break;
            }
            // Let's break processing
            return false;
        }

        return true;
    }

private:
    Deque<String> _pre_wildcard_tokens {};
    String _pre_wildcard {};
    String _post_wildcard {};
    Optional<String> _result {};
};

class KeySelectorResolverVisitior : public Visitor {
public:
    virtual ~KeySelectorResolverVisitior() = default;
    KeySelectorResolverVisitior(const String& xpath_to_resolve) {
        _pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find(XPath::ITEM_NAME_SUBSCRIPT));
        if ((xpath_to_resolve.find(XPath::ITEM_NAME_SUBSCRIPT) + 6) < xpath_to_resolve.size()) {
            _post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find(XPath::ITEM_NAME_SUBSCRIPT) + 7);
        }

        spdlog::debug("Fillout all subnodes for xpath {}", _pre_wildcard);
        spdlog::debug("Left xpath {}", _post_wildcard);
        _pre_wildcard_tokens = XPath::parse(_pre_wildcard);
    }

    ForwardList<String> getEvaluatedXpath() { return _result; }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        if (_pre_wildcard_tokens.empty()
            || (node->Name() != _pre_wildcard_tokens.front())) {
            spdlog::debug("Failed to find key selector");
            return true;
        }

        _pre_wildcard_tokens.pop_front();
        if (_pre_wildcard_tokens.empty()) {
            spdlog::debug("We reached leaf token at node {}", node->Name());
            SubnodeChildsVisitor subnode_visitor(node->Name());
            node->Accept(subnode_visitor);
            for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                String resolved_wildcard_xpath = _pre_wildcard + XPath::SEPARATOR + subnode;
                if (!_post_wildcard.empty()) {
                    if ((resolved_wildcard_xpath.size() > 1)
                        && (resolved_wildcard_xpath.at(resolved_wildcard_xpath.size() - 1) != Config::XPATH_NODE_SEPARATOR[0])) {
                        if (_post_wildcard.at(0) != Config::XPATH_NODE_SEPARATOR[0]) {
                            resolved_wildcard_xpath += Config::XPATH_NODE_SEPARATOR;
                        }
                    }

                    resolved_wildcard_xpath += _post_wildcard;
                }

                spdlog::debug("Resolved key selector {}", resolved_wildcard_xpath);
                _result.emplace_front(resolved_wildcard_xpath);
            }
            // Let's break processing
            return false;
        }

        return true;
    }

private:
    Deque<String> _pre_wildcard_tokens {};
    String _pre_wildcard {};
    String _post_wildcard {};
    ForwardList<String> _result {};
};

class WildcardDependencyResolverVisitior : public Visitor {
public:
    virtual ~WildcardDependencyResolverVisitior() {
        while (!_pre_wildcard_tokens.empty()) _pre_wildcard_tokens.pop_front();
        _result.clear();
    };

    WildcardDependencyResolverVisitior(const String& xpath_to_resolve) {
        _pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find(WILDCARD_XPATH_NODE));
        if ((xpath_to_resolve.find(WILDCARD_XPATH_NODE) + 2) < xpath_to_resolve.size()) {
            _post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find(WILDCARD_XPATH_NODE) + 3);
        }

        spdlog::debug("Fillout all subnodes for xpath {}", _pre_wildcard);
        spdlog::debug("Left xpath {}", _post_wildcard);
        _pre_wildcard_tokens = XPath::parse(_pre_wildcard);
    }

    ForwardList<String> getEvaluatedXpath() { return _result; }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        if (_pre_wildcard_tokens.empty()
            || (node->Name() != _pre_wildcard_tokens.front())) {
            spdlog::debug("Failed to resolve wildcard");
            return true;
        }

        _pre_wildcard_tokens.pop_front();
        if (_pre_wildcard_tokens.empty()) {
            spdlog::debug("We reached leaf token at node {}", node->Name());
            SubnodeChildsVisitor subnode_visitor(node->Name());
            node->Accept(subnode_visitor);
            for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                String resolved_wildcard_xpath = _pre_wildcard + XPath::SEPARATOR + subnode;
                if (!_post_wildcard.empty()) {
                    resolved_wildcard_xpath += XPath::SEPARATOR + _post_wildcard;
                }

                spdlog::debug("Resolved wildcard {}", resolved_wildcard_xpath);
                _result.emplace_front(resolved_wildcard_xpath);
            }
            // Let's break processing
            return false;
        }

        return true;
    }

private:
    Deque<String> _pre_wildcard_tokens {};
    String _pre_wildcard {};
    String _post_wildcard {};
    ForwardList<String> _result {};
};

// TODO: Write next another version of DependencyGetterVisitor class to load all nodes
class DependencyGetterVisitior : public Visitor {
public:
    virtual ~DependencyGetterVisitior() = default;
    DependencyGetterVisitior(SharedPtr<Config::Manager>& config_mngr)
        : _config_mngr { config_mngr }, _dependencies { std::make_shared<Map<String, Set<String>>>() } {}
    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        auto xpath = XPath::toString(node);
        spdlog::debug("Get xpath: {}", xpath);
        auto schema_node = _config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::debug("Does not have schema");
            return true;
        }

        auto update_depend_attrs = schema_node->FindAttr(Config::PropertyName::UPDATE_DEPENDENCIES);
        if (update_depend_attrs.empty()) {
            spdlog::debug("{} does not have dependencies", xpath);
            auto xpath_schema_node = XPath::toString(schema_node);
            if (_dependencies->find(xpath_schema_node) == _dependencies->end()) {
                // Just create empty set
                (*_dependencies)[xpath_schema_node].clear();
            }

            return true;
        }

        for (auto& dep : update_depend_attrs) {
            auto xpath_schema_node = XPath::toString(schema_node);
            (*_dependencies)[xpath_schema_node].emplace(dep);
        }

        return true;
    }

    SharedPtr<Map<String, Set<String>>> getDependencies() { return _dependencies; }

private:
    SharedPtr<Map<String, Set<String>>> _dependencies;
    SharedPtr<Config::Manager> _config_mngr;
};

class DependencyMapperVisitior : public Visitor {
public:
    virtual ~DependencyMapperVisitior() = default;
    DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : _set_of_ordered_cmds { set_of_ordered_cmds } {}
    virtual bool visit(SharedPtr<Node> node) override {
        auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->SchemaNode());
        if (!schema_node) {
            return true;
        }

        auto schema_node_xpath = XPath::toString(schema_node);
        if (_set_of_ordered_cmds.find(schema_node_xpath) != _set_of_ordered_cmds.end()) {
            _ordered_nodes[schema_node_xpath].emplace_front(node);
        }
        else if (_ordered_nodes.find(XPath::toString(schema_node->Parent())) != _ordered_nodes.end()) {
            _ordered_nodes[XPath::toString(schema_node->Parent())].emplace_back(node);
        }
        else {
            _unordered_nodes[schema_node_xpath].emplace_back(node);
        }

        return true;
    }

    Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return _ordered_nodes; }
    Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return _unordered_nodes; }

private:
    Set<String> _set_of_ordered_cmds {};
    Map<String, std::list<SharedPtr<Node>>> _ordered_nodes {};
    Map<String, std::list<SharedPtr<Node>>> _unordered_nodes {};
};

NodeDependencyManager::NodeDependencyManager(SharedPtr<Config::Manager>& config_mngr)
: _config_mngr { config_mngr } {
}

bool NodeDependencyManager::resolve(const SharedPtr<Node>& config, List<String>& ordered_nodes_by_xpath) {
    DependencyGetterVisitior depVisitor(_config_mngr);
    spdlog::debug("Looking for dependencies");
    spdlog::debug("m_config_mngr: {}, running_config: {}", _config_mngr != nullptr, config != nullptr);
    config->Accept(depVisitor);
    spdlog::debug("Completed looking for dependencies");

    auto dependencies = std::make_shared<Map<String, Set<String>>>();
    auto without_dependencies = std::make_shared<Map<String, Set<String>>>();
    auto not_resolved_dependencies = depVisitor.getDependencies();
    for (auto schema_node : *not_resolved_dependencies) {
        spdlog::debug("Processing node: {}", schema_node.first);
        if (schema_node.second.empty()) {
            // Leave node to just load all nodes
            (*without_dependencies)[schema_node.first].clear();
        }

        for (const auto& dep : schema_node.second) {
            String xpath_to_evaluate = dep;
            bool is_xpath_evaluated = false;
            if (xpath_to_evaluate.find(WILDCARD_XPATH_NODE) != String::npos) {
                spdlog::debug("Found wildcard in dependency {} for node {}", xpath_to_evaluate, schema_node.first);
                auto wildcard_resolver = WildcardDependencyResolverVisitior(xpath_to_evaluate);
                config->Accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getEvaluatedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto resolved_wildcard : resolved_xpath) {
                        spdlog::debug("Checking evaluated wildcard '{}'", resolved_wildcard);
                        if ((resolved_wildcard.find(XPath::ITEM_NAME_SUBSCRIPT) == StringEnd())
                            && (resolved_wildcard.find(REFERENCE_XPATH_NODE) == StringEnd())) {
                            spdlog::debug("Put resolved wildcard {}", resolved_wildcard);
                            (*dependencies)[schema_node.first].insert(resolved_wildcard);
                            is_xpath_evaluated = true;
                            continue;
                        }

                        if (resolved_wildcard.find(XPath::ITEM_NAME_SUBSCRIPT) != StringEnd()) {
                            spdlog::debug("Found complex [@item] in dependency {} for node {}", resolved_wildcard, schema_node.first);
                            auto wildcard_resolver2 = KeySelectorResolverVisitior(resolved_wildcard);
                            config->Accept(wildcard_resolver2);
                            auto resolved_xpath2 = wildcard_resolver2.getEvaluatedXpath();
                            if (!resolved_xpath2.empty()) {
                                for (auto& resolved_wildcard2 : resolved_xpath2) {
                                    if (resolved_wildcard2.find(REFERENCE_XPATH_NODE) == StringEnd()) {
                                        spdlog::debug("Put resolved complex [@item] {}", resolved_wildcard2);
                                        (*dependencies)[schema_node.first].insert(resolved_wildcard2);
                                        is_xpath_evaluated = true;
                                    }
                                }
                            }
                        }

                        if (resolved_wildcard.find(REFERENCE_XPATH_NODE) != StringEnd()) {
                            spdlog::debug("Found complex  node reference @ in dependency {} for node {}", resolved_wildcard, schema_node.first);
                            spdlog::debug("Start at node '{}'", config->Name());
                            auto wildcard_resolver3 = NodeReferenceResolverVisitior(resolved_wildcard);
                            config->Accept(wildcard_resolver3);
                            auto evaluated_xpath = wildcard_resolver3.getEvaluatedXpath();
                            if (evaluated_xpath.has_value()) {
                                spdlog::debug("Put resolved complex  node reference @ {}", evaluated_xpath.value());
                                (*dependencies)[schema_node.first].insert(evaluated_xpath.value());
                                is_xpath_evaluated = true;
                            }
                        }
                    }
                }
                else {
                    spdlog::debug("Not resolved any wildcard in dependency {} for node {}", xpath_to_evaluate, schema_node.first);
                }

                if (is_xpath_evaluated) {
                    continue;
                }
            }
            
            if (xpath_to_evaluate.find(XPath::ITEM_NAME_SUBSCRIPT) != String::npos) {
                spdlog::debug("Found [@item] in dependency {} for node {}", xpath_to_evaluate, schema_node.first);
                auto wildcard_resolver = KeySelectorResolverVisitior(xpath_to_evaluate);
                config->Accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getEvaluatedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto& resolved_wildcard : resolved_xpath) {
                        if (xpath_to_evaluate.find(REFERENCE_XPATH_NODE) == StringEnd()) {
                            spdlog::debug("Put resolved [@item] {}", resolved_wildcard);
                            (*dependencies)[schema_node.first].insert(resolved_wildcard);
                            xpath_to_evaluate = resolved_wildcard;
                            is_xpath_evaluated = true;
                        }
                    }
                }
            }
            
            if (xpath_to_evaluate.find(REFERENCE_XPATH_NODE) != StringEnd()) {
                spdlog::debug("Found node reference @ in dependency {} for node {}", xpath_to_evaluate, schema_node.first);
                spdlog::debug("Start at node '{}'", config->Name());
                auto wildcard_resolver = NodeReferenceResolverVisitior(xpath_to_evaluate);
                config->Accept(wildcard_resolver);
                auto evaluated_xpath = wildcard_resolver.getEvaluatedXpath();
                if (evaluated_xpath.has_value()) {
                    spdlog::debug("Put resolved node reference @ {}", evaluated_xpath.value());
                    (*dependencies)[schema_node.first].insert(evaluated_xpath.value());
                    xpath_to_evaluate = evaluated_xpath.value();
                    is_xpath_evaluated = true;
                }
            }

            if (!is_xpath_evaluated) {
                (*dependencies)[schema_node.first].insert(dep);
            }
        }
    }

    // TODO: If it is starting load config, then merge both list: without_dependencies and dependencies
    if (1) {
        dependencies->insert(without_dependencies->begin(), without_dependencies->end());
    }

    std::list<String> ordered_cmds;
    auto sort_result = run_update_op(dependencies, ordered_cmds);
    Set<String> set_of_ordered_cmds;
    for (auto& cmd : ordered_cmds) {
        set_of_ordered_cmds.emplace(cmd);
    }

    DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
    config->Accept(depMapperVisitor);
    std::list<SharedPtr<Node>> nodes_to_execute {};
    spdlog::debug("Sorting unordered nodes");
    for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
        spdlog::debug("Schema: {}", schema_name);
        spdlog::debug("Nodes: ");
        for (auto& n : unordered_nodes) {
            // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
            nodes_to_execute.emplace_back(n);
            spdlog::debug("{}", n->Name());
        }
    }

    spdlog::debug("Sorting ordered nodes");
    for (auto& ordered_cmd : ordered_cmds) {
        spdlog::debug("Schema: {}", ordered_cmd);
        spdlog::debug("Nodes:");
        auto ordered_nodes = depMapperVisitor.getOrderedNodes();
        for (auto& n : ordered_nodes[ordered_cmd]) {
            nodes_to_execute.emplace_back(n);
            spdlog::debug("{}", n->Name());
        }
    }

    ordered_nodes_by_xpath = ordered_cmds;
    return true;
}
