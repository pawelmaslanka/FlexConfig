#pragma once

#include "lib/std_types.hpp"
#include "multiinherit_shared.hpp"

class Node;

class IVisitableNode {
  public:
    ~IVisitableNode() = default;
    virtual SharedPtr<Node> getNode() const = 0;
};

class Visitor
 : public inheritable_enable_shared_from_this<Visitor> {
  public:
    virtual ~Visitor() = default;
    Visitor() = default;
    virtual bool visit(SharedPtr<Node> node) = 0;
    // TODO: Replace with: virtual bool visit(IVisitableNode& node) = 0;
    // TODO: bool goDFS(); // DFS or BFS
    // TODO: size_t depthLevel(); // When stop recurrency
};

class IVisitable {
  public:
    virtual ~IVisitable() = default;
    virtual void accept(Visitor& visitor) = 0;
};
