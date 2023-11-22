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