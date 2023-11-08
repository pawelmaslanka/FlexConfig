// For some reason this include statement for spdlog must be at the top due to an error:
// "function template partial specialization is not allowed"
#include <spdlog/spdlog.h>

#include "config.hpp"

#include <nlohmann/json-schema.hpp>

#include "composite.hpp"
#include "constraint_checking.hpp"
#include "lib/utils.hpp"
#include "nodes_ordering.hpp"
#include "xpath.hpp"
#include "node/leaf.hpp"

#include <filesystem>
#include <fstream>

#include "httplib.h"

nlohmann::json getSchemaByXPath2(const String& xpath, const String& schema_filename);
static nlohmann::json g_running_jconfig;
static nlohmann::json g_candidate_jconfig;

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
        return false;
	}

    return true;
}

bool parseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config);
// bool parseAndLoadConfig2(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config, std::shared_ptr<SchemaComposite>& root_schema);
bool loadPatternProperties(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    spdlog::debug("\n[{}:{}] {}\n ->\n {}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jconfig.items()) {
        spdlog::trace("{} -> {}\n", k, v.dump());
        for (auto& [ksch, vsch] : jschema.items()) {
            if (std::regex_match(k, Regex { ksch })) {
                spdlog::trace("Matched key {} to pattern {}\n", k, ksch);
                // Composite::Factory(k, root_config).get_object<Composite>()
                node = std::make_shared<Composite>(k, root_config);
                root_config->add(node);
                spdlog::debug("{} -> {}\n", node->getParent()->getName(), node->getName());
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

bool parseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    spdlog::debug("\n[{}:{}] {}\n -> \n{}\n\n", __func__, __LINE__, jconfig.dump(), jschema.dump());
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jschema.items()) {
        spdlog::debug("Processing schema node {}", k);
        if (jconfig.find(k) == jconfig.end()) {
            continue;
        }

        spdlog::debug("Found {} in config based on schema", k);

        // Composite::Factory(k, root_config).get_object<Composite>();

        if ((v.find("type") != v.end()) && (v.at("type") == "array")) {
            spdlog::debug("Found {} is array. Load its items", k);
            auto leaf = std::make_shared<Composite>(k, root_config);
            root_config->add(leaf);
            for (const auto& item : jconfig.at(k)) {
                Value val(Value::Type::STRING);
                // TODO: Make a reference instead of string
                val.set_string(item.get<String>());
                auto item_leaf = std::make_shared<Leaf>(item.get<String>(), val, leaf);
                leaf->add(item_leaf);
                // val.set_string(item.get<String>());
            }
        } // leaf node does not include "properties"
        else if (((v.find("properties") == v.end())
            && (v.find("patternProperties") == v.end()))
            // leaf node is not "object"
            || ((v.find("type") != v.end()) && (v.at("type") != "object"))) {
            // TODO: Check is the leaf is a string or other specific type
            spdlog::debug("Reached config leaf node '{}' -> '{}'", k, jconfig.at(k).get<String>());
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
            node = std::make_shared<Composite>(k, root_config);
            root_config->add(node);
            spdlog::debug("{} -> {}\n", node->getParent()->getName(), node->getName());
        }

        if (!parseAndLoadConfig(jconfig[k], v, node)) {
            spdlog::error("Failed to parse schema node '{}'", k);
            return false;
        }
    }

    spdlog::debug("Further processing");
    for (auto& [k, v] : jschema.items()) {
        if (k == "patternProperties") {
            spdlog::debug("Found patternProperties");
            if (!loadPatternProperties(jconfig, v, node)) {
                spdlog::error("Failed to parse schema node '{}'", k);
                return false;
            }
        }

        if (k == "properties") {
            spdlog::debug("Found properties");
            if (!parseAndLoadConfig(jconfig, v, node)) {
                spdlog::error("Failed to parse schema node '{}'", k);
                return false;
            }
        }

        if (jconfig.find(k) == jconfig.end()) {
            continue;
        }
    }

    return true;
}

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

// TODO:
// SharedPtr<Node> make_schema_tree(const nlohmann::json& jschema) {
//     auto root_node = Node::Factory("/").get_object<Node>();
//     // TODO: Allocate schema in Node composite?
//     return root_node;
// }

Config::Manager::Manager(StringView config_filename, StringView schema_filename, SharedPtr<RegistryClass>& registry)
 : m_config_filename { config_filename }, m_schema_filename { schema_filename } {

}

bool gPerformAction(SharedPtr<Config::Manager> config_mngr) {
    auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    if (!dependency_mngr->resolve(config_mngr->getRunningConfig(), ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        return false;
    }

    // Check if updated nodes pass all constraints
    try {
        // parser.parse("if (1 ~ 3) then 'true'", val);
        // parser.parse("if (1 = 3) then 9", val);
        std::cout << "Start processing\n";
        // Get all update-contraints and go through for-loop
        for (auto& xpath : ordered_nodes_by_xpath) {
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::debug("There isn't schema at node {}", xpath);
                continue;
            }

            auto config = config_mngr->getRunningConfig();
            auto constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, config);
            auto update_constraints = schema_node->findAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
            for (auto& update_constraint : update_constraints) {
                spdlog::debug("Processing update constraint '{}' at node {}", update_constraint, xpath);
                auto node = XPath::select2(config_mngr->getRunningConfig(), xpath);
                if (!node) {
                    spdlog::debug("Not node indicated by xpath {}", xpath);
                    continue;
                }

                if (!constraint_checker->validate(node, update_constraint)) {
                    spdlog::error("Failed to validate againts constraints {} for node {}", update_constraint, xpath);
                    return false;
                }
            }
        }

        // Perform action to remote server
        for (auto& xpath : ordered_nodes_by_xpath) {
            spdlog::debug("Select xpath {} from JSON config", xpath);
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
            auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                continue;
            }
            
            // auto json_node = nlohmann::json().parse(config_mngr->getConfigNode(xpath.substr(0, xpath.find_last_of('/'))));
            auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
            // std::cout << std::setw(4) << json_node << std::endl << std::endl;
            std::cout << std::setw(4) << json_node2 << std::endl << std::endl;
            spdlog::debug("Patch:");
            // std::cout << std::setw(4) << nlohmann::json(nlohmann::json(json_node).patch(nlohmann::json(json_node).diff({}, nlohmann::json(json_node)))) << std::endl;
            // auto diff = json_node.diff(json_node, json_node2);
            auto diff = json_node2.diff({}, json_node2);
            std::cout << std::setw(4) << diff << std::endl;
            diff[0]["op"] = "add";
            diff[0]["path"] = xpath;
            if (diff[0]["value"].is_object()) {
                // TODO: We should consider if we could put "string" with single value insead of empty object
                // "value":{"2":{}}
                // vs
                // "value": "2"
                for (auto it : diff[0]["value"].items()) {
                    it.value() = nlohmann::json::object();
                    // diff[0]["value"].front() = nlohmann::json::object();
                }
            }

            std::cout << std::setw(4) << diff[0] << std::endl;
            auto server_addr = server_addr_attr.front();
            spdlog::debug("Connect to server: {}", server_addr);
            httplib::Client cli(server_addr);
            auto path = action_attr.front();
            auto body = diff[0].dump();
            auto content_type = "application/json";
            spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
            auto result = cli.Post(path, body, content_type);
            if (!result) {
                spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
                // TODO: Rollback all changes
                return false;
            }

            spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
        }
    }
    catch (std::bad_any_cast& ex) {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
        return false;
    }

    return true;
}

