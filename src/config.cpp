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
#include "visitor_spec.hpp"

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

Config::Manager::Manager(StringView config_filename, StringView schema_filename, SharedPtr<RegistryClass>& registry)
 : m_config_filename { config_filename }, m_schema_filename { schema_filename } {

}

bool gPerformAction(SharedPtr<Config::Manager> config_mngr, SharedPtr<Node> node_config) {
    auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    if (!dependency_mngr->resolve(node_config, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        return false;
    }

    // Check if updated nodes pass all constraints
    try {
        // Get all update-contraints and go through for-loop
        for (auto& xpath : ordered_nodes_by_xpath) {
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::debug("There isn't schema at node {}", xpath);
                continue;
            }

            auto config = node_config;
            auto constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, config);
            auto update_constraints = schema_node->findAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
            for (auto& update_constraint : update_constraints) {
                spdlog::debug("Processing update constraint '{}' at node {}", update_constraint, xpath);
                auto node = XPath::select2(node_config, xpath);
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
            
            auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
            spdlog::debug("Patch:");
            auto diff = json_node2.diff({}, json_node2);
            diff[0]["op"] = "add";
            diff[0]["path"] = xpath;
            if (diff[0]["value"].is_object()) {
                // TODO: We should consider if we could put "string" with single value insead of empty object
                // "value":{"2":{}}
                // vs
                // "value": "2"
                for (auto it : diff[0]["value"].items()) {
                    it.value() = nlohmann::json::object();
                }
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

            spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
        }
    }
    catch (std::bad_any_cast& ex) {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
        return false;
    }

    return true;
}

bool Config::Manager::saveXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config) {
    auto candidate_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
    for (const auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::debug("Not found schema for node at xpath {}", xpath);
            continue;
        }

        auto node = XPath::select2(root_config, xpath);
        if (!node) {
            spdlog::debug("There is not exists node with reference attribute");
            continue;
        }

        auto reference_attr = schema_node->findAttr(Config::PropertyName::REFERENCE);
        for (const auto& ref : reference_attr) {
            if (ref.find("@") != String::npos) {
                String ref_xpath = ref;
                Utils::find_and_replace_all(ref_xpath, "@", node->getName());
                spdlog::debug("Created new reference xpath at xpath {}", ref_xpath, xpath);
                if (XPath::select2(root_config, ref_xpath)) {
                    spdlog::debug("Found reference node {}", ref_xpath);
                    candidate_xpath_source_reference_by_target[ref_xpath].emplace(xpath);
                }
            }
        }
    }

    m_candidate_xpath_source_reference_by_target = candidate_xpath_source_reference_by_target;
    return true;
}

bool Config::Manager::saveXPathReference_backup(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config) {
    auto candidate_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
    for (const auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::debug("Not found schema for node at xpath {}", xpath);
            continue;
        }

        auto node = XPath::select2(root_config, xpath);
        if (!node) {
            spdlog::debug("There is not exists node with reference attribute");
            continue;
        }

        SubnodeChildsVisitor subnode_child_visitor(node->getName());
        node->accept(subnode_child_visitor);
        auto subnode_names = subnode_child_visitor.getAllSubnodes();
        auto reference_attr = schema_node->findAttr(Config::PropertyName::REFERENCE);
        for (const auto& ref : reference_attr) {
            if (ref.find("@") != String::npos) {
                for (auto& subnode_name : subnode_names) {
                    spdlog::debug("Checking if {} exists", subnode_name);
                    String ref_xpath = ref;
                    Utils::find_and_replace_all(ref_xpath, "@", subnode_name);
                    spdlog::debug("Created new reference xpath: {}", ref_xpath);
                    if (XPath::select2(root_config, ref_xpath)) {
                        spdlog::debug("Found reference node {}", ref_xpath);
                        candidate_xpath_source_reference_by_target[ref_xpath].emplace(xpath);
                    }
                }
            }
        }
    }

    m_candidate_xpath_source_reference_by_target = candidate_xpath_source_reference_by_target;
    return true;
}

