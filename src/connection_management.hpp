#pragma once

#include "lib/std_types.hpp"

#include <map>

namespace ConnectionManagement {
namespace URIRequestPath {
namespace Config {
    static constexpr auto CANDIDATE = "/config/candidate";
    static constexpr auto RUNNING = "/config/running";
    static constexpr auto RUNNING_UPDATE = "/config/running/update";
    static constexpr auto RUNNING_DIFF = "/config/running/diff";
} // namespace Config

namespace Session {
    static constexpr auto TOKEN = "/session/token";
} // namespace Session
}

enum class Method {
    GET,
    PUT,
    POST,
    DELETE
};

namespace HTTP {
    enum StatusCode {
        // For internal use
        INTERNAL_SUCCESS = 0,
        // Informational responses
        CONTINUE = 100,
        // Successful responses
        START_SUCCESS = 200,
        OK = START_SUCCESS,
        CREATED = 201,
        END_SUCCESS = 299,
        // Redirection messages
        SEE_OTHER = 303,
        // Client error responses
        CONFLICT = 409,
        INVALID_TOKEN = 498,
        TOKEN_REQUIRED = 499,
        // Server error responses
        INTERNAL_SERVER_ERROR = 500,
    };

    bool IsSuccess(const StatusCode status_code);
}

class Client {
public:

private:
};

using RequestCallback = std::function<HTTP::StatusCode(const String& path, String data_request, String& return_data)>;

class Server {
public:
    bool addOnDeleteConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnDeleteConnectionHandler(const String& id);
    bool addOnGetConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnGetConnectionHandler(const String& id);
    bool addOnPostConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnPostConnectionHandler(const String& id);
    bool addOnPutConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnPutConnectionHandler(const String& id);
    bool Run(const String& host, const UInt16 port);

private:
    HTTP::StatusCode processRequest(const Method method, const String& path, const String& request_data, String& return_data);
    bool addConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id, RequestCallback handler);
    bool removeConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id);
    Map<String, RequestCallback> m_on_delete_callback_by_id;
    Map<String, RequestCallback> m_on_get_callback_by_id;
    Map<String, RequestCallback> m_on_post_callback_by_id;
    Map<String, RequestCallback> m_on_put_callback_by_id;
    struct SessionDetails {
        std::chrono::time_point<std::chrono::system_clock> LastRequestAt;
        std::chrono::time_point<std::chrono::system_clock> StartAt;
    };
    Map<String, SessionDetails> m_leased_session_token;
};
} // ConnectionManagement