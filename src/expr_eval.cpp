#include "expr_eval.hpp"
#include "visitor.hpp"

#include "node/node.hpp"
#include "node/composite.hpp"
#include "xpath.hpp"
#include "lib/utils.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <deque>
#include <regex>
#include <sstream>

#include <iostream>

#define MultiMap std::multimap

bool to_bool(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    std::istringstream is(str);
    bool b;
    is >> std::boolalpha >> b;
    return b;
}
	
// stack to store operators.
std::deque<std::string> function;
std::deque<Value> argument; // TODO: std::deque<Value> argument;

std::string regex;
std::string key;
std::string val_str;
std::string val_num;
std::string val_bool;
std::string right_arg;
std::string left_arg;
std::string value;
std::string result;

ExprEval::ExprEval()
  : _initialized { false } {
}

bool ExprEval::init(SharedPtr<Node> root) {
    if (_initialized || !root) {
        return false;
    }

    _visitor = std::make_shared<ExprEval::ExprEvalVisitor>();
    _visitor->init(shared_from_this());
    std::cout << "Init with root node: " << root->getName() << std::endl;
    _processing_node = root;
    _initialized = true;

    return true;
}

void ExprEval::deinit() {
    _processing_node = nullptr;
    _initialized = false;
}

bool ExprEval::ExprEvalVisitor::init(SharedPtr<ExprEval> expr_eval) {
    _expr_eval = expr_eval;
    return true;
}

bool ExprEval::ExprEvalVisitor::visit(SharedPtr<Node> node) {
    std::cout << "Visiting node: " << node->getName() << " to find " << _expr_eval->_visit_node_name << std::endl;
    if (node->getName() == _expr_eval->_visit_node_name) {
        std::cout << "Ending searching node: " << _expr_eval->_visit_node_name << std::endl;
        _expr_eval->_visit_node = node;
        return false;
    }

  return true;
}

