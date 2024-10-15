/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#include "constraint_checking.hpp"

#include "node/leaf.hpp"
#include "node/visitor_spec.hpp"
#include "xpath.hpp"
#include "lib/utils.hpp"

#include "peglib.h"
#include <spdlog/spdlog.h>

#include <fstream>
#include <variant>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

using namespace peg;
using namespace peg::udl;

#include <any>
#include <functional>
#include <iomanip>

template<class T, class F>
inline std::pair<const std::type_index, std::function<void(std::any const&)>>
    to_any_visitor(F const &f)
{
    return
    {
        std::type_index(typeid(T)),
        [g = f](std::any const &a)
        {
            if constexpr (std::is_void_v<T>)
                g();
            else
                g(AnyCast<T const&>(a));
        }
    };
}
 
static std::unordered_map<std::type_index, std::function<void(std::any const&)>>
    any_visitor
{
    to_any_visitor<void>([] { std::cout << "{}" << std::endl; }),
    to_any_visitor<bool>([](bool x) { std::cout << std::boolalpha << x << std::endl; }),
    to_any_visitor<int>([](int x) { std::cout << x << std::endl; }),
    to_any_visitor<unsigned>([](unsigned x) { std::cout << x << std::endl; }),
    to_any_visitor<float>([](float x) { std::cout << x << std::endl; }),
    to_any_visitor<double>([](double x) { std::cout << x << std::endl; }),
    to_any_visitor<char>([](char x) { std::cout << x << std::endl; }),
    to_any_visitor<long>([](long x) { std::cout << x << std::endl; }),
    to_any_visitor<char const*>([](char const *s)
        { std::cout << std::quoted(s) << std::endl; }),
    to_any_visitor<std::string_view>([](std::string_view s)
        { std::cout << std::quoted(s) << std::endl; }),
    // ... add more handlers for your types ...
};
 
inline void process(const std::any& a)
{
  std::cout << a.type().name() << std::endl;
    if (const auto it = any_visitor.find(std::type_index(a.type()));
        it != any_visitor.cend())
        it->second(a);
    else
        std::cout << "Unregistered type "<< std::quoted(a.type().name());
}

ConstraintChecker::ConstraintChecker(SharedPtr<Config::Manager>& ConfigMngr, SharedPtr<Node>& RootNodeConfig)
: m_config_mngr { ConfigMngr }, m_root_config { RootNodeConfig } {

}

namespace SemanticAction {
struct Argument {
    SharedPtr<Config::Manager> ConfigMngr; // TODO: Every node should be associated with its schema
    SharedPtr<Node> RootNodeConfig; // TODO: It can be derived from CurrentProcessingNode
    SharedPtr<Node> CurrentProcessingNode;
    bool ContinueProcessing = { true };
    bool ExpressionResult = { false };
};

#define CHECK_PEG_ARGUMENT(PEG_ARGUMENT, SV) \
    AnyCast<Argument&>(PEG_ARGUMENT); \
    spdlog::debug("Enter to rule action '{}' at line '{}': '{}' ({})", SV.name(), __LINE__, SV.token_to_string(), SV.size()); \
    if (!pegArg.ContinueProcessing) { \
        spdlog::debug("Stop to continue processing of the '{}' rule", SV.name()); \
        return {}; \
    } \
    do { } while (0)

#define STOP_PROCESSING(PEG_ARGUMENT, SV) \
    { \
        auto pegArg = AnyCast<Argument&>(PEG_ARGUMENT); \
        spdlog::error("Stopped to process action on the rule '{}'", SV.name()); \
        pegArg.ContinueProcessing = false; \
    } \
    return {}

#define TRY_CATCH_PROLOG() \
    try { \
        do { } while (0)

#define TRY_CATCH_EPILOG() \
    } catch (const BadAnyCast& exc) { \
        spdlog::error("Caught cast exception: {}", exc.what()); \
    } \
    pegArg.ExpressionResult = false

#define ACTION_PROLOG(DT, SV) \
    CHECK_PEG_ARGUMENT(DT, SV); \
    TRY_CATCH_PROLOG()

#define ACTION_EPILOG(DT, SV) \
    TRY_CATCH_EPILOG(); \
    STOP_PROCESSING(DT, SV)

// Token Rule handlers
bool BooleanHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String token = vs.token_to_string();
    if (token.find("true") != StringEnd()) {
        return true;
    }
    else if (token.find("false") != StringEnd()) {
        return false;
    }

