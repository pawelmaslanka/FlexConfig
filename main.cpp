#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <regex>
#include <cctype>
#include <memory>

#include <spdlog/spdlog.h>

#include "node/node.hpp"
#include "node/leaf.hpp"
#include "node/composite.hpp"
#include "lib/utils.hpp"
#include "config.hpp"
#include "expr_eval.hpp"
#include "lib/topo_sort.hpp"
#include "xpath.hpp"

#include <fstream>
#include <variant>
#include "peglib.h"
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

class VisitorImpl : public Visitor {
  public:
    virtual ~VisitorImpl() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
        // node->accept(*this);
        return true;
    }
};

class VisitorGE1Speed : public Visitor {
  public:
    virtual ~VisitorGE1Speed() = default;
    virtual bool visit(SharedPtr<Node> node) override {
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Visit node: " << node->getName() << std::endl;
        std::clog << "[" << __FILE__ << ":" << __func__ << ":" << __LINE__ << "] " << "Node " << node->getName() << " with schema " << (node->getSchemaNode() ? node->getSchemaNode()->getName() : "none") << std::endl;
        if (node->getName() == "speed") {
            m_speed_node = node;
            return false;
        }

        return true;
    }

    SharedPtr<Node> getSpeedNode() { return m_speed_node; }

private:
    SharedPtr<Node> m_speed_node;
};