Pair<bool, Optional<Value>> ExprEval::xpath_evaluate(std::string xpath) {
    if (xpath.at(0) != XPath::SEPARATOR[0]) {
        return { false, {} };
    }

    Utils::find_and_replace_all(xpath, XPath::KEY_NAME_SUBSCRIPT, String(XPath::SEPARATOR) + XPath::KEY_NAME_SUBSCRIPT);
    MultiMap<String, std::size_t> idx_by_xpath_element;
    Vector<String> xpath_elements;
    // Returns first token
    char* token = ::strtok(const_cast<char*>(xpath.c_str()), XPath::SEPARATOR);
    // Keep printing tokens while one of the
    // delimiters present in str[].
    size_t idx = 0;
    while (token != nullptr) {
        printf("%s\n", token);
        xpath_elements.push_back(token);
        idx_by_xpath_element.emplace(token, idx);
        token = strtok(NULL, XPath::SEPARATOR);
        ++idx;
    }

    std::cout << "Xpath consists of " << xpath_elements.size() << " elements\n";
    std::cout << "Processing node: " << _processing_node->getName() << std::endl;
    // Resolve @key key
    auto key_name_pos = idx_by_xpath_element.find(XPath::KEY_NAME_SUBSCRIPT);
    if (key_name_pos != std::end(idx_by_xpath_element)) {
        // Find last idx
        idx = 0; 
        auto range = idx_by_xpath_element.equal_range(XPath::KEY_NAME_SUBSCRIPT);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second > idx) {
                idx = it->second;
            }
        }

        std::cout << "Found last idx of " << XPath::KEY_NAME_SUBSCRIPT << ": " << idx << " at pos " << idx + 1 << std::endl;
        String wanted_child = xpath_elements.at(idx);
        if (xpath_elements.size() > (idx + 1)) {
            wanted_child = xpath_elements.at(idx + 1);
        }

        std::cout << "Wanted child node: " << wanted_child << std::endl;

        auto curr_node = _processing_node;
        while ((curr_node->getName() != wanted_child)
                && (curr_node->getParent() != nullptr)) {
            curr_node = curr_node->getParent();
        }

        String resolved_key_name;
        if (curr_node->getName() != wanted_child) {
            std::cout << "Last resort\n";
            curr_node = _processing_node;
            do {
                if (auto list = std::dynamic_pointer_cast<Composite>(curr_node->getParent())) {
                    std::cout << "Found list type node " << curr_node->getName() << std::endl;
                    resolved_key_name = curr_node->getName();
                    break;
                }

                curr_node = curr_node->getParent();
            } while (curr_node);

            // std::cerr << "Mismatch current node " << curr_node->getName() << " with wanted " << wanted_child << std::endl;
            // ::exit(EXIT_FAILURE);
        }
        else {
            resolved_key_name = curr_node->getParent()->getName(); // We want to resolve name of parent node based on its child node
        }

        std::cout << "Resolved parent of key name: " << resolved_key_name << std::endl;
        // Resolve all occurences of XPath::KEY_NAME_SUBSCRIPT
        for (auto it = range.first; it != range.second; ++it) {
            std::cout << "Replacing " << xpath_elements.at(it->second) << " with " << resolved_key_name << std::endl;
            xpath_elements.at(it->second) = resolved_key_name;
        }
    }

    // Start from root node
    auto curr_node = _processing_node;
    while (curr_node->getParent() != nullptr) {
        curr_node = curr_node->getParent();
    }

    std::cout << "Found root node: " << curr_node->getName() << std::endl;
    size_t count = 0;
    for (idx = 0; idx < xpath_elements.size() - 1; ++idx) {
        if (count == (xpath_elements.size() - 1)) {
            break;
        }

        _visit_node_name = xpath_elements.at(idx);
        std::cout << "Visit node: " << _visit_node_name << std::endl;
        curr_node->accept(*_visitor);

        if (!_visit_node) {
            return { false, {} };
        }

        curr_node = _visit_node;
        _visit_node = nullptr;
        ++count;
        std::cout << "Current node after visit " << count << ": " << curr_node->getName() << std::endl;
    }

    if (count != (xpath_elements.size() - 1)) {
        return { false, {} };
    }

    if (xpath_elements.at(xpath_elements.size() - 1) == "value") {
        if (auto leaf = std::dynamic_pointer_cast<Leaf>(curr_node)) {
            return { true, leaf->getValue() };
        }
    }
    else if (xpath_elements.at(xpath_elements.size() - 1) == "name") {
        Value val(Value::Type::STRING);
        val.set_string(curr_node->getName());
        return { true, val };
    }

    return { false, {} };
}

bool is_equal(const Value& arg1, const Value& arg2) {
    bool result_bool = false;
    switch (arg1.type()) {
      case Value::Type::BOOL: {
        return arg1.get_bool() == arg2.get_bool();
      }
      case Value::Type::NUMBER: {
        return arg1.get_number() == arg2.get_number();
      }
      case Value::Type::STRING: {
        return arg1.get_string() == arg2.get_string();
      }
      default: {
        std::cerr << "Undefined type of aguments: arg1 (%u) and arg2 (%u)\n";
        ::exit(EXIT_FAILURE);
      }
    }
}

