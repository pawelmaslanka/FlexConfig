// For some reason this include statement for spdlog must be at the top due to an error:
// "function template partial specialization is not allowed"
#include <spdlog/spdlog.h>

#include "config.hpp"

#include <nlohmann/json-schema.hpp>

#include "composite.hpp"
#include "lib/utils.hpp"
#include "xpath.hpp"

#include <fstream>

static constexpr auto SCHEMA_NODE_PROPERTIES_NAME { "properties" };
static constexpr auto SCHEMA_NODE_PATTERN_PROPERTIES_NAME { "patternProperties" };

bool validateJsonConfig(nlohmann::json& jconfig, nlohmann::json& jschema) {
    nlohmann::json_schema::json_validator validator; // create validator

	try {
		validator.set_root_schema(jschema); // insert root-schema
	} catch (const std::exception &e) {
		spdlog::error("Validation of schema failed, here is why: {}\n", e.what());
		return false;
	}

	try {
		auto defaultPatch = validator.validate(jconfig); // validate the document - uses the default throwing error-handler
	}
    catch (const std::exception &e) {
		spdlog::critical("Validation failed, here is why: {}\n", e.what());
	}

    return true;
}

bool parseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config);
// bool parseAndLoadConfig2(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config, std::shared_ptr<SchemaComposite>& root_schema);
bool loadPatternProperties(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    spdlog::info("\n[{}:{}] {}\n ->\n {}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jconfig.items()) {
        spdlog::trace("{} -> {}\n", k, v.dump());
        for (auto& [ksch, vsch] : jschema.items()) {
            if (std::regex_match(k, Regex { ksch })) {
                spdlog::trace("Matched key {} to pattern {}\n", k, ksch);
                // Composite::Factory(k, root_config).get_object<Composite>()
                node = std::make_shared<Composite>(k, root_config);
                root_config->add(node);
                spdlog::info("{} -> {}\n", node->getParent()->getName(), node->getName());
                if ((vsch.find("properties") == vsch.end())
                    && (vsch.find("patternProperties") == vsch.end())) {
                    spdlog::trace("Reached schema leaf node '{}'", ksch);
                    // TODO: Create leaf object
                    // root_config.set_value(jconfig.begin());
                    continue;
                }

                if (!parseAndLoadConfig(v, vsch, node)) {
                    spdlog::error("Failed to parse config node '{}'", k);
                    return false;
                }
            }
        }
    }

    return true;
}

// bool loadPatternProperties2(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config, std::shared_ptr<SchemaComposite>& root_schema) {
//     spdlog::info("\n[{}:{}] {}\n ->\n {}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
//     SharedPtr<Composite> node = root_config;
//     SharedPtr<SchemaComposite> schema_node = root_schema;
//     for (auto& [k, v] : jconfig.items()) {
//         spdlog::trace("{} -> {}\n", k, v.dump());
//         for (auto& [ksch, vsch] : jschema.items()) {
//             if (std::regex_match(k, Regex { ksch })) {
//                 spdlog::trace("Matched key {} to pattern {}\n", k, ksch);
//                 // Composite::Factory(k, root_config).get_object<Composite>()
//                 node = std::make_shared<Composite>(k, root_config);
//                 root_config->add(node);
//                 spdlog::info("{} -> {}\n", node->getParent()->getName(), node->getName());
//                 schema_node = std::make_shared<SchemaComposite>(k, root_schema);
//                 root_schema->add(schema_node);
//                 if (((vsch.find("properties") == vsch.end())
//                     && (vsch.find("patternProperties") == vsch.end()))
//                     || ((vsch.find("type") != vsch.end()) && (vsch.at("type") != "object"))) {
//                     // TODO: Check is the leaf is a string or other specific type
//                     spdlog::trace("Reached config leaf node '{}' -> '{}'", ksch, jconfig.at(ksch).get<String>());
//                     // TODO: Replace Value with std::variant
//                     Value val(Value::Type::STRING);
//                     val.set_string(jconfig.at(ksch).get<String>());
//                     auto leaf = std::make_shared<Leaf>(ksch, val, root_config);
//                     root_config->add(leaf);
//                     auto schema_node = std::make_shared<SchemaNode>(ksch, root_schema);
//                     root_schema->add(schema_node);

//                     spdlog::trace("Reached schema leaf node '{}'", ksch);
//                     continue;
//                 }

