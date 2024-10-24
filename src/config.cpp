/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
// For some reason this include statement for spdlog must be at the top due to an error:
// "function template partial specialization is not allowed"
#include <spdlog/spdlog.h>
#include <nlohmann/json-schema.hpp>

#include "config.hpp"

#include "composite.hpp"
#include "connection_management.hpp"
#include "constraint_checking.hpp"
#include "lib/utils.hpp"
#include "nodes_ordering.hpp"
#include "xpath.hpp"
#include "node/leaf.hpp"
#include "visitor_spec.hpp"

#include <filesystem>
#include <fstream>

#include "httplib.h"

nlohmann::json GetJsonSchemaByXPath(const String& xpath);

static constexpr auto SCHEMA_NODE_PROPERTIES_NAME { "properties" };
static constexpr auto SCHEMA_NODE_PATTERN_PROPERTIES_NAME { "patternProperties" };
static constexpr auto SCHEMA_NODE_ACTION_PARAM_ON_UPDATE_NAME { "on-update" };
static constexpr auto SCHEMA_NODE_ACTION_PARAM_ON_DELETE_NAME { "on-delete" };
static constexpr auto SCHEMA_NODE_ACTION_PARAM_NAME { "action-parameters" };

namespace JsonDiffField {
    static const String OPERATION = { "op" };
    static const String PARAMETERS = { "params" };
    static const String PATH = { "path" };
    static const String VALUE = { "value" };
}

namespace JsonDiffOperation {
    static const String ADD = { "add" };
    static const String REMOVE = { "remove" };
    static const String REPLACE = { "replace" };
}

/**
    Stores and manages an access to uncommitted configuration changes.
 */
class CandidateJsonConfig {
public:
    // FIXME: Move constructor to private section
    CandidateJsonConfig() = default;
    static CandidateJsonConfig& instance() {
        static SharedPtr<CandidateJsonConfig> json_config;
        static Mutex mtx;
        if (!json_config) {
            LockGuard<Mutex> lock(mtx);
            if (!json_config) {
                json_config = MakeSharedPtr<CandidateJsonConfig>();
            }
        }

        return *json_config;
    }

    const nlohmann::json& get() {
        if (_jconfig == nlohmann::json()) {
            throw std::runtime_error("Candidate JSON config is not initialized yet");
        }

        return _jconfig;
    }

protected:
    friend Config::Manager;
    static void save(const nlohmann::json& jconfig) {
        _jconfig = jconfig;
    }

    static void save(const nlohmann::json&& jconfig) {
        _jconfig = jconfig;
    }

private:
    static nlohmann::json _jconfig;
};

nlohmann::json CandidateJsonConfig::_jconfig;

/**
    Stores and manages an access to committed configuration changes. Initially stores the loaded config.
 */
class RunningJsonConfig {
public:
    // FIXME: Move constructor to private section
    RunningJsonConfig() = default;
    static RunningJsonConfig& instance() {
        static SharedPtr<RunningJsonConfig> json_schema;
        static Mutex mtx;
        if (!json_schema) {
            LockGuard<Mutex> lock(mtx);
            if (!json_schema) {
                json_schema = MakeSharedPtr<RunningJsonConfig>();
            }
        }

        return *json_schema;
    }

    const nlohmann::json& get() {
        if (_jconfig == nlohmann::json()) {
            throw std::runtime_error("Running JSON config is not initialized yet");
        }

        return _jconfig;
    }

protected:
    friend Config::Manager;
    static void save(const nlohmann::json& jconfig) {
        _jconfig = jconfig;
    }

    static void save(const nlohmann::json&& jconfig) {
        _jconfig = jconfig;
    }

private:
    static nlohmann::json _jconfig;
};

nlohmann::json RunningJsonConfig::_jconfig;

/**
    Stores and manages an access to loaded JSON schema.
 */
class JsonSchema {
public:
    // FIXME: Move constructor to private section
    JsonSchema() = default;
    static JsonSchema& instance() {
        static SharedPtr<JsonSchema> json_schema;
        static Mutex mtx;
        if (!json_schema) {
            LockGuard<Mutex> lock(mtx);
            if (!json_schema) {
                json_schema = MakeSharedPtr<JsonSchema>();
            }
        }

        return *json_schema;
    }

    nlohmann::json& get() {
        if (_jschema == nlohmann::json()) {
            throw std::runtime_error("JSON schema is not initialized yet");
        }

        return _jschema;
    }

protected:
    friend Config::Manager;
    static bool load(const String& root_json_schema_filename) {
        std::ifstream schema_ifs(root_json_schema_filename);
        if (!schema_ifs.is_open()) {
            spdlog::error("Failed to open JSON schema file '{}'", root_json_schema_filename);
            return false;
        }

        nlohmann::json jschema = nlohmann::json::parse(schema_ifs);
        const std::filesystem::path path = root_json_schema_filename;
        for(const auto& p: std::filesystem::recursive_directory_iterator(path.parent_path())) {
            if (!std::filesystem::is_directory(p)) {
                if (p.path() == root_json_schema_filename) {
                    continue;
                }

                std::ifstream subschema_ifs(p.path());
                nlohmann::json jsubschema = nlohmann::json::parse(subschema_ifs);
                nlohmann::json patch = nlohmann::json::diff(jschema, jsubschema);
                nlohmann::json parsed_patch = nlohmann::json::array();
                size_t i = 0;
                for (auto& diff_item : patch) {
                    if (diff_item[JsonDiffField::OPERATION] == JsonDiffOperation::ADD) {
                        parsed_patch[i++] = diff_item;
                    }
                }

                jschema = jschema.patch(parsed_patch);
            }
        }

        if (jschema == nlohmann::json()) {
            spdlog::error("JSON schema file is empty");
            return false;
        }

        _jschema = jschema;
        return true;
    }

private:
    static nlohmann::json _jschema;
};

nlohmann::json JsonSchema::_jschema;

static nlohmann::json fFindAndAppendParamAction(nlohmann::json& node_jschema, nlohmann::json& node_jconfig, const String& action_jnode_name) {
    nlohmann::json parameters = nlohmann::json({});
    if (node_jschema.find(SCHEMA_NODE_ACTION_PARAM_NAME) == node_jschema.end()) {
        return parameters;
    }

    auto& action_params_jschema = node_jschema.at(SCHEMA_NODE_ACTION_PARAM_NAME);
    if (action_params_jschema.find(action_jnode_name) != action_params_jschema.end()) {
        for (auto param : action_params_jschema.at(action_jnode_name)) {
            if (node_jconfig.find(param) != node_jconfig.end()) {
                parameters[param] = node_jconfig.at(param);
            }
        }
    }

    return parameters;
}

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
        spdlog::error("Failed to validate json config");
        spdlog::critical("Here is actual schema:\n{}\n", jschema.dump(4));
        spdlog::critical("Here is actual config:\n{}\n", jconfig.dump(4));
        spdlog::critical("Validation failure reason: {}\n", e.what());
        return false;
	}

    return true;
}

