#pragma once

#include "visitor.hpp"

#include <lib/multiinherit_shared.hpp>
#include <lib/types.hpp>

class Node : public IVisitable, public inheritable_enable_shared_from_this<Node> {
  public:
    virtual ~Node();
    Node(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    String getName();
    void setParent(SharedPtr<Node> parent);
    SharedPtr<Node> getParent() const;
    void setSchemaNode(SharedPtr<Node> schema_node);
    SharedPtr<Node> getSchemaNode() const;

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

  private:
    static const Set<String> ATTR_NAME;
    static const Set<String> TYPE_NAME;
    Map<String, ForwardList<String>> m_attr_by_name; // Attribute definition by its name
};

class Leaf : virtual public Node, public inheritable_enable_shared_from_this<Leaf> {
public:
    virtual ~Leaf() = default;
    Leaf(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr, const String value = {});
    void setValue(const String value);
    String getValue() const;

private:
    String m_value;
};
