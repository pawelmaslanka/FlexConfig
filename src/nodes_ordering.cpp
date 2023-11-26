#include "nodes_ordering.hpp"

#include "config.hpp"
#include "visitor.hpp"
#include "node/visitor_spec.hpp"
#include "xpath.hpp"
#include "lib/topo_sort.hpp"
#include "lib/std_types.hpp"

#include <spdlog/spdlog.h>

#include <iostream>

class KeySelectorResolverVisitior : public Visitor {
public:
    virtual ~KeySelectorResolverVisitior() = default;
    KeySelectorResolverVisitior(const String& xpath_to_resolve) {
        m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("[@key]"));
        if ((xpath_to_resolve.find("[@key]") + 6) < xpath_to_resolve.size()) {
            m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("[@key]") + 7);
        }

        spdlog::debug("Fillout all subnodes for xpath {}", m_pre_wildcard);
        spdlog::debug("Left xpath {}", m_post_wildcard);
        m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
    }

    ForwardList<String> getResolvedXpath() { return m_result; }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        if (m_pre_wildcard_tokens.empty()
            || (node->getName() != m_pre_wildcard_tokens.front())) {
            spdlog::debug("Failed to find key selector");
            return true;
        }

        m_pre_wildcard_tokens.pop();
        if (m_pre_wildcard_tokens.empty()) {
            spdlog::debug("We reached leaf token at node {}", node->getName());
            SubnodeChildsVisitor subnode_visitor(node->getName());
            node->accept(subnode_visitor);
            for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                if (!m_post_wildcard.empty()) {
                    resolved_wildcard_xpath += "/" + m_post_wildcard;
                }

                spdlog::debug("Resolved key selector {}", resolved_wildcard_xpath);
                m_result.emplace_front(resolved_wildcard_xpath);
            }
            // Let's break processing
            return false;
        }

        return true;
    }

private:
    Queue<String> m_pre_wildcard_tokens {};
    String m_pre_wildcard {};
    String m_post_wildcard {};
    ForwardList<String> m_result {};
};

class WildcardDependencyResolverVisitior : public Visitor {
public:
    virtual ~WildcardDependencyResolverVisitior() {
        while (!m_pre_wildcard_tokens.empty()) m_pre_wildcard_tokens.pop();
        m_result.clear();
    };

    WildcardDependencyResolverVisitior(const String& xpath_to_resolve) {
        m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("/*"));
        if ((xpath_to_resolve.find("/*") + 2) < xpath_to_resolve.size()) {
            m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("/*") + 3);
        }

        spdlog::debug("Fillout all subnodes for xpath {}", m_pre_wildcard);
        spdlog::debug("Left xpath {}", m_post_wildcard);
        m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
    }

    ForwardList<String> getResolvedXpath() { return m_result; }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }

        if (m_pre_wildcard_tokens.empty()
            || (node->getName() != m_pre_wildcard_tokens.front())) {
            spdlog::debug("Failed to resolve wildcard");
            return true;
        }

        m_pre_wildcard_tokens.pop();
        if (m_pre_wildcard_tokens.empty()) {
            spdlog::debug("We reached leaf token at node {}", node->getName());
            SubnodeChildsVisitor subnode_visitor(node->getName());
            node->accept(subnode_visitor);
            for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                if (!m_post_wildcard.empty()) {
                    resolved_wildcard_xpath += "/" + m_post_wildcard;
                }

                spdlog::debug("Resolved wildcard {}", resolved_wildcard_xpath);
                m_result.emplace_front(resolved_wildcard_xpath);
            }
            // Let's break processing
            return false;
        }

        return true;
    }

private:
    Queue<String> m_pre_wildcard_tokens {};
    String m_pre_wildcard {};
    String m_post_wildcard {};
    ForwardList<String> m_result {};
};

