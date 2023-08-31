#include "load_config.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"

#include <iostream>
#include <cstdlib>
#include <regex>

template <typename T>
void createNode(const String node_name, SharedPtr<T>& parent_node) {
    auto new_node = std::make_shared<T>(node_name, parent_node);
    if (!parent_node->add(new_node)) {
        std::cerr << "Failed to add new node as a child\n";
        exit(EXIT_FAILURE);
    }

    parent_node = new_node;
}

void Config::loadSchema(std::ifstream& config_file, std::shared_ptr<SchemaComposite> root_config) {
    auto last_node = root_config;
    std::string property;
    std::string property_value;
    std::string line;
    bool is_in_quota = false;
    while (std::getline(config_file, line)) {
        line = Utils::trim(line);
        std::cout << line << "\n";
        std::string word;
        for (size_t i = 0; i < line.size(); ++i) {
            auto ch = line[i];
            if (ch == '{' && !is_in_quota) {
                Config::loadSchema(config_file, last_node);
                last_node = root_config;
                word.clear();
                property.clear();
                continue;
            }

            if (ch == '}' && !is_in_quota) {
                return;
            }

            if ((!std::isspace(ch) || is_in_quota) && (ch != ':') && !((ch == '"') && is_in_quota)) {
                if (ch == '"') {
                    is_in_quota = true;
                }
                else {
                    word += ch;
                }

                if ((i + 1) < line.size()) {
                    continue;
                }
            }

            if (word.empty()) {
                continue;
            }

            if (ch == ':' && !is_in_quota) {
                if (property_value.empty()) {
                    property = word;
                }
            }
            else if ((std::isspace(ch) != is_in_quota) || (!std::isspace(ch) && !is_in_quota)) {
                if (!property.empty()) {
                    if (ch != '{') {
                        property_value = word;
                    }

                    if (property_value == "group" || property_value == "container" || property_value == "array" || property_value == "leaf") {
                        createNode<SchemaComposite>(property, last_node);
                    }
                    else {
                        last_node->addAttr(property, property_value);
                    }

                    property.clear();
                    property_value.clear();
                }
                else if (word == "@item") {
                    createNode<SchemaComposite>(word, last_node);
                }
            }

            word.clear();
            is_in_quota = false;
        }
    }
}