    ACTION_EPILOG(dt, vs);
}

Int64 CountHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    Int64 count = 0;
    for (const auto& xpath : AnyCast<Vector<String>>(vs[0])) {
        if (XPath::select(pegArg.RootNodeConfig, xpath)) {
            ++count;
        }
    }

    return count;

    ACTION_EPILOG(dt, vs);
}

bool ExistsHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    for (const auto& xpath : AnyCast<Vector<String>>(vs[0])) {
        if (XPath::select(pegArg.RootNodeConfig, xpath)) {
            return true;
        }
    }

    return false;

    ACTION_EPILOG(dt, vs);
}

bool InfixExpressionHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    bool result = false;
    if (vs[0].type() == typeid(bool)) {
        bool arg1 = AnyCast<bool>(vs[0]);
        result = arg1;
        if (vs.size() > 1) {
            String op = AnyCast<String>(vs[1]);
            bool arg2 = AnyCast<bool>(vs[2]);
            if (op == "==") {
                result = arg1 == arg2;
            }
            else if (op == "<>") {
                result = arg1 != arg2;
            }
            else {
                spdlog::error("Failed to recognize relative operator '{}'", op);
                pegArg.ExpressionResult = false;
                STOP_PROCESSING(dt, vs);
            }
        }
    }
    else if (vs[0].type() == typeid(String)) {
        String arg1 = AnyCast<String>(vs[0]);
        result = !arg1.empty();
        if (vs.size() > 1) {
            String op = AnyCast<String>(vs[1]);
            String arg2 = AnyCast<String>(vs[2]);
            if (op == "==") {
                result = arg1 == arg2;
            }
            else if (op == "<>") {
                result = arg1 != arg2;
            }
            else {
                spdlog::error("Failed to recognize relative operator '{}'", op);
                pegArg.ExpressionResult = false;
                STOP_PROCESSING(dt, vs);
            }
        }
    }
    else if (vs[0].type() == typeid(Int64)) {
        Int64 arg1 = AnyCast<Int64>(vs[0]);
        result = arg1 != 0;
        if (vs.size() > 1) {
            auto op = AnyCast<String>(vs[1]);
            auto arg2 = AnyCast<Int64>(vs[2]);
            if (op == "==") {
                result = arg1 == arg2;
            }
            else if (op == "<>") {
                result = arg1 != arg2;
            }
            else {
                spdlog::error("Failed to recognize relative operator '{}'", op);
                pegArg.ExpressionResult = false;
                STOP_PROCESSING(dt, vs);
            }
        }
    }
    else {
        spdlog::error("Not recognized type of argument '{}'", vs.sv());
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    pegArg.ContinueProcessing = result;
    return result;

    ACTION_EPILOG(dt, vs);
}

bool MustHandle(const SemanticValues& vs, Any& dt) {
    spdlog::debug("Enter to rule action '{}' at line '{}': '{}' ({})", vs.name(), __LINE__, vs.token_to_string(), vs.size());
    Argument& pegArg = AnyCast<Argument&>(dt);
    if (!pegArg.ContinueProcessing) {
        spdlog::debug("Stop to continue processing '{}' rule", vs.name());
        return pegArg.ExpressionResult;
    }

    try {
        pegArg.ExpressionResult = AnyCast<bool>(vs[0]);
        return pegArg.ExpressionResult;
    }
    catch (const BadAnyCast& exc) {
        spdlog::error("Caught bad cast exception: {}", exc.what());
        pegArg.ContinueProcessing = pegArg.ExpressionResult = false;
    }

    return false;
}

