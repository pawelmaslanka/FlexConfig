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
#include "lib/http_common.hpp"
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
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST method: {}", path, data_request);
        return config_mngr->makeCandidateConfig(data_request) ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    cm.addOnGetConnectionHandler("config_running_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request running on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpRunningConfig();
        return HTTP::StatusCode::OK;
    });

    cm.addOnPostConnectionHandler("config_running_diff", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST diff method: {}", path, data_request);
        return_data = config_mngr->getConfigDiff(data_request);
        return HTTP::StatusCode::OK;
    });

    cm.addOnGetConnectionHandler("config_candidate_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    cm.addOnPutConnectionHandler("config_candidate_apply", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with PUT method: {}", path, data_request);
        return_data = config_mngr->applyCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    cm.addOnDeleteConnectionHandler("config_candidate_delete", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with DELETE method: {}", path, data_request);
        return_data = config_mngr->cancelCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    if (!cm.Run("localhost", 8001)) {
        spdlog::error("Failed to run connection management server");
        ::exit(EXIT_FAILURE);
    }

    ::exit(EXIT_SUCCESS);
}