bool fParseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config);
bool fLoadPatternProperties(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jconfig.items()) {
        for (auto& [ksch, vsch] : jschema.items()) {
            if (std::regex_match(k, Regex { ksch })) {
                node = MakeSharedPtr<Composite>(k, root_config);
                root_config->Add(node);
                if ((vsch.find(SCHEMA_NODE_PROPERTIES_NAME) == vsch.end())
                    && (vsch.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME) == vsch.end())) {
                    continue;
                }

                if (!fParseAndLoadConfig(v, vsch, node)) {
                    spdlog::error("Failed to parse config node '{}'", k);
                    return false;
                }
            }
        }
    }

    return true;
}

bool fParseAndLoadConfig(nlohmann::json& jconfig, nlohmann::json& jschema, std::shared_ptr<Composite>& root_config) {
    SharedPtr<Composite> node = root_config;
    for (auto& [k, v] : jschema.items()) {
        if (jconfig.find(k) == jconfig.end()) {
            continue;
        }

        if ((v.find("type") != v.end()) && (v.at("type") == "array")) {
            auto leaf = MakeSharedPtr<Composite>(k, root_config);
            root_config->Add(leaf);
            for (const auto& item : jconfig.at(k)) {
                Value val(Value::Type::STRING);
                // TODO: Make a reference instead of string
                val.set_string(item.get<String>());
                auto item_leaf = MakeSharedPtr<Leaf>(item.get<String>(), val, leaf);
                leaf->Add(item_leaf);
            }
        } // leaf node does not include SCHEMA_NODE_PROPERTIES_NAME
        else if (((v.find(SCHEMA_NODE_PROPERTIES_NAME) == v.end())
            && (v.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME) == v.end()))
            // leaf node is not "object"
            || ((v.find("type") != v.end()) && (v.at("type") != "object"))) {
            // TODO: Replace Value with std::variant
            Value val(Value::Type::STRING);
            val.set_string(jconfig.at(k).get<String>());
            auto leaf = MakeSharedPtr<Leaf>(k, val, root_config);
            root_config->Add(leaf);
            continue;
        }
        else {
            node = MakeSharedPtr<Composite>(k, root_config);
            root_config->Add(node);
        }

        if (!fParseAndLoadConfig(jconfig[k], v, node)) {
            spdlog::error("Failed to parse schema node '{}'", k);
            return false;
        }
    }

    for (auto& [k, v] : jschema.items()) {
        if (k == SCHEMA_NODE_PATTERN_PROPERTIES_NAME) {
            if (!fLoadPatternProperties(jconfig, v, node)) {
                spdlog::error("Failed to parse schema node '{}'", k);
                return false;
            }
        }

        if (k == SCHEMA_NODE_PROPERTIES_NAME) {
            if (!fParseAndLoadConfig(jconfig, v, node)) {
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

Config::Manager::Manager(StringView config_filename, StringView schema_filename, SharedPtr<RegistryClass>& registry)
 : _config_filename { config_filename }, _schema_filename { schema_filename } {

}

bool fPerformAction(SharedPtr<Config::Manager> config_mngr, SharedPtr<Node> node_config, const String& schema_filename) {
    auto dependency_mngr = MakeSharedPtr<NodeDependencyManager>(config_mngr);
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
                continue;
            }

            auto config = node_config;
            auto constraint_checker = MakeSharedPtr<ConstraintChecker>(config_mngr, config);
            auto update_constraints = schema_node->FindAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
            for (auto& update_constraint : update_constraints) {
                auto node = XPath::select(node_config, xpath);
                if (!node) {
                    continue;
                }

                if (!constraint_checker->validate(node, update_constraint)) {
                    spdlog::error("Failed to validate againts constraints {} for node {}", update_constraint, xpath);
                    return false;
                }
            }
        }

        Stack<nlohmann::json> action_to_rollback;
        // Perform action to remote server
        for (auto& xpath : ordered_nodes_by_xpath) {
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->FindAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
            auto server_addr_attr = schema_node->FindAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                continue;
            }
            
            auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
            auto diff = json_node2.diff({}, json_node2);
            diff[0][JsonDiffField::OPERATION] = JsonDiffOperation::ADD;
            diff[0][JsonDiffField::PATH] = xpath;
            // Replace:
            // { JsonDiffField::OPERATION: "add", JsonDiffField::PATH: "/interface/ethernet/eth-2", JsonDiffField::VALUE: null }
            // with:
            // { JsonDiffField::OPERATION: "add", JsonDiffField::PATH: "/interface/ethernet", JsonDiffField::VALUE: "eth-2" }
            auto xpath_jschema = GetJsonSchemaByXPath(xpath);
            if (xpath_jschema.find("type") != xpath_jschema.end()) {
                if ((xpath_jschema.at("type") == "null") || (xpath_jschema.at("type") == "object")) {
                    auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
                    diff[0][JsonDiffField::VALUE] = xpath_jpointer.back();
                    xpath_jpointer.pop_back();
                    diff[0][JsonDiffField::PATH] = xpath_jpointer.to_string();
                }

                diff[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, json_node2, SCHEMA_NODE_ACTION_PARAM_ON_UPDATE_NAME);
            }

            if (!ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), diff[0].dump())) {
                spdlog::error("Failed to send action request message");
                while (!action_to_rollback.empty()) {
                    auto action = action_to_rollback.top();
                    action_to_rollback.pop();
                    // It can be simply converted to "remove" operation since it is startup steps and there is
                    // not required to consider a "replace" operation
                    action[JsonDiffField::OPERATION] = JsonDiffOperation::REMOVE;
                    action[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, json_node2, SCHEMA_NODE_ACTION_PARAM_ON_DELETE_NAME);
                    ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), action.dump());
                }

                return false;
            }

            action_to_rollback.push(diff[0]);
        }
    }
    catch (std::bad_any_cast& ex) {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
        return false;
    }

    return true;
}