Int64 NumberHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    return vs.token_to_number<Int64>();

    ACTION_EPILOG(dt, vs);
}

String ReferenceHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String ref_token = vs.token_to_string();
    if (ref_token != "@") {
        spdlog::error("Unexpected reference token '{}'", ref_token);
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    auto node = pegArg.CurrentProcessingNode;
    auto schema_node = pegArg.ConfigMngr->getSchemaByXPath(XPath::toString(pegArg.CurrentProcessingNode));
    if (!schema_node) {
        spdlog::debug("Not found schema node at xpath '{}'", XPath::toString(pegArg.CurrentProcessingNode));
        return {};
    }

    auto ref_attrs = schema_node->FindAttr("reference");
    if (ref_attrs.empty()) {
        spdlog::debug("Not found attribute 'reference' at schema node {}", XPath::toString(schema_node));
        return {};
    }

    for (String ref : ref_attrs) {
        String node_name = pegArg.CurrentProcessingNode->Name();
        if ((ref.find("/@") != StringEnd()) && (ref[ref.length() - 1] == '@')) {
            Utils::find_and_replace_all(ref, "/@", XPath::SEPARATOR + node_name);
        }

        auto evaluated_ref = XPath::evaluateXPath(pegArg.CurrentProcessingNode, ref);
        spdlog::debug("Evaluated reference of '{} to '{}''", ref, evaluated_ref);
        if (XPath::select(pegArg.RootNodeConfig, evaluated_ref)) {
            spdlog::debug("Found node at xpath '{}'", evaluated_ref);
            return evaluated_ref;
        }
    }

    return {};

    ACTION_EPILOG(dt, vs);
}

String RelativeOperatorHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    return Utils::Trim(vs.token_to_string());

    ACTION_EPILOG(dt, vs);
}

String StringHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    return Utils::Trim(vs.token_to_string());

    ACTION_EPILOG(dt, vs);
}

void ThenStatementHandle(const SemanticValues& vs, Any& dt) {
    spdlog::debug("Enter to rule action '{}' at line '{}': '{}' ({})", vs.name(), __LINE__, vs.token_to_string(), vs.size());
    Argument& pegArg = AnyCast<Argument&>(dt);
    if (!pegArg.ContinueProcessing) {
        spdlog::debug("Stop to continue processing '{}' rule", vs.name());
        pegArg.ExpressionResult = true;
    }
}

Vector<String> XPathHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String xpath = AnyCast<String>(vs[0]);
    if (xpath.empty()) {
        spdlog::debug("There is not any xpath to convert to node");
        return {};
    }

    auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
    if (!XPath::select(pegArg.RootNodeConfig, resolved_xpath)) {
        spdlog::debug("Not found node at xpath '{}'", xpath);
        return {};
    }

    return Vector<String> { resolved_xpath };

    ACTION_EPILOG(dt, vs);
}

Any XPathValueHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String xpath = AnyCast<String>(vs[0]);
    if (xpath.empty()) {
        spdlog::debug("There is not any xpath to convert to node");
        return {};
    }

    Utils::find_and_replace_all(xpath, "[@item]", "/[@item]");
    auto xpath_tokens = XPath::parse(xpath);
    auto resolved_xpath = XPath::toString(pegArg.CurrentProcessingNode);
    // FIXME: XPath::evaluateXPath() does not do the same what this code
    // auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
    auto resolved_xpath_tokens = XPath::parse(resolved_xpath);
    String rebuild_resolved_xpath;
    while (!xpath_tokens.empty()) {
        auto token = xpath_tokens.front();
        xpath_tokens.pop_front();
        if ((token == "[@item]")
            && (!resolved_xpath_tokens.empty())) {
            spdlog::debug("Found '{}' token and it is going to be replaced with '{}'", token, resolved_xpath_tokens.front());
            token = resolved_xpath_tokens.front();
        }

        rebuild_resolved_xpath += "/" + token;
        if (!resolved_xpath_tokens.empty()) {
            resolved_xpath_tokens.pop_front();
        }
    }

    spdlog::debug("Resolved xpath '{}' for current node '{}'", rebuild_resolved_xpath, pegArg.CurrentProcessingNode->Name());
    auto node = XPath::select(pegArg.RootNodeConfig, rebuild_resolved_xpath);
    if (!node) {
        spdlog::debug("Not found node at xpath '{}'", xpath);
        return {};
    }

    auto leaf_node = std::dynamic_pointer_cast<Leaf>(node);
    if (!leaf_node) {
        spdlog::debug("Node '{}' at xpath '{}' is not a leaf", node->Name(), xpath);
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    auto value = leaf_node->getValue();
    if (value.is_bool()) {
        return Any { value.get_bool() };
    }
    else if (value.is_number()) {
        return Any { value.get_number() };
    }
    else if (value.is_string()) {
        return Any { value.get_string() };
    }
    else {
        spdlog::error("Unsupported type of node value");
    }

    pegArg.ExpressionResult = false;
    ACTION_EPILOG(dt, vs);
}