//                 if (!parseAndLoadConfig(v, vsch, node, schema_node)) {
//                     spdlog::error("Failed to parse config node '{}'", k);
//                     return false;
//                 }
//             }
//         }
//     }

//     return true;
// }

// bool parseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
//     spdlog::info("\n[{}:{}] {}\n -> \n{}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
//     SharedPtr<Composite> node = root_config;
//     for (auto& [k, v] : jschema.items()) {
//         if (jconfig.find(k) == jconfig.end()) {
//             continue;
//         }

//         spdlog::info("Found {} in config based on schema", k);

//         // Composite::Factory(k, root_config).get_object<Composite>();

//         // leaf node does not include "properties"
//         if (((v.find("properties") == v.end())
//             && (v.find("patternProperties") == v.end()))
//             // leaf node is not "object"
//             || ((v.find("type") != v.end()) && (v.at("type") != "object"))) {
//             // TODO: Check is the leaf is a string or other specific type
//             spdlog::trace("Reached config leaf node '{}' -> '{}'", k, jconfig.at(k).get<String>());
//             // TODO: Replace Value with std::variant
//             Value val(Value::Type::STRING);
//             val.set_string(jconfig.at(k).get<String>());
//             auto leaf = std::make_shared<Leaf>(k, val, root_config);
//             root_config->add(leaf);
//             // TODO: Create leaf object
//             // root_config.set_value(jconfig.begin());
//             continue;
//         }
//         else {
//             node = std::make_shared<Composite>(k, root_config);
//             root_config->add(node);
//             spdlog::info("{} -> {}\n", node->getParent()->getName(), node->getName());
//         }

//         if (!parseAndLoadConfig(jconfig[k], v, node)) {
//             spdlog::error("Failed to parse schema node '{}'", k);
//             return false;
//         }
//     }

//     for (auto& [k, v] : jschema.items()) {
//         if (k == "patternProperties") {
//             if (!loadPatternProperties(jconfig, v, node)) {
//                 spdlog::error("Failed to parse schema node '{}'", k);
//                 return false;
//             }
//         }

//         if (k == "properties") {
//             if (!parseAndLoadConfig(jconfig, v, node)) {
//                 spdlog::error("Failed to parse schema node '{}'", k);
//                 return false;
//             }
//         }

//         if (jconfig.find(k) == jconfig.end()) {
//             continue;
//         }
//     }

//     return true;
// }

bool parseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    spdlog::info("\n[{}:{}] {}\n -> \n{}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jschema.items()) {
        spdlog::info("Processing schema node {}", k);
        if (jconfig.find(k) == jconfig.end()) {
            spdlog::info("{} {}", __func__, __LINE__);
            continue;
        }

        spdlog::info("Found {} in config based on schema", k);

        // Composite::Factory(k, root_config).get_object<Composite>();

        if ((v.find("type") != v.end()) && (v.at("type") == "array")) {
            spdlog::info("{} {}", __func__, __LINE__);
            for (const auto& item : jconfig.at(k)) {
                Value val(Value::Type::STRING);
                val.set_string(item.get<String>());
                auto leaf = std::make_shared<Leaf>(k, val, root_config);
                root_config->add(leaf);
            }
        } // leaf node does not include "properties"
        else if (((v.find("properties") == v.end())
            && (v.find("patternProperties") == v.end()))
            // leaf node is not "object"
            || ((v.find("type") != v.end()) && (v.at("type") != "object"))) {
            // TODO: Check is the leaf is a string or other specific type
            spdlog::info("Reached config leaf node '{}' -> '{}'", k, jconfig.at(k).get<String>());
            // TODO: Replace Value with std::variant
            Value val(Value::Type::STRING);
            val.set_string(jconfig.at(k).get<String>());
            auto leaf = std::make_shared<Leaf>(k, val, root_config);
            root_config->add(leaf);
            // TODO: Create leaf object
            // root_config.set_value(jconfig.begin());
            continue;
        }
        else {
            spdlog::info("{} {}", __func__, __LINE__);
            node = std::make_shared<Composite>(k, root_config);
            root_config->add(node);
            spdlog::info("{} -> {}\n", node->getParent()->getName(), node->getName());
        }

        spdlog::info("{} {}", __func__, __LINE__);
        if (!parseAndLoadConfig(jconfig[k], v, node)) {
            spdlog::error("Failed to parse schema node '{}'", k);
            return false;
        }
    }

    spdlog::info("Further processing");
    for (auto& [k, v] : jschema.items()) {
        if (k == "patternProperties") {
            spdlog::info("Found patternProperties");
            if (!loadPatternProperties(jconfig, v, node)) {
                spdlog::error("Failed to parse schema node '{}'", k);
                return false;
            }
        }

        if (k == "properties") {
            spdlog::info("Found properties");
            if (!parseAndLoadConfig(jconfig, v, node)) {
                spdlog::error("Failed to parse schema node '{}'", k);
                return false;
            }
        }

        if (jconfig.find(k) == jconfig.end()) {
            spdlog::info("{} {}", __func__, __LINE__);
            continue;
        }
    }

    spdlog::info("{} {}", __func__, __LINE__);
    return true;
}