bool Config::Manager::saveXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config) {
    auto candidate_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
    for (const auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = getSchemaByXPath(xpath);
        if (!schema_node) {
            continue;
        }

        auto node = XPath::select(root_config, xpath);
        if (!node) {
            continue;
        }

        auto reference_attr = schema_node->FindAttr(Config::PropertyName::REFERENCE);
        for (const auto& xpath_reference : reference_attr) {
            String ref = xpath_reference;
            if (ref.find(XPath::ITEM_NAME_SUBSCRIPT) != StringEnd()) {
                auto evaluated_xpath_ref = XPath::evaluateXPath(node, ref);
                if (evaluated_xpath_ref.size() > 0) {
                    ref = evaluated_xpath_ref;
                }
            }

            if (ref.find("@") != StringEnd()) {
                String ref_xpath = ref;
                Utils::find_and_replace_all(ref_xpath, "@", node->Name());
                if (XPath::select(root_config, ref_xpath)) {
                    candidate_xpath_source_reference_by_target[ref_xpath].emplace(xpath);
                }
            }
        }
    }

    _candidate_xpath_source_reference_by_target = candidate_xpath_source_reference_by_target;
    return true;
}

bool Config::Manager::removeXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config) {
    auto candidate_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
    auto nodes_by_xpath = ordered_nodes_by_xpath;

    for (const auto& xpath : nodes_by_xpath) {
        for (auto& [target, sources] : candidate_xpath_source_reference_by_target) {
            auto ref_source_it = sources.find(xpath);
            if (ref_source_it != sources.end()) {
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
        spdlog::error("There are still references which has not been removed");
        return false;
    }

    for (const auto& xpath : reference_to_remove) {
        candidate_xpath_source_reference_by_target.erase(xpath);
    }

    _candidate_xpath_source_reference_by_target = candidate_xpath_source_reference_by_target;
    return true;
}

bool CheckIfThereIsDuplicatedKey(const String& json_filename) {
    std::ifstream jfile(json_filename);
    bool is_duplicated_key = false;
    Stack<Set<nlohmann::json>> parse_stack;
    nlohmann::json::parser_callback_t check_duplicate_keys =
        [&](int /* depth */, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
            if (is_duplicated_key) {
                return false;
            }

            switch (event) {
                case nlohmann::json::parse_event_t::object_start: {
                    parse_stack.push(Set<nlohmann::json>());
                    break;
                }
                case nlohmann::json::parse_event_t::object_end: {
                    parse_stack.pop();
                    break;
                }
                case nlohmann::json::parse_event_t::key: {
                    const auto result = parse_stack.top().insert(parsed);
                    if (!result.second) {
                      std::cerr << "key " << parsed << " was already seen in this object!" << std::endl;
                      is_duplicated_key = true;
                      return false;
                    }
                    break;
                }
                default: break;
            }
            // yes, store parsed value
            return true;
        };
    
    std::ignore = nlohmann::json::parse(jfile, check_duplicate_keys);
    return is_duplicated_key;
}

SharedPtr<Node> fReloadNodeConfig(nlohmann::json& jschema, nlohmann::json& jconfig, SharedPtr<Config::Manager> config_mngr) {
    if (!validateJsonConfig(jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return nullptr;
    }

    auto dependency_mngr = MakeSharedPtr<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    auto root_config = MakeSharedPtr<Composite>(Config::ROOT_TREE_CONFIG_NAME);
    auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!fParseAndLoadConfig(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'properties' node");
            return nullptr;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return nullptr;
        }

        config_mngr->saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));
        return root_config;
    }

    properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!fLoadPatternProperties(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'patternProperties' node");
            return nullptr;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return nullptr;
        }

        config_mngr->saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));
        return root_config;
    }

    spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
    return nullptr;
}

bool Config::Manager::load() {
    if (!JsonSchema::instance().load(_schema_filename)) {
        spdlog::error("Failed to load JON schema from file '{}'", _schema_filename);
        return false;
    }

    auto jschema = JsonSchema::instance().get();
    if (CheckIfThereIsDuplicatedKey(_config_filename)) {
        spdlog::error("There is duplicated key");
        return false;
    }

    std::ifstream config_ifs(_config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);
    if (!validateJsonConfig(jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    auto config_mngr = shared_from_this();
    auto dependency_mngr = MakeSharedPtr<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    auto root_config = MakeSharedPtr<Composite>(ROOT_TREE_CONFIG_NAME);
    auto properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!fParseAndLoadConfig(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'properties' node");
            return false;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return false;
        }

        saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));

        if (!fPerformAction(shared_from_this(), root_config, _schema_filename)) {
            spdlog::error("Failed to perform action on the config");
            return false;
        }

        _running_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
        _running_config = root_config;
        RunningJsonConfig::instance().save(jconfig);
        return true;
    }

    properties_it = jschema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME);
    if (properties_it != jschema.end()) {
        if (!fLoadPatternProperties(jconfig, *properties_it, root_config)) {
            spdlog::error("Failed to load config based on 'patternProperties' node");
            return false;
        }

        if (!dependency_mngr->resolve(root_config, ordered_nodes_by_xpath)) {
            spdlog::error("Failed to resolve nodes dependency");
            return false;
        }

        saveXPathReference(ordered_nodes_by_xpath, std::dynamic_pointer_cast<Node>(root_config));
        if (!fPerformAction(shared_from_this(), root_config, _schema_filename)) {
            spdlog::error("Failed to perform action on the config");
            return false;
        }

        _running_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
        _running_config = root_config;
        RunningJsonConfig::instance().save(jconfig);
        return true;
    }

    spdlog::error("Not found node 'properties' nor 'patternProperties' in root schema");
    return false;
}

String Config::Manager::getConfigNode(const String& xpath) {
    std::ifstream config_ifs(_config_filename);
    nlohmann::json jconfig = nlohmann::json::parse(config_ifs);
    return jconfig[nlohmann::json::json_pointer(xpath)].dump();
}

