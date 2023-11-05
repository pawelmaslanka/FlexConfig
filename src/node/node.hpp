#pragma once

#include "visitor.hpp"

#include <lib/multiinherit_shared.hpp>
#include <lib/types.hpp>

class Node : public IVisitable, public inheritable_enable_shared_from_this<Node> {
  public:
    virtual ~Node();
    Node(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    String getName() const;
    void setParent(SharedPtr<Node> parent);
    SharedPtr<Node> getParent() const;
    void setSchemaNode(SharedPtr<Node> schema_node);
    SharedPtr<Node> getSchemaNode() const;
    virtual SharedPtr<Node> makeCopy(SharedPtr<Node> parent = nullptr) const;

    virtual void accept(Visitor& visitor) override;

  private:
    SharedPtr<Node> m_parent;
    SharedPtr<Node> m_schema_node;
    String m_name;
};

class SchemaNode : virtual public Node, public inheritable_enable_shared_from_this<SchemaNode> {
  public:
    virtual ~SchemaNode();
    SchemaNode(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    void addAttr(const String& attr_name, const String& attr_val);
    ForwardList<String> findAttr(const String& attr_name);
    virtual void accept(Visitor& visitor) override;
    // TODO: Implement them
    // addReference
    // removeReference

  private:
    static const Set<String> ATTR_NAME;
    static const Set<String> TYPE_NAME;
    Map<String, ForwardList<String>> m_attr_by_name; // Attribute definition by its name
    Set<String> m_references; // Or to do from that just observer? onUpdate()/onDelete(), use with attribute: ref
};
