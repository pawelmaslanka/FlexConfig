#pragma once

#include "node.hpp"
#include "visitor.hpp"
#include <lib/multiinherit_shared.hpp>
#include <lib/types.hpp>

class Composite
 : virtual public Node, public inheritable_enable_shared_from_this<Composite> {
  public:
    virtual ~Composite();
    Composite(const String name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);

    virtual bool add(SharedPtr<Node> node);
    virtual bool remove(const String node_name);

    virtual SharedPtr<Node> makeCopy(SharedPtr<Node> parent = nullptr) const override;

    virtual void accept(Visitor& visitor) override;
    virtual SharedPtr<Node> findNode(const String node_name);
    virtual size_t count() const;

  private:
    Map<String, SharedPtr<Node>> m_node_by_name;
};

// TODO: Czy potrzebny jest SchemaComposite? Niewystarczy nam posiadanie samego Composite?
class SchemaComposite
 : virtual public SchemaNode, virtual public Composite, public inheritable_enable_shared_from_this<SchemaComposite> {
  public:
    virtual ~SchemaComposite();
    SchemaComposite(const String name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    virtual void accept(Visitor& visitor) override;
};
