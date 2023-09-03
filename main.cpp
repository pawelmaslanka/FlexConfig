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

    auto root_config = std::make_shared<Composite>("/");
    Config::load(config_file, root_config, root_schema);
    std::cout << "Dump nodes creating backtrace:\n";
    root_config->accept(visitor);
    VisitorGE1Speed visit_ge1_speed;
    root_config->accept(visit_ge1_speed);
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

    std::list<String> ordered_cmds;
    auto depends = std::make_shared<Map<String, Set<String>>>();
    // TODO: Add to the ordered list all nodes which does not have refererence, like "/platform/port[@key]/num_breakout"
    // TODO: Transform Node name into xpath
    (*depends)["/interface/ethernet[@key]"].emplace("/platform/port[@key]/num_breakout");
    (*depends)["/interface/aggregate[@key]/members"].emplace("/interface/ethernet[@key]");
    (*depends)["/interface/ethernet[@key]/speed"].emplace("/platform/port[@key]/breakout_speed");
    (*depends)["/platform/port[@key]/breakout_speed"].emplace("/platform/port[@key]/num_breakout");
    auto sort_result = run_update_op(depends, ordered_cmds);

    exit(EXIT_SUCCESS);
}