Vector<String> XPathMatchRegexHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    if (vs.size() < 3) {
        spdlog::error("There is too few arguments {} to perform action", vs.size());
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    Vector<String> result;
    String regex_expression;
    ForwardList<String> xpath_subnodes;
    for (auto i = 0; i < 3; ++i) {
        String xpath = AnyCast<String>(vs[2 - i]);
        if (xpath.empty()) {
            spdlog::debug("There is not any xpath to convert to node");
            return {};
        }

        if (i == 0) {
            regex_expression = xpath;
            continue;
        }

        auto wildcard_pos = xpath.find("/*");
        if ((i == 1) && (wildcard_pos != String::npos)) {
            xpath = xpath.substr(0, wildcard_pos);
            auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
            auto node = XPath::select(pegArg.RootNodeConfig, resolved_xpath);
            if (!node) {
                spdlog::debug("Node at xpath '{}' does not exists", resolved_xpath);
                continue;
            }
            // TODO: Implement it based on WildcardDependencyResolverVisitior
            NodeChildsOnlyVisitor node_childs_only_visitor(node);
            node->Accept(node_childs_only_visitor);
            xpath_subnodes = node_childs_only_visitor.getAllSubnodes();
            continue;
        }
        else if (i == 1) {
            xpath_subnodes.emplace_front(xpath);
            continue;
        }

        auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
        auto node = XPath::select(pegArg.RootNodeConfig, resolved_xpath);
        if (!node) {
            spdlog::debug("Not found node at xpath '{}'", xpath);
            return {};
        }

        auto node_xpath = XPath::toString(node);
        ForwardList<String> matched_xpaths;
        for (auto& xpath_node_child : xpath_subnodes) {
            if (xpath_node_child.find(node_xpath) != String::npos) {
                matched_xpaths.emplace_front(xpath_node_child);
            }
        }

        for (auto& matched_xpath : matched_xpaths) {
            if (std::regex_match(matched_xpath, Regex { regex_expression })) {
                if (XPath::select(pegArg.RootNodeConfig, matched_xpath)) {
                    result.emplace_back(matched_xpath);
                }
            }
        }
    }

    return result;

    ACTION_EPILOG(dt, vs);
}