// Dwuargumentowe funkcje musza od razu wychodzić z funkcji
// Jednoargumentowe funkcje moga kontynuowac dzialanie, za wyjatkiem kontrolnych blokow: if, then, else
void ExprEval::get_result_of_eval(int nested_lvl) {
    std::cout << "Nested level: " << nested_lvl << std::endl;
    while (function.size() > 0
            && argument.size() > 0) {
        std::cout << "Analyze function: " << function.front() << std::endl;
        std::cout << "Analyze argument: " << argument.front().to_string() << std::endl;
        if (function.front() == "if") {
            function.pop_front();
            while (function.size() > 0
                    && function.front() != "then") {
                get_result_of_eval(nested_lvl + 1);
            }

            if (argument.back().is_bool()
                && (argument.back().get_bool() == false)) {
                std::cout << "Stopped processing statement" << std::endl;
                result = "true";
                function.clear();
                argument.clear();
                return;
            }

            std::cout << "Continue processing statement" << std::endl;
            // argument.pop_back();
        }
        else if (function.front() == "then") {
            if (argument.back().is_bool()
                && (argument.back().get_bool() == false)) {
                std::cout << "Stopped processing statement because it should not achieve this point" << std::endl;
                result = "false";
                function.clear();
                argument.clear();
                return;
            }

            function.pop_front();
            argument.pop_back();
        }
        else if (function.front() == "regex") {
            function.pop_front();
            regex = argument.front().get_string();
            argument.pop_front();
            Value regex_val(Value::Type::STRING);
            regex_val.set_string(regex);
            argument.push_back(regex_val);
        }
        else if (function.front() == "string") {
            auto string_val = argument.front();
            std::cout << "Match " << string_val.to_string() << " to " << function.front() << std::endl;
            function.pop_front();
            argument.pop_front();
            argument.push_back(string_val);
        }
        else if (function.front() == "number") {
            Value number_val(Value::Type::NUMBER);
            Number converted_number;
            auto string_number = argument.front().get_string();
            auto [ptr, ec] = std::from_chars(string_number.data(), string_number.data() + string_number.size(), converted_number);
            if (ec != std::errc()) {
                std::cerr << "Failed to convert string " << string_number << " into number\n";
            }

            number_val.set_number(converted_number);
            std::cout << "Match " << number_val.to_string() << " to " << function.front() << std::endl;
            function.pop_front();
            argument.pop_front();
            argument.push_back(number_val);
        }

        else if (function.front() == "bool") {
            Value bool_val(Value::Type::BOOL);
            bool_val.set_bool(to_bool(argument.front().to_string()));
            std::cout << "Match " << bool_val.to_string() << " to " << function.front() << std::endl;
            function.pop_front();
            argument.pop_front();
            argument.push_back(bool_val);
        }
        else if (function.front() == "equal") {
            function.pop_front();
            auto arg1 = argument.back();
            argument.pop_back();
            get_result_of_eval(nested_lvl + 1);
            auto arg2 = argument.back();
            argument.pop_back();
            std::cout << "Check if: " << arg1.to_string() << " EQUAL " << arg2.to_string() << std::endl;
            Value result(Value::Type::BOOL);
            result.set_bool(is_equal(arg1, arg2));
            argument.push_back(result);
        }
        else if (function.front() == "not_equal") {
            function.pop_front();
            auto arg1 = argument.back();
            argument.pop_back();
            get_result_of_eval(nested_lvl + 1);
            auto arg2 = argument.back();
            argument.pop_back();
            std::cout << "Check if: " << arg1.to_string() << " NOT_EQUAL " << arg2.to_string() << std::endl;
            Value result(Value::Type::BOOL);
            result.set_bool(!is_equal(arg1, arg2));
            argument.push_back(result);
        }
        else if (function.front() == "or") {
            function.pop_front();
            auto arg1 = argument.back();
            argument.pop_back();
            get_result_of_eval(nested_lvl + 1);
            auto arg2 = argument.back();
            argument.pop_back();
            std::cout << "Check if: " << arg1.to_string() << " OR " << arg2.to_string() << std::endl;
            Value result(Value::Type::BOOL);
            result.set_bool(arg1.get_bool() || arg2.get_bool());
            argument.push_back(result);
        }
        else if (function.front() == "must") {
            function.pop_front();
            get_result_of_eval(nested_lvl + 1);
            get_result_of_eval(nested_lvl + 1);
            auto arg1 = argument.back();
            argument.pop_back();
            // std::string arg2;
            // if (regex.size() > 0) {
            //     arg2 = regex;
            //     regex.clear();
            // }
            // else {
            //     arg2 = argument.back();
            //     argument.pop_back();
            // }
            std::cout << "Must: " << arg1.to_string() << " == true" << std::endl;
            Value result(Value::Type::BOOL);
            result.set_bool(arg1.get_bool() == true);
            argument.push_back(result);
        }
        else if (function.front() == "regex_match") {
            function.pop_front();
            std::cout << "Recheck analyze function: " << function.front() << std::endl;
            get_result_of_eval(nested_lvl + 1);
            get_result_of_eval(nested_lvl + 1);
            auto arg1 = argument.back();
            argument.pop_back();
            auto arg2 = argument.back();
            argument.pop_back();
            // }
            std::cout << "Match regex: " << arg2.to_string() << " to " << arg1.to_string() << std::endl;
            std::regex regexp(arg2.get_string());
            std::smatch match;
            String str = arg1.to_string();
            Value bool_val(Value::Type::BOOL);
            bool_val.set_bool(std::regex_match(str, match, regexp));
            argument.push_back(bool_val);
        }
        else if (function.front() == "count") {
            function.pop_front();
            std::cout << "Count members of xpath: " << argument.front().to_string() << std::endl;
            auto xpath = XPath::evaluate_xpath(_processing_node, argument.front().to_string());
            if (xpath.empty()) {
                std::cerr << "Failed to evaluate xpath " << argument.front().to_string() << std::endl;
                ::exit(EXIT_FAILURE);
            }

            argument.pop_front();
            auto node_cnt = XPath::count_members(XPath::get_root(_processing_node), xpath);
            Value number_val(Value::Type::NUMBER);
            number_val.set_number(node_cnt);
            std::cout << "Counted " << number_val.to_string() << " subnodes" << std::endl;
            argument.push_back(number_val);
        }
        else if (function.front() == "exists") {
            function.pop_front();
            std::cout << "Exists of xpath: " << argument.front().to_string() << std::endl;
            auto xpath = XPath::evaluate_xpath(_processing_node, argument.front().to_string());
            if (xpath.empty()) {
                std::cerr << "Failed to evaluate xpath " << argument.front().to_string() << std::endl;
                ::exit(EXIT_FAILURE);
            }

            argument.pop_front();
            auto node_cnt = XPath::count_members(XPath::get_root(_processing_node), xpath);
            Value bool_val(Value::Type::BOOL);
            bool_val.set_bool(node_cnt != 0);
            std::cout << "Exists: " << bool_val.to_string() << std::endl;
            argument.push_back(bool_val);
        }
        else if (function.front() == "xpath") {
            function.pop_front();
            // TODO: Evaluate xpath
            std::cout << "Evaluate xpath: " << argument.front().to_string() << std::endl;
            if (argument.front().is_string()
                && (argument.front().get_string() == "@value")) {
                std::cout << "Evaluate argument: " << argument.front().to_string() << std::endl;
                // std::string val = argument.front();
                argument.pop_front();
                if (auto leaf = std::dynamic_pointer_cast<Leaf>(_processing_node)) {
                    std::cout << "Evaluated " << _processing_node->getName() << " to " << leaf->getValue().to_string() << std::endl;
                    argument.push_back(leaf->getValue());
                }
                else {
                    std::cerr << "Current node does not represent attribute!\n";
                    ::exit(EXIT_FAILURE);
                }
            }
            else if (argument.front().is_string()
                && (argument.front().get_string() == XPath::KEY_NAME)) {
                std::cout << "Evaluate argument: " << argument.front().to_string() << std::endl;
                // std::string val = argument.front();
                argument.pop_front();
                auto traversing_node = _processing_node;
                // Subnode of "list" is wanted a "key"
                while (traversing_node->getParent()) {
                    if (auto list = std::dynamic_pointer_cast<Composite>(traversing_node->getParent())) {
                        std::cout << "Evaluated @key to " << traversing_node->getName() << std::endl;
                        Value string_val(Value::Type::STRING);
                        string_val.set_string(traversing_node->getName());
                        argument.push_back(string_val);
                        break;
                    }

                    traversing_node = traversing_node->getParent();
                }

                if (!traversing_node->getParent()) {
                    std::cerr << "Current node " << _processing_node->getName() << " does not represent list!\n";
                    ::exit(EXIT_FAILURE);
                }
            }
            else {
                auto [ success, xpath_value ] = xpath_evaluate(argument.front().get_string());
                if (!success) {
                    std::cerr << "Failed to evaluate xpath expression\n";
                    ::exit(EXIT_FAILURE);
                }

                argument.pop_front();
                std::cout << "Xpath evaluated to: " << xpath_value.value().to_string() << std::endl;
                argument.push_back(xpath_value.value());
            }
        }
        else {
            std::cerr << "Unrecognize function: " << function.front() << std::endl;
            ::exit(EXIT_FAILURE);
        }

        if (nested_lvl > 0) {
            std::cout << "Return from nested level: " << nested_lvl << std::endl;
            return;
        }
    }

    std::cout << "Left nested level: " << nested_lvl << std::endl;
}

