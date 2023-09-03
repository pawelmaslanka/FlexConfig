#pragma once

#include "node.hpp"

#include "value.hpp"
#include "visitor.hpp"

#include "lib/multiinherit_shared.hpp"
#include "lib/types.hpp"

#include <optional>
#define Optional std::optional

class ExprEval
  : public std::enable_shared_from_this<ExprEval> {
  public:
    virtual ~ExprEval() = default;
    ExprEval();
    bool init(SharedPtr<Node> root);
    void deinit();
    size_t evaluate(const std::string tokens, const size_t start_idx = 0, const size_t nested_lvl = 0);

  private:
    SharedPtr<Node> _processing_node;
    SharedPtr<Node> _visit_node;
    String _visit_node_name;
    bool _initialized;

    class ExprEvalVisitor : public Visitor {
      public:
        virtual ~ExprEvalVisitor() = default;
        virtual bool visit(SharedPtr<Node> node) override;

        bool init(SharedPtr<ExprEval> expr_eval);

      private:
        SharedPtr<ExprEval> _expr_eval;
    };

    friend class ExprEvalVisitor;
    
    SharedPtr<ExprEvalVisitor> _visitor;
    Pair<bool, Optional<Value>> xpath_evaluate(std::string xpath);
    void get_result_of_eval(int nested_lvl = 0);
};