bool Config::Manager::load() {
    std::ifstream schema_ifs(m_schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    std::ifstream config_ifs(m_config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);

    if (!validateJsonConfig(jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    auto config_mngr = shared_from_this();
    auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    auto root_config = std::make_shared<Composite>(ROOT_TREE_CONFIG_NAME);
    auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!parseAndLoadConfig(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'properties' node");
            return false;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return false;
        }

        // if (!gPerformAction(shared_from_this())) {
        //     spdlog::error("Failed to perform action on the config");
        //     return false;
        // }

        m_running_config = root_config;
        g_running_jconfig = jconfig;
        return true;
    }

    properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!loadPatternProperties(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'patternProperties' node");
            return false;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return false;
        }

        // if (!gPerformAction(shared_from_this())) {
        //     spdlog::error("Failed to perform action on the config");
        //     return false;
        // }

        m_running_config = root_config;
        g_running_jconfig = jconfig;
        return true;
    }

    spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
    return false;
}

String Config::Manager::getConfigNode(const String& xpath) {
    std::ifstream config_ifs(m_config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);
    return jconfig[nlohmann::json::json_pointer(xpath)].dump();
}

#include <iostream>
SharedPtr<SchemaNode> Config::Manager::getSchemaByXPath(const String& xpath) {
    // spdlog::debug("Find schema for xpath: {}", xpath);
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
        Config::PropertyName::ACTION, Config::PropertyName::ACTION_ON_DELETE_PATH, Config::PropertyName::ACTION_ON_UPDATE_PATH, Config::PropertyName::ACTION_SERVER_ADDRESS,
        Config::PropertyName::DEFAULT, Config::PropertyName::DESCRIPTION, Config::PropertyName::DELETE_CONSTRAINTS, Config::PropertyName::REFERENCE, Config::PropertyName::UPDATE_CONSTRAINTS, Config::PropertyName::UPDATE_DEPENDENCIES
    };

    auto load_schema_attributes = [](nlohmann::json& schema, String& node_name, SharedPtr<SchemaComposite>& root_schema_node) {
        auto schema_node = std::make_shared<SchemaComposite>(node_name, root_schema_node);
        auto attributes = nlohmann::json(schema.patch(schema.diff({}, schema)));
        for (auto& [k, v] : attributes.items()) {
            if (SUPPORTED_ATTRIBUTES.find(k) != SUPPORTED_ATTRIBUTES.end()) {
                if (k == Config::PropertyName::ACTION) {
                    for (auto& [action_attr, action_val] : v.items()) {
                        schema_node->addAttr(String(Config::PropertyName::ACTION) + "-" + action_attr, action_val);
                    }
                }
                else if (v.is_array()) {
                    for (auto& [_, item] : v.items()) {
                        schema_node->addAttr(k, item);
                    }
                }
                else {
                    schema_node->addAttr(k, v);
                }
            }
        }

        root_schema_node->add(schema_node);
        return schema_node;
    };

    String schema_xpath_composed = {};
    // TODO: Create node hierarchy dynamically. Save in cache. Chek cache next time before traverse
    for (auto token : xpath_tokens) {
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                schema_xpath_composed += "/properties/" + token;
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
                    return {};
                }

                schema_node = load_schema_attributes(schema, token, schema_node);
            }
            else {
                bool pattern_matched = false;
                for (auto& [k, v] : schema["patternProperties"].items()) {
                    if (std::regex_match(token, Regex { k })) {
                        pattern_matched = true;
                        schema_xpath_composed += "/patternProperties/" + k;
                        auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                        schema = jschema[selector];
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
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
            schema_node = load_schema_attributes(schema, token, schema_node);
            // TODO: Create schema node and add to cache
        }
    }

    // TODO: Load all schema nodes
    return schema_node;
}

nlohmann::json getSchemaByXPath2(const String& xpath, const String& schema_filename) {
    std::ifstream schema_ifs(schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    auto root_properties_it = jschema.find("properties");
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return {};
    }

    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse2(xpath);

    String schema_xpath_composed = {};
    // TODO: Create node hierarchy dynamically. Save in cache. Chek cache next time before traverse
    for (auto token : xpath_tokens) {
        // spdlog::debug("Loaded schema:\n{}", schema.dump());
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                schema_xpath_composed += "/properties/" + token;
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::error("Not found token '{}' in schema:\n{}", token, schema.dump());
                    return {};
                }
            }
            else {
                bool pattern_matched = false;
                for (auto& [k, v] : schema["patternProperties"].items()) {
                    if (std::regex_match(token, Regex { k })) {
                        pattern_matched = true;
                        schema_xpath_composed += "/patternProperties/" + k;
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
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
            // TODO: Create schema node and add to cache
        }
    }

    return schema;
}

String Config::Manager::getConfigDiff(const String& patch) {
    std::ifstream schema_ifs(m_schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    auto new_jconfig = nlohmann::json().parse(patch);

    if (!validateJsonConfig(new_jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return {};
    }

    auto diff2 = nlohmann::json::diff(g_running_jconfig, new_jconfig);
    std::cout << "Diff2:\n" << std::setw(4) << diff2 << std::endl << std::endl;
    auto diff_patch = g_running_jconfig.patch(diff2);
    std::cout << "Patch:\n" << std::setw(4) << diff_patch << std::endl << std::endl;
    auto copy_jconfig = g_running_jconfig;
    copy_jconfig.merge_patch(diff_patch);
    auto diff = nlohmann::json::diff(g_running_jconfig, copy_jconfig);
    std::cout << "Diff:\n" << std::setw(4) << diff << std::endl << std::endl;
    // g_running_jconfig.merge_patch(patch);
    // std::cout << "Patched:\n" << std::setw(4) << g_running_jconfig << std::endl << std::endl;
    return diff.dump();
}

SharedPtr<Node> Config::Manager::getRunningConfig() {
    return m_running_config;
}

class SubnodesGetterVisitor : public Visitor {
public:
    virtual ~SubnodesGetterVisitor() = default;

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        // auto root_xpath = XPath::to_string2(node->getParent());
        // if (root_xpath == m_root_xpath) {
        //     // Allocate new bucket
        //     m_subnodes_xpath.push_back(List<String>());
        // }

        m_subnodes_xpath.emplace(XPath::to_string2(node));
        return true;
    };

    Set<String> getSubnodesXPath() { return m_subnodes_xpath; }

private:
    Set<String> m_subnodes_xpath;
    // String m_root_xpath;
};

class PrintVisitor : public Visitor {
  public:
    virtual ~PrintVisitor() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        spdlog::debug("{}", node->getName());
        return true;
    }
};

