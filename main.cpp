#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include <spdlog/spdlog.h>
#include "httplib.h"

#include "constraint_checking.hpp"
#include "node/node.hpp"
#include "node/leaf.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"
#include "config.hpp"
#include "expr_eval.hpp"
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
    auto registry = std::make_shared<RegistryClass>();
    auto config_mngr = std::make_shared<Config::Manager>("../config_test.json", "../schema_test.json", registry);
    if (!config_mngr->load()) {
        spdlog::error("Failed to load config file");
        ::exit(EXIT_FAILURE);
    }

    VisitorImpl visitor;
    config_mngr->getRunningConfig()->accept(visitor);

    VisitorGE1Speed visit_ge1_speed;
    config_mngr->getRunningConfig()->accept(visit_ge1_speed);
    if (!visit_ge1_speed.getSpeedNode()) {
        std::cerr << "Not found speed node!\n";
        exit(EXIT_FAILURE);
    }
    else {
        spdlog::info("Found interface speed");
    }

    auto dependency_mngr = std::make_shared<NodeDependencyManager>(config_mngr);
    List<String> ordered_nodes_by_xpath = {};
    spdlog::info("{}:{}", __func__, __LINE__);
    if (!dependency_mngr->resolve(config_mngr->getRunningConfig(), ordered_nodes_by_xpath)) {
        spdlog::error("Failed to resolve nodes dependency");
        ::exit(EXIT_FAILURE);
    }
    spdlog::info("{}:{}", __func__, __LINE__);

    // Check if updated nodes pass all constraints
    try {
        // parser.parse("if (1 ~ 3) then 'true'", val);
        // parser.parse("if (1 = 3) then 9", val);
        std::cout << "Start processing\n";
        // Get all update-contraints and go through for-loop
        for (auto& xpath : ordered_nodes_by_xpath) {
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::info("There isn't schema at node {}", xpath);
                continue;
            }

            auto config = config_mngr->getRunningConfig();
            auto constraint_checker = std::make_shared<ConstraintChecker>(config_mngr, config);
            auto update_constraints = schema_node->findAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
            for (auto& update_constraint : update_constraints) {
                spdlog::info("Processing update constraint '{}' at node {}", update_constraint, xpath);
                auto node = XPath::select2(config_mngr->getRunningConfig(), xpath);
                if (!node) {
                    spdlog::info("Not node indicated by xpath {}", xpath);
                    continue;
                }

                if (!constraint_checker->validate(node, update_constraint)) {
                    spdlog::error("Failed to validate againts constraints {} for node {}", update_constraint, xpath);
                    ::exit(EXIT_FAILURE);
                }
            }
        }

        // Perform action to remote server
        for (auto& xpath : ordered_nodes_by_xpath) {
            spdlog::info("Select xpath {} from JSON config", xpath);
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
            spdlog::info("Patch:");
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
            spdlog::info("Connect to server: {}", server_addr);
            httplib::Client cli(server_addr);
            auto path = action_attr.front();
            auto body = diff[0].dump();
            auto content_type = "application/json";
            spdlog::info("Path: {}\n Body: {}\n Content type: {}", path, body, content_type);
            auto result = cli.Post(path, body, content_type);
            if (!result) {
                spdlog::error("Failed to get response from server {}: {}", server_addr, httplib::to_string(result.error()));
                // TODO: Rollback all changes
                continue;
            }

            spdlog::info("POST result, status: {}, body: {}", result->status, result->body);
        }

        // parser.parse("if exists('/interface/gigabit-ethernet/ge-1') then 9", a);
    }
    catch (std::bad_any_cast& ex) {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
    }

    // Config is already loaded here. Let's do some action on this config

    // auto ge3_string = R"(
    //     {
    //         "interface": {
    //             "gigabit-ethernet": {
    //                 "ge-3": {
    //                     "speed": "100G"
    //                 }
    //             }
    //         }
    //     }
    // )";

    // auto ge3_config_diff = config_mngr->getConfigDiff(ge3_string);
    // spdlog::info("Get config diff: {}", ge3_config_diff);
    
    // if (!config_mngr->makeCandidateConfig(ge3_string)) {
    //     spdlog::error("Failed to make a candidate config");
    //     ::exit(EXIT_FAILURE);
    // }

    // if (!config_mngr->applyCandidateConfig()) {
    //     spdlog::error("Failed to appply a candidate config");
    //     ::exit(EXIT_FAILURE);
    // }

    // config_mngr->getRunningConfig()->accept(visitor);
    // TODO: Apply and create nodes
    // TODO: Go through all constraints
    // TODO: Copy candidate config to running

    // auto ge4_diff = R"(
    //     [
    //         {
    //             "op": "add",
    //             "path": "/interface/gigabit-ethernet/ge-4",
    //             "value": {
    //                 "speed": "100G"
    //             }
    //         }
    //     ]
    // )";

    // if (!config_mngr->makeCandidateConfig(ge4_diff)) {
    //     spdlog::error("Failed to patch config");
    //     ::exit(EXIT_FAILURE);
    // }

    auto ge2_ae1_diff = R"(
        [
            {
                "op": "add",
                "path": "/platform/port/ge-4",
                "value": {
                    "breakout-mode": "none"
                }
            },
            {
                "op": "add",
                "path": "/interface/gigabit-ethernet/ge-4",
                "value": {
                    "speed": "100G"
                }
            },
            {
                "op": "remove",
                "path": "/interface/gigabit-ethernet/ge-2",
                "value": {
                    "speed": "100G"
                }
            },
            {
                "op": "remove",
                "path": "/interface/aggregate-ethernet/ae-1",
                "value": {
                    "members": [
                        "ge-2"
                    ]
                }
            }
        ]
    )";

    if (!config_mngr->makeCandidateConfig(ge2_ae1_diff)) {
        spdlog::error("Failed to patch config");
        ::exit(EXIT_FAILURE);
    }

    ::exit(EXIT_SUCCESS);
}