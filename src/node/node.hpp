/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "visitor.hpp"

#include <lib/multiinherit_shared.hpp>
#include <lib/std_types.hpp>

class Node : public IVisitable, public inheritable_enable_shared_from_this<Node> {
  public:
    virtual ~Node();
    Node(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    String Name() const;
    void SetParent(SharedPtr<Node> parent);
    SharedPtr<Node> Parent() const;
    void SetSchemaNode(SharedPtr<Node> schema_node);
    SharedPtr<Node> SchemaNode() const;
    virtual SharedPtr<Node> MakeCopy(SharedPtr<Node> parent = nullptr) const;

    virtual void Accept(Visitor& visitor) override;

  private:
    SharedPtr<Node> m_parent;
    SharedPtr<Node> m_schema_node;
    String m_name;
};

class SchemaNode : virtual public Node, public inheritable_enable_shared_from_this<SchemaNode> {
  public:
    virtual ~SchemaNode();
    SchemaNode(const String& name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    void AddAttr(const String& attr_name, const String& attr_val);
    ForwardList<String> FindAttr(const String& attr_name);
    virtual void Accept(Visitor& visitor) override;

  private:
    static const Set<String> ATTR_NAME;
    static const Set<String> TYPE_NAME;
    Map<String, ForwardList<String>> m_attr_by_name; // Attribute definition by its name
    Set<String> m_references; // Or to do from that just observer? onUpdate()/onDelete(), use with attribute: ref
};