class NodesCollectorVisitor : public Visitor {
public:
    virtual ~NodesCollectorVisitor() = default;
    NodesCollectorVisitor(const Set<String>& nodes_to_collect)
    : m_nodes_to_collect { nodes_to_collect },
      m_root_config { std::make_shared<Composite>(Config::ROOT_TREE_CONFIG_NAME) } { }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        auto xpath = XPath::to_string2(node);
        if (xpath.empty()) {
            spdlog::warn("Cannot convert node {} to xpath", node->getName());
            return true;
        }

        auto node_it = m_nodes_to_collect.find(xpath);
        if (node_it != m_nodes_to_collect.end()) {
            spdlog::debug("Found xpath {} in collect of nodes", xpath);
            auto xpath_tokens = XPath::parse4(xpath);
            xpath_tokens.pop_back();
            auto parent_node = m_root_config;
            if (!xpath_tokens.empty()) {
                auto parent_xpath = XPath::mergeTokens(xpath_tokens);
                parent_node = XPath::select2(m_root_config, parent_xpath);
                if (!parent_node) {
                    spdlog::error("Failed to find parent node {} in new config", parent_xpath);
                    return false;
                }
            }

            auto new_node = node->makeCopy(parent_node);
            if (auto parent_node_ptr = std::dynamic_pointer_cast<Composite>(parent_node)) {
                parent_node_ptr->add(new_node);
            }

            m_node_by_xpath.emplace(xpath, new_node);
        }

        return true;
    };

    Map<String, SharedPtr<Node>> getNodesByXPath() { return m_node_by_xpath; }
    SharedPtr<Node> getRootConfig() { return m_root_config; }

private:
    Set<String> m_nodes_to_collect;
    Map<String, SharedPtr<Node>> m_node_by_xpath;
    SharedPtr<Node> m_root_config;
};

class NodeCopyMakerVisitor : public Visitor {
public:
    virtual ~NodeCopyMakerVisitor() = default;
    NodeCopyMakerVisitor()
    : m_root_config { std::make_shared<Composite>(Config::ROOT_TREE_CONFIG_NAME) } { }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        auto parent_node = node->getParent();
        if (!parent_node || (parent_node->getName() == Config::ROOT_TREE_CONFIG_NAME)) {
            parent_node = m_root_config;
        }

        auto new_node = node->makeCopy(parent_node);
        if (auto parent_node_ptr = std::dynamic_pointer_cast<Composite>(parent_node)) {
            parent_node_ptr->add(new_node);
        }

        return true;
    };

    SharedPtr<Node> getNodeConfigCopy() { return m_root_config; }

private:
    SharedPtr<Node> m_root_config;
};