#include <iostream>
SharedPtr<SchemaNode> Config::Manager::getSchemaByXPath(const String& xpath) {
    auto jschema = JsonSchema::instance().get();
    auto root_properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return {};
    }

    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse(xpath);

    auto root_schema_node = MakeSharedPtr<SchemaComposite>("/");
    auto schema_node = root_schema_node;
    static const Set<String> SUPPORTED_ATTRIBUTES = {
        Config::PropertyName::ACTION, Config::PropertyName::ACTION_ON_DELETE_PATH, Config::PropertyName::ACTION_ON_UPDATE_PATH, Config::PropertyName::ACTION_SERVER_ADDRESS,
        Config::PropertyName::DEFAULT, Config::PropertyName::DESCRIPTION, Config::PropertyName::DELETE_CONSTRAINTS, Config::PropertyName::REFERENCE, Config::PropertyName::UPDATE_CONSTRAINTS, Config::PropertyName::UPDATE_DEPENDENCIES
    };

    auto load_schema_attributes = [](nlohmann::json& schema, String& node_name, SharedPtr<SchemaComposite>& root_schema_node) {
        auto schema_node = MakeSharedPtr<SchemaComposite>(node_name, root_schema_node);
        auto attributes = nlohmann::json(schema.patch(schema.diff({}, schema)));
        for (auto& [k, v] : attributes.items()) {
            if (SUPPORTED_ATTRIBUTES.find(k) != SUPPORTED_ATTRIBUTES.end()) {
                if (k == Config::PropertyName::ACTION) {
                    for (auto& [action_attr, action_val] : v.items()) {
                        schema_node->AddAttr(String(Config::PropertyName::ACTION) + "-" + action_attr, action_val);
                    }
                }
                else if (v.is_array()) {
                    for (auto& [_, item] : v.items()) {
                        schema_node->AddAttr(k, item);
                    }
                }
                else {
                    schema_node->AddAttr(k, v);
                }
            }
        }

        root_schema_node->Add(schema_node);
        return schema_node;
    };

    String schema_xpath_composed = {};
    // TODO: Create node hierarchy dynamically. Save in cache. Check cache next time before traverse
    for (auto token : xpath_tokens) {
        if (schema.find(token) == schema.end()) {
            if (schema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME) == schema.end()) {
                schema_xpath_composed += String(XPath::SEPARATOR) + SCHEMA_NODE_PROPERTIES_NAME + XPath::SEPARATOR + token;
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    return {};
                }

                schema_node = load_schema_attributes(schema, token, schema_node);
            }
            else {
                bool pattern_matched = false;
                for (auto& [k, v] : schema[SCHEMA_NODE_PATTERN_PROPERTIES_NAME].items()) {
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
            schema_xpath_composed += String(XPath::SEPARATOR) + SCHEMA_NODE_PROPERTIES_NAME + XPath::SEPARATOR + token;
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
            schema_node = load_schema_attributes(schema, token, schema_node);
            // TODO: Create schema node and add to cache
        }
    }

    // TODO: Load all schema nodes
    return schema_node;
}

nlohmann::json GetJsonSchemaByXPath(const String& xpath) {
    auto jschema = JsonSchema::instance().get();
    auto root_properties_it = jschema.find(SCHEMA_NODE_PROPERTIES_NAME);
    if (root_properties_it == jschema.end()) {
        spdlog::error("Invalid schema - missing 'properties' node on the top");
        return {};
    }

    auto schema = *root_properties_it;
    auto xpath_tokens = XPath::parse(xpath);

    String schema_xpath_composed = {};
    // TODO: Create node hierarchy dynamically. Save in cache. Chek cache next time before traverse
    for (auto token : xpath_tokens) {
        if (schema.find(token) == schema.end()) {
            if (schema.find(SCHEMA_NODE_PATTERN_PROPERTIES_NAME) == schema.end()) {
                schema_xpath_composed += String(XPath::SEPARATOR) + SCHEMA_NODE_PROPERTIES_NAME + XPath::SEPARATOR + token;
                auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
                schema = jschema[selector];
                if (schema.begin() == schema.end()) {
                    return {};
                }
            }
            else {
                bool pattern_matched = false;
                for (auto& [k, v] : schema[SCHEMA_NODE_PATTERN_PROPERTIES_NAME].items()) {
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
            schema_xpath_composed += String(XPath::SEPARATOR) + SCHEMA_NODE_PROPERTIES_NAME + XPath::SEPARATOR + token;
            auto selector = nlohmann::json::json_pointer(schema_xpath_composed);
            schema = jschema[selector];
            // TODO: Create schema node and add to cache
        }
    }

    return schema;
}

String Config::Manager::getConfigDiff(const String& patch) {
    auto jschema = JsonSchema::instance().get();
    auto new_jconfig = nlohmann::json().parse(patch);
    if (!validateJsonConfig(new_jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return {};
    }

    auto diff2 = nlohmann::json::diff(RunningJsonConfig::instance().get(), new_jconfig);
    auto diff_patch = RunningJsonConfig::instance().get().patch(diff2);
    auto copy_jconfig = RunningJsonConfig::instance().get();
    copy_jconfig.merge_patch(diff_patch);
    auto diff = nlohmann::json::diff(RunningJsonConfig::instance().get(), copy_jconfig);
    return diff2.dump();
}

SharedPtr<Node> Config::Manager::getRunningConfig() {
    return _running_config;
}

/**
    Get all subnodes of the 'node' as list of xpaths
 */
class SubnodesGetterVisitor : public Visitor {
public:
    virtual ~SubnodesGetterVisitor() = default;

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        _subnodes_xpath.emplace(XPath::toString(node));
        return true;
    };

    Set<String> getSubnodesXPath() { return _subnodes_xpath; }

private:
    Set<String> _subnodes_xpath;
};

class PrintVisitor : public Visitor {
  public:
    virtual ~PrintVisitor() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        spdlog::debug("{}", node->Name());
        return true;
    }
};

/**
    The 'NodesCollectorVisitor' class collects all nodes based on passed list as 'nodes_to_collect_by_xpath'
 */
class NodesCollectorVisitor : public Visitor {
public:
    virtual ~NodesCollectorVisitor() = default;
    NodesCollectorVisitor(const Set<String>& nodes_to_collect_by_xpath)
    : _nodes_to_collect_by_xpath { nodes_to_collect_by_xpath },
      _root_config { MakeSharedPtr<Composite>(Config::ROOT_TREE_CONFIG_NAME) } { }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        auto xpath = XPath::toString(node);
        if (xpath.empty()) {
            spdlog::warn("Cannot convert node {} to xpath", node->Name());
            return true;
        }

        auto node_it = _nodes_to_collect_by_xpath.find(xpath);
        if (node_it != _nodes_to_collect_by_xpath.end()) {
            auto xpath_tokens = XPath::parse(xpath);
            xpath_tokens.pop_back();
            auto parent_node = _root_config;
            if (!xpath_tokens.empty()) {
                auto parent_xpath = XPath::mergeTokens(xpath_tokens);
                parent_node = XPath::select(_root_config, parent_xpath);
                if (!parent_node) {
                    spdlog::error("Failed to find parent node {} in new config", parent_xpath);
                    return false;
                }
            }

            auto new_node = node->MakeCopy(parent_node);
            if (auto parent_node_ptr = std::dynamic_pointer_cast<Composite>(parent_node)) {
                parent_node_ptr->Add(new_node);
            }

            _node_by_xpath.emplace(xpath, new_node);
        }

        return true;
    };

    Map<String, SharedPtr<Node>> getNodesByXPath() { return _node_by_xpath; }
    SharedPtr<Node> getRootConfig() { return _root_config; }

private:
    Set<String> _nodes_to_collect_by_xpath;
    Map<String, SharedPtr<Node>> _node_by_xpath;
    SharedPtr<Node> _root_config;
};

