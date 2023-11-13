#pragma once

#include "node.hpp"

#include <memory>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

template <typename T>
union RVT {
    std::string err_msg;
    T value;
};
// template <typename ValueType>
// struct Result : std::pair<bool, RVT<ValueType>&&> {
//     Result(const bool success, RVT<ValueType>&& value)
//     : m_success { success }, m_value(value) {
        
//     }

//     operator bool() const {

//     }
// }

// class Logger {
    // seet_module_level_log() logging from modules
// }

class RegistryClass {
    // Logger logger
};

namespace Config {

static auto constexpr ROOT_TREE_CONFIG_NAME { "/" };

namespace PropertyName {
    static const auto ACTION = "action";
    static const auto ACTION_ON_DELETE_PATH = "action-on-delete-path";
    static const auto ACTION_ON_UPDATE_PATH = "action-on-update-path";
    static const auto ACTION_SERVER_ADDRESS = "action-server-address";
    static const auto DEFAULT = "default";
    static const auto DESCRIPTION = "description";
    static const auto DELETE_CONSTRAINTS = "delete-constraints";
    static const auto UPDATE_CONSTRAINTS = "update-constraints";
    static const auto UPDATE_DEPENDENCIES = "update-dependencies";
    static const auto REFERENCE = "reference";
};

class Manager : public std::enable_shared_from_this<Manager> {
public:
    Manager(std::string_view config_filename, std::string_view schema_filename, SharedPtr<RegistryClass>& registry);
    ~Manager() = default;
    bool load();
    SharedPtr<SchemaNode> getSchemaByXPath(const String& xpath);
    String getConfigNode(const String& xpath);
    String getConfigDiff(const String& patch);
    SharedPtr<Node> getRunningConfig();
    bool makeCandidateConfig(const String& patch);
    bool applyCandidateConfig();
    // bool rollbackCandidateConfig(); ?
    bool cancelCandidateConfig();
    String dumpRunningConfig();
    String dumpCandidateConfig();

    bool saveXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config);
    bool removeXPathReference(const List<String>& ordered_nodes_by_xpath, SharedPtr<Node> root_config);

private:
    const std::string m_config_filename;
    const std::string m_schema_filename;
    SharedPtr<Node> m_running_config;
    SharedPtr<Node> m_candidate_config;
    bool m_is_candidate_config_ready = { false };

    // /interface/gigabit-ethernet/ge-1 -> /vlan/id/2/members/ge2
    Map<String, Set<String>> m_running_xpath_source_reference_by_target;
    Map<String, Set<String>> m_candidate_xpath_source_reference_by_target;
    // Map<String, Set<String>> m_candidate_xpath_source_reference_by_target; // Double keyed for faster lookup?
    // TODO: Hide nlohman::json
    bool gMakeCandidateConfigInternal(const String& patch, nlohmann::json& json_config, SharedPtr<Node>& node_config, const String& schema_filename, SharedPtr<Config::Manager>& config_mngr);

}; // class Manager

} // namespace Config