bool gMakeCandidateConfigInternal(const String& patch, nlohmann::json& json_config, SharedPtr<Node>& node_config, const String& schema_filename, SharedPtr<Config::Manager>& config_mngr) {
    PrintVisitor print_visitor;
    spdlog::debug("Patching...");
    json_config = json_config.patch(nlohmann::json::parse(patch));
    spdlog::debug("Patched config:\n", json_config .dump(4));
    spdlog::debug("Diff config:\n", nlohmann::json::diff(json_config, json_config).dump(4));
    std::ifstream schema_ifs(schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);

    if (!validateJsonConfig(json_config, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    spdlog::debug("{}", nlohmann::json::parse(patch).flatten().dump(4));

    Set<String> xpaths_to_remove = {};
    Set<String> path_nodes = {};
    Set<String> subnodes_xpath = {};
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::info("There is not 'value' field in diff... restoring it from config");
            diff_item["value"] = json_config[nlohmann::json::json_pointer(path)];
        }

        spdlog::debug("{}", diff_item.dump(4));
        auto value = diff_item["value"];
        spdlog::debug("OP: {}", op);
        spdlog::debug("PATH: {}", path);
        spdlog::debug("VALUE: {}", value.dump());
        if (op != "remove") {
            continue;
        }

        auto xpath_tokens = XPath::parse3(path.get<String>());
        if (xpath_tokens.empty()) {
            spdlog::error("Path is empty!");
            return false;
        }

        String xpath = Config::ROOT_TREE_CONFIG_NAME;
        // 'path_nodes' will include set of xpath as follows:
        // /interface -> /interface/gigabit-ethernet -> /interface/gigabit-ethernet/ge-1 -> ...
        while (xpath_tokens.size() > 1) {
            if (xpath.at(xpath.size() - 1) != '/') {
                xpath += "/";
            }

            xpath += xpath_tokens.front();
            xpath_tokens.pop();
            path_nodes.insert(xpath);
        }

        auto root_node_name_to_remove = xpath_tokens.front();
        // auto root_node_name_to_remove = value.begin().key();
        auto root_node_xpath_to_remove = xpath + "/" + root_node_name_to_remove;
        // auto root_node_xpath_to_remove = path.get<String>() + "/" + root_node_name_to_remove;
        auto root_node_to_remove = XPath::select2(node_config, root_node_xpath_to_remove);
        if (!root_node_to_remove) {
            spdlog::error("Failed to find root node to remove!");
            return false;
        }

        SubnodesGetterVisitor subnodes_getter_visitor;
        root_node_to_remove->accept(subnodes_getter_visitor);
        subnodes_xpath.merge(subnodes_getter_visitor.getSubnodesXPath());
        subnodes_xpath.insert(root_node_xpath_to_remove);
        // xpaths_to_remove.merge(path_nodes); // Extracts elements from source
        std::for_each(path_nodes.begin(), path_nodes.end(), [&xpaths_to_remove](const String& xpath) {
            xpaths_to_remove.insert(xpath);
        });
        // xpaths_to_remove.merge(subnodes_xpath);
        std::for_each(subnodes_xpath.begin(), subnodes_xpath.end(), [&xpaths_to_remove](const String& xpath) {
            xpaths_to_remove.insert(xpath);
        });
    }

    spdlog::debug("Found list of subnodes to remove:");
    for (auto& subnode : xpaths_to_remove) {
        // Check if all nodes (xpath) exists in config
        if (!XPath::select2(node_config, subnode)) {
            spdlog::error("There is not {} in config", subnode);
            return false;
        }
    }

    NodesCollectorVisitor nodes_collector_visitor(xpaths_to_remove);
    node_config->accept(nodes_collector_visitor);
    auto nodes_by_xpath = nodes_collector_visitor.getNodesByXPath();

    auto root_config_to_remove = nodes_collector_visitor.getRootConfig();

    auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    if (!dependency_mngr->resolve(root_config_to_remove, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        return false;
    }

    ordered_nodes_by_xpath.remove_if([&path_nodes](const String& xpath) {
        return path_nodes.find(xpath) != path_nodes.end();
    });

    spdlog::debug("Ordered nodes to remove after filtered out nodes from path_nodes:");
    for (const auto& xpath : ordered_nodes_by_xpath) {
        spdlog::debug("{}", xpath);
    }

    // Select only these which has appear in path of remove changes
    // This is hack when parent of removed node has more childrens so all childrens are marked to remove
    // "gigabit-ethernet": {
    //     "ge-1": {
    //         "speed": "100G"
    //     },
    // -   "ge-4": {
    // -       "speed": "100G"
    // -   }
    // }
    //
    // "ge-4" node is marked to remove but "ge-1" is also in ordered_nodes_by_xpath 
    Set<String> not_marked_to_be_removed = {};
    for (const auto& xpath : ordered_nodes_by_xpath) {
        bool should_be_removed = false;
        for (auto& diff_item : nlohmann::json::parse(patch)) {
            auto op = diff_item["op"];
            if (op == "remove") {
                auto path = diff_item["path"];
                if (xpath.find(path) != String::npos) {
                    should_be_removed = true;
                    break;
                }
            }
        }

        if (!should_be_removed) {
            spdlog::debug("{} is not marked to be removed", xpath);
            not_marked_to_be_removed.insert(xpath);
        }
    }

    ordered_nodes_by_xpath.remove_if([&not_marked_to_be_removed](const String& xpath) {
        return not_marked_to_be_removed.find(xpath) != not_marked_to_be_removed.end();
    });

    ordered_nodes_by_xpath.reverse();
    auto constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, root_config_to_remove);
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::error("Failed to find schema node for {}", xpath);
            return false;
        }

        auto delete_constraint_attr = schema_node->findAttr("delete-constraints");
        auto node_to_remove = XPath::select2(root_config_to_remove, xpath);
        if (!node_to_remove) {
            spdlog::error("Failed to find node to remove {}", xpath);
            return false;
        }

        for (auto& constraint : delete_constraint_attr) {
            if (!constraint_checker->validate(node_to_remove, constraint)) {
                spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
                return false;
            }
        }

        auto parent_node = std::dynamic_pointer_cast<Composite>(node_to_remove->getParent());
        if (!parent_node->remove(node_to_remove->getName())) {
            spdlog::error("Failed to remove node {} from collection of {}", node_to_remove->getName(), parent_node->getName());
            return false;
        }
        
        node_to_remove.reset();
    }

    spdlog::debug("Nodes to perform delete action:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
        auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            spdlog::debug("{} has NOT delete action", xpath);
            continue;
        }

        spdlog::debug("{} HAS delete action", xpath);
        // auto json_node = nlohmann::json().parse(config_mngr->getConfigNode(xpath.substr(0, xpath.find_last_of('/'))));
        auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
        // std::cout << std::setw(4) << json_node << std::endl << std::endl;
        // std::cout << std::setw(4) << nlohmann::json(nlohmann::json(json_node).patch(nlohmann::json(json_node).diff({}, nlohmann::json(json_node)))) << std::endl;
        // auto diff = json_node.diff(json_node, json_node2);
        auto diff = json_node2.diff({}, json_node2);
        std::cout << std::setw(4) << diff << std::endl;
        diff[0]["op"] = "remove";
        diff[0]["path"] = xpath;
        if (diff[0]["value"].is_object()) {
            diff[0]["value"] = nullptr;
        }

        std::cout << std::setw(4) << diff[0] << std::endl;
        auto server_addr = server_addr_attr.front();
        spdlog::debug("Connect to server: {}", server_addr);
        httplib::Client cli(server_addr);
        auto path = action_attr.front();
        auto body = diff[0].dump();
        auto content_type = "application/json";
        spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
        auto result = cli.Post(path, body, content_type);
        if (!result) {
            spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
            // TODO: Rollback all changes
            continue;
        }

        spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
    }

    for (auto& xpath : ordered_nodes_by_xpath) {
        auto node = XPath::select2(node_config, xpath);
        if (!node) {
            spdlog::error("Failed to select node to remove at xpath {}", xpath);
            return false;
        }

        auto node_parent = node->getParent();
        if (auto node_parent_ptr = std::dynamic_pointer_cast<Composite>(node_parent)) {
            if (!node_parent_ptr->remove(node->getName())) {
                spdlog::error("Failed to remove node {} from collection of nodes come from its parent {}", node->getName(), node_parent_ptr->getName());
                return false;
            }
        }

        if (auto node_leaf_ptr = std::dynamic_pointer_cast<Leaf>(node)) {
            spdlog::debug("Node {} is a leaf so clear its value", node_leaf_ptr->getName());
        }

        node.reset();
    }

    spdlog::debug("Successfully removed all changes");

    spdlog::debug("New config dump:");
    std::cout << std::setw(4) << json_config << std::endl;

    path_nodes.clear();
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::info("There is not 'value' field in diff... restoring it from config");
            diff_item["value"] = json_config[nlohmann::json::json_pointer(path)];
        }
        // std::cout << __LINE__ << std::setw(4) << diff_item << std::endl;
        auto value = diff_item["value"];
        spdlog::debug("OP: {}", op);
        spdlog::debug("PATH: {}", path);
        spdlog::debug("VALUE: {}", value.dump());
        if (op == "remove") {
            continue;
        }

        auto xpath_tokens = XPath::parse3(path);
        if (xpath_tokens.empty()) {
            spdlog::error("Path is empty!");
            return false;
        }

        String xpath = Config::ROOT_TREE_CONFIG_NAME;
        while (xpath_tokens.size() > 1) {
            if (xpath.at(xpath.size() - 1) != '/') {
                xpath += "/";
            }

            xpath += xpath_tokens.front();
            xpath_tokens.pop();
            path_nodes.insert(xpath);
            spdlog::debug("Added {} to path nodes", xpath);
        }

        spdlog::debug("XPath to parent of new node: {}", xpath);

        auto jschema = getSchemaByXPath2(xpath, schema_filename);
        if (jschema == nlohmann::json({})) {
            spdlog::error("Not found schema at xpath {}", xpath);
            // TODO: Rollback all changes
            return false;
        }

        auto root_node = XPath::select2(node_config, xpath);
        if (!root_node) {
            spdlog::error("Failed to find node at xpath {}", xpath);
            return false;
        }

        spdlog::debug("JSON config before load:");
        std::cout << std::setw(4) << json_config[nlohmann::json::json_pointer(xpath)] << std::endl;
        auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
        if (properties_it != jschema.end()) {
            auto node = std::dynamic_pointer_cast<Composite>(root_node);
            if (!parseAndLoadConfig(json_config[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
                spdlog::error("Failed to load config based on 'properties' node");
                json_config = nlohmann::json();
                return false;
            }
        }
        else {
            properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
            if (properties_it != jschema.end()) {
                auto node = std::dynamic_pointer_cast<Composite>(root_node);
                if (!loadPatternProperties(json_config[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
                    spdlog::error("Failed to load config based on 'patternProperties' node");
                    g_candidate_jconfig = nlohmann::json();
                    return false;
                }
            }
        }

        spdlog::debug("JSON config after load:");
        std::cout << std::setw(4) << json_config[nlohmann::json::json_pointer(xpath)] << std::endl;
    }

    ordered_nodes_by_xpath.clear();
    if (!dependency_mngr->resolve(node_config, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        return false;
    }
    
    spdlog::debug("Nodes ordered by its xpath:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        spdlog::debug("{}", xpath);
    }

    // Exclude nodes which had been loaded already
    Set<String> not_marked_to_be_added = {};
    for (const auto& xpath : ordered_nodes_by_xpath) {
        bool should_be_added = false;
        for (auto& diff_item : nlohmann::json::parse(patch)) {
            auto op = diff_item["op"];
            if (op != "remove") {
                auto path = diff_item["path"];
                if (xpath.find(path) != String::npos) {
                    should_be_added = true;
                    break;
                }
            }
        }

        if (!should_be_added) {
            spdlog::debug("{} is not marked to be added", xpath);
            not_marked_to_be_added.insert(xpath);
        }
    }

    ordered_nodes_by_xpath.remove_if([&not_marked_to_be_added](const String& xpath) {
        return not_marked_to_be_added.find(xpath) != not_marked_to_be_added.end();
    });

    spdlog::debug("Nodes ordered by its xpath after remove the nodes which should be updated:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        spdlog::debug("{}", xpath);
    }

    constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, node_config);
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::error("Failed to find schema node for {}", xpath);
            return false;
        }

        auto constraint_attr = schema_node->findAttr("update-constraints");
        auto node_to_check = XPath::select2(node_config, xpath);
        if (!node_to_check) {
            spdlog::error("Failed to find node to check {}", xpath);
            return false;
        }

        for (auto& constraint : constraint_attr) {
            if (!constraint_checker->validate(node_to_check, constraint)) {
                spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
                return false;
            }
        }
    }

    spdlog::debug("Candidate JSON config:\n{}", json_config.dump(4));
    spdlog::debug("Nodes to perform update action:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
        auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            continue;
        }

        auto json_node2 = nlohmann::json().parse(json_config[nlohmann::json::json_pointer(xpath)].dump());
        std::cout << std::setw(4) << json_node2 << std::endl << std::endl;
        auto diff = json_node2.diff({}, json_node2);
        std::cout << std::setw(4) << diff << std::endl;
        diff[0]["op"] = XPath::select2(node_config, xpath) ? "replace" : "add";
        diff[0]["path"] = xpath;
        if (diff[0]["value"].is_object()) {
            diff[0]["value"] = nullptr;
        }

        auto server_addr = server_addr_attr.front();
        spdlog::debug("Connect to server: {}", server_addr);
        httplib::Client cli(server_addr);
        auto path = action_attr.front();
        auto body = diff[0].dump();
        auto content_type = "application/json";
        spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
        auto result = cli.Post(path, body, content_type);
        if (!result) {
            spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
            // TODO: Rollback all changes
            return false;
        }
    }

    return true;
}