Vector<String> XPathAnyHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String xpath = AnyCast<String>(vs[0]);
    if (xpath.empty()) {
        spdlog::debug("There is not any xpath to convert to node");
        return {};
    }

    if (xpath[xpath.length() - 1] == '@') {
        xpath = xpath.substr(0, xpath.length() - 1) + pegArg.CurrentProcessingNode->Name();
        spdlog::debug("Replaced reference mark as {} in xpath {}", pegArg.CurrentProcessingNode->Name(), xpath);
    }

    ForwardList<String> xpath_subnodes;
    auto wildcard_pos = xpath.find("/*");
    if (wildcard_pos != StringEnd()) {
        spdlog::debug("Found wildcard mark at xpath {}", xpath);
        auto parent_xpath = xpath.substr(0, wildcard_pos);
        auto node = XPath::select(pegArg.RootNodeConfig, parent_xpath);
        if (!node) {
            spdlog::debug("Node at xpath '{}' does not exists", parent_xpath);
            return {};
        }
        spdlog::debug("Converted node '{}' to xpath '{}'", node->Name(), XPath::toString(node));
        // TODO: Implement it based on WildcardDependencyResolverVisitior
        NodeChildsOnlyVisitor node_childs_only_visitor(node);
        node->Accept(node_childs_only_visitor);
        xpath_subnodes = node_childs_only_visitor.getAllSubnodes();
    }

    Vector<String> result;
    ForwardList<String> matched_xpaths;
    for (auto& xpath_node_child : xpath_subnodes) {
        auto resolved_wildcard_xpath = xpath_node_child + ((wildcard_pos != StringEnd()) ? (XPath::SEPARATOR + xpath.substr(wildcard_pos + 3)) : "" );
        spdlog::debug("Checking resolved xpath '{}'", resolved_wildcard_xpath);
        auto node = XPath::select(pegArg.RootNodeConfig, resolved_wildcard_xpath);
        if (node) {
            spdlog::debug("Found node at xpath '{}'", resolved_wildcard_xpath);
            result.emplace_back(resolved_wildcard_xpath);
            break;
        }
    }

    spdlog::debug("Found {} nodes at xpath '{}'", result.size(), AnyCast<String>(vs[0]));
    return result;
    ACTION_EPILOG(dt, vs);
}

Vector<String> XPathAllHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    String xpath = AnyCast<String>(vs[0]);
    if (xpath.empty()) {
        spdlog::debug("There is not any xpath to convert to node");
        return {};
    }

    if (xpath[xpath.length() - 1] == '@') {
        xpath = xpath.substr(0, xpath.length() - 1) + pegArg.CurrentProcessingNode->Name();
        spdlog::debug("Replaced reference mark as {} in xpath {}", pegArg.CurrentProcessingNode->Name(), xpath);
    }

    ForwardList<String> xpath_subnodes;
    auto wildcard_pos = xpath.find("/*");
    if (wildcard_pos != StringEnd()) {
        spdlog::debug("Found wildcard mark at xpath {}", xpath);
        auto parent_xpath = xpath.substr(0, wildcard_pos);
        auto node = XPath::select(pegArg.RootNodeConfig, parent_xpath);
        if (!node) {
            spdlog::debug("Node at xpath '{}' does not exists", parent_xpath);
            return {};
        }
        spdlog::debug("Converted node '{}' to xpath '{}'", node->Name(), XPath::toString(node));
        // TODO: Implement it based on WildcardDependencyResolverVisitior
        NodeChildsOnlyVisitor node_childs_only_visitor(node);
        node->Accept(node_childs_only_visitor);
        xpath_subnodes = node_childs_only_visitor.getAllSubnodes();
    }

    Vector<String> result;
    ForwardList<String> matched_xpaths;
    for (auto& xpath_node_child : xpath_subnodes) {
        auto resolved_wildcard_xpath = xpath_node_child + ((wildcard_pos != StringEnd()) ? (XPath::SEPARATOR + xpath.substr(wildcard_pos + 3)) : "" );
        spdlog::debug("Checking resolved xpath '{}'", resolved_wildcard_xpath);
        auto node = XPath::select(pegArg.RootNodeConfig, resolved_wildcard_xpath);
        if (node) {
            spdlog::debug("Found node at xpath '{}'", resolved_wildcard_xpath);
            result.emplace_back(resolved_wildcard_xpath);
        }
    }

    spdlog::debug("Found {} nodes at xpath '{}'", result.size(), AnyCast<String>(vs[0]));
    return result;
    ACTION_EPILOG(dt, vs);
}

