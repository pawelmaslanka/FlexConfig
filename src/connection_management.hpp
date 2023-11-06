#pragma once

#include "lib/types.hpp"

#include <map>

namespace ConnectionManagement {
enum class Method {
    GET,
    PUT,
    POST,
    DELETE
};

class Client {
public:

private:
};

using RequestCallback = std::function<bool(const String& path, String data_request, String& return_data)>;

class Server {
public:
    bool addOnGetConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnGetConnectionHandler(const String& id);
    bool addOnPostConnectionHandler(const String& id, RequestCallback handler);
    bool removeOnPostConnectionHandler(const String& id);
    bool run(const String& host, const UInt16 port);

private:
    bool processRequest(const Method method, const String& path, const String& request_data, String& return_data);
    bool addConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id, RequestCallback handler);
    bool removeConnectionHandler(Map<String, RequestCallback>& callbacks, const String& id);
    Map<String, RequestCallback> m_on_get_callback_by_id;
    Map<String, RequestCallback> m_on_post_callback_by_id;
};
} // ConnectionManagement