bool Config::Manager::makeCandidateConfig(const String& patch) {
    
    NodeCopyMakerVisitor node_copy_maker;
    m_running_config->accept(node_copy_maker);
    auto candidate_config = node_copy_maker.getNodeConfigCopy();
    PrintVisitor print_visitor;

    auto candidate_jconfig = g_running_jconfig;
    auto config_mngr = shared_from_this();
    if (!gMakeCandidateConfigInternal(patch, candidate_jconfig, candidate_config, m_schema_filename, config_mngr)) {
        spdlog::error("Failed to make candidate config");
        return false;
    }
    // spdlog::debug("Patching...");
    // auto patched_jconfig = candidate_jconfig.patch(nlohmann::json::parse(patch));
    // spdlog::debug("Patched config:\n", patched_jconfig .dump(4));
    // spdlog::debug("Diff config:\n", nlohmann::json::diff(patched_jconfig, g_running_jconfig).dump(4));
    // std::ifstream schema_ifs(m_schema_filename);
    // nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    // auto new_jconfig = patched_jconfig;

    // if (!validateJsonConfig(new_jconfig, jschema)) {
    //     spdlog::error("Failed to validate config");
    //     return false;
    // }

    // spdlog::debug("{}", nlohmann::json::parse(patch).flatten().dump(4));

    // Set<String> xpaths_to_remove = {};
    // Set<String> path_nodes = {};
    // Set<String> subnodes_xpath = {};
    // for (auto& diff_item : nlohmann::json::parse(patch)) {
    //     auto op = diff_item["op"];
    //     auto path = diff_item["path"];
    //     if (diff_item.find("value") == diff_item.end()) {
    //         spdlog::info("There is not 'value' field in diff... restoring it from config");
    //         diff_item["value"] = candidate_jconfig[nlohmann::json::json_pointer(path)];
    //     }

    //     spdlog::debug("{}", diff_item.dump(4));
    //     auto value = diff_item["value"];
    //     spdlog::debug("OP: {}", op);
    //     spdlog::debug("PATH: {}", path);
    //     spdlog::debug("VALUE: {}", value.dump());
    //     if (op != "remove") {
    //         continue;
    //     }

    //     auto xpath_tokens = XPath::parse3(path.get<String>());
    //     if (xpath_tokens.empty()) {
    //         spdlog::error("Path is empty!");
    //         return false;
    //     }

    //     String xpath = ROOT_TREE_CONFIG_NAME;
    //     // 'path_nodes' will include set of xpath as follows:
    //     // /interface -> /interface/gigabit-ethernet -> /interface/gigabit-ethernet/ge-1 -> ...
    //     while (xpath_tokens.size() > 1) {
    //         if (xpath.at(xpath.size() - 1) != '/') {
    //             xpath += "/";
    //         }

    //         xpath += xpath_tokens.front();
    //         xpath_tokens.pop();
    //         path_nodes.insert(xpath);
    //     }

    //     auto root_node_name_to_remove = xpath_tokens.front();
    //     // auto root_node_name_to_remove = value.begin().key();
    //     auto root_node_xpath_to_remove = xpath + "/" + root_node_name_to_remove;
    //     // auto root_node_xpath_to_remove = path.get<String>() + "/" + root_node_name_to_remove;
    //     auto root_node_to_remove = XPath::select2(candidate_config, root_node_xpath_to_remove);
    //     if (!root_node_to_remove) {
    //         spdlog::error("Failed to find root node to remove!");
    //         return false;
    //     }

    //     SubnodesGetterVisitor subnodes_getter_visitor;
    //     root_node_to_remove->accept(subnodes_getter_visitor);
    //     subnodes_xpath.merge(subnodes_getter_visitor.getSubnodesXPath());
    //     subnodes_xpath.insert(root_node_xpath_to_remove);
    //     // xpaths_to_remove.merge(path_nodes); // Extracts elements from source
    //     std::for_each(path_nodes.begin(), path_nodes.end(), [&xpaths_to_remove](const String& xpath) {
    //         xpaths_to_remove.insert(xpath);
    //     });
    //     // xpaths_to_remove.merge(subnodes_xpath);
    //     std::for_each(subnodes_xpath.begin(), subnodes_xpath.end(), [&xpaths_to_remove](const String& xpath) {
    //         xpaths_to_remove.insert(xpath);
    //     });
    // }

    // spdlog::debug("Found list of subnodes to remove:");
    // for (auto& subnode : xpaths_to_remove) {
    //     // Check if all nodes (xpath) exists in config
    //     if (!XPath::select2(candidate_config, subnode)) {
    //         spdlog::error("There is not {} in config", subnode);
    //         return false;
    //     }
    // }

    // NodesCollectorVisitor nodes_collector_visitor(xpaths_to_remove);
    // candidate_config->accept(nodes_collector_visitor);
    // auto nodes_by_xpath = nodes_collector_visitor.getNodesByXPath();

    // auto root_config_to_remove = nodes_collector_visitor.getRootConfig();

    // auto config_mngr = shared_from_this();
    // auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    // List<String> ordered_nodes_by_xpath = {};
    // if (!dependency_mngr->resolve(root_config_to_remove, ordered_nodes_by_xpath)) {
    //     spdlog::error("Failed to resolve nodes dependency");
    //     return false;
    // }

    // ordered_nodes_by_xpath.remove_if([&path_nodes](const String& xpath) {
    //     return path_nodes.find(xpath) != path_nodes.end();
    // });

    // spdlog::debug("Ordered nodes to remove after filtered out nodes from path_nodes:");
    // for (const auto& xpath : ordered_nodes_by_xpath) {
    //     spdlog::debug("{}", xpath);
    // }

    // // Select only these which has appear in path of remove changes
    // // This is hack when parent of removed node has more childrens so all childrens are marked to remove
    // // "gigabit-ethernet": {
    // //     "ge-1": {
    // //         "speed": "100G"
    // //     },
    // // -   "ge-4": {
    // // -       "speed": "100G"
    // // -   }
    // // }
    // //
    // // "ge-4" node is marked to remove but "ge-1" is also in ordered_nodes_by_xpath 
    // Set<String> not_marked_to_be_removed = {};
    // for (const auto& xpath : ordered_nodes_by_xpath) {
    //     bool should_be_removed = false;
    //     for (auto& diff_item : nlohmann::json::parse(patch)) {
    //         auto op = diff_item["op"];
    //         if (op == "remove") {
    //             auto path = diff_item["path"];
    //             if (xpath.find(path) != String::npos) {
    //                 should_be_removed = true;
    //                 break;
    //             }
    //         }
    //     }

    //     if (!should_be_removed) {
    //         spdlog::debug("{} is not marked to be removed", xpath);
    //         not_marked_to_be_removed.insert(xpath);
    //     }
    // }

    // ordered_nodes_by_xpath.remove_if([&not_marked_to_be_removed](const String& xpath) {
    //     return not_marked_to_be_removed.find(xpath) != not_marked_to_be_removed.end();
    // });

    // ordered_nodes_by_xpath.reverse();
    // auto constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, root_config_to_remove);
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     auto schema_node = getSchemaByXPath(xpath);
    //     if (!schema_node) {
    //         spdlog::error("Failed to find schema node for {}", xpath);
    //         return false;
    //     }

    //     auto delete_constraint_attr = schema_node->findAttr("delete-constraints");
    //     auto node_to_remove = XPath::select2(root_config_to_remove, xpath);
    //     if (!node_to_remove) {
    //         spdlog::error("Failed to find node to remove {}", xpath);
    //         return false;
    //     }

    //     for (auto& constraint : delete_constraint_attr) {
    //         if (!constraint_checker->validate(node_to_remove, constraint)) {
    //             spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
    //             return false;
    //         }
    //     }

    //     auto parent_node = std::dynamic_pointer_cast<Composite>(node_to_remove->getParent());
    //     if (!parent_node->remove(node_to_remove->getName())) {
    //         spdlog::error("Failed to remove node {} from collection of {}", node_to_remove->getName(), parent_node->getName());
    //         return false;
    //     }
        
    //     node_to_remove.reset();
    // }

    // spdlog::debug("Nodes to perform delete action:");
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     auto schema_node = config_mngr->getSchemaByXPath(xpath);
    //     auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
    //     auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
    //     if (action_attr.empty() || server_addr_attr.empty()) {
    //         spdlog::debug("{} has NOT delete action", xpath);
    //         continue;
    //     }

    //     spdlog::debug("{} HAS delete action", xpath);
    //     // auto json_node = nlohmann::json().parse(config_mngr->getConfigNode(xpath.substr(0, xpath.find_last_of('/'))));
    //     auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
    //     // std::cout << std::setw(4) << json_node << std::endl << std::endl;
    //     // std::cout << std::setw(4) << nlohmann::json(nlohmann::json(json_node).patch(nlohmann::json(json_node).diff({}, nlohmann::json(json_node)))) << std::endl;
    //     // auto diff = json_node.diff(json_node, json_node2);
    //     auto diff = json_node2.diff({}, json_node2);
    //     std::cout << std::setw(4) << diff << std::endl;
    //     diff[0]["op"] = "remove";
    //     diff[0]["path"] = xpath;
    //     if (diff[0]["value"].is_object()) {
    //         diff[0]["value"] = nullptr;
    //     }

    //     std::cout << std::setw(4) << diff[0] << std::endl;
    //     auto server_addr = server_addr_attr.front();
    //     spdlog::debug("Connect to server: {}", server_addr);
    //     httplib::Client cli(server_addr);
    //     auto path = action_attr.front();
    //     auto body = diff[0].dump();
    //     auto content_type = "application/json";
    //     spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
    //     auto result = cli.Post(path, body, content_type);
    //     if (!result) {
    //         spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
    //         // TODO: Rollback all changes
    //         continue;
    //     }

    //     spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
    // }

    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     auto node = XPath::select2(candidate_config, xpath);
    //     if (!node) {
    //         spdlog::error("Failed to select node to remove at xpath {}", xpath);
    //         return false;
    //     }

    //     auto node_parent = node->getParent();
    //     if (auto node_parent_ptr = std::dynamic_pointer_cast<Composite>(node_parent)) {
    //         if (!node_parent_ptr->remove(node->getName())) {
    //             spdlog::error("Failed to remove node {} from collection of nodes come from its parent {}", node->getName(), node_parent_ptr->getName());
    //             return false;
    //         }
    //     }

    //     if (auto node_leaf_ptr = std::dynamic_pointer_cast<Leaf>(node)) {
    //         spdlog::debug("Node {} is a leaf so clear its value", node_leaf_ptr->getName());
    //     }

    //     node.reset();
    // }

    // spdlog::debug("Successfully removed all changes");

    // spdlog::debug("New config dump:");
    // std::cout << std::setw(4) << new_jconfig << std::endl;

    // path_nodes.clear();
    // for (auto& diff_item : nlohmann::json::parse(patch)) {
    //     auto op = diff_item["op"];
    //     auto path = diff_item["path"];
    //     if (diff_item.find("value") == diff_item.end()) {
    //         spdlog::info("There is not 'value' field in diff... restoring it from config");
    //         diff_item["value"] = candidate_jconfig[nlohmann::json::json_pointer(path)];
    //     }
    //     // std::cout << __LINE__ << std::setw(4) << diff_item << std::endl;
    //     auto value = diff_item["value"];
    //     spdlog::debug("OP: {}", op);
    //     spdlog::debug("PATH: {}", path);
    //     spdlog::debug("VALUE: {}", value.dump());
    //     if (op == "remove") {
    //         continue;
    //     }

    //     auto xpath_tokens = XPath::parse3(path);
    //     if (xpath_tokens.empty()) {
    //         spdlog::error("Path is empty!");
    //         return false;
    //     }

    //     String xpath = ROOT_TREE_CONFIG_NAME;
    //     while (xpath_tokens.size() > 1) {
    //         if (xpath.at(xpath.size() - 1) != '/') {
    //             xpath += "/";
    //         }

    //         xpath += xpath_tokens.front();
    //         xpath_tokens.pop();
    //         path_nodes.insert(xpath);
    //         spdlog::debug("Added {} to path nodes", xpath);
    //     }

    //     spdlog::debug("XPath to parent of new node: {}", xpath);

    //     auto jschema = getSchemaByXPath2(xpath, m_schema_filename);
    //     if (jschema == nlohmann::json({})) {
    //         spdlog::error("Not found schema at xpath {}", xpath);
    //         // TODO: Rollback all changes
    //         return false;
    //     }

    //     auto root_node = XPath::select2(candidate_config, xpath);
    //     if (!root_node) {
    //         spdlog::error("Failed to find node at xpath {}", xpath);
    //         return false;
    //     }

    //     spdlog::debug("JSON config before load:");
    //     std::cout << std::setw(4) << new_jconfig[nlohmann::json::json_pointer(xpath)] << std::endl;
    //     auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    //     if (properties_it != jschema.end()) {
    //         auto node = std::dynamic_pointer_cast<Composite>(root_node);
    //         if (!parseAndLoadConfig(new_jconfig[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
    //             spdlog::error("Failed to load config based on 'properties' node");
    //             g_candidate_jconfig = nlohmann::json();
    //             return false;
    //         }
    //     }
    //     else {
    //         properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
    //         if (properties_it != jschema.end()) {
    //             auto node = std::dynamic_pointer_cast<Composite>(root_node);
    //             if (!loadPatternProperties(new_jconfig[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
    //                 spdlog::error("Failed to load config based on 'patternProperties' node");
    //                 g_candidate_jconfig = nlohmann::json();
    //                 return false;
    //             }
    //         }
    //     }

    //     spdlog::debug("JSON config after load:");
    //     std::cout << std::setw(4) << new_jconfig[nlohmann::json::json_pointer(xpath)] << std::endl;
    // }

    // ordered_nodes_by_xpath.clear();
    // if (!dependency_mngr->resolve(candidate_config, ordered_nodes_by_xpath)) {
    //     spdlog::error("Failed to resolve nodes dependency");
    //     return false;
    // }
    
    // spdlog::debug("Nodes ordered by its xpath:");
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     spdlog::debug("{}", xpath);
    // }

    // // Exclude nodes which had been loaded already
    // Set<String> not_marked_to_be_added = {};
    // for (const auto& xpath : ordered_nodes_by_xpath) {
    //     bool should_be_added = false;
    //     for (auto& diff_item : nlohmann::json::parse(patch)) {
    //         auto op = diff_item["op"];
    //         if (op != "remove") {
    //             auto path = diff_item["path"];
    //             if (xpath.find(path) != String::npos) {
    //                 should_be_added = true;
    //                 break;
    //             }
    //         }
    //     }

    //     if (!should_be_added) {
    //         spdlog::debug("{} is not marked to be added", xpath);
    //         not_marked_to_be_added.insert(xpath);
    //     }
    // }

    // ordered_nodes_by_xpath.remove_if([&not_marked_to_be_added](const String& xpath) {
    //     return not_marked_to_be_added.find(xpath) != not_marked_to_be_added.end();
    // });

    // spdlog::debug("Nodes ordered by its xpath after remove the nodes which should be updated:");
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     spdlog::debug("{}", xpath);
    // }

    // constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, candidate_config);
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     auto schema_node = getSchemaByXPath(xpath);
    //     if (!schema_node) {
    //         spdlog::error("Failed to find schema node for {}", xpath);
    //         return false;
    //     }

    //     auto constraint_attr = schema_node->findAttr("update-constraints");
    //     auto node_to_check = XPath::select2(candidate_config, xpath);
    //     if (!node_to_check) {
    //         spdlog::error("Failed to find node to check {}", xpath);
    //         return false;
    //     }

    //     for (auto& constraint : constraint_attr) {
    //         if (!constraint_checker->validate(node_to_check, constraint)) {
    //             spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
    //             return false;
    //         }
    //     }
    // }

    // spdlog::debug("Candidate JSON config:\n{}", new_jconfig.dump(4));
    // spdlog::debug("Nodes to perform update action:");
    // for (auto& xpath : ordered_nodes_by_xpath) {
    //     auto schema_node = config_mngr->getSchemaByXPath(xpath);
    //     auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
    //     auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
    //     if (action_attr.empty() || server_addr_attr.empty()) {
    //         continue;
    //     }

    //     auto json_node2 = nlohmann::json().parse(new_jconfig[nlohmann::json::json_pointer(xpath)].dump());
    //     std::cout << std::setw(4) << json_node2 << std::endl << std::endl;
    //     auto diff = json_node2.diff({}, json_node2);
    //     std::cout << std::setw(4) << diff << std::endl;
    //     diff[0]["op"] = XPath::select2(m_running_config, xpath) ? "replace" : "add";
    //     diff[0]["path"] = xpath;
    //     if (diff[0]["value"].is_object()) {
    //         diff[0]["value"] = nullptr;
    //     }

    //     auto server_addr = server_addr_attr.front();
    //     spdlog::debug("Connect to server: {}", server_addr);
    //     httplib::Client cli(server_addr);
    //     auto path = action_attr.front();
    //     auto body = diff[0].dump();
    //     auto content_type = "application/json";
    //     spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
    //     auto result = cli.Post(path, body, content_type);
    //     if (!result) {
    //         spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
    //         // TODO: Rollback all changes
    //         return false;
    //     }
    // }

    m_candidate_config = candidate_config;
    // g_candidate_jconfig = new_jconfig;
    g_candidate_jconfig = candidate_jconfig;
    m_is_candidate_config_ready = true;

    spdlog::debug("Candidate JSON config:\n{}", g_candidate_jconfig.dump(4));
    spdlog::debug("Candidate nodes config:");
    m_candidate_config->accept(print_visitor);

    return true;
}

