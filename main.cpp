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

class VisitorImpl : public Visitor {
  public:
    virtual ~VisitorImpl() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        node->accept(*this);
        return true;
    }
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

    exit(EXIT_SUCCESS);
}