Vector<String> XPathKeyBasedHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    if (vs.size() < 2) {
        spdlog::error("There is too few arguments {} to perform action", vs.size());
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    Vector<String> result;
    String xpath_key_reference;
    ForwardList<String> xpath_subnodes;
    for (auto i = 0; i < 2; ++i) {
        String xpath = AnyCast<String>(vs[1 - i]);
        if (xpath.empty()) {
            spdlog::debug("There is not any xpath to convert to node");
            return {};
        }

        if (i == 0) {
            xpath_key_reference = xpath;
            continue;
        }

        Utils::find_and_replace_all(xpath_key_reference, "[@item]", "/[@item]");
        auto resolved_reference_xpath_key = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath_key_reference);
        if (resolved_reference_xpath_key.empty()) {
            spdlog::debug("Failed to resolve reference key at xpath '{}'", xpath_key_reference);
            return {};
        }

        Utils::find_and_replace_all(xpath, "[@item]", "/" + resolved_reference_xpath_key);
        if (!XPath::select(pegArg.RootNodeConfig, xpath)) {
            spdlog::debug("Not found node at xpath '{}'", xpath);
            return {};
        }

        result.emplace_back(xpath);
    }

    return result;

    ACTION_EPILOG(dt, vs);
}

Vector<String> XPathKeyRegexReplaceHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    if (vs.size() < 2) {
        spdlog::error("There is too few arguments {} to perform action", vs.size());
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    Vector<String> result;
    String regex_expression;
    ForwardList<String> xpath_subnodes;
    for (auto i = 0; i < 2; ++i) {
        String xpath = AnyCast<String>(vs[1 - i]);
        if (xpath.empty()) {
            spdlog::debug("There is not any xpath to convert to node");
            return {};
        }

        if (i == 0) {
            regex_expression = xpath;
            continue;
        }

        Utils::find_and_replace_all(xpath, "[@item]", "/[@item]");
        auto xpath_tokens = XPath::parse(xpath);
        auto resolved_xpath = XPath::toString(pegArg.CurrentProcessingNode);
        // FIXME: XPath::evaluateXPath() does not do the same what this code
        // auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
        auto resolved_xpath_tokens = XPath::parse(resolved_xpath);
        String rebuild_resolved_xpath;
        while (!xpath_tokens.empty()) {
            auto token = xpath_tokens.front();
            xpath_tokens.pop_front();
            if ((token == "[@item]")
                && (!resolved_xpath_tokens.empty())) {
                std::smatch matched;
                if (std::regex_search(resolved_xpath_tokens.front(), matched, Regex { regex_expression })) {
                    token = matched[0];
                }
            }

            rebuild_resolved_xpath += "/" + token;
            if (!resolved_xpath_tokens.empty()) {
                resolved_xpath_tokens.pop_front();
            }
        }

        spdlog::debug("Composed xpath {}", rebuild_resolved_xpath);
        if (!XPath::select(pegArg.RootNodeConfig, rebuild_resolved_xpath)) {
            spdlog::debug("Not found node at xpath {}", xpath);
            return {};
        }

        result.emplace_back(rebuild_resolved_xpath);
    }

    return result;

    ACTION_EPILOG(dt, vs);
}