/**
    Make copy of passed 'node'.
 */
class NodeCopyMakerVisitor : public Visitor {
public:
    virtual ~NodeCopyMakerVisitor() = default;
    NodeCopyMakerVisitor()
    : _root_config { MakeSharedPtr<Composite>(Config::ROOT_TREE_CONFIG_NAME) } { }

    virtual bool visit(SharedPtr<Node> node) override {
        if (!node) {
            spdlog::error("Node is null!");
            return false;
        }

        auto parent_node = node->Parent();
        if (!parent_node || (parent_node->Name() == Config::ROOT_TREE_CONFIG_NAME)) {
            parent_node = _root_config;
        }

        auto new_node = node->MakeCopy(parent_node);
        if (auto parent_node_ptr = std::dynamic_pointer_cast<Composite>(parent_node)) {
            parent_node_ptr->Add(new_node);
        }

        return true;
    };

    SharedPtr<Node> getNodeConfigCopy() { return _root_config; }

private:
    SharedPtr<Node> _root_config;
};

/**
    Validate the patch against JSON schema.
 */
static bool fValidatePatch(const String& patch, const nlohmann::json& jconfig) {
    auto diff_patch = jconfig.patch(nlohmann::json::parse(patch));
    auto origin_diff = nlohmann::json::diff(jconfig, diff_patch);
    if (nlohmann::json::parse(patch) != origin_diff) {
        spdlog::error("Failed to validate JSON diff:\n {}",
            nlohmann::json::diff(
                nlohmann::json::parse(patch),
                origin_diff).dump(2));
        return true;
    }

    return false;
}

/**
    Makes candidate config based on passed the 'patch' and a result puts into passed 'jconfig' argument.
 */