bool Config::Manager::applyCandidateConfig() {
    if (m_is_candidate_config_ready) {
        auto config_filename_tmp = m_config_filename + ".tmp";
        // spdlog::debug("Save JSON config file to temporary file {}", config_filename_tmp);
        // spdlog::debug("{}", 
        std::ofstream json_file(config_filename_tmp);
        if (!json_file) {
            spdlog::error("Failed to open file {} to save candidate config", config_filename_tmp);
            return false;
        }

        json_file << std::setw(4) << g_candidate_jconfig;
        if (!json_file) {
            spdlog::error("Failed to write candidate config to file {}", config_filename_tmp);
            return false;
        }

        json_file.flush();
        json_file.close();
        // auto end_of_dir_pos = config_filename_tmp.find_last_of("/");
        // auto path = end_of_dir_pos != String::npos ? config_filename_tmp.substr(0, end_of_dir_pos + 1) : "./";
        // auto filename = end_of_dir_pos != String::npos ? config_filename_tmp.substr(end_of_dir_pos + 1) : 
        //  file_path = path;
        std::error_code err_code = {};
        std::filesystem::rename(std::filesystem::path(config_filename_tmp), m_config_filename, err_code);
        if (err_code) {
            spdlog::error("Failed to save temporary filename {} into final config filename {}: {}",
                config_filename_tmp, m_config_filename, err_code.message());
            return false;
        }

        err_code.clear();
        std::filesystem::remove(std::filesystem::path(config_filename_tmp), err_code);
        if (err_code) {
            spdlog::warn("Failed to remove temporary filename {}: {}",
                config_filename_tmp, err_code.message());
        }

        // TODO: Do action on nodes
        // TODO: Clear old m_running_config
        m_running_config = m_candidate_config;
        g_running_jconfig = g_candidate_jconfig;
    }
    else {
        spdlog::warn("There is not candidate config");
        return false;
    }

    m_is_candidate_config_ready = false;
    return true;
}

