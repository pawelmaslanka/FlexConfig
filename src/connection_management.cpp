/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include <spdlog/spdlog.h>

#include "connection_management.hpp"
#include "lib/utils.hpp"

#include "httplib.h"

using namespace ConnectionManagement;
namespace Http = httplib;

using namespace std::chrono_literals;

Server::Server()
: m_session_mngr(360s /* session timeout */) {
    m_session_mngr.RegisterSessionTimeoutCallback(m_callback_register_id, [this](const String session_token) {
        m_session_mngr.RemoveSessionToken(session_token);
    });
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
    Http::Server srv;
    srv.Post(ConnectionManagement::URIRequestPath::Session::TOKEN, [this](const Http::Request &req, Http::Response &res) {
        m_session_mngr.RegisterSessionToken(req, res);
        return res.status;
    });

    srv.Delete(ConnectionManagement::URIRequestPath::Session::TOKEN, [this](const Http::Request &req, Http::Response &res) {
        m_session_mngr.RemoveSessionToken(req, res);
    });

    srv.Get(ConnectionManagement::URIRequestPath::Config::RUNNING, [this](const Http::Request &req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got running GET request:\n {}", req.body);
        auto status = processRequest(HTTP::Method::GET, ConnectionManagement::URIRequestPath::Config::RUNNING, req.body, return_data);
        auto return_message = status ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
        res.status = status ? HTTP::StatusCode::OK : HTTP::StatusCode::INTERNAL_SERVER_ERROR;
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, [this](const Http::Request &req, Http::Response &res) {
        if (!m_session_mngr.SetActiveSessionToken(req, res)) {
            return;
        }

        m_session_mngr.CancelSessionTokenTimerOnce(req);
        String return_data;
        spdlog::debug("Got POST request:\n {}", req.body);
        res.status = processRequest(HTTP::Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE, req.body, return_data);
        auto return_message = HTTP::IsSuccess(static_cast<HTTP::StatusCode>(res.status)) ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
        if (!m_session_mngr.SetSessionTokenTimerOnce(req, [this]([[maybe_unused]] const String session_token) {
                String req_data_stub;
                String res_data_stub;
                processRequest(HTTP::Method::DEL, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req_data_stub, res_data_stub);
                m_session_mngr.RemoveActiveSessionToken(session_token);
            },
            180s)) {
            // FIXME: Handle error
        }
    });

    srv.Post(ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, [this](const Http::Request &req, Http::Response &res) {
        String return_data;
        spdlog::debug("Got POST diff request:\n {}", req.body);
        res.status = processRequest(HTTP::Method::POST, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF, req.body, return_data);
        auto return_message = HTTP::IsSuccess((HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Get(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request &req, Http::Response &res) {
        if (!m_session_mngr.CheckActiveSessionToken(req, res)) {
            return;
        }

        String return_data;
        spdlog::debug("Got candidate GET request:\n {}", req.body);
        res.status = processRequest(HTTP::Method::GET, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = HTTP::IsSuccess((HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Put(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request &req, Http::Response &res) {
        if (!m_session_mngr.CheckActiveSessionToken(req, res)) {
            return;
        }

        m_session_mngr.CancelSessionTokenTimerOnce(req);
        String return_data;
        spdlog::debug("Got PUT request:\n {}", req.body);
        res.status = processRequest(HTTP::Method::PUT, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = HTTP::IsSuccess((HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
    });

    srv.Delete(ConnectionManagement::URIRequestPath::Config::CANDIDATE, [this](const Http::Request &req, Http::Response &res) {
        if (!m_session_mngr.CheckActiveSessionToken(req, res)) {
            return;
        }

        m_session_mngr.CancelSessionTokenTimerOnce(req);
        String return_data;
        spdlog::debug("Got DELETE request:\n {}", req.body);
        res.status = processRequest(HTTP::Method::DEL, ConnectionManagement::URIRequestPath::Config::CANDIDATE, req.body, return_data);
        auto return_message = HTTP::IsSuccess((HTTP::StatusCode) res.status) ? return_data : "Failed";
        res.set_content(return_message, HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
    });

    return srv.listen(host, port);;
}

// FIXME: Extend about Error Message
HTTP::StatusCode Server::processRequest(const HTTP::Method method, const String& path, const String& request_data, String& return_data) {
    auto check_internal_success = [](const HTTP::StatusCode status_code) {
        return status_code == HTTP::StatusCode::INTERNAL_SUCCESS;
    };

    HTTP::StatusCode status_code = HTTP::StatusCode::INTERNAL_SERVER_ERROR;

    switch (method) {
    case HTTP::Method::GET: {
        for (auto& [_, cb] : m_on_get_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case HTTP::Method::POST: {
        for (auto& [_, cb] : m_on_post_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case HTTP::Method::PUT: {
        for (auto& [_, cb] : m_on_put_callback_by_id) {
            status_code = cb(path, request_data, return_data);
            if (check_internal_success(status_code)) {
                continue;
            }
            
            return status_code;
        }

        break;
    }
    case HTTP::Method::DEL: {
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