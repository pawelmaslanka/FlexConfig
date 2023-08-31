#pragma once

#include "types.hpp"
#include "multiinherit_shared.hpp"

class Node;

class Visitor
 : public inheritable_enable_shared_from_this<Visitor> {
  public:
    virtual ~Visitor() = default;
    Visitor() = default;
    virtual bool visit(SharedPtr<Node> node) = 0;
};

class IVisitable {
  public:
    virtual ~IVisitable() = default;
    virtual void accept(Visitor& visitor) = 0;
};