bool Config::Manager::removeXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config) {
    auto candidate_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
    auto nodes_by_xpath = ordered_nodes_by_xpath;

    for (const auto& xpath : nodes_by_xpath) {
        for (auto& [target, sources] : candidate_xpath_source_reference_by_target) {
            auto ref_source_it = sources.find(xpath);
            if (ref_source_it != sources.end()) {
                spdlog::debug("No more need source {} to target reference {}", xpath, target);
                sources.erase(ref_source_it);
            }
        }
    }

    ForwardList<String> reference_to_remove;
    bool success = true;
    for (const auto& xpath : nodes_by_xpath) {
        auto ref_target_it = candidate_xpath_source_reference_by_target.find(xpath);
        if (ref_target_it != candidate_xpath_source_reference_by_target.end()) {
            if (!ref_target_it->second.empty()) {
                spdlog::debug("There are still reference to {}", xpath);
                for (const auto& ref_source : ref_target_it->second) {
                    spdlog::error("There is still reference from {} to {}", ref_source, xpath);
                }

                success = false;
            }
        }
        else {
            reference_to_remove.emplace_front(xpath);
        }
    }

    if (!success) {
        spdlog::error("There are still references which has not beed removed");
        return false;
    }

    for (const auto& xpath : reference_to_remove) {
        candidate_xpath_source_reference_by_target.erase(xpath);
    }

    m_candidate_xpath_source_reference_by_target = candidate_xpath_source_reference_by_target;
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

        saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));

        if (!gPerformAction(shared_from_this(), root_config)) {
            spdlog::error("Failed to perform action on the config");
            return false;
        }

        m_running_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
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

        saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));

        if (!gPerformAction(shared_from_this(), root_config)) {
            spdlog::error("Failed to perform action on the config");
            return false;
        }

        m_running_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
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
                    spdlog::debug("At xpath {} not found token '{}' in schema:\n{}", xpath, token, schema.dump());
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
        if (schema.find(token) == schema.end()) {
            if (schema.find("patternProperties") == schema.end()) {
                schema_xpath_composed += "/properties/" + token;
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    spdlog::debug("At xpath {} not found token '{}' in schema:\n{}", xpath, token, schema.dump());
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
    auto diff_patch = g_running_jconfig.patch(diff2);
    auto copy_jconfig = g_running_jconfig;
    copy_jconfig.merge_patch(diff_patch);
    auto diff = nlohmann::json::diff(g_running_jconfig, copy_jconfig);
    // g_running_jconfig.merge_patch(patch);
    return diff2.dump();
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

        m_subnodes_xpath.emplace(XPath::to_string2(node));
        return true;
    };

    Set<String> getSubnodesXPath() { return m_subnodes_xpath; }

private:
    Set<String> m_subnodes_xpath;
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

bool Config::Manager::gMakeCandidateConfigInternal(const String& patch, nlohmann::json& json_config, SharedPtr<Node>& node_config, const String& schema_filename, SharedPtr<Config::Manager>& config_mngr) {
    PrintVisitor print_visitor;
    spdlog::debug("Patching...");
    auto diff_patch = json_config.patch(nlohmann::json::parse(patch));
    json_config = diff_patch;
    spdlog::debug("Dump patched config");
    spdlog::debug("Patched config:{}\n", json_config.dump(4));
    spdlog::debug("Diff config:{}\n", nlohmann::json::diff(json_config, json_config).dump(4));
    std::ifstream schema_ifs(schema_filename);
    nlohmann::json jschema = nlohmann::json::parse(schema_ifs);

    if (!validateJsonConfig(json_config, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    spdlog::debug("{}", nlohmann::json::parse(patch).flatten().dump(4));

    spdlog::debug("Candidate config before move diff items for remove changes:\n{}", json_config.dump(4));

    Set<String> xpaths_to_remove = {};
    Set<String> path_nodes = {};
    Set<String> subnodes_xpath = {};
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::debug("There is not 'value' field in diff... restoring it from config");
            if (json_config.find(nlohmann::json::json_pointer(path).to_string()) != json_config.end()) {
                diff_item["value"] = json_config[nlohmann::json::json_pointer(path)];
            }
            else {
                diff_item["value"] = nullptr;
            }
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
        // /interface -> /interface/ethernet -> /interface/ethernet/eth-1 -> ...
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

        spdlog::debug("Candidate config after process diff item {}:\n{}", diff_item.dump(4), json_config.dump(4));
    }

    spdlog::debug("Candidate config after move diff items for remove changes:\n{}", json_config.dump(4));

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
    // "ethernet": {
    //     "eth-1": {
    //         "speed": "100G"
    //     },
    // -   "eth-4": {
    // -       "speed": "100G"
    // -   }
    // }
    //
    // "eth-4" node is marked to remove but "eth-1" is also in ordered_nodes_by_xpath 
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
    spdlog::debug("Candidate config before validate against constraints:\n{}", json_config.dump(4));
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

    auto copy_candidate_xpath_source_reference_by_xpath = m_candidate_xpath_source_reference_by_target;
    if (!config_mngr->removeXPathReference(ordered_nodes_by_xpath, node_config)) {
        spdlog::error("Failed to remove node reference");
        m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
        return false;
    }

    Stack<String> removed_nodes_by_xpath;
    auto rollback_removed_nodes = [&removed_nodes_by_xpath, &config_mngr]() {
        while (!removed_nodes_by_xpath.empty()) {
            auto xpath = removed_nodes_by_xpath.top();
            auto jnode = g_running_jconfig[nlohmann::json::json_pointer(xpath)];
            removed_nodes_by_xpath.pop();
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
            auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                spdlog::debug("There is not update action under the path {}", xpath);
                continue;
            }

            // NOTE: For added_node_by_xpath use node_config
            auto node = XPath::select2(config_mngr->getRunningConfig(), xpath);
            if (!node) {
                spdlog::error("There is not exists node under the path {}", xpath);
                continue;
            }

            spdlog::debug("Procesing rollback node {}", node->getName());

            nlohmann::json action = nlohmann::json::object();
            action["op"] = "add";
            action["path"] = xpath;
            action["value"] = nullptr; // nlohmann::json::object();
            if (jnode.is_array()) {
                spdlog::debug("It is array:\n{}", jnode.dump(4));
                action["value"] = /* * */ jnode;
            }
            else if (std::dynamic_pointer_cast<Leaf>(node)) {
                spdlog::debug("It is leaf:\n{}", jnode.dump(4));
                action["value"] = /* * */ jnode;
            }

            spdlog::debug("Action to rollback:\n{}", action.dump(4));

            auto server_addr = server_addr_attr.front();
            spdlog::debug("Connect to server: {}", server_addr);
            httplib::Client cli(server_addr);
            auto path = action_attr.front();
            auto body = action.dump();
            auto content_type = "application/json";
            spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
            auto result = cli.Post(path, body, content_type);
            if (!result) {
                spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
        }

        return true;
    };

    spdlog::debug("Candidate config before perform remove actions:\n{}", json_config.dump(4));
    spdlog::debug("Nodes to perform delete action:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
        auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            spdlog::debug("{} has NOT delete action", xpath);
            removed_nodes_by_xpath.push(xpath);
            continue;
        }

        spdlog::debug("{} HAS delete action", xpath);
        auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
        auto diff = json_node2.diff({}, json_node2);
        diff[0]["op"] = "remove";
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
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        removed_nodes_by_xpath.push(xpath);

        spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
    }

    for (auto& xpath : ordered_nodes_by_xpath) {
        auto node = XPath::select2(node_config, xpath);
        if (!node) {
            spdlog::error("Failed to select node to remove at xpath {}", xpath);
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto node_parent = node->getParent();
        if (auto node_parent_ptr = std::dynamic_pointer_cast<Composite>(node_parent)) {
            if (!node_parent_ptr->remove(node->getName())) {
                spdlog::error("Failed to remove node {} from collection of nodes come from its parent {}", node->getName(), node_parent_ptr->getName());
                m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
                if (!rollback_removed_nodes()) {
                    spdlog::error("Failed to rollback removed nodes");
                    ::exit(EXIT_FAILURE);
                }

                return false;
            }
        }

        if (auto node_leaf_ptr = std::dynamic_pointer_cast<Leaf>(node)) {
            spdlog::debug("Node {} is a leaf so clear its value", node_leaf_ptr->getName());
        }

        node.reset();
    }

    spdlog::debug("Successfully removed all changes");

    spdlog::debug("Candidate config after apply remove changes:\n{}", json_config.dump(4));

    path_nodes.clear();
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::debug("There is not 'value' field in diff... restoring it from config");
            if (json_config.find(nlohmann::json::json_pointer(path).to_string()) != json_config.end()) {
                diff_item["value"] = json_config[nlohmann::json::json_pointer(path)];
            }
            else {
                diff_item["value"] = nullptr;
            }
        }
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
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

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
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto root_node = XPath::select2(node_config, xpath);
        if (!root_node) {
            spdlog::error("Failed to find node at xpath {}", xpath);
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
        if (properties_it != jschema.end()) {
            auto node = std::dynamic_pointer_cast<Composite>(root_node);
            if (!parseAndLoadConfig(json_config[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
                spdlog::error("Failed to load config based on 'properties' node");
                json_config = nlohmann::json();
                m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
                if (!rollback_removed_nodes()) {
                    spdlog::error("Failed to rollback removed nodes");
                    ::exit(EXIT_FAILURE);
                }

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
                    m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
                    if (!rollback_removed_nodes()) {
                        spdlog::error("Failed to rollback removed nodes");
                        ::exit(EXIT_FAILURE);
                    }

                    return false;
                }
            }
        }
    }

    spdlog::debug("Candidate config after apply added changes:\n{}", json_config.dump(4));

    ordered_nodes_by_xpath.clear();
    if (!dependency_mngr->resolve(node_config, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
        if (!rollback_removed_nodes()) {
            spdlog::error("Failed to rollback removed nodes");
            ::exit(EXIT_FAILURE);
        }

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
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto constraint_attr = schema_node->findAttr("update-constraints");
        auto node_to_check = XPath::select2(node_config, xpath);
        if (!node_to_check) {
            spdlog::error("Failed to find node to check {}", xpath);
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        for (auto& constraint : constraint_attr) {
            if (!constraint_checker->validate(node_to_check, constraint)) {
                spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
                m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
                if (!rollback_removed_nodes()) {
                    spdlog::error("Failed to rollback removed nodes");
                    ::exit(EXIT_FAILURE);
                }

                return false;
            }
        }
    }

    // Save in case of further error
    if (!config_mngr->saveXPathReference(ordered_nodes_by_xpath, node_config)) {
        spdlog::error("Failed to remove node reference");
        m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
        if (!rollback_removed_nodes()) {
            spdlog::error("Failed to rollback removed nodes");
            ::exit(EXIT_FAILURE);
        }

        return false;
    }

    Stack<String> added_nodes_by_xpath;
    auto rollback_added_nodes = [&added_nodes_by_xpath, &config_mngr, &node_config, &json_config]() {
        while (!added_nodes_by_xpath.empty()) {
            auto xpath = added_nodes_by_xpath.top();
            added_nodes_by_xpath.pop();
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
            auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                spdlog::debug("There is not delete action under the path {}", xpath);
                continue;
            }

            // NOTE: For added_node_by_xpath use node_config
            auto node = XPath::select2(node_config, xpath);
            if (!node) {
                spdlog::error("There is not exists node under the path {}", xpath);
                continue;
            }

            spdlog::debug("Processing node:\n{}", node->getName());

            nlohmann::json action = nlohmann::json::object();
            action["op"] = "remove";
            action["path"] = xpath;
            action["value"] = nullptr;
            if (auto leaf_ptr = std::dynamic_pointer_cast<Leaf>(node)) {
                spdlog::debug("Node {} is leaf", node->getName());
                if (leaf_ptr->getValue().is_string_array()) {
                    if (json_config.find(nlohmann::json::json_pointer(xpath).to_string()) != json_config.end()) {
                        action["value"] = json_config[nlohmann::json::json_pointer(xpath)];
                    }
                    else {
                        action["value"] = nlohmann::json::array();
                    }
                }
            }

            spdlog::debug("Action item to send:\n{}", action.dump(4));
            auto server_addr = server_addr_attr.front();
            spdlog::debug("Connect to server: {}", server_addr);
            httplib::Client cli(server_addr);
            auto path = action_attr.front();
            auto body = action.dump();
            auto content_type = "application/json";
            spdlog::debug("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
            auto result = cli.Post(path, body, content_type);
            if (!result) {
                spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            spdlog::debug("POST result, status: {}, body: {}", result->status, result->body);
        }

        return true;
    };

    spdlog::debug("Candidate JSON config:\n{}", json_config.dump(4));
    spdlog::debug("Nodes to perform update action:");
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->findAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
        auto server_addr_attr = schema_node->findAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            added_nodes_by_xpath.push(xpath);
            continue;
        }

        auto copy_json_config = json_config;
        auto json_node2 = nlohmann::json().parse(copy_json_config[nlohmann::json::json_pointer(xpath)].dump());
        auto diff = json_node2.diff({}, json_node2);
        diff[0]["op"] = XPath::select2(node_config, xpath) ? "replace" : "add";
        diff[0]["path"] = xpath;
        if (diff[0]["value"].is_object()) {
            diff[0]["value"] = nullptr;
        }

        spdlog::debug("Apply:\n{}", diff.dump(4));
        spdlog::debug("Rollback:\n{}", json_node2.diff(json_node2, {}).dump(4));

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
            // Apply diff_item every time, and on failed action, just grab diff and reverse changes
            m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_xpath;
            if (!rollback_added_nodes()) {
                spdlog::error("Failed to rollback added nodes");
                ::exit(EXIT_FAILURE);
            }

            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        // TODO: Add node to rollback
        // TODO: Based on xpath of node, just take jconfig of this node from g_running_jconfig
        added_nodes_by_xpath.push(xpath);
    }

#if 0
    spdlog::debug("Test rollback changes");
    rollback_added_nodes();
    rollback_removed_nodes();
    return false;
#endif

    return true;
}

bool Config::Manager::makeCandidateConfig(const String& patch) {
    
    NodeCopyMakerVisitor node_copy_maker;
    m_running_config->accept(node_copy_maker);
    auto candidate_config = node_copy_maker.getNodeConfigCopy();
    PrintVisitor print_visitor;

    auto candidate_jconfig = g_running_jconfig;
    auto config_mngr = shared_from_this();
    spdlog::debug("Run make candidate config");
    if (!gMakeCandidateConfigInternal(patch, candidate_jconfig, candidate_config, m_schema_filename, config_mngr)) {
        spdlog::error("Failed to make candidate config");
        return false;
    }

    spdlog::debug("Candidate jconfig:\n{}", candidate_jconfig.dump(4));

    m_running_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
    m_candidate_config = candidate_config;
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
        m_candidate_config = nullptr;
        g_candidate_jconfig = nlohmann::json();
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
    spdlog::debug("Changes to restore:\n{}", patch.dump(4));
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

    spdlog::debug("{}", patch.flatten().dump(4));
    // TODO: If there is not "value" then resore it from original config
    for (auto& diff_item : patch) {
        auto op = diff_item["op"];
        auto path = diff_item["path"];
        if (diff_item.find("value") == diff_item.end()) {
            spdlog::debug("There is not 'value' field in diff... restoring it from config");
            diff_item["value"] = candidate_jconfig[nlohmann::json::json_pointer(path)];
        }

        spdlog::debug("\n{}", diff_item.dump(4));
    }

    auto copy_candidate_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
    spdlog::debug("Patch to restore config:\n{}", patch.dump(4));
    auto config_mngr = shared_from_this();
    if (!gMakeCandidateConfigInternal(patch.dump(), candidate_jconfig, m_candidate_config, m_schema_filename, config_mngr)) {
        spdlog::error("Failed to rollback changes");
        m_candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_target;
        return false;
    }

    m_running_xpath_source_reference_by_target = m_candidate_xpath_source_reference_by_target;
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