#include <spdlog/spdlog.h>

#include "session_management.hpp"

#include "lib/http_common.hpp"
#include "lib/std_types.hpp"
#include "lib/utils.hpp"

using namespace std::chrono_literals;

SessionManager::SessionManager(const std::chrono::seconds session_timeout_sec)
: m_session_timeout_sec { session_timeout_sec },
  m_checking_session_expiration_thread { [this]() {
        for (;;) {
            if (m_checking_session_expiration_quit_flag) {
                spdlog::info("Requested to stop checking session expiration");
                return;
            }

            ForwardList<String> expired_session_token;
            auto now = std::chrono::system_clock::now();

            {
                LockGuard<Mutex> _(m_session_token_mutex);
                for (const auto& session_token : m_leased_session_tokens) {
                    if (std::chrono::duration_cast<std::chrono::seconds>((now - session_token.second.LastRequestAt)) > std::chrono::duration_cast<std::chrono::seconds>(m_session_timeout_sec)) {
                        expired_session_token.emplace_front(session_token.first);
                    }
                }
            }

            for (const auto& session_token : expired_session_token) {
                LockGuard<Mutex> _(m_session_timeout_callbacks_mutex);
                for (const auto &[_, timeout_cb] : m_session_timeout_callbacks) {
                    timeout_cb(session_token);
                }

                LockGuard<Mutex> __(m_session_token_mutex);
                m_leased_session_tokens.erase(session_token);
            }

            expired_session_token.clear();
            ForwardList<String> stopped_timers;

            {
                LockGuard<Mutex> _(m_session_token_timers_mutex);
                for (auto& [session_token, timer_details] : m_session_token_timers) {
                    if (timer_details.QuitFlag) {
                        stopped_timers.emplace_front(session_token);
                        continue;
                    }

                    if (std::chrono::duration_cast<std::chrono::seconds>((now - timer_details.StartAt)) > std::chrono::duration_cast<std::chrono::seconds>(timer_details.Timeout)) {
                        expired_session_token.emplace_front(session_token);
                    }
                }
            }

            for (const auto &session_token : stopped_timers) {
                LockGuard<Mutex> _(m_session_token_timers_mutex);
                m_session_token_timers.erase(session_token);
            }

            for (const auto &session_token : expired_session_token) {
                LockGuard<Mutex> _(m_session_token_timers_mutex);
                auto timer_it = m_session_token_timers.find(session_token);
                timer_it->second.TimerCB(session_token);
                m_session_token_timers.erase(timer_it);
            }

            std::this_thread::sleep_for(10s);
        }
    } } {
    m_checking_session_expiration_thread.detach();
}

SessionManager::~SessionManager() {
    m_checking_session_expiration_quit_flag = true;
}

bool SessionManager::RegisterSessionToken(const Http::Request &req, Http::Response &res) {
    LockGuard<Mutex> _(m_session_token_mutex);
    if (m_leased_session_tokens.find(req.body) != m_leased_session_tokens.end()) {
        res.status = HTTP::StatusCode::CONFLICT; // Resource already exists
        return false;
    }

    auto now = std::chrono::system_clock::now();
    m_leased_session_tokens[req.body] = SessionDetails { now, now };
    spdlog::info("Registered new session token '{}'", req.body);
    res.status =  HTTP::StatusCode::CREATED;
    return true;
}

bool SessionManager::CheckSessionToken(const Http::Request &req, Http::Response &res) {
    LockGuard<Mutex> _(m_session_token_mutex);
    if (req.headers.find(HTTP::Header::Tokens::AUTHORIZATION) == req.headers.end()) {
        spdlog::info("Not found authorization token");
        res.status = HTTP::StatusCode::TOKEN_REQUIRED;
        return false;
    }

    String auth = req.get_header_value(HTTP::Header::Tokens::AUTHORIZATION);
    Utils::Trim(auth);
    // Authorization: Bearer TOKEN
    String session_token = auth.substr(std::strlen(HTTP::Header::Tokens::BEARER) + 1);
    if (m_leased_session_tokens.find(session_token) == m_leased_session_tokens.end()) {
        spdlog::info("Not found session '{}'", session_token);
        res.status = HTTP::StatusCode::INVALID_TOKEN;
        return false;
    }

    m_leased_session_tokens[session_token].LastRequestAt = std::chrono::system_clock::now();
    res.status =  HTTP::StatusCode::OK;
    return true;
};

bool SessionManager::SetActiveSessionToken(const Http::Request &req, Http::Response &res) {
    if (!CheckSessionToken(req, res)) {
        return false;
    }

    LockGuard<Mutex> _(m_session_token_mutex);
    String auth = req.get_header_value(HTTP::Header::Tokens::AUTHORIZATION);
    Utils::Trim(auth);
    // Authorization: Bearer TOKEN
    String session_token = auth.substr(std::strlen(HTTP::Header::Tokens::BEARER) + 1);
    if (m_active_session_token.has_value() && (m_active_session_token.value() != session_token)) {
        res.set_content("There is already active session '" + m_active_session_token.value() + "'", HTTP::ContentType::TEXT_PLAIN_RESP_CONTENT);
        res.status = HTTP::StatusCode::CONFLICT;
        return false;
    }

    m_active_session_token = session_token;
    res.status =  HTTP::StatusCode::OK;
    return true;
};

