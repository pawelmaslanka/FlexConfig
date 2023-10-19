#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include <spdlog/spdlog.h>

#include "node/node.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"
#include "config.hpp"
#include "expr_eval.hpp"
#include "lib/topo_sort.hpp"
#include "xpath.hpp"

class VisitorImpl : public Visitor {
  public:
    virtual ~VisitorImpl() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
        // node->accept(*this);
        return true;
    }
};

class VisitorGE1Speed : public Visitor {
  public:
    virtual ~VisitorGE1Speed() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
        if (node->getName() == "speed") {
            m_speed_node = node;
            return false;
        }

        return true;
    }

    SharedPtr<Node> getSpeedNode() { return m_speed_node; }

private:
    SharedPtr<Node> m_speed_node;
};

int main(int argc, char* argv[]) {
    auto registry = std::make_shared<RegistryClass>();
    auto config_mngr = std::make_shared<Config::Manager>("../config_test.json", "../schema_test.json", registry);
    std::shared_ptr<Node> root_config;
    if (!config_mngr->load(root_config)) {
        spdlog::error("Failed to load config file");
        ::exit(EXIT_FAILURE);
    }

    VisitorImpl visitor;
    root_config->accept(visitor);

    VisitorGE1Speed visit_ge1_speed;
    root_config->accept(visit_ge1_speed);
    if (!visit_ge1_speed.getSpeedNode()) {
        std::cerr << "Not found speed node!\n";
        exit(EXIT_FAILURE);
    }
    else {
        spdlog::info("Found interface speed");
    }

    auto ee = std::make_shared<ExprEval>();
    ee->init(visit_ge1_speed.getSpeedNode());
    if (ee->evaluate("#if #xpath</interface/gigabit-ethernet[@key]/speed/value> #equal #string<400G> #then #must<#regex_match<@value, #regex<^(400G)$>>>")) {
        std::cout << "Successfully evaluated expression\n";
    }
    else {
        std::cerr << "Failed to evaluate expression\n";
    }

    // spdlog::info("Diff to empty JSON: {}",
    // config_mngr->getSchemaByXPath(XPath::to_string2(visit_ge1_speed.getSpeedNode())));
    // return 0;

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
                // spdlog::info("Visiting node: {}", node->getName());
            }

            // TODO: XPath::to_string2(node->getSchemaNode())
            auto xpath = XPath::to_string2(node);
            spdlog::info("Get xpath: {}", xpath);
            auto schema_node = m_config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::info("Does not have schema");
                return true;
            }

            auto update_depend_attrs = schema_node->findAttr("update-dependencies");
            if (update_depend_attrs.empty()) {
                spdlog::info("{} does not have dependencies", xpath);
                auto xpath_schema_node = XPath::to_string2(schema_node);
                if (m_dependencies->find(xpath_schema_node) == m_dependencies->end()) {
                    // Just create empty set
                    (*m_dependencies)[xpath_schema_node].clear();
                }

                return true;
            }

            for (auto& dep : update_depend_attrs) {
                // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
                // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
                auto xpath_schema_node = XPath::to_string2(schema_node);
                (*m_dependencies)[xpath_schema_node].emplace(dep);
                std::clog << "Node " << node->getName() << " with schema " << xpath_schema_node << " depends on " << dep << std::endl;
            }

            // if (std::dynamic_pointer_cast<Leaf>(node)) {
            //     return false; // Terminate visiting on Leaf object
            // }

            return true;
        }

        SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

    private:
        SharedPtr<Map<String, Set<String>>> m_dependencies;
        SharedPtr<Config::Manager> m_config_mngr;
    };

    DependencyGetterVisitior2 depVisitor(config_mngr);
    spdlog::info("Looking for dependencies");
    root_config->accept(depVisitor);
    spdlog::info("Completed looking for dependencies");
            class SubnodeChildsVisitor : public Visitor {
        public:
            virtual ~SubnodeChildsVisitor() = default;
            SubnodeChildsVisitor(const String& parent_name)
                : m_parent_name { parent_name } {}

            virtual bool visit(SharedPtr<Node> node) override {
                if (node &&
                    (node->getParent()->getName() == m_parent_name)) {
                    spdlog::info("{} is child of {}", node->getName(), m_parent_name);
                    m_child_subnode_names.emplace_front(node->getName());
                }

                return true;
            }

            ForwardList<String> getAllSubnodes() { return m_child_subnode_names; }

        private:
            String m_parent_name;
            ForwardList<String> m_child_subnode_names;
        };

    // TODO: Resolve [@key] dependencies - get node name and replace it e.g. "/platform/port[@key]"
    class KeySelectorResolverVisitior : public Visitor {
    public:
        virtual ~KeySelectorResolverVisitior() = default;
        KeySelectorResolverVisitior(const String& xpath_to_resolve) {
            m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("[@key]"));
            if ((xpath_to_resolve.find("[@key]") + 6) < xpath_to_resolve.size()) {
                m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("[@key]") + 7);
            }

            spdlog::info("Fillout all subnodes for xpath {}", m_pre_wildcard);
            spdlog::info("Left xpath {}", m_post_wildcard);
            m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
        }

        ForwardList<String> getResolvedXpath() { return m_result; }

        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }
            else {
                // spdlog::info("Visiting node: {}", node->getName());
            }

            if (m_pre_wildcard_tokens.empty()
                || (node->getName() != m_pre_wildcard_tokens.front())) {
                spdlog::info("Failed to resolve wildcard");
                return true;
            }

            m_pre_wildcard_tokens.pop();
            if (m_pre_wildcard_tokens.empty()) {
                spdlog::info("We reached leaf token at node {}", node->getName());
                SubnodeChildsVisitor subnode_visitor(node->getName());
                node->accept(subnode_visitor);
                for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                    String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                    if (!m_post_wildcard.empty()) {
                        resolved_wildcard_xpath += "/" + m_post_wildcard;
                    }

                    spdlog::info("Resolved wildcard {}", resolved_wildcard_xpath);
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
            spdlog::info("{}:{}", __func__, __LINE__);
            while (!m_pre_wildcard_tokens.empty()) m_pre_wildcard_tokens.pop();
            spdlog::info("{}:{}", __func__, __LINE__);
            m_result.clear();
            spdlog::info("Call WildCard Dependency destroying for node {}", m_pre_wildcard + m_post_wildcard);
        };

        WildcardDependencyResolverVisitior(const String& xpath_to_resolve) {
            m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("/*"));
            if ((xpath_to_resolve.find("/*") + 2) < xpath_to_resolve.size()) {
                m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("/*") + 3);
            }

            spdlog::info("Fillout all subnodes for xpath {}", m_pre_wildcard);
            spdlog::info("Left xpath {}", m_post_wildcard);
            m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
        }

        ForwardList<String> getResolvedXpath() { return m_result; }

        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }
            else {
                // spdlog::info("Visiting node: {}", node->getName());
            }

            if (m_pre_wildcard_tokens.empty()
                || (node->getName() != m_pre_wildcard_tokens.front())) {
                spdlog::info("Failed to resolve wildcard");
                return true;
            }

            m_pre_wildcard_tokens.pop();
            if (m_pre_wildcard_tokens.empty()) {
                spdlog::info("We reached leaf token at node {}", node->getName());
                SubnodeChildsVisitor subnode_visitor(node->getName());
                node->accept(subnode_visitor);
                for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                    String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                    if (!m_post_wildcard.empty()) {
                        resolved_wildcard_xpath += "/" + m_post_wildcard;
                    }

                    spdlog::info("Resolved wildcard {}", resolved_wildcard_xpath);
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

    auto dependencies = std::make_shared<Map<String, Set<String>>>();
    auto without_dependencies = std::make_shared<Map<String, Set<String>>>();
    auto not_resolved_dependencies = depVisitor.getDependencies();
    for (auto schema_node : *not_resolved_dependencies) {
        spdlog::info("Processing node: {}", schema_node.first);
        if (schema_node.second.empty()) {
            // Leave node to just load all nodes
            (*without_dependencies)[schema_node.first].clear();
        }

        for (auto dep : schema_node.second) {
            if (dep.find("/*") != String::npos) {
                spdlog::info("Found wildcard in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = WildcardDependencyResolverVisitior(dep);
                root_config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto resolved_wildcard : resolved_xpath) {
                        spdlog::info("Put resolved wildcard {}", resolved_wildcard);
                        (*dependencies)[schema_node.first].insert(resolved_wildcard);
                    }

                    continue;
                }
            }
            else if (dep.find("[@key]") != String::npos) {
                spdlog::info("Found [@key] in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = KeySelectorResolverVisitior(dep);
                root_config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto& resolved_wildcard : resolved_xpath) {
                        spdlog::info("Put resolved [@key] {}", resolved_wildcard);
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

    // exit(EXIT_SUCCESS);
    std::list<String> ordered_cmds;
    auto sort_result = run_update_op(dependencies, ordered_cmds);
    Set<String> set_of_ordered_cmds;
    for (auto& cmd : ordered_cmds) {
        set_of_ordered_cmds.emplace(cmd);
    }

    class DependencyMapperVisitior : public Visitor {
    public:
        virtual ~DependencyMapperVisitior() = default;
        DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : m_set_of_ordered_cmds { set_of_ordered_cmds } {}
        virtual bool visit(SharedPtr<Node> node) override {
            auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
            if (!schema_node) {
                std::clog << "Node " << node->getName() << " has not schema\n";
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

            // if (std::dynamic_pointer_cast<Leaf>(node)) {
            //     return false; // Terminate visiting on Leaf object
            // }

            return true;
        }

        Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
        Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

    private:
        Set<String> m_set_of_ordered_cmds {};
        Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
        Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
    };

    DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
    root_config->accept(depMapperVisitor);
    std::list<SharedPtr<Node>> nodes_to_execute {};
    std::clog << "Sorting unordered nodes\n";
    for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
        std::clog << "Schema: " << schema_name << std::endl;
        std::clog << "Nodes: ";
        for (auto& n : unordered_nodes) {
            // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
            nodes_to_execute.emplace_back(n);
            std::clog << n->getName() << " ";
        }
        std::clog << std::endl;
    }

    std::clog << "Sorting ordered nodes\n";
    for (auto& ordered_cmd : ordered_cmds) {
        std::clog << "Schema: " << ordered_cmd << std::endl;
        std::clog << "Nodes: ";
        auto ordered_nodes = depMapperVisitor.getOrderedNodes();
        for (auto& n : ordered_nodes[ordered_cmd]) {
            nodes_to_execute.emplace_back(n);
            std::clog << n->getName() << " ";
        }

        std::clog << std::endl;
    }

    ::exit(EXIT_SUCCESS);
}

// int main(int argc, char* argv[]) {
//     std::ifstream schema_file;
//     schema_file.open("../schema.config");
//     if (!schema_file.is_open()) {
//         std::cerr << "Failed to open schema file\n";
//         exit(EXIT_FAILURE);
//     }

//     auto root_schema = std::make_shared<SchemaComposite>("/");
//     Config::loadSchema(schema_file, root_schema);
//     std::cout << "Dump nodes creating backtrace:\n";
//     VisitorImpl visitor;
//     root_schema->accept(visitor);

//     std::cout << std::endl;

//     std::ifstream config_file;
//     config_file.open("../main.config");
//     if (!config_file.is_open()) {
//         std::cerr << "Failed to open config file\n";
//         exit(EXIT_FAILURE);
//     }

//     auto load_config = std::make_shared<Composite>("/");
//     Config::load(config_file, load_config, root_schema);
//     std::cout << "Dump nodes creating backtrace:\n";
//     load_config->accept(visitor);

//     // exit(EXIT_SUCCESS);

//     VisitorGE1Speed visit_ge1_speed;
//     load_config->accept(visit_ge1_speed);
//     if (!visit_ge1_speed.getSpeedNode()) {
//         std::cerr << "Not found speed node!\n";
//         exit(EXIT_FAILURE);
//     }

//     auto ee = std::make_shared<ExprEval>();
//     ee->init(visit_ge1_speed.getSpeedNode());
//     if (ee->evaluate("#if #xpath</interface/gigabit-ethernet[@key]/speed/value> #equal #string<400G> #then #must<#regex_match<@value, #regex<^(400G)$>>>")) {
//         std::cout << "Successfully evaluated expression\n";
//     }
//     else {
//         std::cerr << "Failed to evaluate expression\n";
//     }

//     class DependencyGetterVisitior2 : public Visitor {
//     public:
//         virtual ~DependencyGetterVisitior2() = default;
//         DependencyGetterVisitior2() : m_dependencies { std::make_shared<Map<String, Set<String>>>() } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 return true;
//             }

//             auto update_depend_attrs = schema_node->findAttr("update-depend");
//             if (update_depend_attrs.empty()) {
//                 return true;
//             }

//             for (auto& dep : update_depend_attrs) {
//                 // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
//                 // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
//                 (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep);
//                 std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

//     private:
//         SharedPtr<Map<String, Set<String>>> m_dependencies;
//     };

//     class DependencyGetterVisitior : public Visitor {
//     public:
//         virtual ~DependencyGetterVisitior() = default;
//         DependencyGetterVisitior() : m_dependencies { std::make_shared<Map<String, Map<String, SharedPtr<Node>>>>() } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 return true;
//             }

//             auto update_depend_attrs = schema_node->findAttr("update-depend");
//             if (update_depend_attrs.empty()) {
//                 return true;
//             }

//             for (auto& dep : update_depend_attrs) {
//                 // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
//                 // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
//                 (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep, node);
//                 std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> getDependencies() { return m_dependencies; }

//     private:
//         SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> m_dependencies;
//     };

//     class DependencyMapperVisitior : public Visitor {
//     public:
//         virtual ~DependencyMapperVisitior() = default;
//         DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : m_set_of_ordered_cmds { set_of_ordered_cmds } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 std::clog << "Node " << node->getName() << " has not schema\n";
//                 return true;
//             }

//             auto schema_node_xpath = XPath::to_string2(schema_node);
//             if (m_set_of_ordered_cmds.find(schema_node_xpath) != m_set_of_ordered_cmds.end()) {
//                 m_ordered_nodes[schema_node_xpath].emplace_front(node);
//             }
//             else if (m_ordered_nodes.find(XPath::to_string2(schema_node->getParent())) != m_ordered_nodes.end()) {
//                 m_ordered_nodes[XPath::to_string2(schema_node->getParent())].emplace_back(node);
//             }
//             else {
//                 m_unordered_nodes[schema_node_xpath].emplace_back(node);
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
//         Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

//     private:
//         Set<String> m_set_of_ordered_cmds {};
//         Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
//         Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
//     };

//     DependencyGetterVisitior2 depVisitor;
//     load_config->accept(depVisitor);
//     std::list<String> ordered_cmds;
//     auto sort_result = run_update_op(depVisitor.getDependencies(), ordered_cmds);
//     Set<String> set_of_ordered_cmds;
//     for (auto& cmd : ordered_cmds) {
//         set_of_ordered_cmds.emplace(cmd);
//     }

//     DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
//     load_config->accept(depMapperVisitor);
//     std::list<SharedPtr<Node>> nodes_to_execute {};
//     std::clog << "Sorting unordered nodes\n";
//     for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
//         std::clog << "Schema: " << schema_name << std::endl;
//         std::clog << "Nodes: ";
//         for (auto& n : unordered_nodes) {
//             // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
//             nodes_to_execute.emplace_back(n);
//             std::clog << n->getName() << " ";
//         }
//         std::clog << std::endl;
//     }

//     std::clog << "Sorting ordered nodes\n";
//     for (auto& ordered_cmd : ordered_cmds) {
//         std::clog << "Schema: " << ordered_cmd << std::endl;
//         std::clog << "Nodes: ";
//         auto ordered_nodes = depMapperVisitor.getOrderedNodes();
//         for (auto& n : ordered_nodes[ordered_cmd]) {
//             nodes_to_execute.emplace_back(n);
//             std::clog << n->getName() << " ";
//         }

//         std::clog << std::endl;
//     }

//     std::clog << "Execute action for nodes\n";
//     for (auto n : nodes_to_execute) {
//         std::clog << n->getName() << ": " << XPath::to_string2(n) << std::endl;
//         auto expr_eval = std::make_shared<ExprEval>();
//         expr_eval->init(n);
//         auto schema_node = std::dynamic_pointer_cast<SchemaNode>(n->getSchemaNode());
//         if (!schema_node) {
//             std::clog << schema_node->getName() << " has not reference to schema node\n";
//             continue;
//         }

//         for (auto& constraint : schema_node->findAttr("update-constraint")) {
//             std::clog << "Evaluating expression '" << constraint << "' for node " << n->getName() << std::endl;
//             if (expr_eval->evaluate(constraint)) {
//                 std::cout << "Successfully evaluated expression\n";
//             }
//             else {
//                 std::cerr << "Failed to evaluate expression\n";
//                 exit(EXIT_FAILURE);
//             }
//         }
//     }

//     // auto depends = std::make_shared<Map<String, Set<String>>>();
//     // // TODO: Add to the ordered list all nodes which does not have refererence, like "/platform/port[@key]/num_breakout"
//     // // TODO: Transform Node name into xpath
//     // (*depends)["/interface/ethernet[@key]"].emplace("/platform/port[@key]/num_breakout");
//     // (*depends)["/interface/aggregate[@key]/members"].emplace("/interface/ethernet[@key]");
//     // (*depends)["/interface/ethernet[@key]/speed"].emplace("/platform/port[@key]/breakout_speed");
//     // (*depends)["/platform/port[@key]/breakout_speed"].emplace("/platform/port[@key]/num_breakout");
//     // auto sort_result = run_update_op(depends, ordered_cmds);

//     // TODO: Go through all nodes and find all nodes which have update-depends 
//     // TODO #1: Map sorted elements into real nodes based on loaded nodes
//     // for (auto& cmd : ordered_cmds)
//     //     auto node = XPath::select(load_config, cmd);
//     // TODO #2: 
//     auto root_config = std::make_shared<Composite>("/");
//     // Make a diff with sorted nodes and root_config <-- it is real load with action taken
//     // If leaf exists in both set, check if it has different value

//     exit(EXIT_SUCCESS);
// }