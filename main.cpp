#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include "node/node.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"
#include "load_config.hpp"
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
    std::ifstream schema_file;
    schema_file.open("../schema.config");
    if (!schema_file.is_open()) {
        std::cerr << "Failed to open schema file\n";
        exit(EXIT_FAILURE);
    }

    auto root_schema = std::make_shared<SchemaComposite>("/");
    Config::loadSchema(schema_file, root_schema);
    std::cout << "Dump nodes creating backtrace:\n";
    VisitorImpl visitor;
    root_schema->accept(visitor);

    std::cout << std::endl;

    std::ifstream config_file;
    config_file.open("../main.config");
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file\n";
        exit(EXIT_FAILURE);
    }

    auto load_config = std::make_shared<Composite>("/");
    Config::load(config_file, load_config, root_schema);
    std::cout << "Dump nodes creating backtrace:\n";
    load_config->accept(visitor);
    VisitorGE1Speed visit_ge1_speed;
    load_config->accept(visit_ge1_speed);
    if (!visit_ge1_speed.getSpeedNode()) {
        std::cerr << "Not found speed node!\n";
        exit(EXIT_FAILURE);
    }

    auto ee = std::make_shared<ExprEval>();
    ee->init(visit_ge1_speed.getSpeedNode());
    if (ee->evaluate("#if #xpath</interface/gigabit-ethernet[@key]/speed/value> #equal #string<400G> #then #must<#regex_match<@value, #regex<^(400G)$>>>")) {
        std::cout << "Successfully evaluated expression\n";
    }
    else {
        std::cerr << "Failed to evaluate expression\n";
    }

    class DependencyGetterVisitior2 : public Visitor {
    public:
        virtual ~DependencyGetterVisitior2() = default;
        DependencyGetterVisitior2() : m_dependencies { std::make_shared<Map<String, Set<String>>>() } {}
        virtual bool visit(SharedPtr<Node> node) override {
            auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
            if (!schema_node) {
                return true;
            }

            auto update_depend_attrs = schema_node->findAttr("update-depend");
            if (update_depend_attrs.empty()) {
                return true;
            }

            for (auto& dep : update_depend_attrs) {
                // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
                // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
                (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep);
                std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
            }

            return true;
        }

        SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

    private:
        SharedPtr<Map<String, Set<String>>> m_dependencies;
    };

    class DependencyGetterVisitior : public Visitor {
    public:
        virtual ~DependencyGetterVisitior() = default;
        DependencyGetterVisitior() : m_dependencies { std::make_shared<Map<String, Map<String, SharedPtr<Node>>>>() } {}
        virtual bool visit(SharedPtr<Node> node) override {
            auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
            if (!schema_node) {
                return true;
            }

            auto update_depend_attrs = schema_node->findAttr("update-depend");
            if (update_depend_attrs.empty()) {
                return true;
            }

            for (auto& dep : update_depend_attrs) {
                // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
                // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
                (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep, node);
                std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
            }

            return true;
        }

        SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> getDependencies() { return m_dependencies; }

    private:
        SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> m_dependencies;
    };

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

            return true;
        }

        Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
        Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

    private:
        Set<String> m_set_of_ordered_cmds {};
        Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
        Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
    };

    DependencyGetterVisitior2 depVisitor;
    load_config->accept(depVisitor);
    std::list<String> ordered_cmds;
    auto sort_result = run_update_op(depVisitor.getDependencies(), ordered_cmds);
    Set<String> set_of_ordered_cmds;
    for (auto& cmd : ordered_cmds) {
        set_of_ordered_cmds.emplace(cmd);
    }

    DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
    load_config->accept(depMapperVisitor);
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

    std::clog << "Execute action for nodes\n";
    for (auto n : nodes_to_execute) {
        std::clog << n->getName() << ": " << XPath::to_string2(n) << std::endl;
    }

    // auto depends = std::make_shared<Map<String, Set<String>>>();
    // // TODO: Add to the ordered list all nodes which does not have refererence, like "/platform/port[@key]/num_breakout"
    // // TODO: Transform Node name into xpath
    // (*depends)["/interface/ethernet[@key]"].emplace("/platform/port[@key]/num_breakout");
    // (*depends)["/interface/aggregate[@key]/members"].emplace("/interface/ethernet[@key]");
    // (*depends)["/interface/ethernet[@key]/speed"].emplace("/platform/port[@key]/breakout_speed");
    // (*depends)["/platform/port[@key]/breakout_speed"].emplace("/platform/port[@key]/num_breakout");
    // auto sort_result = run_update_op(depends, ordered_cmds);

    // TODO: Go through all nodes and find all nodes which have update-depends 
    // TODO #1: Map sorted elements into real nodes based on loaded nodes
    // for (auto& cmd : ordered_cmds)
    //     auto node = XPath::select(load_config, cmd);
    // TODO #2: 
    auto root_config = std::make_shared<Composite>("/");
    // Make a diff with sorted nodes and root_config <-- it is real load with action taken
    // If leaf exists in both set, check if it has different value

    exit(EXIT_SUCCESS);
}