Any XPathValueKeyRegexReplaceHandle(const SemanticValues& vs, Any& dt) {
    Argument& pegArg = ACTION_PROLOG(dt, vs);
    if (vs.size() < 2) {
        spdlog::error("There is too few arguments {} to perform action", vs.size());
        pegArg.ExpressionResult = false;
        STOP_PROCESSING(dt, vs);
    }

    String regex_expression;
    ForwardList<String> xpath_subnodes;
    for (auto i = 0; i < 2; ++i) {
        String xpath = AnyCast<String>(vs[1 - i]);
        if (xpath.empty()) {
            spdlog::debug("There is not any xpath to convert to node");
            return {};
        }

        if (i == 0) {
            regex_expression = xpath;
            continue;
        }

        Utils::find_and_replace_all(xpath, "[@item]", "/[@item]");
        auto xpath_tokens = XPath::parse(xpath);
        auto resolved_xpath = XPath::toString(pegArg.CurrentProcessingNode);
        // FIXME: XPath::evaluateXPath() does not do the same what this code
        // auto resolved_xpath = XPath::evaluateXPath(pegArg.CurrentProcessingNode, xpath);
        auto resolved_xpath_tokens = XPath::parse(resolved_xpath);
        String rebuild_resolved_xpath;
        while (!xpath_tokens.empty()) {
            auto token = xpath_tokens.front();
            xpath_tokens.pop_front();
            if ((token == "[@item]")
                && (!resolved_xpath_tokens.empty())) {
                std::smatch matched;
                if (std::regex_search(resolved_xpath_tokens.front(), matched, Regex { regex_expression })) {
                    token = matched[0];
                }
            }

            rebuild_resolved_xpath += "/" + token;
            if (!resolved_xpath_tokens.empty()) {
                resolved_xpath_tokens.pop_front();
            }
        }

        auto node = XPath::select(pegArg.RootNodeConfig, rebuild_resolved_xpath);
        if (!node) {
            spdlog::debug("Not found node at xpath {}", xpath);
            return {};
        }

        auto leaf_node = std::dynamic_pointer_cast<Leaf>(node);
        if (!leaf_node) {
            spdlog::debug("Node {} at xpath '{}' is not a leaf", node->Name(), xpath);
            return {};
        }

        auto value = leaf_node->getValue();
        if (value.is_bool()) {
            return Any { value.get_bool() };
        }
        else if (value.is_number()) {
            return Any { value.get_number() };
        }
        else if (value.is_string()) {
            return Any { value.get_string() };
        }
        else {
            spdlog::error("Unsupported type of node value");
        }
    }

    ACTION_EPILOG(dt, vs);
}

} // namespace Action

