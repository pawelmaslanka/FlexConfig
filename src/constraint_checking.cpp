#include "constraint_checking.hpp"

#include "node/leaf.hpp"
#include "node/visitor_spec.hpp"
#include "xpath.hpp"
#include "lib/utils.hpp"

#include <peglib.h>
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
                g(std::any_cast<T const&>(a));
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

ConstraintChecker::ConstraintChecker(SharedPtr<Config::Manager>& config_mngr, SharedPtr<Node>& root_config)
: m_config_mngr { config_mngr }, m_root_config { root_config } {

}

bool ConstraintChecker::validate(SharedPtr<Node>& node_to_validate, const String& constraint_definition) {
    struct PEGArgument {
        SharedPtr<Config::Manager> config_mngr;
        SharedPtr<Node> root_config;
        SharedPtr<Node> current_processing_node;
        Stack<String> string_stack = {};
        Stack<long> number_stack {};
        Stack<bool> bool_stack {};
        Stack<String> operator_stack = {};
        Stack<SharedPtr<Node>> node_stack = {};
        bool continue_processing = { true };
        bool expression_result = { false };
    };

    auto grammar =(R"(
        # Top rule seems to not required to have reference!!!
        # Syntax Rules
        EXPRESSION              <- CONDITION
        CONDITION               <- MULTIPLICATIVE (ConditionOperator MULTIPLICATIVE)?
        MULTIPLICATIVE          <- CALL (MultiplicativeOperator CALL)*
        CALL                    <- PRIMARY (EXPRESSION)?
        PRIMARY                 <- COUNT_FUNC / EXISTS_FUNC / MUST_FUNC / XPATH_FUNC / '(' EXPRESSION ')' / REFERENCE / Keyword / Number / String
        COUNT_FUNC              <- 'count' '(' EXPRESSION ')'
        EXISTS_FUNC             <- 'exists' '(' EXPRESSION ')'
        MUST_FUNC               <- 'must' '(' EXPRESSION ')'
        XPATH_FUNC              <- 'xpath' '(' EXPRESSION ')'
        REFERENCE               <- '@'

        # Token Rules
        ConditionOperator       <- < [=~&|] >
        MultiplicativeOperator  <- '%'
        String                  <- "'" < ([^'] .)* > "'"
        Number                  <- < [0-9]+ >

        Keyword                 <- ('if' / 'then' / 'else') ![a-zA-Z]
        %whitespace             <- [ \t\r\n]*

        # Declare order of precedence
        # INFIX_EXPRESSION(A, O) <-  A (O A)* {
        # precedence
        #     L = ~ & |
        # }
    )");

    parser parser;
    parser.set_logger([](size_t line, size_t col, const String& msg, const String &rule) {
        std::cerr << line << ":" << col << ": " << msg << " (" << rule << "\n";
    });

    auto ok = parser.load_grammar(grammar);
    assert(ok);

    parser["REFERENCE"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        peg_arg.string_stack.push("@");
        return;
    };

    parser["Number"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        peg_arg.number_stack.push(vs.token_to_number<long>());
        // return vs.sv();
    };

    parser["String"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        auto token = vs.token_to_string();
        // if (token.find("'") != String::npos) {
        //     spdlog::debug("String has extra ' character. Skip it");
        //     return;
        // }

        // if (token.size() > 1) {
        //     if ((token[0] == '/')
        //         && (token[1] == '/')) {
        //         // This is hack because peglib has problem with string like '/interface/' or '/interface/aggregate-ethernet[@key]/members'
        //         token = token.substr(1, token.size() - 1);
        //     }
        // }

        // spdlog::debug("Converted to {}", token);

        peg_arg.string_stack.push(token);
        // return vs.sv();
    };

    parser["ConditionOperator"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        peg_arg.operator_stack.push(vs.token_to_string());
        // return vs.sv();
    };

    parser["COUNT_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        if (peg_arg.node_stack.empty()) {
            spdlog::error("There is not any node to count its members");
            peg_arg.continue_processing = false;
            peg_arg.expression_result = false;
            return;
        }

        if (!peg_arg.node_stack.top()) {
            spdlog::error("The node is null so cannot to count its members");
            peg_arg.node_stack.pop();
            peg_arg.continue_processing = false;
            peg_arg.expression_result = true;
            return;
        }

        auto node = peg_arg.node_stack.top();
        peg_arg.node_stack.pop();
        
        SubnodeChildsVisitor subnode_child_visitor(node->getName());
        node->accept(subnode_child_visitor);
        auto subnodes = subnode_child_visitor.getAllSubnodes();
        long subnodes_count = 0;
        for (auto it = subnodes.begin(); it != subnodes.end(); ++it) {
            ++subnodes_count;
        }

        spdlog::debug("Counted {} subnodes of parent {}", subnodes_count, node->getName());
        peg_arg.number_stack.push(subnodes_count);
    };

    parser["EXISTS_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        // There is exception like this rule: must(exists(@))
        // @ means reference
        if (peg_arg.string_stack.top() == "@") {
            peg_arg.string_stack.pop();
            // TODO: Get "subnodes" directly from config by json_pointer
            // auto xpath = XPath::to_string2(peg_arg.current_processing_node);
            // peg_arg.config_mngr->getArray(xpath);
            auto node = peg_arg.current_processing_node;
            SubnodeChildsVisitor subnode_child_visitor(node->getName());
            node->accept(subnode_child_visitor);
            auto subnode_names = subnode_child_visitor.getAllSubnodes();
            auto schema_node = peg_arg.config_mngr->getSchemaByXPath(XPath::to_string2(peg_arg.current_processing_node));
            if (!schema_node) {
                spdlog::error("Not found schema node for xpath: {}", XPath::to_string2(peg_arg.current_processing_node));
                peg_arg.continue_processing = false;
                return;
            }

            auto ref_attrs = schema_node->findAttr("reference");
            if (ref_attrs.empty()) {
                spdlog::error("Not found attribute 'reference' at schema node {}", XPath::to_string2(schema_node));
                peg_arg.continue_processing = false;
                return;
            }

            spdlog::debug("Found attribute reference: {}", ref_attrs.front());
            for (auto& subnode_name : subnode_names) {
                spdlog::debug("Checking if {} exists", subnode_name);
                bool found = false;
                for (auto& ref_attr : ref_attrs) {
                    String ref_xpath = ref_attr;
                    Utils::find_and_replace_all(ref_xpath, "@", subnode_name);
                    spdlog::debug("Created new reference xpath: {}", ref_xpath);
                    if (XPath::select2(peg_arg.root_config, ref_xpath)) {
                        spdlog::debug("Found node {}", ref_xpath);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    spdlog::debug("Not found reference node {}", subnode_name);
                    peg_arg.bool_stack.push(false);
                    return;
                }
            }

            peg_arg.bool_stack.push(true);
            return;
        }

        if (peg_arg.node_stack.empty()) {
            spdlog::error("There is not any node to check if that exists");
            peg_arg.continue_processing = false;
            peg_arg.expression_result = false;
            return;
        }

        if (!peg_arg.node_stack.top()) {
            peg_arg.node_stack.pop();
            spdlog::debug("Node is null");
            peg_arg.bool_stack.push(false);
            return;
        }

        peg_arg.bool_stack.push(peg_arg.node_stack.top() != nullptr);
        peg_arg.node_stack.pop();
    };

    parser["XPATH_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        if (peg_arg.string_stack.empty()) {
            spdlog::error("There is not any xpath to convert to node");
            peg_arg.continue_processing = false;
            peg_arg.expression_result = false;
            return;
        }

        auto xpath = peg_arg.string_stack.top();
        peg_arg.string_stack.pop();
        if (xpath.size() == 0) {
            spdlog::error("xpath is empty");
            peg_arg.node_stack.push(nullptr);
            return;
        }

        if (xpath.size() > 1) {
            if ((xpath[0] == '/')
                && (xpath[1] == '/')) {
                // This is hack because peglib has problem with string like '/interface/' or '/interface/aggregate-ethernet[@key]/members'
                xpath = xpath.substr(1, xpath.size() - 1);
                spdlog::debug("Converted to string {}", xpath);
            }
        }

        spdlog::debug("Resolving xpath: {}", xpath);
        auto xpath_tokens = XPath::parse3(xpath);
        auto resolved_xpath = XPath::evaluate_xpath2(peg_arg.current_processing_node, xpath);
        spdlog::debug("Resolved xpath: {}", resolved_xpath);
        auto node = XPath::select2(peg_arg.root_config, resolved_xpath);
        peg_arg.node_stack.push(node);
        if (!node) {
            spdlog::debug("Not found node: {}", xpath);
            return;
        }

        if (xpath_tokens.back() == "value") {
            auto value = std::dynamic_pointer_cast<Leaf>(node)->getValue();
            spdlog::debug("Got value {} from xpath {}", value.to_string(), xpath);
            if (value.is_bool()) {
                peg_arg.bool_stack.push(value.get_bool());
            }
            else if (value.is_number()) {
                peg_arg.number_stack.push(value.get_number());
            }
            else if (value.is_string()) {
                peg_arg.string_stack.push(value.get_string());
            }
            else {
                spdlog::error("Unrecognized type of node value");
                peg_arg.continue_processing = false;
                peg_arg.expression_result = false;
            }
        }
    };

    parser["Keyword"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        if (vs.token_to_string() == "true") {
            peg_arg.bool_stack.push(true);
        }
        else if (vs.token_to_string() == "false") {
            peg_arg.bool_stack.push(false);
        }
    };

    parser["CONDITION"] = [](const SemanticValues& vs, std::any& dt) { 
        if (vs.size() != 3) {
            spdlog::debug("There are not required 3 arguments");
            return;
        }

        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        if ((peg_arg.bool_stack.size() > 1) && (!peg_arg.operator_stack.empty())) {
            auto arg2 = peg_arg.bool_stack.top();
            peg_arg.bool_stack.pop();
            auto arg1 = peg_arg.bool_stack.top();
            peg_arg.bool_stack.pop();
            auto op = peg_arg.operator_stack.top();
            peg_arg.operator_stack.pop();
            spdlog::debug("Comparing 2 booleans: {} {} {}", arg1, op, arg2);
            if (op == "=") {
                peg_arg.bool_stack.push(arg1 == arg2);
            }
            else if (op == "~") {
                peg_arg.bool_stack.push(arg1 != arg2);
            }
        }
        else if ((peg_arg.number_stack.size() > 1) && (!peg_arg.operator_stack.empty())) {
            auto arg2 = peg_arg.number_stack.top();
            peg_arg.number_stack.pop();
            auto arg1 = peg_arg.number_stack.top();
            peg_arg.number_stack.pop();
            auto op = peg_arg.operator_stack.top();
            peg_arg.operator_stack.pop();
            spdlog::debug("Comparing 2 numbers: {} {} {}", arg1, op, arg2);
            if (op == "=") {
                peg_arg.bool_stack.push(arg1 == arg2);
            }
            else if (op == "~") {
                peg_arg.bool_stack.push(arg1 != arg2);
            }
        }
        else if ((peg_arg.string_stack.size() > 1) && (!peg_arg.operator_stack.empty())) {
            auto arg2 = peg_arg.string_stack.top();
            peg_arg.string_stack.pop();
            auto arg1 = peg_arg.string_stack.top();
            peg_arg.string_stack.pop();
            auto op = peg_arg.operator_stack.top();
            peg_arg.operator_stack.pop();
            spdlog::debug("Comparing 2 strings: {} {} {}", arg1, op, arg2);
            if (op == "=") {
                peg_arg.bool_stack.push(arg1 == arg2);
            }
            else if (op == "~") {
                peg_arg.bool_stack.push(arg1 != arg2);
            }
        }
        else {
            spdlog::error("Cannot recognize type of conditional arguments");
            peg_arg.continue_processing = peg_arg.expression_result = false;
        }
    };

    parser["MUST_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        // Added due to not resolved xpath
        if (!peg_arg.continue_processing) {
            spdlog::debug("Stop to further processing on the rule");
            return;
        }

        if (peg_arg.bool_stack.empty()) {
            spdlog::error("There is not boolean result to pass to 'must' function");
            peg_arg.continue_processing = peg_arg.expression_result = false;
            return;
        }

        auto success = peg_arg.bool_stack.top();
        peg_arg.bool_stack.pop();
        if (!peg_arg.string_stack.empty()) {
            spdlog::error("There are still {} unprocessed strings on the stack", peg_arg.string_stack.size());
            success = false;
        }

        if (!peg_arg.number_stack.empty()) {
            spdlog::error("There are still {} unprocessed numbers on the stack", peg_arg.number_stack.size());
            success = false;
        }

        if (!peg_arg.bool_stack.empty()) {
            spdlog::error("There are still {} unprocessed booleans on the stack", peg_arg.bool_stack.size());
            success = false;
        }

        if (!peg_arg.operator_stack.empty()) {
            spdlog::error("There are still {} unprocessed operators on the stack", peg_arg.operator_stack.size());
            success = false;
        }

        peg_arg.expression_result = success;
        spdlog::debug("'must' function finished with result: {}", success);
    };

    // (4) Parse
    // parser.enable_packrat_parsing(); // Enable packrat parsing.

    PEGArgument peg_arg = {
        m_config_mngr, m_root_config, node_to_validate
    };

    auto peg_arg_opaque = std::any(peg_arg);
    parser.parse(constraint_definition, peg_arg_opaque);
    auto result = std::any_cast<PEGArgument>(peg_arg_opaque).expression_result;
    spdlog::debug("\nFor {} evaluated: {}", constraint_definition, result);
    if (!result) {
        spdlog::error("Failed to pass constraint: {}", constraint_definition);
        return false;
    }

    return result;
}