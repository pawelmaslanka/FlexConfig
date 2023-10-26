#pragma once

#include "node.hpp"

#include <memory>
#include <string>
#include <string_view>

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
    static const auto UPDATE_CONSTRAINTS = "update-constraints";
    static const auto UPDATE_DEPENDENCIES = "update-dependencies";
    static const auto REFERENCE = "reference";
};

class Manager {
public:
    Manager(std::string_view config_filename, std::string_view schema_filename, SharedPtr<RegistryClass>& registry);
    ~Manager() = default;
    bool load(SharedPtr<Node>& root_config_ptr);
    SharedPtr<SchemaNode> getSchemaByXPath(const String& xpath);
    String getConfigNode(const String& xpath);

private:
    const std::string m_config_filename;
    const std::string m_schema_filename;
}; // class Manager

} // namespace Config