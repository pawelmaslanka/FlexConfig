#include "xpath.hpp"
#include "node/composite.hpp"
#include "utils.hpp"

#include <iostream>

bool XPath::NodeFinder::init(const String wanted_node_name) {
    _wanted_node_name = wanted_node_name;
    _wanted_node = nullptr;
    return true;
}

bool XPath::NodeFinder::visit(SharedPtr<Node> node) {
    if (node->getName() == _wanted_node_name) {
        _wanted_node = node;
        return false; // Stop visitng another nodes
    }

    return true; // Continue visiting other nodes
}

SharedPtr<Node> XPath::NodeFinder::get_result() {
    return _wanted_node;
}

SharedPtr<Queue<String>> XPath::parse(const String xpath) {
    auto xpath_items = std::make_shared<Queue<String>>();
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    while (token != nullptr) {
        xpath_items->push(token);
        token = ::strtok(nullptr, XPath::SEPARATOR);
    }

    return xpath_items;
}

Queue<String> XPath::parse3(const String xpath) {
    Queue<String> xpath_items;
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    while (token != nullptr) {
        xpath_items.push(token);
        token = ::strtok(nullptr, XPath::SEPARATOR);
    }

    return xpath_items;
}

Deque<String> XPath::parse4(const String xpath) {
    Deque<String> xpath_items;
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    while (token != nullptr) {
        xpath_items.push_back(token);
        token = ::strtok(nullptr, XPath::SEPARATOR);
    }

    return xpath_items;
}

SharedPtr<Node> XPath::select(SharedPtr<Node> root_node, const String xpath) {
    if (!root_node || xpath.empty()) {
        return nullptr;
    }

    auto xpath_items = parse(xpath);
    auto node_finder = std::make_shared<XPath::NodeFinder>();
    auto visiting_node = root_node;
    while (!xpath_items->empty()) { 
        auto item = xpath_items->front();
        auto left_pos = item.find(XPath::SUBSCRIPT_LEFT_PARENTHESIS);
        auto right_pos = item.find(XPath::SUBSCRIPT_RIGHT_PARENTHESIS);
        if (left_pos != String::npos
            && right_pos != String::npos) {
            xpath_items->pop();
            for (auto new_item : { item.substr(0, left_pos), item.substr(left_pos + 1, (right_pos - left_pos - 1)) }) {
                node_finder->init(new_item);
                visiting_node->accept(*node_finder);
                visiting_node = node_finder->get_result();
                if (!visiting_node) {
                    return nullptr;
                }
            }

            continue;
        }

        node_finder->init(xpath_items->front());
        visiting_node->accept(*node_finder);
        visiting_node = node_finder->get_result();
        if (!visiting_node) {
            return nullptr;
        }

        xpath_items->pop();
    }

    return visiting_node;
}

SharedPtr<Node> XPath::select2(SharedPtr<Node> root_node, const String xpath) {
    if (!root_node || xpath.empty()) {
        return nullptr;
    }

    auto xpath_items = parse(xpath);
    auto node_finder = std::make_shared<XPath::NodeFinder>();
    auto visiting_node = root_node;
    while (!xpath_items->empty()) { 
        auto item = xpath_items->front();
        if ((item == "value") && (xpath_items->size() == 1)) {
            return visiting_node;
        }

        auto left_pos = item.find(XPath::SUBSCRIPT_LEFT_PARENTHESIS);
        auto right_pos = item.find(XPath::SUBSCRIPT_RIGHT_PARENTHESIS);
        if (left_pos != String::npos
            && right_pos != String::npos) {
            xpath_items->pop();
            for (auto new_item : { item.substr(0, left_pos), item.substr(left_pos + 1, (right_pos - left_pos - 1)) }) {
                node_finder->init(new_item);
                visiting_node->accept(*node_finder);
                visiting_node = node_finder->get_result();
                if (!visiting_node) {
                    return nullptr;
                }
            }

            continue;
        }

        node_finder->init(xpath_items->front());
        visiting_node->accept(*node_finder);
        visiting_node = node_finder->get_result();
        if (!visiting_node) {
            return nullptr;
        }

        xpath_items->pop();
    }

    return visiting_node;
}

String XPath::mergeTokens(const Deque<String>& xpath_tokens) {
    String xpath = "/";
    for (auto& token : xpath_tokens) {
        if (xpath.at(xpath.size() - 1) != '/') {
            xpath += "/";
        }

        xpath += token;
    }

    return xpath;
}