bool validateAndCreateAttributeValue(SharedPtr<Composite>& config_node, SharedPtr<SchemaComposite> schema_node, const String property, const String property_value) {
    auto pattern_val = schema_node->findAttr("pattern-value");
    if (!pattern_val.empty()) {
        std::clog << "Found pattern-value: " << pattern_val << std::endl;
        std::regex regexp(pattern_val);
        std::smatch match {};
        if (!std::regex_match(property_value, match, regexp)) {
            std::cerr << "Not matching regex " << pattern_val << " to property name " << property_value << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    auto new_node = std::make_shared<Leaf>(property, config_node);
    new_node->setValue(property_value);
    if (!config_node->add(new_node)) {
        std::cerr << "Failed to add new node as a child\n";
        exit(EXIT_FAILURE);
    }

    return true;
}

bool validateContainerAttributes(SharedPtr<Composite> config_node, SharedPtr<SchemaComposite> schema_node, const String property_value) {
    std::clog << "Validate container element\n";
    auto attr = schema_node->findAttr("pattern-name");
    if (!attr.empty()) {
        std::clog << "Check pattern " << attr << " for name of " << property_value << std::endl;
        std::regex regexp(attr);
        std::smatch match {};
        if (!std::regex_match(property_value, match, regexp)) {
            std::cerr << "Not matching regex " << attr << " to property name " << property_value << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    auto schema_node_parent = std::dynamic_pointer_cast<SchemaComposite>(schema_node->getParent());
    attr = schema_node_parent ? schema_node_parent->findAttr("max") : String {};
    if (!attr.empty()) {
        std::clog << "Check if container size " << config_node->count() << " is less than " << attr << std::endl;
        if (std::stoul(attr) <= config_node->count()) {
            std::cerr << "Container " << config_node->getName() << " exceeded its maximum size " << std::stoul(attr) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    return true;
}

void load_internal(std::ifstream& config_file, SharedPtr<Composite> root_config, SharedPtr<SchemaComposite> root_schema, Vector<Pair<SharedPtr<Node>, String>>& ref_by_node) {
    std::clog << "Root schema node: " << root_schema->getName() << std::endl;
    auto last_node = root_config;
    std::string property;
    std::string property_value;
    std::string line;
    SharedPtr<SchemaComposite> schema_node_ptr;
    bool is_in_quota = false;
    while (std::getline(config_file, line)) {
        line = Utils::trim(line);
        std::cout << line << "\n";
        std::string word;
        for (size_t i = 0; i < line.size(); ++i) {
            auto ch = line[i];
            if (ch == '{' && !is_in_quota) {
                schema_node_ptr = std::dynamic_pointer_cast<SchemaComposite>(root_schema->findNode(last_node->getName()));
                if (!schema_node_ptr) {
                    schema_node_ptr = std::dynamic_pointer_cast<SchemaComposite>(root_schema->findNode("@item"));
                }

                load_internal(config_file, last_node, schema_node_ptr ? schema_node_ptr : root_schema, ref_by_node);
                last_node = root_config;
                word.clear();
                property.clear();
                continue;
            }

            if (ch == '}' && !is_in_quota) {
                return;
            }

            if ((!std::isspace(ch) || is_in_quota) && (ch != ':') && !((ch == '"') && is_in_quota)) {
                if (ch == '"') {
                    is_in_quota = true;
                }
                else {
                    word += ch;
                }

                if ((i + 1) < line.size()) {
                    continue;
                }
            }

            if (word.empty()) {
                continue;
            }

            if (ch == ':' && !is_in_quota) {
                if (property_value.empty()) {
                    property = word;
                }
            }
            else if ((std::isspace(ch) != is_in_quota) || (!std::isspace(ch) && !is_in_quota)) {
                std::cout << "Parsed word: " << word << std::endl;
                if (!property.empty()) {
                    std::clog << "===========\n";
                    if (ch != '{') {
                        property_value = word;
                    }

                    if (auto item_ptr = std::dynamic_pointer_cast<SchemaComposite>(root_schema->findNode("@item"))) {
                        if (validateContainerAttributes(last_node, item_ptr, property_value)) {
                            auto parent = std::dynamic_pointer_cast<Composite>(last_node->getParent());
                            createNode<Composite>(property_value, parent);
                        }
                    }
                    else if (auto node_ptr = std::dynamic_pointer_cast<SchemaComposite>(root_schema->findNode(property))) {
                        std::clog << "--> Found attribute: " << node_ptr->getName() << std::endl;
                        if (!validateAndCreateAttributeValue(last_node, node_ptr, property, property_value)) {
                            std::cerr << "Failed to validate attribute value\n";
                            exit(EXIT_FAILURE);
                        }
                    }
                    else {
                        std::cerr << "Not found attribute: (" << property << ")" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    property.clear();
                    property_value.clear();
                }
                else {
                    property = word;
                    if (root_schema->findNode(property)) {
                        createNode<Composite>(property, last_node);
                        property.clear();
                    }
                    else if (auto item_ptr = std::dynamic_pointer_cast<SchemaComposite>(root_schema->findNode("@item"))) {
                        if (validateContainerAttributes(last_node, item_ptr, property)) {
                            createNode<Composite>(property, last_node);
                        }
                    }
                    else {
                        std::cerr << "Not found property: " << property << std::endl;
                        exit(EXIT_FAILURE);
                    }
                }
            }
            else {
                std::cerr << "Cos zle, bo mam: (" << word << ")" << std::endl;
            }

            word.clear();
            is_in_quota = false;
        }
    }
}

void Config::load(std::ifstream& config_file, SharedPtr<Composite> root_config, SharedPtr<SchemaComposite> root_schema) {
    // TODO: After the load, validate all references
    // TODO: Keep all nodes with references to others
    Vector<Pair<SharedPtr<Node>, String>> ref_by_node;
    load_internal(config_file, root_config, root_schema, ref_by_node);
}
