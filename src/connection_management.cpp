#include <spdlog/spdlog.h>

#include "connection_management.hpp"

#include "httplib.h"

using namespace ConnectionManagement;
namespace Http = httplib;

bool Server::addConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id, RequestCallback handler) {
    callbacks[id] = handler;
    return true;
}

bool Server::removeConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id) {
    callbacks.erase(id);
    return true;
}

bool Server::addOnDeleteConnectionHandler(const String& id, RequestCallback handler) {
    return addConnectionHandler(m_on_delete_callback_by_id, id, handler);
}

bool Server::removeOnDeleteConnectionHandler(const String& id) {
    return removeConnectionHandler(m_on_delete_callback_by_id, id);
}

bool Server::addOnGetConnectionHandler(const String& id, RequestCallback handler) {
    return addConnectionHandler(m_on_get_callback_by_id, id, handler);
}

bool Server::removeOnGetConnectionHandler(const String& id) {
    return removeConnectionHandler(m_on_get_callback_by_id, id);
}

bool Server::addOnPostConnectionHandler(const String& id, RequestCallback handler) {
    return addConnectionHandler(m_on_post_callback_by_id, id, handler);
}

bool Server::removeOnPostConnectionHandler(const String& id) {
    return removeConnectionHandler(m_on_post_callback_by_id, id);
}

bool Server::addOnPutConnectionHandler(const String& id, RequestCallback handler) {
    return addConnectionHandler(m_on_put_callback_by_id, id, handler);
}

bool Server::removeOnPutConnectionHandler(const String& id) {
    return removeConnectionHandler(m_on_put_callback_by_id, id);
}

bool Server::run(const String& host, const UInt16 port) {
    Http::Server srv;
    srv.Post("/config/update", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got POST request:\n {}", req.body);
        auto return_message = processRequest(Method::POST, "/config/update", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    srv.Get("/config/running/get", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got GET request:\n {}", req.body);
        auto return_message = processRequest(Method::GET, "/config/running/get", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    srv.Post("/config/running/diff", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got POST diff request:\n {}", req.body);
        auto return_message = processRequest(Method::POST, "/config/running/diff", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    srv.Get("/config/candidate", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got GET request:\n {}", req.body);
        auto return_message = processRequest(Method::GET, "/config/candidate", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    srv.Put("/config/candidate", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got PUT request:\n {}", req.body);
        auto return_message = processRequest(Method::PUT, "/config/candidate", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    srv.Delete("/config/candidate", [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::info("Got DELETE request:\n {}", req.body);
        auto return_message = processRequest(Method::DELETE, "/config/candidate", req.body, return_data) ? return_data : "Failed";
        res.set_content(return_message, "text/plain");
    });

    return srv.listen(host, port);;
}

bool Server::processRequest(const Method method, const String& path, const String& request_data, String& return_data) {
    switch (method) {
    case Method::GET: {
        for (auto& [_, cb] : m_on_get_callback_by_id) {
            if (!cb(path, request_data, return_data)) {
                return false;
            }
        }

        break;
    }
    case Method::POST: {
        for (auto& [_, cb] : m_on_post_callback_by_id) {
            if (!cb(path, request_data, return_data)) {
                return false;
            }
        }

        break;
    }
    case Method::PUT: {
        for (auto& [_, cb] : m_on_put_callback_by_id) {
            if (!cb(path, request_data, return_data)) {
                return false;
            }
        }

        break;
    }
    case Method::DELETE: {
        for (auto& [_, cb] : m_on_delete_callback_by_id) {
            if (!cb(path, request_data, return_data)) {
                return false;
            }
        }

        break;
    }
    default: {
        spdlog::error("Unsupported HTTP method request");
        return false;
    }
    }

    return true;
}