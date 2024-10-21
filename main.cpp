/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "httplib.h"
#include "args.hxx"

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

bool setupRequestHandlersF(UniquePtr<ConnectionManagement::Server>& cm, SharedPtr<Config::Manager>& config_mngr) {
    cm->addOnPostConnectionHandler("config_running_update", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST method: {}", path, data_request);
        return config_mngr->makeCandidateConfig(data_request) ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    cm->addOnGetConnectionHandler("config_running_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request running on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpRunningConfig();
        return HTTP::StatusCode::OK;
    });

    cm->addOnPostConnectionHandler("config_running_diff", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST diff method: {}", path, data_request);
        return_data = config_mngr->getConfigDiff(data_request);
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_candidate_get", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with GET method: {}", path, data_request);
        return_data = config_mngr->dumpCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    cm->addOnPutConnectionHandler("config_candidate_apply", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with PUT method: {}", path, data_request);
        return_data = config_mngr->applyCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    cm->addOnDeleteConnectionHandler("config_candidate_delete", [&config_mngr](const String& path, String data_request, String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with DELETE method: {}", path, data_request);
        return_data = config_mngr->cancelCandidateConfig();
        return HTTP::StatusCode::OK;
    });

    return true;
}

int main(int argc, char* argv[]) {
    args::ArgumentParser arg_parser("Configuration Management System");
    args::HelpFlag help(arg_parser, "HELP", "Show this help menu", {'h', "help"});
    args::ValueFlag<String> config_file(arg_parser, "CONFIG", "The configuration file", { 'c', "config" });
    args::ValueFlag<String> schema_file(arg_parser, "SCHEMA", "The schema file", { 's', "schema" });
    args::ValueFlag<String> host_address(arg_parser, "ADDRESS", "The host binding address (hostname or IP address)", { 'a', "address" });
    args::ValueFlag<UInt16> host_port(arg_parser, "PORT", "The host binding port", { 'p', "port" });
    try {
        arg_parser.ParseCLI(argc, argv);
    }
    catch (args::Help) {
        std::cout << arg_parser;
        ::exit(EXIT_SUCCESS);
    }
    catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << arg_parser;
        ::exit(EXIT_FAILURE);
    }
    catch (args::ValidationError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << arg_parser;
        ::exit(EXIT_FAILURE);
    }

    if (!config_file || !schema_file || !host_address || !host_port) {
        std::cout << arg_parser;
        ::exit(EXIT_SUCCESS);
    }

    auto json_config_filename = args::get(config_file);
    auto json_schema_filename = args::get(schema_file);
    auto registry = std::make_shared<RegistryClass>();
    auto config_mngr = std::make_shared<Config::Manager>(json_config_filename, json_schema_filename, registry);
    try {
        if (!config_mngr->load()) {
            spdlog::error("Failed to load config file");
            ::exit(EXIT_FAILURE);
        }
    }
    catch (std::exception& ex) {
        spdlog::critical("Failed to load config file {} along with schema {}", json_config_filename, json_schema_filename);
        ::exit(EXIT_FAILURE);
    }
    catch (...) {
        spdlog::critical("Unexpected error during loading config file {} along with schema {}", json_config_filename, json_schema_filename);
        ::exit(EXIT_FAILURE);
    }

    auto cm = MakeUniquePtr<ConnectionManagement::Server>();
    if (!setupRequestHandlersF(cm, config_mngr)) {
        spdlog::error("Failed to setup request handlers");
        ::exit(EXIT_FAILURE);
    }

    if (!cm->Run(args::get(host_address), args::get(host_port))) {
        spdlog::error("Failed to run connection management server");
        ::exit(EXIT_FAILURE);
    }

    spdlog::info("The '{}' daemon is going to shutdown", argv[0]);
    ::exit(EXIT_SUCCESS);
}