String XPath::to_string(SharedPtr<Node> node) {
    String xpath;
    Stack<String> xpath_stack;
    auto processing_node = node;

    while (processing_node) {
        if (processing_node->getParent()) {
            if (auto dict = std::dynamic_pointer_cast<Composite>(processing_node->getParent())) {
                xpath_stack.push(XPath::SUBSCRIPT_LEFT_PARENTHESIS + processing_node->getName() + XPath::SUBSCRIPT_RIGHT_PARENTHESIS);
                processing_node = processing_node->getParent();
                continue;
            }
        }

        xpath_stack.push(processing_node->getName());
        processing_node = processing_node->getParent();
    }

    while (!xpath_stack.empty()) {
        if (xpath_stack.top() != XPath::SEPARATOR) {
            if (xpath_stack.top()[0] != '[') {
                xpath += XPath::SEPARATOR;
            }

            xpath += xpath_stack.top();
        }

        xpath_stack.pop();
    }

    return xpath;
}

String XPath::to_string2(SharedPtr<Node> node) {
    String xpath;
    Stack<String> xpath_stack;
    auto processing_node = node;

    while (processing_node) {
        xpath_stack.push(processing_node->getName());
        processing_node = processing_node->getParent();
    }

    while (!xpath_stack.empty()) {
        if (xpath_stack.top() != XPath::SEPARATOR) {
            if (xpath_stack.top()[0] != '[') {
                xpath += XPath::SEPARATOR;
            }

            xpath += xpath_stack.top();
        }

        xpath_stack.pop();
    }

    return xpath;
}

struct NodeCounter : public Visitor {
    size_t node_cnt;
    NodeCounter() : node_cnt(0) {}
    virtual ~NodeCounter() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        ++node_cnt;
        return true;
    }
};

size_t XPath::count_members(SharedPtr<Node> root_node, const String xpath) {
    auto wanted_node = select(root_node, xpath);
    if (!wanted_node) {
        return 0;
    }

    auto node_counter = std::make_shared<NodeCounter>();
    wanted_node->accept(*node_counter);
    return node_counter->node_cnt;
}

SharedPtr<Node> XPath::get_root(SharedPtr<Node> start_node) {
    if (!start_node) {
        return nullptr;
    }

    while (start_node->getParent()) {
        start_node = start_node->getParent();
    }

    return start_node;
}

String XPath::evaluate_xpath_key(SharedPtr<Node> start_node, String xpath) {
    if (xpath.at(0) != XPath::SEPARATOR[0]) {
        return {};
    }

    Utils::find_and_replace_all(xpath, XPath::KEY_NAME_SUBSCRIPT, String(XPath::SEPARATOR) + XPath::KEY_NAME_SUBSCRIPT);
    auto node_xpath = to_string2(start_node);
    auto xpath_tokens = parse3(xpath);
    auto node_xpath_tokens = parse3(node_xpath);
    while (!xpath_tokens.empty()) {
        auto token = xpath_tokens.front();
        xpath_tokens.pop();
        if (token == XPath::KEY_NAME_SUBSCRIPT) {
            return node_xpath_tokens.front();
        }

        node_xpath_tokens.pop();
    }

    return {};
}

String XPath::evaluate_xpath2(SharedPtr<Node> start_node, String xpath) {
    if (xpath.at(0) != XPath::SEPARATOR[0]) {
        return {};
    }

    Utils::find_and_replace_all(xpath, XPath::KEY_NAME_SUBSCRIPT, String(XPath::SEPARATOR) + XPath::KEY_NAME_SUBSCRIPT);
    MultiMap<String, std::size_t> idx_by_xpath_item;
    Vector<String> xpath_items;
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    size_t idx = 0;
    while (token != nullptr) {
        xpath_items.push_back(token);
        idx_by_xpath_item.emplace(token, idx);
        token = strtok(NULL, XPath::SEPARATOR);
        ++idx;
    }

    // Resolve @key key
    auto key_name_pos = idx_by_xpath_item.find(XPath::KEY_NAME_SUBSCRIPT);
    if (key_name_pos != std::end(idx_by_xpath_item)) {
        // Find last idx
        idx = 0; 
        auto range = idx_by_xpath_item.equal_range(XPath::KEY_NAME_SUBSCRIPT);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second > idx) {
                idx = it->second;
            }
        }

        String wanted_child = xpath_items.at(idx);
        if (xpath_items.size() > (idx + 1)) {
            wanted_child = xpath_items.at(idx + 1);
        }

        auto curr_node = start_node;
        while ((curr_node->getName() != wanted_child)
                && (curr_node->getParent() != nullptr)) {
            curr_node = curr_node->getParent();
        }

        String resolved_key_name;
        if (curr_node->getName() != wanted_child) {
            curr_node = start_node;
            do {
                if (auto list = std::dynamic_pointer_cast<Composite>(curr_node->getParent())) {
                    resolved_key_name = curr_node->getName();
                    break;
                }

                curr_node = curr_node->getParent();
            } while (curr_node);
        }
        else {
            resolved_key_name = curr_node->getParent()->getName(); // We want to resolve name of parent node based on its child node
        }

        // Resolve all occurences of XPath::KEY_NAME_SUBSCRIPT
        for (auto it = range.first; it != range.second; ++it) {
            xpath_items.at(it->second) = resolved_key_name;
        }
    }

    String evaluated_xpath;
    for (auto item : xpath_items) {
        evaluated_xpath += XPath::SEPARATOR;
        evaluated_xpath += item;
    }

    return evaluated_xpath;
}

