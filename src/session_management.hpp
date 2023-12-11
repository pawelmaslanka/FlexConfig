#pragma once

#include "lib/std_types.hpp"
#include "httplib.h"

namespace Http = httplib;

using SessionTimeoutCB = std::function<void(const String session_token)>;
using SessionTokenTimerCB = std::function<void(const String session_token)>;

/*
    Manages client session and transaction.
    Client start a session and SessionManager start counter. If client won't confirm its changes in 
    5 minutes then his changes are withdrawing.
*/
class SessionManager {
public:
    SessionManager(const std::chrono::seconds session_timeout_sec);
    ~SessionManager();

    bool RegisterSessionToken(const Http::Request &req, Http::Response &res);
    bool CheckSessionToken(const Http::Request &req, Http::Response &res);
    bool SetActiveSessionToken(const Http::Request &req, Http::Response &res);
    bool CheckActiveSessionToken(const Http::Request &req, Http::Response &res);
    bool RemoveSessionToken(const Http::Request &req, Http::Response &res);
    bool RemoveSessionToken(const String &session_token);

    Optional<String> GetSessionToken(const Http::Request &req);
    Optional<String> GetActiveSessionToken();

    bool RegisterSessionTimeoutCallback(const String& callback_receiver, SessionTimeoutCB session_timeout_cb);
    void RemoveSessionTimeoutCallback(const String& callback_receiver);

    // 30 sec resolution 
    bool SetSessionTokenTimerOnce(const Http::Request &req, SessionTokenTimerCB timer_callback, const std::chrono::seconds timeout_sec);
    bool CancelSessionTokenTimerOnce(const Http::Request &req);

private:
    struct SessionDetails {
        std::chrono::time_point<std::chrono::system_clock> LastRequestAt;
        std::chrono::time_point<std::chrono::system_clock> StartAt;
    };

    Map<String, SessionDetails> m_leased_session_tokens;
    Optional<String> m_active_session_token;
    Mutex m_session_token_mutex;
    Thread m_checking_session_expiration_thread;
    Map<String, SessionTimeoutCB> m_session_timeout_callbacks;
    Mutex m_session_timeout_callbacks_mutex;
    std::chrono::seconds m_session_timeout_sec;
    std::atomic_bool m_checking_session_expiration_quit_flag = false;
    struct TimerThreadDetails {
        SessionTokenTimerCB TimerCB;
        std::chrono::time_point<std::chrono::system_clock> StartAt;
        std::chrono::seconds Timeout;
        bool QuitFlag; // TODO: Since the structure is protected by mutex, is required atomic operation here?
    };
    Map<String, TimerThreadDetails> m_session_token_timers;
    Mutex m_session_token_timers_mutex;
};
