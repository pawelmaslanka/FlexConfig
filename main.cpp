#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include <spdlog/spdlog.h>
#include "httplib.h"

#include "connection_management.hpp"
#include "constraint_checking.hpp"
#include "node/node.hpp"
#include "node/leaf.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"
#include "config.hpp"
#include "lib/topo_sort.hpp"
#include "xpath.hpp"

#include "nodes_ordering.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <variant>
#include "peglib.h"
#include <type_traits>
#include <typeindex>
#include <typeinfo>

using namespace peg;
using namespace peg::udl;

#include <any>
#include <functional>
#include <iomanip>

template<class T, class F>
inline std::pair<const std::type_index, std::function<void(std::any const&)>>
    to_any_visitor(F const &f)
{
    return
    {
        std::type_index(typeid(T)),
        [g = f](std::any const &a)
        {
            if constexpr (std::is_void_v<T>)
                g();
            else
                g(std::any_cast<T const&>(a));
        }
    };
}
 
static std::unordered_map<std::type_index, std::function<void(std::any const&)>>
    any_visitor
{
    to_any_visitor<void>([] { std::cout << "{}" << std::endl; }),
    to_any_visitor<bool>([](bool x) { std::cout << std::boolalpha << x << std::endl; }),
    to_any_visitor<int>([](int x) { std::cout << x << std::endl; }),
    to_any_visitor<unsigned>([](unsigned x) { std::cout << x << std::endl; }),
    to_any_visitor<float>([](float x) { std::cout << x << std::endl; }),
    to_any_visitor<double>([](double x) { std::cout << x << std::endl; }),
    to_any_visitor<char>([](char x) { std::cout << x << std::endl; }),
    to_any_visitor<long>([](long x) { std::cout << x << std::endl; }),
    to_any_visitor<char const*>([](char const *s)
        { std::cout << std::quoted(s) << std::endl; }),
    to_any_visitor<std::string_view>([](std::string_view s)
        { std::cout << std::quoted(s) << std::endl; }),
    // ... add more handlers for your types ...
};
 
inline void process(const std::any& a)
{
  std::cout << a.type().name() << std::endl;
    if (const auto it = any_visitor.find(std::type_index(a.type()));
        it != any_visitor.cend())
        it->second(a);
    else
        std::cout << "Unregistered type "<< std::quoted(a.type().name());
}

class VisitorImpl : public Visitor {
  public:
    virtual ~VisitorImpl() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        // std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        // std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
        // node->accept(*this);
        return true;
    }
};

class VisitorGE1Speed : public Visitor {
  public:
    virtual ~VisitorGE1Speed() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        // std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        // std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
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
    if (argc < 3) {
        spdlog::error("Too few arguments passed to {} to run program", argv[0]);
        spdlog::debug("Usage:\n{} <JSON_CONFIG_FILENAME> <JSON_SCHEMA_FILENAME>\n", argv[0]);
        ::exit(EXIT_FAILURE);
    }

    auto json_config_filename = argv[1];
    auto json_schema_filename = argv[2];
    auto registry = std::make_shared<RegistryClass>();
    auto config_mngr = std::make_shared<Config::Manager>(json_config_filename, json_schema_filename, registry);
    if (!config_mngr->load()) {
        spdlog::error("Failed to load config file");
        ::exit(EXIT_FAILURE);
    }

    // VisitorImpl visitor;
    // config_mngr->getRunningConfig()->accept(visitor);

    // VisitorGE1Speed visit_ge1_speed;
    // config_mngr->getRunningConfig()->accept(visit_ge1_speed);
    // if (!visit_ge1_speed.getSpeedNode()) {
    //     std::cerr << "Not found speed node!\n";
    //     exit(EXIT_FAILURE);
    // }
    // else {
    //     spdlog::debug("Found interface speed");
    // }

    // auto ge2_ae1_diff = R"(
    //     [
    //         {
    //             "op": "add",
    //             "path": "/platform/port/ge-4",
    //             "value": {
    //                 "breakout-mode": "none"
    //             }
    //         },
    //         {
    //             "op": "add",
    //             "path": "/interface/gigabit-ethernet/ge-4",
    //             "value": {
    //                 "speed": "100G"
    //             }
    //         },
    //         {
    //             "op": "remove",
    //             "path": "/interface/gigabit-ethernet/ge-2",
    //             "value": {
    //                 "speed": "100G"
    //             }
    //         },
    //         {
    //             "op": "remove",
    //             "path": "/interface/aggregate-ethernet/ae-1",
    //             "value": {
    //                 "members": [
    //                     "ge-2"
    //                 ]
    //             }
    //         }
    //     ]
    // )";

    // if (!config_mngr->makeCandidateConfig(ge2_ae1_diff)) {
    //     spdlog::error("Failed to patch config");
    //     ::exit(EXIT_FAILURE);
    // }

    // spdlog::info("Dump running config:\n{}", config_mngr->dumpRunningConfig());
    // spdlog::info("Dump candidate config:\n{}", config_mngr->dumpCandidateConfig());

    // if (!config_mngr->applyCandidateConfig()) {
    //     spdlog::error("Failed to apply new candidate config");
    //     ::exit(EXIT_FAILURE);
    // }

    // spdlog::info("Successfully operate on config file");

    // if (!config_mngr->cancelCandidateConfig()) {
    //     spdlog::error("Failed to cancel candidate config");
    //     ::exit(EXIT_FAILURE);
    // }

    ConnectionManagement::Server cm;
    cm.addOnPostConnectionHandler("config_running_update", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/update") {
            return true;
        }

        spdlog::debug("Get request on {} with POST method: {}", path, data_request);
        config_mngr->makeCandidateConfig(data_request);
        return true;
    });

    cm.addOnGetConnectionHandler("config_running_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/running") {
            return true;
        }

        spdlog::debug("Get request running on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpRunningConfig();
        return true;
    });

    cm.addOnPostConnectionHandler("config_running_diff", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/running/diff") {
            return true;
        }

        spdlog::debug("Get request on {} with POST diff method: {}", path, data_request);
        return_data = config_mngr->getConfigDiff(data_request);
        return true;
    });

    cm.addOnGetConnectionHandler("config_candidate_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/candidate") {
            return true;
        }

        spdlog::debug("Get request candidate on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpCandidateConfig();
        return true;
    });

    cm.addOnPutConnectionHandler("config_candidate_apply", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/candidate") {
            return true;
        }

        spdlog::debug("Get request candidate on {} with PUT method: {}", path, data_request);
        return_data = config_mngr->applyCandidateConfig();
        return true;
    });

    cm.addOnDeleteConnectionHandler("config_candidate_delete", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != "/config/candidate") {
            return true;
        }

        spdlog::debug("Get request candidate on {} with DELETE method: {}", path, data_request);
        return_data = config_mngr->cancelCandidateConfig();
        return true;
    });

    if (!cm.run("localhost", 8001)) {
        spdlog::error("Failed to run connection management server");
        ::exit(EXIT_FAILURE);
    }

    ::exit(EXIT_SUCCESS);
}