// bool parseAndLoadConfig2(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config, std::shared_ptr<SchemaComposite>& root_schema) {
//     spdlog::info("\n[{}:{}] {}\n -> \n{}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
//     SharedPtr<Composite> node = root_config;
//     SharedPtr<SchemaComposite> schema = root_schema;
//     for (auto& [k, v] : jschema.items()) {
//         if (jconfig.find(k) == jconfig.end()) {
//             continue;
//         }

//         spdlog::info("Found {} in config based on schema", k);

//         // Composite::Factory(k, root_config).get_object<Composite>();

//         // leaf node does not include "properties"
//         if (((v.find("properties") == v.end())
//             && (v.find("patternProperties") == v.end()))
//             // leaf node is not "object"
//             || ((v.find("type") != v.end()) && (v.at("type") != "object"))) {
//             // TODO: Check is the leaf is a string or other specific type
//             spdlog::trace("Reached config leaf node '{}' -> '{}'", k, jconfig.at(k).get<String>());
//             // TODO: Replace Value with std::variant
//             Value val(Value::Type::STRING);
//             val.set_string(jconfig.at(k).get<String>());
//             auto leaf = std::make_shared<Leaf>(k, val, root_config);
//             root_config->add(leaf);
//             auto schema_node = std::make_shared<SchemaNode>(k, root_schema);
//             root_schema->add(schema_node);
//             // TODO: Load all attributes
//             // TODO: Set config node to schema node
//             continue;
//         }
//         else {
//             node = std::make_shared<Composite>(k, root_config);
//             root_config->add(node);
//             spdlog::info("{} -> {}\n", node->getParent()->getName(), node->getName());
//             schema = std::make_shared<Composite>(k, root_schema);
//             root_schema->add(schema);
//         }

//         if (!parseAndLoadConfig2(jconfig[k], v, node, schema)) {
//             spdlog::error("Failed to parse schema node '{}'", k);
//             return false;
//         }
//     }

//     for (auto& [k, v] : jschema.items()) {
//         if (k == "patternProperties") {
//             if (!loadPatternProperties2(jconfig, v, node, schema)) {
//                 spdlog::error("Failed to parse schema node '{}'", k);
//                 return false;
//             }
//         }

//         if (k == "properties") {
//             if (!parseAndLoadConfig2(jconfig, v, node, schema)) {
//                 spdlog::error("Failed to parse schema node '{}'", k);
//                 return false;
//             }
//         }

//         if (jconfig.find(k) == jconfig.end()) {
//             continue;
//         }
//     }

//     return true;
// }