bool SessionManager::CheckActiveSessionToken(const Http::Request &req, Http::Response &res) {
    LockGuard<Mutex> _(m_session_token_mutex);
    if (req.headers.find(HTTP::Header::Tokens::AUTHORIZATION) == req.headers.end()) {
        spdlog::info("Not found authorization token");
        res.status = HTTP::StatusCode::TOKEN_REQUIRED;
        return false;
    }

    String auth = req.get_header_value(HTTP::Header::Tokens::AUTHORIZATION);
    Utils::Trim(auth);
    // Authorization: Bearer TOKEN
    String session_token = auth.substr(std::strlen(HTTP::Header::Tokens::BEARER) + 1);
    if (m_leased_session_tokens.find(session_token) == m_leased_session_tokens.end()) {
        spdlog::info("Not found session '{}'", session_token);
        res.status = HTTP::StatusCode::INVALID_TOKEN;
        return false;
    }

    if (!m_active_session_token.has_value() || (m_active_session_token.value() != session_token)) {
        spdlog::info("'{}' is not active session token", session_token);
        res.status = HTTP::StatusCode::INVALID_TOKEN;
        return false;
    }

    res.status =  HTTP::StatusCode::OK;
    return true;
};

bool SessionManager::RemoveSessionToken(const Http::Request &req, Http::Response &res) {
    if (!CheckSessionToken(req, res)) {
        return false;
    }

    LockGuard<Mutex> _(m_session_token_mutex);
    String auth = req.get_header_value(HTTP::Header::Tokens::AUTHORIZATION);
    Utils::Trim(auth);
    String session_token = auth.substr(std::strlen(HTTP::Header::Tokens::BEARER) + 1);
    m_leased_session_tokens.erase(session_token);
    spdlog::info("Successfully removed session token '{}'", session_token);
    if (m_active_session_token.has_value() && (session_token == m_active_session_token)) {
        spdlog::info("Removed active session token '{}'", session_token);
        m_active_session_token = NullOpt;
    }

    res.status =  HTTP::StatusCode::OK;

    return true;
};

bool SessionManager::RemoveSessionToken(const String &session_token) {
    httplib::Request req;
    req.set_header(HTTP::Header::Tokens::AUTHORIZATION,
        String(HTTP::Header::Tokens::BEARER) + " " + session_token);
    httplib::Response res;
    return RemoveSessionToken(req, res);
}

Optional<String> SessionManager::GetSessionToken(const Http::Request &req) {
    LockGuard<Mutex> _(m_session_token_mutex);
    if (req.headers.find(HTTP::Header::Tokens::AUTHORIZATION) == req.headers.end()) {
        spdlog::info("Not found authorization token");
        return NullOpt;
    }

    String auth = req.get_header_value(HTTP::Header::Tokens::AUTHORIZATION);
    Utils::Trim(auth);
    // Authorization: Bearer TOKEN
    String session_token = auth.substr(std::strlen(HTTP::Header::Tokens::BEARER) + 1);
    if (m_leased_session_tokens.find(session_token) == m_leased_session_tokens.end()) {
        spdlog::info("Not found session '{}'", session_token);
        return NullOpt;
    }

    return session_token;
}

Optional<String> SessionManager::GetActiveSessionToken() {
    return m_active_session_token;
}

bool SessionManager::RegisterSessionTimeoutCallback(const String& callback_receiver_id, SessionTimeoutCB session_timeout_cb) {
    LockGuard<Mutex> _(m_session_timeout_callbacks_mutex);
    m_session_timeout_callbacks[callback_receiver_id] = session_timeout_cb;
    return true;
}

void SessionManager::RemoveSessionTimeoutCallback(const String& callback_receiver_id) {
    LockGuard<Mutex> _(m_session_timeout_callbacks_mutex);
    m_session_timeout_callbacks.erase(callback_receiver_id);
}

bool SessionManager::SetSessionTokenTimerOnce(const Http::Request &req, SessionTokenTimerCB timer_callback, const std::chrono::seconds timeout_sec) {
    auto session_token = GetSessionToken(req).value_or("");
    if (session_token.empty()) {
        spdlog::error("There is not exists session token '{}'", session_token);
        return false;
    }

    auto now = std::chrono::system_clock::now();
    TimerThreadDetails timer_details { timer_callback, now, timeout_sec, false };
    LockGuard<Mutex> _(m_session_token_timers_mutex);
    auto timer_it = m_session_token_timers.find(session_token);
    if ((timer_it != m_session_token_timers.end()) && !timer_it->second.QuitFlag) {
        spdlog::error("Timer for session token '{}' already exists", session_token);
        return false;
    }

    m_session_token_timers.emplace(session_token, timer_details);

    return true;
}

bool SessionManager::CancelSessionTokenTimerOnce(const Http::Request &req) {
    auto session_token = GetSessionToken(req).value_or("");
    if (session_token.empty()) {
        spdlog::debug("There is not exists session token '{}'", session_token);
        return false;
    }

    LockGuard<Mutex> _(m_session_token_timers_mutex);
    auto timer_it = m_session_token_timers.find(session_token);
    if (timer_it == m_session_token_timers.end()) {
        spdlog::debug("Not found registered timer for session token '{}'", session_token);
        return false;
    }

    m_session_token_timers[session_token].QuitFlag = true;

    return true;
}
