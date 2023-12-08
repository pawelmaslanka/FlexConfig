#include <spdlog/spdlog.h>

#include "connection_management.hpp"
#include "lib/utils.hpp"

#include "httplib.h"

using namespace ConnectionManagement;
namespace Http = httplib;

bool ConnectionManagement::HTTP::IsSuccess(const StatusCode status_code) {
    return (status_code >= ConnectionManagement::HTTP::StatusCode::START_SUCCESS) && (status_code <= ConnectionManagement::HTTP::StatusCode::END_SUCCESS);
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
    auto check_session_token = [this](const Http::Request &req, Http::Response &res) {
        if (req.headers.find("Authorization") == req.headers.end()) {
            spdlog::info("Not found authorization token");
            res.status = ConnectionManagement::HTTP::StatusCode::TOKEN_REQUIRED;
            return false;
        }

        String auth = req.get_header_value("Authorization");
        Utils::Trim(auth);
        // Authorization: Bearer TOKEN
        String session_token = auth.substr(sizeof ("Bearer"));
        spdlog::info("Session token: '{}'", session_token);
        if (m_leased_session_token.find(session_token) == m_leased_session_token.end()) {
            spdlog::info("Not found session '{}'", session_token);
            res.status = ConnectionManagement::HTTP::StatusCode::INVALID_TOKEN;
            return false;
        }

        m_leased_session_token[session_token].LastRequestAt = std::chrono::system_clock::now();
        return true;
    };

    Http::Server srv;
    srv.Post(ConnectionManagement::URIRequestPath::Session::TOKEN, [this](const Http::Request &req, Http::Response &res) {
        if (m_leased_session_token.find(req.body) != m_leased_session_token.end()) {
            return ConnectionManagement::HTTP::StatusCode::CONFLICT; // Resource already exists
        }

        auto now = std::chrono::system_clock::now();
        m_leased_session_token[req.body] = SessionDetails { now, now };
        spdlog::info("Registered new session token '{}'", req.body);
        // FIXME: Run thread to handle expired session
        return ConnectionManagement::HTTP::StatusCode::CREATED;
    });

    srv.Get(ConnectionManagement::URIRequestPath::Config::RUNNING, [this](const Http::Request &req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got running GET request:\n {}", req.body);
        auto status = processRequest(Method::GET, ConnectionManagement::URIRequestPath::Config::RUNNING, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, [this, &check_session_token](const Http::Request &req, Http::Response &res) {
        if (!check_session_token(req, res)) {
            return;
        }

        String return_data;
        spdlog::debug("Got POST request:\n {}", req.body);
        res.status = processRequest(Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, req.body, return_data);
        auto return_message = ConnectionManagement::HTTP::IsSuccess((ConnectionManagement::HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, [this](const Http::Request &req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got POST diff request:\n {}", req.body);
        res.status = processRequest(Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, req.body, return_data);
        auto return_message = ConnectionManagement::HTTP::IsSuccess((ConnectionManagement::HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Get(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this, &check_session_token](const Http::Request &req, Http::Response &res) {
        if (!check_session_token(req, res)) {
            return;
        }

        String return_data;
        spdlog::debug("Got candidate GET request:\n {}", req.body);
        res.status = processRequest(Method::GET, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = ConnectionManagement::HTTP::IsSuccess((ConnectionManagement::HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Put(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this, &check_session_token](const Http::Request &req, Http::Response &res) {
        if (!check_session_token(req, res)) {
            return;
        }

        String return_data;
        spdlog::debug("Got PUT request:\n {}", req.body);
        res.status = processRequest(Method::PUT, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = ConnectionManagement::HTTP::IsSuccess((ConnectionManagement::HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Delete(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this, &check_session_token](const Http::Request &req, Http::Response &res) {
        if (!check_session_token(req, res)) {
            return;
        }

        String return_data;
        spdlog::debug("Got DELETE request:\n {}", req.body);
        res.status = processRequest(Method::DELETE, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = ConnectionManagement::HTTP::IsSuccess((ConnectionManagement::HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, TEXT_PLAIN_RESP_CONTENT);
    });

    return srv.listen(host, port);;
}

// FIXME: Extend about Error Message
ConnectionManagement::HTTP::StatusCode Server::processRequest(const Method method, const String& path, const String& request_data, String& return_data) {
    auto check_internal_success = [](const ConnectionManagement::HTTP::StatusCode status_code) {
        return status_code == ConnectionManagement::HTTP::StatusCode::INTERNAL_SUCCESS;
    };

    ConnectionManagement::HTTP::StatusCode status_code = ConnectionManagement::HTTP::StatusCode::INTERNAL_SERVER_ERROR;

    switch (method) {
    case Method::GET: {
        for (auto& [_, cb] : m_on_get_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case Method::POST: {
        for (auto& [_, cb] : m_on_post_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case Method::PUT: {
        for (auto& [_, cb] : m_on_put_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case Method::DELETE: {
        for (auto& [_, cb] : m_on_delete_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    default: {
        spdlog::error("Unsupported HTTP method request");
    }
    }

    return status_code;
}