String XPath::evaluate_xpath(SharedPtr<Node> start_node, String xpath) {
    if (xpath.at(0) != XPath::SEPARATOR[0]) {
        return {};
    }

    Utils::find_and_replace_all(xpath, XPath::KEY_NAME_SUBSCRIPT, String(XPath::SEPARATOR) + XPath::KEY_NAME_SUBSCRIPT);
    MultiMap<String, std::size_t> idx_by_xpath_item;
    Vector<String> xpath_items;
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    size_t idx = 0;
    while (token != nullptr) {
        xpath_items.push_back(token);
        idx_by_xpath_item.emplace(token, idx);
        token = strtok(NULL, XPath::SEPARATOR);
        ++idx;
    }

    // Resolve @key key
    auto key_name_pos = idx_by_xpath_item.find(XPath::KEY_NAME_SUBSCRIPT);
    if (key_name_pos != std::end(idx_by_xpath_item)) {
        auto start_node_xpath = to_string2(start_node);
        // Find last idx
        idx = 0; 
        auto range = idx_by_xpath_item.equal_range(XPath::KEY_NAME_SUBSCRIPT);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second > idx) {
                idx = it->second;
            }
        }

        String wanted_child = xpath_items.at(idx);
        if (xpath_items.size() > (idx + 1)) {
            wanted_child = xpath_items.at(idx + 1);
        }

        auto curr_node = start_node;
        while ((curr_node->getName() != wanted_child)
                && (curr_node->getParent() != nullptr)) {
            curr_node = curr_node->getParent();
        }

        Queue<String> xpath_stack {};
        token = ::strtok(const_cast<char*>(start_node_xpath.c_str()), XPath::SEPARATOR);
        // Keep printing tokens while one of the
        // delimiters present in str[].
        while (token != nullptr) {
            xpath_stack.push(token);
            token = strtok(NULL, XPath::SEPARATOR);
        }

        String resolved_key_name;
        auto root_node = get_root(start_node);
        class KeyFindVisitor : public Visitor {
        public:
            virtual ~KeyFindVisitor() = default;
            KeyFindVisitor(Queue<String> xpath_stack) : m_xpath_stack { xpath_stack } {}
            virtual bool visit(SharedPtr<Node> node) {
                if (m_xpath_stack.front() != node->getName()) {
                    std::clog << "XPath stack node is invalid: " << node->getName() << ", expected: " << m_xpath_stack.front() << std::endl;
                    return true;
                }

                m_xpath_stack.pop();
                auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
                if (schema_node && schema_node->getName() == "@item") {
                    m_key = node->getName();
                    return false;
                }

                return true;
            }

            String getKey() { return m_key; }

        private:
            String m_key {};
            Queue<String> m_xpath_stack {};
        };

        KeyFindVisitor keyFindVisitor(xpath_stack);
        root_node->accept(keyFindVisitor);
        resolved_key_name = keyFindVisitor.getKey();
        // Resolve all occurences of XPath::KEY_NAME_SUBSCRIPT
        for (auto it = range.first; it != range.second; ++it) {
            xpath_items.at(it->second) = resolved_key_name;
        }
    }

    String evaluated_xpath;
    for (auto item : xpath_items) {
        evaluated_xpath += XPath::SEPARATOR;
        evaluated_xpath += item;
    }

    return evaluated_xpath;
}

List<String> XPath::parse2(const String xpath) {
    std::list<String> xpath_items = {};
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    while (token != nullptr) {
        xpath_items.emplace_back(token);
        token = ::strtok(nullptr, XPath::SEPARATOR);
    }

    return xpath_items;
}