// TODO: Write next another version of DependencyGetterVisitor class to load all nodes
class DependencyGetterVisitior2 : public Visitor {
public:
    virtual ~DependencyGetterVisitior2() = default;
    DependencyGetterVisitior2(SharedPtr<Config::Manager>& config_mngr)
        : m_config_mngr { config_mngr }, m_dependencies { std::make_shared<Map<String, Set<String>>>() } {}
    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null");
            return false;
        }
        else {
            // spdlog::debug("Visiting node: {}", node->getName());
        }

        // TODO: XPath::to_string2(node->getSchemaNode())
        auto xpath = XPath::to_string2(node);
        spdlog::debug("Get xpath: {}", xpath);
        auto schema_node = m_config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::debug("Does not have schema");
            return true;
        }

        auto update_depend_attrs = schema_node->findAttr("update-dependencies");
        if (update_depend_attrs.empty()) {
            spdlog::debug("{} does not have dependencies", xpath);
            auto xpath_schema_node = XPath::to_string2(schema_node);
            if (m_dependencies->find(xpath_schema_node) == m_dependencies->end()) {
                // Just create empty set
                (*m_dependencies)[xpath_schema_node].clear();
            }

            return true;
        }

        for (auto& dep : update_depend_attrs) {
            auto xpath_schema_node = XPath::to_string2(schema_node);
            (*m_dependencies)[xpath_schema_node].emplace(dep);
        }

        return true;
    }

    SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

private:
    SharedPtr<Map<String, Set<String>>> m_dependencies;
    SharedPtr<Config::Manager> m_config_mngr;
};

class DependencyMapperVisitior : public Visitor {
public:
    virtual ~DependencyMapperVisitior() = default;
    DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : m_set_of_ordered_cmds { set_of_ordered_cmds } {}
    virtual bool visit(SharedPtr<Node> node) override {
        auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
        if (!schema_node) {
            return true;
        }

        auto schema_node_xpath = XPath::to_string2(schema_node);
        if (m_set_of_ordered_cmds.find(schema_node_xpath) != m_set_of_ordered_cmds.end()) {
            m_ordered_nodes[schema_node_xpath].emplace_front(node);
        }
        else if (m_ordered_nodes.find(XPath::to_string2(schema_node->getParent())) != m_ordered_nodes.end()) {
            m_ordered_nodes[XPath::to_string2(schema_node->getParent())].emplace_back(node);
        }
        else {
            m_unordered_nodes[schema_node_xpath].emplace_back(node);
        }

        return true;
    }

    Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
    Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

private:
    Set<String> m_set_of_ordered_cmds {};
    Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
    Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
};

NodeDependencyManager::NodeDependencyManager(SharedPtr<Config::Manager>& config_mngr)
: m_config_mngr { config_mngr } {
}

bool NodeDependencyManager::resolve(const SharedPtr<Node>& config, List<String>& ordered_nodes_by_xpath) {
    DependencyGetterVisitior2 depVisitor(m_config_mngr);
    spdlog::debug("Looking for dependencies");
    spdlog::debug("m_config_mngr: {}, running_config: {}", m_config_mngr != nullptr, config != nullptr);
    config->accept(depVisitor);
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

        for (auto dep : schema_node.second) {
            if (dep.find("/*") != String::npos) {
                spdlog::debug("Found wildcard in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = WildcardDependencyResolverVisitior(dep);
                config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto resolved_wildcard : resolved_xpath) {
                        spdlog::debug("Put resolved wildcard {}", resolved_wildcard);
                        (*dependencies)[schema_node.first].insert(resolved_wildcard);
                    }

                    continue;
                }
            }
            else if (dep.find("[@key]") != String::npos) {
                spdlog::debug("Found [@key] in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = KeySelectorResolverVisitior(dep);
                config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto& resolved_wildcard : resolved_xpath) {
                        spdlog::debug("Put resolved [@key] {}", resolved_wildcard);
                        (*dependencies)[schema_node.first].insert(resolved_wildcard);
                    }

                    continue;
                }
            }

            (*dependencies)[schema_node.first].insert(dep);
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
    config->accept(depMapperVisitor);
    std::list<SharedPtr<Node>> nodes_to_execute {};
    spdlog::debug("Sorting unordered nodes");
    for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
        spdlog::debug("Schema: {}", schema_name);
        spdlog::debug("Nodes: ");
        for (auto& n : unordered_nodes) {
            // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
            nodes_to_execute.emplace_back(n);
            spdlog::debug("{}", n->getName());
        }
    }

    spdlog::debug("Sorting ordered nodes");
    for (auto& ordered_cmd : ordered_cmds) {
        spdlog::debug("Schema: {}", ordered_cmd);
        spdlog::debug("Nodes:");
        auto ordered_nodes = depMapperVisitor.getOrderedNodes();
        for (auto& n : ordered_nodes[ordered_cmd]) {
            nodes_to_execute.emplace_back(n);
            spdlog::debug("{}", n->getName());
        }
    }

    ordered_nodes_by_xpath = ordered_cmds;
    return true;
}