// Function that returns value of
// expression after evaluation.
size_t ExprEval::evaluate(const std::string tokens, const size_t start_idx, const size_t nested_lvl) {
    if (!_initialized) {
        return 0;
    }

    Value val(Value::Type::STRING);
    std::string token;
	int result_nested_lvl = 0;
    int i;
	for(i = start_idx; i < tokens.length(); i++) {
        std::cout << std::endl << tokens[i] << std::endl;
        token.clear();
		if (tokens[i] == '#') { // function
            ++i;
            while (i < tokens.size()
                    && tokens[i] != '<' // argument is passed
                    && tokens[i] != ' ') { // no argument
                token += tokens[i++];
            }

            if (!token.empty()) {
                std::cout << "Function token: " << token << std::endl;
                function.push_back(token);
            }
            else {
                std::cout << "Failed to parse function name. So far parsed: " << token << std::endl;
                ::exit(EXIT_FAILURE);
            }

            if (nested_lvl > 0) {
                return i;
            }
        }
        
        if (tokens[i] == '<'
            || tokens[i] == ',') { // argument
            token.clear();
            ++i;
            while (tokens[i] != '>'
                    && i < tokens.size()) { // end of argument
                std::cout << tokens[i];
                if (tokens[i] == '#') { // nested function
                    i = evaluate(tokens, i, nested_lvl + 1);
                    std::cout << "Nested level: " << nested_lvl << std::endl;
                }
                else if (function.front() != "regex"
                            && (tokens[i] == ' '
                                || tokens[i] == ',')) {
                    if (tokens[i] == ',') {
                        --i; // Next iteration will parse next argument
                        break;
                    }
                }
                else {
                    token += tokens[i];
                }

                ++i;
            }

            if (token.size() > 0) {
                std::cout << std::endl << "Argument token: " << token << std::endl;
                if (token == "@value") {
                    auto leaf = std::dynamic_pointer_cast<Leaf>(_processing_node);
                    if (!leaf) {
                        std::cerr << "Not found leaf object whre we require to get value object\n";
                        exit(EXIT_FAILURE);
                    }

                    token = leaf->getValue().to_string();
                }

                val.set_string(token);
                argument.push_back(val);
                token.clear();
            }
        }

        continue;
    }

    // Entire expression has been parsed at this
    // point, apply remaining ops to remaining
    // values.
    get_result_of_eval();

    if (function.size() > 0) {
        std::cout << "Left unevaluated function size: " << function.size() << std::endl;
        std::cout << "Left unevaluated function: " << function.front() << std::endl;
    }

    if (argument.size() > 0) {
         std::cout << "Left argument size: " << argument.size() << std::endl;
         std::cout << "Left argument: " << argument.front().to_string() << std::endl;
    }

    bool result = argument.size() > 0 && argument.back().is_bool() && (argument.back().get_bool() == true);
    while (argument.size() > 0) {
        std::cout << "Left unevaluated argument: " << argument.front().to_string() << std::endl;
        argument.pop_front();
    }

    while (function.size() > 0) {
        std::cout << "Left unevaluated function: " << function.front() << std::endl;
        function.pop_front();
    }

    return result;
}