bool ConstraintChecker::validate(SharedPtr<Node>& node_to_validate, const String& constraint_definition) {
    auto grammar = R"(
        # Syntax Rules
        PROGRAM                                 <-  IF_STATEMENT / EXPRESSION / LOGICAL_EXPRESSION
        IF_STATEMENT                            <-  'if' '(' INFIX_EXPRESSION(LOGICAL_EXPRESSION, RELATIVE_OPERATOR) ')' THEN_STATEMENT EXPRESSION ('else' EXPRESSION)?
        THEN_STATEMENT                          <-  'then'
        LOGICAL_EXPRESSION                      <-  MUST_FUNC / COUNT_FUNC / EXISTS_FUNC / XPATH_FUNC_FAMILY / STRING / NUMBER / BOOLEAN
        EXPRESSION                              <-  Additive / MUST_FUNC / COUNT_FUNC / EXISTS_FUNC / XPATH_FUNC_FAMILY / PRINT_FUNC / PRIMARY
        MUST_FUNC                               <-  'must' '(' INFIX_EXPRESSION(LOGICAL_EXPRESSION, RELATIVE_OPERATOR) ')'
        COUNT_FUNC                              <-  'count' '(' XPATH_FUNC_FAMILY ')'
        EXISTS_FUNC                             <-  'exists' '(' XPATH_FUNC_FAMILY ')'
        XPATH_FUNC_FAMILY                       <-  (XPATH_FUNC / XPATH_VALUE_FUNC / XPATH_ALL_FUNC / XPATH_ANY_FUNC / XPATH_MATCH_REGEX_FUNC / XPATH_KEY_BASED_FUNC / XPATH_KEY_REGEX_REPLACE_FUNC / XPATH_VALUE_KEY_REGEX_REPLACE_FUNC)
        XPATH_FUNC                              <-  'xpath' '(' (STRING / REFERENCE) ')'
        XPATH_VALUE_FUNC                        <-  'xpath_value' '(' (STRING / REFERENCE) ')'
        XPATH_ANY_FUNC                          <-  'xpath_any' '(' STRING ')'
        XPATH_ALL_FUNC                          <-  'xpath_all' '(' STRING ')'
        XPATH_MATCH_REGEX_FUNC                  <-  'xpath_match_regex' '(' STRING (',' STRING){2} ')'
        XPATH_KEY_BASED_FUNC                    <-  'xpath_key_based' '(' STRING ',' STRING ')'
        XPATH_KEY_REGEX_REPLACE_FUNC            <-  'xpath_key_regex_replace' '(' STRING ',' STRING ')'
        XPATH_VALUE_KEY_REGEX_REPLACE_FUNC      <-  'xpath_value_key_regex_replace' '(' STRING ',' STRING ')'
        PRINT_FUNC                              <-  'print' '(' EXPRESSION ')'
        Additive                                <-  Multitive '+' Additive / Divisive '+' Additive / Subtractive
        Subtractive                             <-  Multitive '-' Subtractive / Divisive '-' Subtractive / Multitive / Divisive
        Multitive                               <-  PRIMARY '*' Multitive / PRIMARY
        Divisive                                <-  PRIMARY '/' Divisive / PRIMARY
        PRIMARY                                 <-  '(' EXPRESSION ')' / STRING / NUMBER / BOOLEAN
        # Token Rules
        REFERENCE                               <-  '@'
        RELATIVE_OPERATOR                       <-  < ('=='|'<>') >
        STRING                                  <-  "'" < ([^'] )* > "'"
        NUMBER                                  <-  < '-'? [0-9]+ >
        BOOLEAN                                 <-  ('true' / 'false')
        %whitespace                             <-  [ \t\r\n]*

        # Declare order of precedence
        INFIX_EXPRESSION(A, O) <-  A (O A)* {
          precedence
            L '==' '<>'
        }
    )";

    // (2) Make a parser
    parser parser;
    parser.set_logger([](size_t line, size_t col, const String& msg, const String &rule) {
        std::cerr << line << ":" << col << ": " << msg << " (" << rule << "\n";
    });

    auto ok = parser.load_grammar(grammar);
    assert(ok);

    // (3) Setup actions
    parser["BOOLEAN"] = SemanticAction::BooleanHandle;
    parser["COUNT_FUNC"] = SemanticAction::CountHandle;
    parser["EXISTS_FUNC"] = SemanticAction::ExistsHandle;
    parser["INFIX_EXPRESSION"] = SemanticAction::InfixExpressionHandle;
    parser["MUST_FUNC"] = SemanticAction::MustHandle;
    parser["NUMBER"] = SemanticAction::NumberHandle;
    parser["REFERENCE"] = SemanticAction::ReferenceHandle;
    parser["RELATIVE_OPERATOR"] = SemanticAction::RelativeOperatorHandle;
    parser["STRING"] = SemanticAction::StringHandle;
    parser["THEN_STATEMENT"] = SemanticAction::ThenStatementHandle;
    parser["XPATH_FUNC"] = SemanticAction::XPathHandle;
    parser["XPATH_KEY_BASED_FUNC"] = SemanticAction::XPathKeyBasedHandle;
    parser["XPATH_KEY_REGEX_REPLACE_FUNC"] = SemanticAction::XPathKeyRegexReplaceHandle;
    parser["XPATH_MATCH_REGEX_FUNC"] = SemanticAction::XPathMatchRegexHandle;
    parser["XPATH_ALL_FUNC"] = SemanticAction::XPathAllHandle;
    parser["XPATH_ANY_FUNC"] = SemanticAction::XPathAnyHandle;
    parser["XPATH_VALUE_FUNC"] = SemanticAction::XPathValueHandle;
    parser["XPATH_VALUE_KEY_REGEX_REPLACE_FUNC"] = SemanticAction::XPathValueKeyRegexReplaceHandle;

    // (4) Parse
    parser.enable_packrat_parsing(); // Enable packrat parsing

    SemanticAction::Argument pegArg = {
        m_config_mngr, m_root_config, node_to_validate
    };

    auto peg_arg_opaque = std::any(pegArg);
    parser.parse(constraint_definition, peg_arg_opaque);
    auto result = AnyCast<SemanticAction::Argument>(peg_arg_opaque).ExpressionResult;
    spdlog::debug("For '{}' evaluated '{}'", constraint_definition, result);
    if (!result) {
        spdlog::error("Failed to pass constraint: {}", constraint_definition);
        return false;
    }

    return result;
}