std::shared_ptr<Composite> loadConfig(std::string_view config_filename, std::string_view schema_filename) {
    std::ifstream schema_ifs(schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    std::ifstream config_ifs(config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);

    if (!validateJsonConfig(jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return {};
    }

    static auto constexpr ROOT_TREE_CONFIG_NAME { "/" };
    // auto root_config = Composite::Factory(ROOT_TREE_CONFIG_NAME).get_object<Composite>();
    auto root_config = std::make_shared<Composite>(ROOT_TREE_CONFIG_NAME);
    auto properties_it = jschema.find("properties");
    if (properties_it != jschema.end()) {
        if (!parseAndLoadConfig(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'properties' node");
            return {};
        }

        return root_config;
    }

    properties_it = jschema.find("patternProperties");
    if (properties_it != jschema.end()) {
        if (!loadPatternProperties(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'patternProperties' node");
            return {};
        }

        return root_config;
    }

    spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
    return {};
}

// TODO: Create schema nodes hierarchy to be reused by getSchemaByXPath!!! Without the nodes schema hierarchy,
//       XPath::to_string2() does not work for schema node
bool load_node_schema(const String& xpath, SharedPtr<Node> root_node, const nlohmann::json& jschema) {
    auto root_properties_it = jschema.find("properties");
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return false;
    }

    auto node = root_node;
    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse2(xpath);
    String config_xpath_composed = {};
    String schema_xpath_composed = {};
    for (auto token : xpath_tokens) {
        spdlog::trace("Processing node '{}'", token);
        // spdlog::trace("Loaded schema:\n{}", schema.dump());
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                schema_xpath_composed += "/properties/" + token;
                spdlog::trace("Select schema node '{}'", schema_xpath_composed);
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
                    return false;
                }
            }
            else {
                spdlog::trace("Matching token '{}' based on 'patternProperties'", token);
                bool pattern_matched = false;
                for (auto& [k, v] : schema["patternProperties"].items()) {
                    if (std::regex_match(token, Regex { k })) {
                        spdlog::trace("Matched token '{}' to 'regex {}'", token, k);
                        pattern_matched = true;
                        schema_xpath_composed += "/patternProperties/" + k;
                        spdlog::trace("Select schema node '{}'", schema_xpath_composed);
                        auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                        schema = jschema[selector];
                        break;
                    }
                }

                if (!pattern_matched) {
                    spdlog::error("Not found node '{}' in schema 'patterProperties'", token);
                    return false;
                }
            }
        }
        else {
            schema_xpath_composed += "/properties/" + token;
            spdlog::trace("Select schema node '{}'", schema_xpath_composed);
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
        }

        config_xpath_composed += "/" + token;
        auto composed_node = XPath::select(root_node, config_xpath_composed);
        if (composed_node) {
            node = composed_node;
            continue;
        }

        // Let's start to create new node
        // node = Composite::Factory(token, node).get_object<Node>();
        node = std::dynamic_pointer_cast<Node>(std::make_shared<Composite>(token, node));
        // TODO: Load schema attributes to node
        // TODO: make a backup list in case of further processing error
    }

    return true;
}

nlohmann::json get_schema_by_xpath(const String& xpath, const nlohmann::json& jschema) {
    auto root_properties_it = jschema.find("properties");
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return false;
    }

    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse2(xpath);
    String schema_xpath_composed = {};
    for (auto token : xpath_tokens) {
        spdlog::trace("Processing node '{}'", token);
        // spdlog::trace("Loaded schema:\n{}", schema.dump());
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                schema_xpath_composed += "/properties/" + token;
                spdlog::trace("Select schema node '{}'", schema_xpath_composed);
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
                    return {};
                }
            }
            else {
                spdlog::trace("Matching token '{}' based on 'patternProperties'", token);
                bool pattern_matched = false;
                for (auto& [k, v] : schema["patternProperties"].items()) {
                    if (std::regex_match(token, Regex { k })) {
                        spdlog::trace("Matched token '{}' to 'regex {}'", token, k);
                        pattern_matched = true;
                        schema_xpath_composed += "/patternProperties/" + k;
                        spdlog::trace("Select schema node '{}'", schema_xpath_composed);
                        auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                        schema = jschema[selector];
                        break;
                    }
                }

                if (!pattern_matched) {
                    spdlog::error("Not found node '{}' in schema 'patterProperties'", token);
                    return {};
                }
            }
        }
        else {
            schema_xpath_composed += "/properties/" + token;
            spdlog::trace("Select schema node '{}'", schema_xpath_composed);
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
        }
    }

    return schema;
}

// TODO:
// SharedPtr<Node> make_schema_tree(const nlohmann::json& jschema) {
//     auto root_node = Node::Factory("/").get_object<Node>();
//     // TODO: Allocate schema in Node composite?
//     return root_node;
// }

Config::Manager::Manager(StringView config_filename, StringView schema_filename, SharedPtr<RegistryClass>& registry)
 : m_config_filename { config_filename }, m_schema_filename { schema_filename } {

}