#if 0
int main(int argc, char* argv[]) {
    // if (evaluate("#must</platform/port[#key<#regex<eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])\\/[1-4]>>]/breakout-mode/value> #not_equal #string<none>> #or #must<#xpath</platform/port[#key<#regex<eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])>>]/breakout-mode/value> #equal #string<none>>", 0, 0)) {
    //     std::cout << "Success\n\n" <<std::endl;
    // }
    // else {
    //     std::cout << "Fail\n\n" << std::endl;
    // }
    // if (evaluate("#if<#xpath</platform/port[#key<#regex<eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])>>]/breakout-mode/value> #not_equal #string<none>> #then<#must<#xpath</platform/port[#key<#regex<eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])\\/[1-4]>>]/breakout-mode/value> #equal #xpath</platform/port[#key<@>]/breakout-mode/value>>>", 0, 0)) {
    //     std::cout << "Success" << std::endl;
    // }
    // else {
    //     std::cout << "Fail" <<std::endl;
    // }
    // if (evaluate("#if<#regex_match<#regex<eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])\\/[1-4]>, #xpath</platform/port[#key<@>]/name>>> #then<#allow<#regex<10000|25000|100000>>>", 0, 0)) {
    //     std::cout << "Success" << std::endl;
    // }
    // else {
    //     std::cout << "Fail" <<std::endl;
    // }

    // Kazda napotkana funkcja-token, powinna spowodowac rekurencyjne wywolanie, i powracac po zdjeciu kolejnej funkcji ze stosu.
    // Bedziemy parsowac do napotkania '>' badz ' '
    // TODO: jak obsluzyc funkcje #regex_match<>, poniewaz posiada ',' oraz ' '. Sprawdzac, czy na topie stacka jest #regex_match?
    // "#if #xpath</platform/port[@key]/num_breakout/value> #not_equal #number<0> #then #must<#exist</interface/ethernet[@key]> #equal #bool<false>> #else #must<#exist</interface/ethernet[@key]> #equal #bool<true>>",
    // // If breakout != none then main ethernet interface should not exist, so it is in platform/port constraints
    // "#if #xpath</platform/port[@key]/num_breakout/value> #equal #number<0> #then #must<#exist</interface/ethernet[@key]> #equal #bool<true>>",
    // "#if #xpath</platform/port[@key]/num_breakout/value> #not_equal #number<0> #then #must<#exist</interface/ethernet[@key]> #equal #bool<false>>",
    // "#if #exists</platform/port[@key]> #equal #bool<true> #then #must<#regex_match<@value, 40000|100000|400000>>",
    // "#if #exists</platform/port[@key]> #equal #bool<false> #then #must</interface/ethernet[@key]/speed/value> #equal /platform/port[@key]/breakout_speed/value>",

    // "#if #exists</platform/port[@key]> #equal #bool<false> #then #must<@value, 10000|25000|100000>>",

    // "#if #regex_match<@key, eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])> #equal #bool<true> #then #must</platform/port[@key]/num_breakout/value #equal #number<0>>",
    // "#if #regex_match</platform/port[@key]/name, eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])> #equal #bool<true> #then #must</platform/port[@key]/num_breakout/value #equal #number<0>>",
    // if (evaluate("#if<#regex_match<#regex<eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])\\/[1-4]>, #xpath</platform/port[#key<@>]/name>>> #then<#allow<#regex<10000|25000|100000>>>", 0, 0)) {
    //     std::cout << "Success" << std::endl;
    // }
    // else {
    //     std::cout << "Fail" <<std::endl;
    // }

    // if (evaluate("#if #must<#xpath</platform/port[@key]/num_breakout/value> #equal #number<0>> #then #regex_match<#xpath</platform/port[@key]/name>, #regex<eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])>> #equal #bool<true>", 0, 0)) {
    //     std::cout << "Success" << std::endl;
    // }
    // else {
    //     std::cout << "Fail" <<std::endl;
    // }

    // if (evaluate("#if #regex_match<#xpath</platform/port[@key]/name>, #regex<eth-(6[5-6])|eth-(6[0-6]|5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[1-9])>> #equal #bool<true> #then #must<#xpath</platform/port[@key]/num_breakout/value> #equal #number<0>>", 0, 0)) {
    //     std::cout << "Success" << std::endl;
    // }
    // else {
    //     std::cout << "Fail" <<std::endl;
    // }

    if (evaluate("#if #xpath</platform/port[@key]/num_breakout/value> #equal #number<0> #then #must<#regex_match<@value, #regex<40000|100000|400000>>>")) {
        std::cout << "Success" << std::endl;
    }
    else {
        std::cout << "Fail" <<std::endl;
    }

    return 0;
}
#endif