static bool fMakeCandidateConfigInternal(const String& patch, nlohmann::json& jconfig, SharedPtr<Node>& node_config, const String& schema_filename, SharedPtr<Config::Manager>& config_mngr, Map<String, Set<String>>& node_references) {
    NodeCopyMakerVisitor node_copy_maker;
    node_config->Accept(node_copy_maker);
    auto candidate_node_config = node_copy_maker.getNodeConfigCopy();
    nlohmann::json json_config = jconfig;
    auto diff_patch = json_config.patch(nlohmann::json::parse(patch));
    json_config = diff_patch;
    auto jschema = JsonSchema::instance().get();
    if (!validateJsonConfig(json_config, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    Set<String> xpaths_to_remove = {};
    Set<String> path_nodes = {};
    Set<String> subnodes_xpath = {};
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item[JsonDiffField::OPERATION];
        auto path = diff_item[JsonDiffField::PATH];
        if (diff_item.find(JsonDiffField::VALUE) == diff_item.end()) {
            auto path_jpointer = nlohmann::json::json_pointer(path);
            if (json_config.contains(path_jpointer.to_string())) {
                diff_item[JsonDiffField::VALUE] = json_config[path_jpointer];
            }
            else {
                diff_item[JsonDiffField::VALUE] = nullptr;
            }
        }

        auto value = diff_item[JsonDiffField::VALUE];
        if (op != JsonDiffOperation::REMOVE) {
            continue;
        }

        auto xpath_node_to_remove = path.get<String>();
        auto xpath_tokens = XPath::parse(xpath_node_to_remove);
        if (xpath_tokens.empty()) {
            spdlog::error("Path is empty!");
            return false;
        }

        String xpath = Config::ROOT_TREE_CONFIG_NAME;
        // 'path_nodes' will include set of xpath as follows:
        // /interface -> /interface/ethernet -> /interface/ethernet/eth-1 -> ...
        while (xpath_tokens.size() > 1) {
            if (xpath.at(xpath.size() - 1) != '/') {
                xpath += Config::XPATH_NODE_SEPARATOR;
            }

            xpath += xpath_tokens.front();
            xpath_tokens.pop_front();
            path_nodes.insert(xpath);
        }

        auto node_to_remove = XPath::select(node_config, xpath_node_to_remove);
        if (!node_to_remove) {
            spdlog::error("Failed to find root node to remove!");
            return false;
        }

        SubnodesGetterVisitor subnodes_getter_visitor;
        node_to_remove->Accept(subnodes_getter_visitor);
        subnodes_xpath.merge(subnodes_getter_visitor.getSubnodesXPath());
        subnodes_xpath.insert(xpath_node_to_remove);
        std::for_each(path_nodes.begin(), path_nodes.end(), [&xpaths_to_remove](const String& xpath) {
            xpaths_to_remove.insert(xpath);
        });

        std::for_each(subnodes_xpath.begin(), subnodes_xpath.end(), [&xpaths_to_remove](const String& xpath) {
            xpaths_to_remove.insert(xpath);
        });
    }

    for (auto& subnode : xpaths_to_remove) {
        // Check if all nodes (xpath) exists in config
        if (!XPath::select(node_config, subnode)) {
            spdlog::error("There is not {} in config", subnode);
            return false;
        }
    }

    NodesCollectorVisitor nodes_collector_visitor(xpaths_to_remove);
    node_config->Accept(nodes_collector_visitor);
    auto nodes_by_xpath = nodes_collector_visitor.getNodesByXPath();
    auto root_config_to_remove = nodes_collector_visitor.getRootConfig();
    auto dependency_mngr = MakeSharedPtr<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    if (!dependency_mngr->resolve(root_config_to_remove, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        return false;
    }

    ordered_nodes_by_xpath.remove_if([&path_nodes](const String& xpath) {
        return path_nodes.find(xpath) != path_nodes.end();
    });

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
            auto op = diff_item[JsonDiffField::OPERATION];
            if (op == JsonDiffOperation::REMOVE) {
                auto path = diff_item[JsonDiffField::PATH];
                if (xpath.find(path) != String::npos) {
                    should_be_removed = true;
                    break;
                }
            }
        }

        if (!should_be_removed) {
            not_marked_to_be_removed.insert(xpath);
        }
    }

    ordered_nodes_by_xpath.remove_if([&not_marked_to_be_removed](const String& xpath) {
        return not_marked_to_be_removed.find(xpath) != not_marked_to_be_removed.end();
    });

    ordered_nodes_by_xpath.reverse();
    auto constraint_checker = MakeSharedPtr<ConstraintChecker>(config_mngr, root_config_to_remove);
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::error("Failed to find schema node for {}", xpath);
            return false;
        }

        auto delete_constraint_attr = schema_node->FindAttr(Config::PropertyName::DELETE_CONSTRAINTS);
        auto node_to_remove = XPath::select(root_config_to_remove, xpath);
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

        auto parent_node = std::dynamic_pointer_cast<Composite>(node_to_remove->Parent());
        if (!parent_node->Remove(node_to_remove->Name())) {
            spdlog::error("Failed to remove node {} from collection of {}", node_to_remove->Name(), parent_node->Name());
            return false;
        }

        node_to_remove.reset();
    }

    Map<String, Set<String>> copy_candidate_xpath_source_reference_by_xpath = node_references;
    if (!config_mngr->removeXPathReference(ordered_nodes_by_xpath, node_config)) {
        spdlog::error("Failed to remove node reference");
        return false;
    }

    Stack<String> removed_nodes_by_xpath;
    auto rollback_removed_nodes = [&removed_nodes_by_xpath, &config_mngr, &schema_filename]() {
        while (!removed_nodes_by_xpath.empty()) {
            auto xpath = removed_nodes_by_xpath.top();
            nlohmann::json running_jconfig = RunningJsonConfig::instance().get();
            auto jnode = running_jconfig[nlohmann::json::json_pointer(xpath)];
            removed_nodes_by_xpath.pop();
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->FindAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
            auto server_addr_attr = schema_node->FindAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                continue;
            }

            // NOTE: For added_node_by_xpath use node_config
            auto node = XPath::select(config_mngr->getRunningConfig(), xpath);
            if (!node) {
                spdlog::error("There is not exists node under the path {}", xpath);
                continue;
            }

            nlohmann::json action = nlohmann::json::object();
            action[JsonDiffField::OPERATION] = JsonDiffOperation::ADD;
            action[JsonDiffField::PATH] = xpath;
            action[JsonDiffField::VALUE] = nullptr;
            // NOTE: This is note a case since the array type is not longer handle
            // if (jnode.is_array()) {
            //     action[JsonDiffField::VALUE] = jnode;
            // }
            // else
            if (std::dynamic_pointer_cast<Leaf>(node)) {
                action[JsonDiffField::VALUE] = jnode;
            }
            else {
                auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
                auto xpath_jschema = GetJsonSchemaByXPath(xpath_jpointer.to_string());
                // Item has to be fixed if the type is 'null', like for member containers with reference
                if (((xpath_jschema.find("type") != xpath_jschema.end())
                        && ((xpath_jschema.at("type") == "object") || (xpath_jschema.at("type") == "null")))) {
                    action[JsonDiffField::VALUE] = xpath_jpointer.back();
                    xpath_jpointer.pop_back();
                    action[JsonDiffField::PATH] = xpath_jpointer.to_string();
                    auto xpath_jconfig = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
                    action[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, xpath_jconfig, SCHEMA_NODE_ACTION_PARAM_ON_UPDATE_NAME);
                }
            }

            // TODO: Render additional parameters come from 'action-parameters'
            if (!ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), action.dump())) {
                spdlog::error("Failed to send action request message. Error to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }
        }

        return true;
    };

    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->FindAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
        auto server_addr_attr = schema_node->FindAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            removed_nodes_by_xpath.push(xpath);
            continue;
        }

        auto json_node2 = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
        auto diff = json_node2.diff({}, json_node2);
        diff[0][JsonDiffField::OPERATION] = JsonDiffOperation::REMOVE;
        diff[0][JsonDiffField::PATH] = xpath;
        auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
        auto xpath_jschema = GetJsonSchemaByXPath(xpath_jpointer.parent_pointer().to_string());
        if (((xpath_jschema.find("type") != xpath_jschema.end())
                && ((xpath_jschema.at("type") == "object") || (xpath_jschema.at("type") == "null")))) {
            auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
            diff[0][JsonDiffField::VALUE] = xpath_jpointer.back();
            xpath_jpointer.pop_back();
            diff[0][JsonDiffField::PATH] = xpath_jpointer.to_string();
            diff[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, json_node2, SCHEMA_NODE_ACTION_PARAM_ON_DELETE_NAME);
        }

        if (!ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), diff[0].dump())) {
            spdlog::error("Failed to send action request message");
            if (!rollback_removed_nodes()) {
                spdlog::critical("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        removed_nodes_by_xpath.push(xpath);
    }

    for (auto& xpath : ordered_nodes_by_xpath) {
        auto node = XPath::select(node_config, xpath);
        if (!node) {
            spdlog::error("Failed to select node to remove at xpath {}", xpath);
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto node_parent = node->Parent();
        if (auto node_parent_ptr = std::dynamic_pointer_cast<Composite>(node_parent)) {
            if (!node_parent_ptr->Remove(node->Name())) {
                spdlog::error("Failed to remove node {} from collection of nodes come from its parent {}", node->Name(), node_parent_ptr->Name());
                if (!rollback_removed_nodes()) {
                    spdlog::error("Failed to rollback removed nodes");
                    ::exit(EXIT_FAILURE);
                }

                return false;
            }
        }

        node.reset();
    }

    path_nodes.clear();
    for (auto& diff_item : nlohmann::json::parse(patch)) {
        auto op = diff_item[JsonDiffField::OPERATION];
        auto path = diff_item[JsonDiffField::PATH];
        if (diff_item.find(JsonDiffField::VALUE) == diff_item.end()) {
            auto path_jpointer = nlohmann::json::json_pointer(path);
            if (json_config.contains(path_jpointer.to_string())) {
                diff_item[JsonDiffField::VALUE] = json_config[path_jpointer];
            }
            else {
                diff_item[JsonDiffField::VALUE] = nullptr;
            }
        }

        auto value = diff_item[JsonDiffField::VALUE];
        if (op == JsonDiffOperation::REMOVE) {
            continue;
        }

        auto xpath_tokens = XPath::parse(path);
        if (xpath_tokens.empty()) {
            spdlog::error("Path is empty!");
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        String xpath = Config::ROOT_TREE_CONFIG_NAME;
        while (xpath_tokens.size() > 1) {
            if (xpath.at(xpath.size() - 1) != '/') {
                xpath += Config::XPATH_NODE_SEPARATOR;
            }

            xpath += xpath_tokens.front();
            xpath_tokens.pop_front();
            path_nodes.insert(xpath);
        }

        auto jschema = GetJsonSchemaByXPath(xpath);
        if (jschema == nlohmann::json({})) {
            spdlog::error("Not found schema at xpath {}", xpath);
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto root_node = XPath::select(node_config, xpath);
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
            if (!fParseAndLoadConfig(json_config[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
                spdlog::error("Failed to load config based on 'properties' node");
                json_config = nlohmann::json();
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
                if (!fLoadPatternProperties(json_config[nlohmann::json::json_pointer(xpath)], *properties_it, node)) {
                    spdlog::error("Failed to load config based on 'patternProperties' node");
                    if (!rollback_removed_nodes()) {
                        spdlog::error("Failed to rollback removed nodes");
                        ::exit(EXIT_FAILURE);
                    }

                    return false;
                }
            }
        }
    }

    ordered_nodes_by_xpath.clear();
    if (!dependency_mngr->resolve(node_config, ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        if (!rollback_removed_nodes()) {
            spdlog::error("Failed to rollback removed nodes");
            ::exit(EXIT_FAILURE);
        }

        return false;
    }

    // Exclude nodes which had been loaded already
    Set<String> not_marked_to_be_added = {};
    for (const auto& xpath : ordered_nodes_by_xpath) {
        bool should_be_added = false;
        for (auto& diff_item : nlohmann::json::parse(patch)) {
            auto op = diff_item[JsonDiffField::OPERATION];
            if (op != JsonDiffOperation::REMOVE) {
                auto path = diff_item[JsonDiffField::PATH];
                if (xpath.find(path) != String::npos) {
                    should_be_added = true;
                    break;
                }
            }
        }

        if (!should_be_added) {
            not_marked_to_be_added.insert(xpath);
        }
    }

    ordered_nodes_by_xpath.remove_if([&not_marked_to_be_added](const String& xpath) {
        return not_marked_to_be_added.find(xpath) != not_marked_to_be_added.end();
    });

    constraint_checker = MakeSharedPtr<ConstraintChecker>(config_mngr, node_config);
    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        if (!schema_node) {
            spdlog::error("Failed to find schema node for {}", xpath);
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        auto constraint_attr = schema_node->FindAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
        auto node_to_check = XPath::select(node_config, xpath);
        if (!node_to_check) {
            spdlog::error("Failed to find node to check {}", xpath);
            if (!rollback_removed_nodes()) {
                spdlog::error("Failed to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }

            return false;
        }

        for (auto& constraint : constraint_attr) {
            if (!constraint_checker->validate(node_to_check, constraint)) {
                spdlog::error("Failed to validate againts constraints {} for node {}", constraint, xpath);
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
        if (!rollback_removed_nodes()) {
            spdlog::error("Failed to rollback removed nodes");
            ::exit(EXIT_FAILURE);
        }

        return false;
    }

    Stack<String> added_nodes_by_xpath;
    auto rollback_added_nodes = [&added_nodes_by_xpath, &config_mngr, &node_config, &json_config, &schema_filename]() {
        while (!added_nodes_by_xpath.empty()) {
            auto xpath = added_nodes_by_xpath.top();
            added_nodes_by_xpath.pop();
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            auto action_attr = schema_node->FindAttr(Config::PropertyName::ACTION_ON_DELETE_PATH);
            auto server_addr_attr = schema_node->FindAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
            if (action_attr.empty() || server_addr_attr.empty()) {
                continue;
            }

            // NOTE: For added_node_by_xpath use node_config
            auto node = XPath::select(node_config, xpath);
            if (!node) {
                spdlog::error("There is not exists node under the path {}", xpath);
                continue;
            }

            nlohmann::json action = nlohmann::json::object();
            action[JsonDiffField::OPERATION] = JsonDiffOperation::REMOVE;
            action[JsonDiffField::PATH] = xpath;
            action[JsonDiffField::VALUE] = nullptr;
            auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
            auto xpath_jschema = GetJsonSchemaByXPath(xpath_jpointer.parent_pointer().to_string());
            auto is_object = (xpath_jschema.find("type") != xpath_jschema.end()) && (xpath_jschema.at("object") == "null");
            // Item has to be fixed if the type is 'null', like for member containers with reference
            auto is_leaf_object = (xpath_jschema.find("type") != xpath_jschema.end()) && (xpath_jschema.at("type") == "null");
            if (is_object || is_leaf_object) {
                action[JsonDiffField::VALUE] = xpath_jpointer.back();
                xpath_jpointer.pop_back();
                action[JsonDiffField::PATH] = xpath_jpointer.to_string();
                auto xpath_jconfig = nlohmann::json().parse(config_mngr->getConfigNode(xpath));
                action[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, xpath_jconfig, SCHEMA_NODE_ACTION_PARAM_ON_DELETE_NAME);
            }
            // NOTE: Since there is not permit array type so the below code is not longer necessary
            // if (auto leaf_ptr = std::dynamic_pointer_cast<Leaf>(node)) {
            //     if (leaf_ptr->getValue().is_string_array()) {
            //         auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
            //         if (json_config.contains(xpath_jpointer.to_string())) {
            //             action[JsonDiffField::VALUE] = json_config[xpath_jpointer];
            //         }
            //         else {
            //             action[JsonDiffField::VALUE] = nlohmann::json::array();
            //         }
            //     }
            // }

            if (!ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), action.dump())) {
                spdlog::error("Failed to send action request message. Error to rollback removed nodes");
                ::exit(EXIT_FAILURE);
            }
        }

        return true;
    };

    for (auto& xpath : ordered_nodes_by_xpath) {
        auto schema_node = config_mngr->getSchemaByXPath(xpath);
        auto action_attr = schema_node->FindAttr(Config::PropertyName::ACTION_ON_UPDATE_PATH);
        auto server_addr_attr = schema_node->FindAttr(Config::PropertyName::ACTION_SERVER_ADDRESS);
        if (action_attr.empty() || server_addr_attr.empty()) {
            added_nodes_by_xpath.push(xpath);
            continue;
        }

        auto copy_json_config = json_config;
        auto json_node2 = nlohmann::json().parse(copy_json_config[nlohmann::json::json_pointer(xpath)].dump());
        auto diff = json_node2.diff({}, json_node2);
        auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
        diff[0][JsonDiffField::OPERATION] = jconfig.contains(xpath_jpointer) ? JsonDiffOperation::REPLACE : JsonDiffOperation::ADD;
        diff[0][JsonDiffField::PATH] = xpath;
        /*
        * Replace:
        * { JsonDiffField::OPERATION: "replace", JsonDiffField::PATH: "/interface/ethernet/eth-2", JsonDiffField::VALUE: null }
        * with:
        * { JsonDiffField::OPERATION: "replace", JsonDiffField::PATH: "/interface/ethernet", JsonDiffField::VALUE: "eth-2" }
        */
        auto xpath_jschema = GetJsonSchemaByXPath(xpath_jpointer.to_string());
        // Item has to be fixed if the type is 'null', like for member containers with reference
        if (((xpath_jschema.find("type") != xpath_jschema.end())
                && ((xpath_jschema.at("type") == "object") || (xpath_jschema.at("type") == "null")))) {
            auto xpath_jpointer = nlohmann::json::json_pointer(xpath);
            diff[0][JsonDiffField::VALUE] = xpath_jpointer.back();
            xpath_jpointer.pop_back();
            diff[0][JsonDiffField::PATH] = xpath_jpointer.to_string();
            if (diff[0][JsonDiffField::OPERATION] == JsonDiffOperation::REPLACE) {
                diff[0][JsonDiffField::OPERATION] = JsonDiffOperation::ADD;
            }

            diff[0][JsonDiffField::PARAMETERS] = fFindAndAppendParamAction(xpath_jschema, json_node2, SCHEMA_NODE_ACTION_PARAM_ON_UPDATE_NAME);
        }

        if (!ConnectionManagement::Client::post(server_addr_attr.front(), action_attr.front(), diff[0].dump())) {
            spdlog::error("Failed to send action request message");
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

        added_nodes_by_xpath.push(xpath);
    }

    jconfig = json_config;
    return true;
}

/**
    Initiates making candidate config based on passed the 'patch'.
    At the end it is stored internally to be a replacement of the running config or just discarded so it consists of uncommitted changes.
 */
bool Config::Manager::makeCandidateConfig(const String& patch) {
    NodeCopyMakerVisitor node_copy_maker;
    _running_config->Accept(node_copy_maker);
    auto root_node_candidate_config = node_copy_maker.getNodeConfigCopy();
    auto candidate_jconfig = RunningJsonConfig::instance().get();
    if (fValidatePatch(patch, candidate_jconfig)) {
        spdlog::error("Patch is invalid... probably does not come from this instance running");
        return false;
    }

    auto config_mngr = shared_from_this();
    if (!fMakeCandidateConfigInternal(patch, candidate_jconfig, root_node_candidate_config, _schema_filename, config_mngr, _candidate_xpath_source_reference_by_target)) {
        spdlog::error("Failed to make candidate config");
        return false;
    }

    auto new_node_config = fReloadNodeConfig(JsonSchema::instance().get(), candidate_jconfig, shared_from_this());
    if (!new_node_config) {
        spdlog::error("Failed to reload node config");
        return false;
    }

    _candidate_config.reset();
    _candidate_config = new_node_config;
    _running_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
    CandidateJsonConfig::instance().save(candidate_jconfig);
    _is_candidate_config_ready = true;
    return true;
}

bool Config::Manager::applyCandidateConfig() {
    if (_is_candidate_config_ready) {
        auto config_filename_tmp = _config_filename + ".tmp";
        std::ofstream json_file(config_filename_tmp);
        if (!json_file) {
            spdlog::error("Failed to open file {} to save candidate config", config_filename_tmp);
            return false;
        }

        json_file << std::setw(4) << CandidateJsonConfig::instance().get();
        if (!json_file) {
            spdlog::error("Failed to write candidate config to file {}", config_filename_tmp);
            return false;
        }

        json_file.flush();
        json_file.close();
        std::error_code err_code = {};
        std::filesystem::rename(std::filesystem::path(config_filename_tmp), _config_filename, err_code);
        if (err_code) {
            spdlog::error("Failed to save temporary filename {} into final config filename {}: {}",
                config_filename_tmp, _config_filename, err_code.message());
            return false;
        }

        err_code.clear();
        std::filesystem::remove(std::filesystem::path(config_filename_tmp), err_code);
        if (err_code) {
            spdlog::warn("Failed to remove temporary filename {}: {}",
                config_filename_tmp, err_code.message());
        }

        _running_config = _candidate_config;
        RunningJsonConfig::instance().save(CandidateJsonConfig::instance().get());
        _candidate_config = nullptr;
        CandidateJsonConfig::instance().save(nlohmann::json());
    }
    else {
        spdlog::warn("There is not candidate config");
        return false;
    }

    _is_candidate_config_ready = false;
    return true;
}

bool Config::Manager::cancelCandidateConfig() {
    if (!_is_candidate_config_ready) {
        spdlog::warn("There is not candidate config");
        return false;
    }

    PrintVisitor print_visitor;
    auto patch = nlohmann::json::diff(CandidateJsonConfig::instance().get(), RunningJsonConfig::instance().get());
    nlohmann::json candidate_jconfig = CandidateJsonConfig::instance().get();
    auto patched_jconfig = candidate_jconfig.patch(patch);
    auto jschema = JsonSchema::instance().get();
    auto new_jconfig = patched_jconfig;
    if (!validateJsonConfig(new_jconfig, jschema)) {
        spdlog::error("Failed to validate config");
        return false;
    }

    // TODO: If there is not JsonDiffField::VALUE then restore it from original config
    for (auto& diff_item : patch) {
        auto op = diff_item[JsonDiffField::OPERATION];
        auto path = diff_item[JsonDiffField::PATH];
        if (diff_item.find(JsonDiffField::VALUE) == diff_item.end()) {
            diff_item[JsonDiffField::VALUE] = candidate_jconfig[nlohmann::json::json_pointer(path)];
        }
    }

    Map<String, Set<String>>  copy_candidate_xpath_source_reference_by_target = _candidate_xpath_source_reference_by_target;
    _candidate_config->Accept(print_visitor);
    auto config_mngr = shared_from_this();
    if (!fMakeCandidateConfigInternal(patch.dump(), candidate_jconfig, _candidate_config, _schema_filename, config_mngr, _candidate_xpath_source_reference_by_target)) {
        spdlog::error("Failed to rollback changes");
        _candidate_xpath_source_reference_by_target = copy_candidate_xpath_source_reference_by_target;
        return false;
    }

    _candidate_config.reset();
    CandidateJsonConfig::instance().save(nlohmann::json());
    _is_candidate_config_ready = false;

    return true;
}

String Config::Manager::dumpRunningConfig() {
    return RunningJsonConfig::instance().get().dump();
}

String Config::Manager::dumpCandidateConfig() {
    return CandidateJsonConfig::instance().get().dump();
}