bool Config::Manager::load(SharedPtr<Node>& root_config_ptr) {
    std::ifstream schema_ifs(m_schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    std::ifstream config_ifs(m_config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);

    if (!validateJsonConfig(jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    auto root_config = std::make_shared<Composite>(ROOT_TREE_CONFIG_NAME);
    auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!parseAndLoadConfig(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'properties' node");
            return false;
        }

        root_config_ptr = root_config;
        return true;
    }

    properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!loadPatternProperties(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'patternProperties' node");
            return false;
        }

        root_config_ptr = root_config;
        return true;
    }

    spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
    return false;
}

// bool Config::Manager::load2(SharedPtr<Node>& root_config_ptr, SharedPtr<SchemaNode>& root_schema_ptr) {
//     std::ifstream schema_ifs(m_schema_filename);
//     nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
//     std::ifstream config_ifs(m_config_filename);
//     nlohmann::json jconfig = nlohmann::json::parse(config_ifs);

//     if (!validateJsonConfig(jconfig, jschema)) {
//         spdlog::error("Failed to validate config");
//         return false;
//     }

//     auto root_config = std::make_shared<Composite>(ROOT_TREE_CONFIG_NAME);
//     auto root_schema = std::make_shared<SchemaComposite>(ROOT_TREE_CONFIG_NAME);
//     auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
//     if (properties_it != jschema.end()) {
//         if (!parseAndLoadConfig2(jconfig, *properties_it, root_config, root_schema)) {
//             spdlog::error("Failed to load config based on 'properties' node");
//             return false;
//         }

//         root_config_ptr = root_config;
//         root_schema_ptr = root_schema;
//         return true;
//     }

//     properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
//     if (properties_it != jschema.end()) {
//         if (!loadPatternProperties2(jconfig, *properties_it, root_config)) {
//             spdlog::error("Failed to load config based on 'patternProperties' node");
//             return false;
//         }

//         root_config_ptr = root_config;
//         root_schema_ptr = root_schema;
//         return true;
//     }

//     spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
//     return false;
// }

#include <iostream>
// SharedPtr<SchemaNode> Config::Manager::getSchemaByXPath_backup(const String& xpath) {
//     // spdlog::info("Find schema for xpath: {}", xpath);
//     std::ifstream schema_ifs(m_schema_filename);
//     nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
//     auto root_properties_it = jschema.find("properties");
//     if (root_properties_it == jschema.end()) {
//         spdlog::error("Invalid schema - missing 'properties' node on the top");
//         return {};
//     }

//     auto schema = *root_properties_it;
//     auto xpath_tokens = XPath::parse2(xpath);
//     String schema_xpath_composed = {};
//     // TODO: Create node hierarchy dynamically. Save in cache. Chek cache next time before traverse
//     for (auto token : xpath_tokens) {
//         // spdlog::info("Processing node '{}'", token);
//         // spdlog::info("Loaded schema:\n{}", schema.dump());
//         if (schema.find(token) == schema.end()) {
//             if (schema.find("patternProperties") == schema.end()) {
//                 schema_xpath_composed += "/properties/" + token;
//                 // spdlog::info("Select schema node '{}'", schema_xpath_composed);
//                 auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
//                 schema = jschema[selector];
//                 if (schema.begin() == schema.end()) {
//                     spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
//                     return {};
//                 }
//             }
//             else {
//                 // spdlog::info("Matching token '{}' based on 'patternProperties'", token);
//                 bool pattern_matched = false;
//                 for (auto& [k, v] : schema["patternProperties"].items()) {
//                     if (std::regex_match(token, Regex { k })) {
//                         // spdlog::info("Matched token '{}' to 'regex {}'", token, k);
//                         pattern_matched = true;
//                         schema_xpath_composed += "/patternProperties/" + k;
//                         // spdlog::info("Select schema node '{}'", schema_xpath_composed);
//                         auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
//                         schema = jschema[selector];
//                         break;
//                     }
//                 }

//                 if (!pattern_matched) {
//                     spdlog::error("Not found node '{}' in schema 'patterProperties'", token);
//                     return {};
//                 }
//             }
//         }
//         else {
//             schema_xpath_composed += "/properties/" + token;
//             // spdlog::info("Select schema node '{}'", schema_xpath_composed);
//             auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
//             schema = jschema[selector];
//             // TODO: Create schema node and add to cache
//         }
//     }

//     // spdlog::info("Selected schema: {}", schema.patch(schema.diff({}, schema)).dump());
//     // TODO: Load all schema nodes
//     auto schema_node = std::make_shared<SchemaNode>(xpath_tokens.back());
//     static const Set<String> SUPPORTED_ATTRIBUTES = {
//         Config::PropertyName::DEFAULT, Config::PropertyName::DESCRIPTION, Config::PropertyName::UPDATE_CONSTRAINTS, Config::PropertyName::UPDATE_DEPENDENCIES
//     };

//     auto attributes = nlohmann::json(schema.patch(schema.diff({}, schema)));
//     spdlog::info("Attribute name: {}", xpath_tokens.back());
//     // std::cout << std::setw(4) << attributes << std::endl;


//     for (auto& [k, v] : attributes.items()) {
//         // spdlog::info("Checking attribute: {}", k);
//         if (SUPPORTED_ATTRIBUTES.find(k) != SUPPORTED_ATTRIBUTES.end()) {
//             if (v.is_array()) {
//                 for (auto& [_, item] : v.items()) {
//                     schema_node->addAttr(k, item);
//                     // spdlog::info("Added attribute {} -> {}", k, item);
//                 }
//             }
//             else {
//                 schema_node->addAttr(k, v);
//                 // spdlog::info("Added attribute {} -> {}", k, v);
//             }
//         }
//     }

//     return schema_node;
// }

#include <iostream>
SharedPtr<SchemaNode> Config::Manager::getSchemaByXPath(const String& xpath) {
    // spdlog::info("Find schema for xpath: {}", xpath);
    std::ifstream schema_ifs(m_schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    auto root_properties_it = jschema.find("properties");
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return {};
    }

    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse2(xpath);

    auto root_schema_node = std::make_shared<SchemaComposite>("/");
    auto schema_node = root_schema_node;
    static const Set<String> SUPPORTED_ATTRIBUTES = {
        Config::PropertyName::DEFAULT, Config::PropertyName::DESCRIPTION, Config::PropertyName::UPDATE_CONSTRAINTS, Config::PropertyName::UPDATE_DEPENDENCIES
    };

    spdlog::info("Attribute name: {}", xpath_tokens.back());

    auto load_schema_attributes = [](nlohmann::json& schema, String& node_name, SharedPtr<SchemaComposite>& root_schema_node) {
        auto schema_node = std::make_shared<SchemaComposite>(node_name, root_schema_node);
        auto attributes = nlohmann::json(schema.patch(schema.diff({}, schema)));
        for (auto& [k, v] : attributes.items()) {
            spdlog::info("Checking attribute: {}", k);
            if (SUPPORTED_ATTRIBUTES.find(k) != SUPPORTED_ATTRIBUTES.end()) {
                if (v.is_array()) {
                    for (auto& [_, item] : v.items()) {
                        schema_node->addAttr(k, item);
                        spdlog::info("Added attribute {} -> {}", k, item);
                    }
                }
                else {
                    schema_node->addAttr(k, v);
                    spdlog::info("Added attribute {} -> {}", k, v);
                }
            }
        }

        root_schema_node->add(schema_node);
        return schema_node;
    };

    String schema_xpath_composed = {};
    // TODO: Create node hierarchy dynamically. Save in cache. Chek cache next time before traverse
    for (auto token : xpath_tokens) {
        spdlog::info("Processing node --> '{}'", token);
        // spdlog::info("Loaded schema:\n{}", schema.dump());
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                spdlog::info("There is not patternProperties");
                schema_xpath_composed += "/properties/" + token;
                // spdlog::info("Select schema node '{}'", schema_xpath_composed);
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
                    return {};
                }

                schema_node = load_schema_attributes(schema, token, schema_node);
            }
            else {
                spdlog::info("Processing patternProperties");
                // spdlog::info("Matching token '{}' based on 'patternProperties'", token);
                bool pattern_matched = false;
                for (auto& [k, v] : schema["patternProperties"].items()) {
                    if (std::regex_match(token, Regex { k })) {
                        spdlog::info("Matched token '{}' to 'regex {}'", token, k);
                        pattern_matched = true;
                        schema_xpath_composed += "/patternProperties/" + k;
                        spdlog::info("Select schema node '{}'", schema_xpath_composed);
                        auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                        schema = jschema[selector];
                        std::cout << std::setw(4) << schema << std::endl;
                        schema_node = load_schema_attributes(schema, token, schema_node);
                        break;
                    }
                }

                if (!pattern_matched) {
                    spdlog::error("Not found node '{}' in schema 'patterProperties'", token);
                    return {};
                }
            }
        }
        else {
            schema_xpath_composed += "/properties/" + token;
            // spdlog::info("Select schema node '{}'", schema_xpath_composed);
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
            schema_node = load_schema_attributes(schema, token, schema_node);
            // TODO: Create schema node and add to cache
        }
    }

    // spdlog::info("Selected schema: {}", schema.patch(schema.diff({}, schema)).dump());
    // TODO: Load all schema nodes

    // std::cout << std::setw(4) << attributes << std::endl;

    return schema_node;
}