bool Config::Manager::cancelCandidateConfig() {
    if (!m_is_candidate_config_ready) {
        spdlog::warn("There is not candidate config");
        return false;
    }

    // TODO: Restore config
    PrintVisitor print_visitor;
    auto patch = nlohmann::json::diff(g_candidate_jconfig, g_running_jconfig);
    spdlog::info("Changes to restore:\n{}", patch.dump(4));
    auto candidate_jconfig = g_candidate_jconfig;
    spdlog::debug("Patching...");
    auto patched_jconfig = candidate_jconfig.patch(patch);
    spdlog::debug("Patched config:\n", patched_jconfig .dump(4));
    spdlog::debug("Diff config:\n", nlohmann::json::diff(patched_jconfig, g_candidate_jconfig).dump(4));
    std::ifstream schema_ifs(m_schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
    auto new_jconfig = patched_jconfig;

    if (!validateJsonConfig(new_jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    spdlog::info("{}", patch.flatten().dump(4));
    // TODO: If there is not "value" then resore it from original config
    for (auto& diff_item : patch) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::info("There is not 'value' field in diff... restoring it from config");
            diff_item["value"] = candidate_jconfig[nlohmann::json::json_pointer(path)];
        }

        spdlog::info("\n{}", diff_item.dump(4));
    }

    spdlog::info("Patch to restore config:\n{}", patch.dump(4));
    auto config_mngr = shared_from_this();
    if (!gMakeCandidateConfigInternal(patch.dump(), candidate_jconfig, m_candidate_config, m_schema_filename, config_mngr)) {
        spdlog::error("Failed to rollback changes");
        return false;
    }

    // TODO: Release config node by node?
    m_candidate_config = nullptr;
    g_candidate_jconfig = nlohmann::json();
    m_is_candidate_config_ready = false;

    return true;
}

String Config::Manager::dumpRunningConfig() {
    return g_running_jconfig.dump();
}

String Config::Manager::dumpCandidateConfig() {
    return g_candidate_jconfig.dump();
}