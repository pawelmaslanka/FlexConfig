#include <spdlog/spdlog.h>

#include "connection_management.hpp"

#include "httplib.h"

using namespace ConnectionManagement;
namespace Http = httplib;

namespace HTTP {
    enum StatusCode {
        OK = 200,
        INTERNAL_SERVER_ERROR = 500
    };
}

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

bool Server::Run(const String& host, const UInt16 port) {
    static constexpr auto TEXT_PLAIN_RESP_CONTENT = "text/plain";
    Http::Server srv;
    srv.Get(ConnectionManagement::URIRequestPath::Config::RUNNING, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got running GET request:\n {}", req.body);
        auto status = processRequest(Method::GET, ConnectionManagement::URIRequestPath::Config::RUNNING, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got POST request:\n {}", req.body);
        auto status = processRequest(Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got POST diff request:\n {}", req.body);
        auto status = processRequest(Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Get(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got candidate GET request:\n {}", req.body);
        auto status = processRequest(Method::GET, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Put(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got PUT request:\n {}", req.body);
        auto status = processRequest(Method::PUT, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Delete(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request & req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got DELETE request:\n {}", req.body);
        auto status = processRequest(Method::DELETE, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    return srv.listen(host, port);;
}

// FIXME: Extend about Error Message
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