int main(int argc, char* argv[]) {
    auto registry = std::make_shared<RegistryClass>();
    auto config_mngr = std::make_shared<Config::Manager>("../config_test.json", "../schema_test.json", registry);
    std::shared_ptr<Node> root_config;
    if (!config_mngr->load(root_config)) {
        spdlog::error("Failed to load config file");
        ::exit(EXIT_FAILURE);
    }

    VisitorImpl visitor;
    root_config->accept(visitor);

    VisitorGE1Speed visit_ge1_speed;
    root_config->accept(visit_ge1_speed);
    if (!visit_ge1_speed.getSpeedNode()) {
        std::cerr << "Not found speed node!\n";
        exit(EXIT_FAILURE);
    }
    else {
        spdlog::info("Found interface speed");
    }

    auto ee = std::make_shared<ExprEval>();
    ee->init(visit_ge1_speed.getSpeedNode());
    if (ee->evaluate("#if #xpath</interface/gigabit-ethernet[@key]/speed/value> #equal #string<400G> #then #must<#regex_match<@value, #regex<^(400G)$>>>")) {
        std::cout << "Successfully evaluated expression\n";
    }
    else {
        std::cerr << "Failed to evaluate expression\n";
    }

    // spdlog::info("Diff to empty JSON: {}",
    // config_mngr->getSchemaByXPath(XPath::to_string2(visit_ge1_speed.getSpeedNode())));
    // return 0;

    // TODO: Write next another version of DependencyGetterVisitor class to load all nodes
    class DependencyGetterVisitior2 : public Visitor {
    public:
        virtual ~DependencyGetterVisitior2() = default;
        DependencyGetterVisitior2(SharedPtr<Config::Manager>& config_mngr)
            : m_config_mngr { config_mngr }, m_dependencies { std::make_shared<Map<String, Set<String>>>() } {}
        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }
            else {
                // spdlog::info("Visiting node: {}", node->getName());
            }

            // TODO: XPath::to_string2(node->getSchemaNode())
            auto xpath = XPath::to_string2(node);
            spdlog::info("Get xpath: {}", xpath);
            auto schema_node = m_config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::info("Does not have schema");
                return true;
            }

            auto update_depend_attrs = schema_node->findAttr("update-dependencies");
            if (update_depend_attrs.empty()) {
                spdlog::info("{} does not have dependencies", xpath);
                auto xpath_schema_node = XPath::to_string2(schema_node);
                if (m_dependencies->find(xpath_schema_node) == m_dependencies->end()) {
                    // Just create empty set
                    (*m_dependencies)[xpath_schema_node].clear();
                }

                return true;
            }

            for (auto& dep : update_depend_attrs) {
                // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
                // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
                auto xpath_schema_node = XPath::to_string2(schema_node);
                (*m_dependencies)[xpath_schema_node].emplace(dep);
                std::clog << "Node " << node->getName() << " with schema " << xpath_schema_node << " depends on " << dep << std::endl;
            }

            // if (std::dynamic_pointer_cast<Leaf>(node)) {
            //     return false; // Terminate visiting on Leaf object
            // }

            return true;
        }

        SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

    private:
        SharedPtr<Map<String, Set<String>>> m_dependencies;
        SharedPtr<Config::Manager> m_config_mngr;
    };

    DependencyGetterVisitior2 depVisitor(config_mngr);
    spdlog::info("Looking for dependencies");
    root_config->accept(depVisitor);
    spdlog::info("Completed looking for dependencies");
    class SubnodeChildsVisitor : public Visitor {
        public:
            virtual ~SubnodeChildsVisitor() = default;
            SubnodeChildsVisitor(const String& parent_name)
                : m_parent_name { parent_name } {}

            virtual bool visit(SharedPtr<Node> node) override {
                if (node &&
                    (node->getParent()->getName() == m_parent_name)) {
                    spdlog::info("{} is child of {}", node->getName(), m_parent_name);
                    m_child_subnode_names.emplace_front(node->getName());
                }

                return true;
            }

            ForwardList<String> getAllSubnodes() { return m_child_subnode_names; }

        private:
            String m_parent_name;
            ForwardList<String> m_child_subnode_names;
        };

    class KeySelectorResolverVisitior : public Visitor {
    public:
        virtual ~KeySelectorResolverVisitior() = default;
        KeySelectorResolverVisitior(const String& xpath_to_resolve) {
            m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("[@key]"));
            if ((xpath_to_resolve.find("[@key]") + 6) < xpath_to_resolve.size()) {
                m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("[@key]") + 7);
            }

            spdlog::info("Fillout all subnodes for xpath {}", m_pre_wildcard);
            spdlog::info("Left xpath {}", m_post_wildcard);
            m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
        }

        ForwardList<String> getResolvedXpath() { return m_result; }

        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }
            else {
                // spdlog::info("Visiting node: {}", node->getName());
            }

            if (m_pre_wildcard_tokens.empty()
                || (node->getName() != m_pre_wildcard_tokens.front())) {
                spdlog::info("Failed to resolve wildcard");
                return true;
            }

            m_pre_wildcard_tokens.pop();
            if (m_pre_wildcard_tokens.empty()) {
                spdlog::info("We reached leaf token at node {}", node->getName());
                SubnodeChildsVisitor subnode_visitor(node->getName());
                node->accept(subnode_visitor);
                for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                    String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                    if (!m_post_wildcard.empty()) {
                        resolved_wildcard_xpath += "/" + m_post_wildcard;
                    }

                    spdlog::info("Resolved wildcard {}", resolved_wildcard_xpath);
                    m_result.emplace_front(resolved_wildcard_xpath);
                }
                // Let's break processing
                return false;
            }

            return true;
        }

    private:
        Queue<String> m_pre_wildcard_tokens {};
        String m_pre_wildcard {};
        String m_post_wildcard {};
        ForwardList<String> m_result {};
    };

    class WildcardDependencyResolverVisitior : public Visitor {
    public:
        virtual ~WildcardDependencyResolverVisitior() {
            spdlog::info("{}:{}", __func__, __LINE__);
            while (!m_pre_wildcard_tokens.empty()) m_pre_wildcard_tokens.pop();
            spdlog::info("{}:{}", __func__, __LINE__);
            m_result.clear();
            spdlog::info("Call WildCard Dependency destroying for node {}", m_pre_wildcard + m_post_wildcard);
        };

        WildcardDependencyResolverVisitior(const String& xpath_to_resolve) {
            m_pre_wildcard = xpath_to_resolve.substr(0, xpath_to_resolve.find("/*"));
            if ((xpath_to_resolve.find("/*") + 2) < xpath_to_resolve.size()) {
                m_post_wildcard = xpath_to_resolve.substr(xpath_to_resolve.find("/*") + 3);
            }

            spdlog::info("Fillout all subnodes for xpath {}", m_pre_wildcard);
            spdlog::info("Left xpath {}", m_post_wildcard);
            m_pre_wildcard_tokens = XPath::parse3(m_pre_wildcard);
        }

        ForwardList<String> getResolvedXpath() { return m_result; }

        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }
            else {
                // spdlog::info("Visiting node: {}", node->getName());
            }

            if (m_pre_wildcard_tokens.empty()
                || (node->getName() != m_pre_wildcard_tokens.front())) {
                spdlog::info("Failed to resolve wildcard");
                return true;
            }

            m_pre_wildcard_tokens.pop();
            if (m_pre_wildcard_tokens.empty()) {
                spdlog::info("We reached leaf token at node {}", node->getName());
                SubnodeChildsVisitor subnode_visitor(node->getName());
                node->accept(subnode_visitor);
                for (const auto& subnode : subnode_visitor.getAllSubnodes()) {
                    String resolved_wildcard_xpath = m_pre_wildcard + "/" + subnode;
                    if (!m_post_wildcard.empty()) {
                        resolved_wildcard_xpath += "/" + m_post_wildcard;
                    }

                    spdlog::info("Resolved wildcard {}", resolved_wildcard_xpath);
                    m_result.emplace_front(resolved_wildcard_xpath);
                }
                // Let's break processing
                return false;
            }

            return true;
        }

    private:
        Queue<String> m_pre_wildcard_tokens {};
        String m_pre_wildcard {};
        String m_post_wildcard {};
        ForwardList<String> m_result {};
    };

    auto dependencies = std::make_shared<Map<String, Set<String>>>();
    auto without_dependencies = std::make_shared<Map<String, Set<String>>>();
    auto not_resolved_dependencies = depVisitor.getDependencies();
    for (auto schema_node : *not_resolved_dependencies) {
        spdlog::info("Processing node: {}", schema_node.first);
        if (schema_node.second.empty()) {
            // Leave node to just load all nodes
            (*without_dependencies)[schema_node.first].clear();
        }

        for (auto dep : schema_node.second) {
            if (dep.find("/*") != String::npos) {
                spdlog::info("Found wildcard in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = WildcardDependencyResolverVisitior(dep);
                root_config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto resolved_wildcard : resolved_xpath) {
                        spdlog::info("Put resolved wildcard {}", resolved_wildcard);
                        (*dependencies)[schema_node.first].insert(resolved_wildcard);
                    }

                    continue;
                }
            }
            else if (dep.find("[@key]") != String::npos) {
                spdlog::info("Found [@key] in dependency {} for node {}", dep, schema_node.first);
                auto wildcard_resolver = KeySelectorResolverVisitior(dep);
                root_config->accept(wildcard_resolver);
                auto resolved_xpath = wildcard_resolver.getResolvedXpath();
                if (!resolved_xpath.empty()) {
                    for (auto& resolved_wildcard : resolved_xpath) {
                        spdlog::info("Put resolved [@key] {}", resolved_wildcard);
                        (*dependencies)[schema_node.first].insert(resolved_wildcard);
                    }

                    continue;
                }
            }

            (*dependencies)[schema_node.first].insert(dep);
        }
    }

    // TODO: If it is starting load config, then merge both list: without_dependencies and dependencies
    if (1) {
        dependencies->insert(without_dependencies->begin(), without_dependencies->end());
    }

    // exit(EXIT_SUCCESS);
    std::list<String> ordered_cmds;
    auto sort_result = run_update_op(dependencies, ordered_cmds);
    Set<String> set_of_ordered_cmds;
    for (auto& cmd : ordered_cmds) {
        set_of_ordered_cmds.emplace(cmd);
    }

    class DependencyMapperVisitior : public Visitor {
    public:
        virtual ~DependencyMapperVisitior() = default;
        DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : m_set_of_ordered_cmds { set_of_ordered_cmds } {}
        virtual bool visit(SharedPtr<Node> node) override {
            auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
            if (!schema_node) {
                std::clog << "Node " << node->getName() << " has not schema\n";
                return true;
            }

            auto schema_node_xpath = XPath::to_string2(schema_node);
            if (m_set_of_ordered_cmds.find(schema_node_xpath) != m_set_of_ordered_cmds.end()) {
                m_ordered_nodes[schema_node_xpath].emplace_front(node);
            }
            else if (m_ordered_nodes.find(XPath::to_string2(schema_node->getParent())) != m_ordered_nodes.end()) {
                m_ordered_nodes[XPath::to_string2(schema_node->getParent())].emplace_back(node);
            }
            else {
                m_unordered_nodes[schema_node_xpath].emplace_back(node);
            }

            // if (std::dynamic_pointer_cast<Leaf>(node)) {
            //     return false; // Terminate visiting on Leaf object
            // }

            return true;
        }

        Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
        Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

    private:
        Set<String> m_set_of_ordered_cmds {};
        Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
        Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
    };

    DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
    root_config->accept(depMapperVisitor);
    std::list<SharedPtr<Node>> nodes_to_execute {};
    std::clog << "Sorting unordered nodes\n";
    for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
        std::clog << "Schema: " << schema_name << std::endl;
        std::clog << "Nodes: ";
        for (auto& n : unordered_nodes) {
            // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
            nodes_to_execute.emplace_back(n);
            std::clog << n->getName() << " ";
        }
        std::clog << std::endl;
    }

    std::clog << "Sorting ordered nodes\n";
    for (auto& ordered_cmd : ordered_cmds) {
        std::clog << "Schema: " << ordered_cmd << std::endl;
        std::clog << "Nodes: ";
        auto ordered_nodes = depMapperVisitor.getOrderedNodes();
        for (auto& n : ordered_nodes[ordered_cmd]) {
            nodes_to_execute.emplace_back(n);
            std::clog << n->getName() << " ";
        }

        std::clog << std::endl;
    }

    class UpdateConstraintVisitor : public Visitor {
    public:
        ~UpdateConstraintVisitor() = default;
        UpdateConstraintVisitor(StringView constraint)
            : m_constraint { constraint } {}

        virtual bool visit(SharedPtr<Node> node) override {
            if (!node) {
                spdlog::error("Node is null");
                return false;
            }

            return true;
        }

    private:
        String m_constraint;
    };

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
        # Syntax Rules
        # Top rule seems to not required to have reference!!!
        # STATEMENT              <-  IF_STATEMENT / IF_ELSE_STATEMENT / MUST_STATEMENT
        # EXPRESSION             <-  INFIX_EXPRESSION(ATOM, OPERATOR)
        # ATOM                   <-  NUMBER / STRING / '(' EXPRESSION ')' / EXISTS_FUNC / XPATH_FUNC
        # OPERATOR               <-  < [=~&|] >
        # NUMBER                 <-  < '-'? [0-9]+ >
        # STRING                 <-  "'" < ([^'] .)* > "'"
        # THEN                   <-  'then'
        # THEN_EXPRESSION        <-  THEN ATOM
        # ELSE_EXPRESSION        <-  'else' ATOM
        # IF_EXPRESSION          <-  'if' EXPRESSION
        # IF_STATEMENT           <-  IF_EXPRESSION THEN_EXPRESSION
        # IF_ELSE_STATEMENT      <-  IF_EXPRESSION THEN_EXPRESSION ELSE_EXPRESSION
        # MUST_STATEMENT         <-  'must' EXPRESSION
        # EXISTS_FUNC            <-  'exists' '(' STRING ')'
        # XPATH_FUNC             <-  'xpath' '(' STRING ')'
        # %whitespace            <-  [ \t\r\n]*

        # Declare order of precedence
        # INFIX_EXPRESSION(A, O) <-  A (O A)* {
        # precedence
        #     L = ~ & |
        # }

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
        INFIX_EXPRESSION(A, O) <-  A (O A)* {
        precedence
            L = ~ & |
        }
    )");

    parser parser;
    parser.set_logger([](size_t line, size_t col, const String& msg, const String &rule) {
        std::cerr << line << ":" << col << ": " << msg << " (" << rule << "\n";
    });

    auto ok = parser.load_grammar(grammar);
    assert(ok);

    parser["REFERENCE"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
            return;
        }

        peg_arg.string_stack.push("@");
        return;
    };

    // parser["NUMBER"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     peg_arg.args.push(vs.token_to_string());
    //     // return vs.sv();
    // };

    parser["Number"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
            return;
        }

        peg_arg.number_stack.push(vs.token_to_number<long>());
        // return vs.sv();
    };

    // parser["STRING"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     peg_arg.args.push(vs.token_to_string());
    //     // return vs.sv();
    // };

    parser["String"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
            return;
        }

        peg_arg.string_stack.push(vs.token_to_string());
        // return vs.sv();
    };

    // parser["OPERATOR"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     peg_arg.args.push(vs.token_to_string());
    //     // return vs.sv();
    // };

    parser["ConditionOperator"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.choice_count() << ")" << std::endl;
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
            return;
        }

        peg_arg.operator_stack.push(vs.token_to_string());
        // return vs.sv();
    };

    parser["COUNT_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        process(dt);
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
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
            peg_arg.expression_result = false;
            return;
        }

        std::clog << "'count' function's argument: " << peg_arg.node_stack.top()->getName() << std::endl;
        auto node = peg_arg.node_stack.top();
        peg_arg.node_stack.pop();
        
        SubnodeChildsVisitor subnode_child_visitor(node->getName());
        node->accept(subnode_child_visitor);
        auto subnodes = subnode_child_visitor.getAllSubnodes();
        long subnodes_count = 0;
        for (auto it = subnodes.begin(); it != subnodes.end(); ++it) {
            ++subnodes_count;
        }

        spdlog::info("Counted {} subnodes of parent {}", subnodes_count, node->getName());
        peg_arg.number_stack.push(subnodes_count);
    };

    parser["EXISTS_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        process(dt);
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
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

            spdlog::info("Found attribute reference: {}", ref_attrs.front());
            for (auto& subnode_name : subnode_names) {
                spdlog::info("Checking if {} exists", subnode_name);
                bool found = false;
                for (auto& ref_attr : ref_attrs) {
                    String ref_xpath = ref_attr;
                    Utils::find_and_replace_all(ref_xpath, "@", subnode_name);
                    spdlog::info("Created new reference xpath: {}", ref_xpath);
                    if (XPath::select2(peg_arg.root_config, ref_xpath)) {
                        spdlog::info("Found node {}", ref_xpath);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    spdlog::info("Not found reference node {}", subnode_name);
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
            spdlog::info("Node is null");
            peg_arg.bool_stack.push(false);
            return;
        }

        std::clog << "Exists() argument: " << peg_arg.node_stack.top()->getName() << std::endl;
        peg_arg.bool_stack.push(peg_arg.node_stack.top() != nullptr);
        peg_arg.node_stack.pop();
        // ForwardList<String> resolved_xpath = {};
        // resolved_xpath.emplace_front(peg_arg.args.top());
        // peg_arg.args.pop();
        // if (resolved_xpath.front().find("[@key]") != String::npos) {
        //     auto key_selector_resolver = KeySelectorResolverVisitior(resolved_xpath.front());
        //     peg_arg.root_config->accept(key_selector_resolver);
        //     resolved_xpath = key_selector_resolver.getResolvedXpath();
        // }
        // else if (resolved_xpath.front().find("/*") != String::npos) {
        //     auto wildcard_resolver = WildcardDependencyResolverVisitior(resolved_xpath.front());
        //     peg_arg.root_config->accept(wildcard_resolver);
        //     resolved_xpath = wildcard_resolver.getResolvedXpath();
        // }

        // for (const auto& xpath : resolved_xpath) {
        //     std::clog << "Checking xpath if exists: " << xpath << std::endl;
        //     if (!XPath::select(peg_arg.root_config, xpath)) {
        //         std::clog << "Not found seeking node\n";
        //         peg_arg.if_cond_result = false; // Start from here to set reurned value from expression
        //     }
        //     else {
        //         std::clog << "Successfully found seeking node\n";
        //         peg_arg.if_cond_result = true; // Start from here to set reurned value from expression
        //     }
        // }

        // return dt;
    };

    // parser["IF_STATEMENT"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     std::clog << "[" << __func__ << ":" << __LINE__ << "] " << std::endl;
    //     // dt = vs[0];
    //     return vs[0];
    // };

    // parser["IF_EXPRESSION"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     // auto result = any_cast<long>(vs[0]);
    //     std::clog << "[" << __func__ << ":" << __LINE__ << "] " << std::endl;
    //     if (vs.size() > 1) {
    //         process(vs[1]);
    //     }

    //     // process(dt);
    //     // PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     // peg_arg.if_cond_result = std::any_cast<bool>(vs[0]);
    //     // dt = vs[0];
    //     // return vs[0];
    // };

    // STATEMENT jako top rule, wzraca wartosc z metody parse()
    // parser["STATEMENT"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     std::clog << "All statement has completed with result: " << peg_arg.expression_result << std::endl;
    //     return dt;
    // };

    // parser["THEN_EXPRESSION"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     if (!peg_arg.if_cond_result) {
    //         std::clog << "Stop to further processing because if conditional was unsuccessfull\n";
    //         return;
    //     }
    // };

    parser["XPATH_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
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

        spdlog::info("Resolving xpath: {}", xpath);
        auto xpath_tokens = XPath::parse3(xpath);
        auto resolved_xpath = XPath::evaluate_xpath2(peg_arg.current_processing_node, xpath);
        spdlog::info("Resolved xpath: {}", resolved_xpath);
        auto node = XPath::select2(peg_arg.root_config, resolved_xpath);
        peg_arg.node_stack.push(node);
        if (!node) {
            spdlog::info("Not found node: {}", xpath);
            return;
        }

        if (xpath_tokens.back() == "value") {
            auto value = std::dynamic_pointer_cast<Leaf>(node)->getValue();
            spdlog::info("Got value {} from xpath {}", value.to_string(), xpath);
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

    // parser["XPATH_FUNC"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     if (peg_arg.args.empty()) {
    //         std::clog << "Stop to further processing because there is not argument to xpath function\n";
    //         peg_arg.expression_result = false;
    //         return;
    //     }

    //     auto xpath = peg_arg.args.top();
    //     peg_arg.args.pop();
    //     spdlog::info("Resolving xpath: {}", xpath);
    //     auto xpath_tokens = XPath::parse3(xpath);
    //     if (xpath_tokens.back() == "value") {
    //         auto resolved_xpath = XPath::evaluate_xpath2(peg_arg.current_processing_node, xpath);
    //         spdlog::info("Resolved xpath: {}", resolved_xpath);
    //         auto node = XPath::select2(peg_arg.root_config, resolved_xpath);
    //         if (!node) {
    //             spdlog::info("Not found node: {}", xpath);
    //             peg_arg.args.push("");
    //             return;
    //         }

    //         auto value = std::dynamic_pointer_cast<Leaf>(node)->getValue();
    //         spdlog::info("Got value {} from xpath {}", value.to_string(), xpath);
    //         peg_arg.args.push(value.to_string());
    //     }
    // };

    // parser["THEN"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     std::clog << "Last 'if' statement result: \n";
    //     process(dt);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     std::clog << peg_arg.if_cond_result << std::endl;
    // };

    parser["Keyword"] = [](const SemanticValues& vs, std::any& dt) { 
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        process(dt);
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
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
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        process(dt);
        if (vs.size() != 3) {
            spdlog::info("There are not required 3 arguments");
            return;
        }

        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
        if (!peg_arg.continue_processing) {
            spdlog::info("Stop to further processing on the rule");
            return;
        }

        if ((peg_arg.bool_stack.size() > 1) && (!peg_arg.operator_stack.empty())) {
            auto arg2 = peg_arg.bool_stack.top();
            peg_arg.bool_stack.pop();
            auto arg1 = peg_arg.bool_stack.top();
            peg_arg.bool_stack.pop();
            auto op = peg_arg.operator_stack.top();
            peg_arg.operator_stack.pop();
            spdlog::info("Comparing 2 booleans: {} {} {}", arg1, op, arg2);
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
            spdlog::info("Comparing 2 numbers: {} {} {}", arg1, op, arg2);
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
            spdlog::info("Comparing 2 strings: {} {} {}", arg1, op, arg2);
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
        std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
        process(vs[0]);
        process(dt);
        PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
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
        spdlog::info("'must' function finished with result: {}", success);
    };

    // parser["EXPRESSION"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     process(dt);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     if (peg_arg.bool_stack.empty()) {
    //         spdlog::error("There is not boolean result to pass to 'expression' rule");
    //         peg_arg.continue_processing = peg_arg.expression_result = false;
    //         return;
    //     }

    //     auto success = peg_arg.bool_stack.top();
    //     peg_arg.bool_stack.pop();
        

    //     if (!peg_arg.string_stack.empty()) {
    //         spdlog::error("There are still {} unprocessed strings on the stack", peg_arg.string_stack.size());
    //         success = false;
    //     }

    //     if (!peg_arg.number_stack.empty()) {
    //         spdlog::error("There are still {} unprocessed numbers on the stack", peg_arg.number_stack.size());
    //         success = false;
    //     }

    //     if (!peg_arg.bool_stack.empty()) {
    //         spdlog::error("There are still {} unprocessed booleans on the stack", peg_arg.bool_stack.size());
    //         success = false;
    //     }

    //     if (!peg_arg.operator_stack.empty()) {
    //         spdlog::error("There are still {} unprocessed operators on the stack", peg_arg.operator_stack.size());
    //         success = false;
    //     }

    //     peg_arg.expression_result = success;
    //     spdlog::info("'expression' rule finished with result: {}", success);
    // };

    // parser["MUST_STATEMENT"] = [](const SemanticValues& vs, std::any& dt) { 
    //     std::clog << vs.name() << ": " << vs.token_to_string() << " (" << vs.size() << ")" << std::endl;
    //     process(vs[0]);
    //     process(dt);
    //     PEGArgument& peg_arg = std::any_cast<PEGArgument&>(dt);
    //     if (peg_arg.args.size() == 3) {
    //         auto arg2 = peg_arg.args.top();
    //         peg_arg.args.pop();
    //         auto op = peg_arg.args.top();
    //         peg_arg.args.pop();
    //         auto arg1 = peg_arg.args.top();
    //         peg_arg.args.pop();
    //         spdlog::info("Processing 3 tokens expression: {} {} {}", arg1, op, arg2);

    //         switch (op[0]) {
    //             case '=': {
    //                 peg_arg.expression_result = (arg1 == arg2);
    //                 break;
    //             }
    //             case '!': {
    //                 peg_arg.expression_result = (arg1 != arg2);
    //                 break;
    //             }
    //         }
    //     }

    //     std::clog << "Expression result: \n";
    //     std::clog << peg_arg.expression_result << std::endl;
    // };

    // (4) Parse
    // parser.enable_packrat_parsing(); // Enable packrat parsing.

    // parser.parse("if (1) then puts 5 % 5 == 0 ? 'FizzBuzz' : 'Fizz'", val);
    // std::string val;
    // bool val;
    // auto a = std::any(peg_arg);
    try {
        // parser.parse("if (1 ~ 3) then 'true'", val);
        // parser.parse("if (1 = 3) then 9", val);
        std::cout << "Start processing\n";
        // Get all update-contraints and go through for-loop
        for (auto& xpath : ordered_cmds) {
            auto schema_node = config_mngr->getSchemaByXPath(xpath);
            if (!schema_node) {
                spdlog::info("There isn't schema at node {}", xpath);
                continue;
            }

            auto update_constraints = schema_node->findAttr(Config::PropertyName::UPDATE_CONSTRAINTS);
            for (auto& update_constraint : update_constraints) {
                spdlog::info("Processing update constraint '{}' at node {}", update_constraint, xpath);
                auto node = XPath::select2(root_config, xpath);
                if (!node) {
                    spdlog::info("Not node indicated by xpath {}", xpath);
                    continue;
                }

                PEGArgument peg_arg = {
                    config_mngr, root_config, node
                };

                auto peg_arg_opaque = std::any(peg_arg);
                parser.parse(update_constraint, peg_arg_opaque);
                process(peg_arg_opaque);
                spdlog::info("\nFor {} at {} evaluated: {}", update_constraint, xpath, std::any_cast<PEGArgument>(peg_arg_opaque).expression_result);
            }
        }

        // parser.parse("if exists('/interface/gigabit-ethernet/ge-1') then 9", a);
    }
    catch (std::bad_any_cast& ex) {
        std::cerr << "Caught exception: " << ex.what() << std::endl;
    }

    // assert(val == 9);

    

    ::exit(EXIT_SUCCESS);
}

// int main(int argc, char* argv[]) {
//     std::ifstream schema_file;
//     schema_file.open("../schema.config");
//     if (!schema_file.is_open()) {
//         std::cerr << "Failed to open schema file\n";
//         exit(EXIT_FAILURE);
//     }

//     auto root_schema = std::make_shared<SchemaComposite>("/");
//     Config::loadSchema(schema_file, root_schema);
//     std::cout << "Dump nodes creating backtrace:\n";
//     VisitorImpl visitor;
//     root_schema->accept(visitor);

//     std::cout << std::endl;

//     std::ifstream config_file;
//     config_file.open("../main.config");
//     if (!config_file.is_open()) {
//         std::cerr << "Failed to open config file\n";
//         exit(EXIT_FAILURE);
//     }

//     auto load_config = std::make_shared<Composite>("/");
//     Config::load(config_file, load_config, root_schema);
//     std::cout << "Dump nodes creating backtrace:\n";
//     load_config->accept(visitor);

//     // exit(EXIT_SUCCESS);

//     VisitorGE1Speed visit_ge1_speed;
//     load_config->accept(visit_ge1_speed);
//     if (!visit_ge1_speed.getSpeedNode()) {
//         std::cerr << "Not found speed node!\n";
//         exit(EXIT_FAILURE);
//     }

//     auto ee = std::make_shared<ExprEval>();
//     ee->init(visit_ge1_speed.getSpeedNode());
//     if (ee->evaluate("#if #xpath</interface/gigabit-ethernet[@key]/speed/value> #equal #string<400G> #then #must<#regex_match<@value, #regex<^(400G)$>>>")) {
//         std::cout << "Successfully evaluated expression\n";
//     }
//     else {
//         std::cerr << "Failed to evaluate expression\n";
//     }

//     class DependencyGetterVisitior2 : public Visitor {
//     public:
//         virtual ~DependencyGetterVisitior2() = default;
//         DependencyGetterVisitior2() : m_dependencies { std::make_shared<Map<String, Set<String>>>() } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 return true;
//             }

//             auto update_depend_attrs = schema_node->findAttr("update-depend");
//             if (update_depend_attrs.empty()) {
//                 return true;
//             }

//             for (auto& dep : update_depend_attrs) {
//                 // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
//                 // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
//                 (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep);
//                 std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         SharedPtr<Map<String, Set<String>>> getDependencies() { return m_dependencies; }

//     private:
//         SharedPtr<Map<String, Set<String>>> m_dependencies;
//     };

//     class DependencyGetterVisitior : public Visitor {
//     public:
//         virtual ~DependencyGetterVisitior() = default;
//         DependencyGetterVisitior() : m_dependencies { std::make_shared<Map<String, Map<String, SharedPtr<Node>>>>() } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 return true;
//             }

//             auto update_depend_attrs = schema_node->findAttr("update-depend");
//             if (update_depend_attrs.empty()) {
//                 return true;
//             }

//             for (auto& dep : update_depend_attrs) {
//                 // (*m_dependencies)[XPath::convertToXPath(schema_node.getParent()].emplace(dep);
//                 // (*m_dependencies)[schema_node->getName() == "@item" ? XPath::to_string2(schema_node->getParent()) : XPath::to_string(schema_node)].emplace(dep);
//                 (*m_dependencies)[XPath::to_string2(schema_node)].emplace(dep, node);
//                 std::clog << "Node " << node->getName() << " depends on " << dep << std::endl;
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> getDependencies() { return m_dependencies; }

//     private:
//         SharedPtr<Map<String, Map<String, SharedPtr<Node>>>> m_dependencies;
//     };

//     class DependencyMapperVisitior : public Visitor {
//     public:
//         virtual ~DependencyMapperVisitior() = default;
//         DependencyMapperVisitior(Set<String> set_of_ordered_cmds) : m_set_of_ordered_cmds { set_of_ordered_cmds } {}
//         virtual bool visit(SharedPtr<Node> node) override {
//             auto schema_node = std::dynamic_pointer_cast<SchemaNode>(node->getSchemaNode());
//             if (!schema_node) {
//                 std::clog << "Node " << node->getName() << " has not schema\n";
//                 return true;
//             }

//             auto schema_node_xpath = XPath::to_string2(schema_node);
//             if (m_set_of_ordered_cmds.find(schema_node_xpath) != m_set_of_ordered_cmds.end()) {
//                 m_ordered_nodes[schema_node_xpath].emplace_front(node);
//             }
//             else if (m_ordered_nodes.find(XPath::to_string2(schema_node->getParent())) != m_ordered_nodes.end()) {
//                 m_ordered_nodes[XPath::to_string2(schema_node->getParent())].emplace_back(node);
//             }
//             else {
//                 m_unordered_nodes[schema_node_xpath].emplace_back(node);
//             }

//             // if (std::dynamic_pointer_cast<Leaf>(node)) {
//             //     return false; // Terminate visiting on Leaf object
//             // }

//             return true;
//         }

//         Map<String, std::list<SharedPtr<Node>>> getOrderedNodes() { return m_ordered_nodes; }
//         Map<String, std::list<SharedPtr<Node>>> getUnorderedNodes() { return m_unordered_nodes; }

//     private:
//         Set<String> m_set_of_ordered_cmds {};
//         Map<String, std::list<SharedPtr<Node>>> m_ordered_nodes {};
//         Map<String, std::list<SharedPtr<Node>>> m_unordered_nodes {};
//     };

//     DependencyGetterVisitior2 depVisitor;
//     load_config->accept(depVisitor);
//     std::list<String> ordered_cmds;
//     auto sort_result = run_update_op(depVisitor.getDependencies(), ordered_cmds);
//     Set<String> set_of_ordered_cmds;
//     for (auto& cmd : ordered_cmds) {
//         set_of_ordered_cmds.emplace(cmd);
//     }

//     DependencyMapperVisitior depMapperVisitor(set_of_ordered_cmds);
//     load_config->accept(depMapperVisitor);
//     std::list<SharedPtr<Node>> nodes_to_execute {};
//     std::clog << "Sorting unordered nodes\n";
//     for (auto& [schema_name, unordered_nodes] : depMapperVisitor.getUnorderedNodes()) {
//         std::clog << "Schema: " << schema_name << std::endl;
//         std::clog << "Nodes: ";
//         for (auto& n : unordered_nodes) {
//             // TODO: If find prefix in depMapperVisitor.getOrderedNodes() then do not add
//             nodes_to_execute.emplace_back(n);
//             std::clog << n->getName() << " ";
//         }
//         std::clog << std::endl;
//     }

//     std::clog << "Sorting ordered nodes\n";
//     for (auto& ordered_cmd : ordered_cmds) {
//         std::clog << "Schema: " << ordered_cmd << std::endl;
//         std::clog << "Nodes: ";
//         auto ordered_nodes = depMapperVisitor.getOrderedNodes();
//         for (auto& n : ordered_nodes[ordered_cmd]) {
//             nodes_to_execute.emplace_back(n);
//             std::clog << n->getName() << " ";
//         }

//         std::clog << std::endl;
//     }

//     std::clog << "Execute action for nodes\n";
//     for (auto n : nodes_to_execute) {
//         std::clog << n->getName() << ": " << XPath::to_string2(n) << std::endl;
//         auto expr_eval = std::make_shared<ExprEval>();
//         expr_eval->init(n);
//         auto schema_node = std::dynamic_pointer_cast<SchemaNode>(n->getSchemaNode());
//         if (!schema_node) {
//             std::clog << schema_node->getName() << " has not reference to schema node\n";
//             continue;
//         }

//         for (auto& constraint : schema_node->findAttr("update-constraint")) {
//             std::clog << "Evaluating expression '" << constraint << "' for node " << n->getName() << std::endl;
//             if (expr_eval->evaluate(constraint)) {
//                 std::cout << "Successfully evaluated expression\n";
//             }
//             else {
//                 std::cerr << "Failed to evaluate expression\n";
//                 exit(EXIT_FAILURE);
//             }
//         }
//     }

//     // auto depends = std::make_shared<Map<String, Set<String>>>();
//     // // TODO: Add to the ordered list all nodes which does not have refererence, like "/platform/port[@key]/num_breakout"
//     // // TODO: Transform Node name into xpath
//     // (*depends)["/interface/ethernet[@key]"].emplace("/platform/port[@key]/num_breakout");
//     // (*depends)["/interface/aggregate[@key]/members"].emplace("/interface/ethernet[@key]");
//     // (*depends)["/interface/ethernet[@key]/speed"].emplace("/platform/port[@key]/breakout_speed");
//     // (*depends)["/platform/port[@key]/breakout_speed"].emplace("/platform/port[@key]/num_breakout");
//     // auto sort_result = run_update_op(depends, ordered_cmds);

//     // TODO: Go through all nodes and find all nodes which have update-depends 
//     // TODO #1: Map sorted elements into real nodes based on loaded nodes
//     // for (auto& cmd : ordered_cmds)
//     //     auto node = XPath::select(load_config, cmd);
//     // TODO #2: 
//     auto root_config = std::make_shared<Composite>("/");
//     // Make a diff with sorted nodes and root_config <-- it is real load with action taken
//     // If leaf exists in both set, check if it has different value

//     exit(EXIT_SUCCESS);
// }