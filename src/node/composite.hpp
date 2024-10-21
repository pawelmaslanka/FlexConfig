/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "node.hpp"
#include "visitor.hpp"
#include <lib/multiinherit_shared.hpp>
#include <lib/std_types.hpp>

class Composite
 : virtual public Node, public inheritable_enable_shared_from_this<Composite> {
  public:
    virtual ~Composite();
    Composite(const String name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);

    virtual bool Add(SharedPtr<Node> node);
    virtual bool Remove(const String node_name);

    virtual SharedPtr<Node> MakeCopy(SharedPtr<Node> parent = nullptr) const override;

    virtual void Accept(Visitor& visitor) override;
    virtual SharedPtr<Node> FindNode(const String node_name);
    virtual size_t Count() const;

  private:
    Map<String, SharedPtr<Node>> _node_by_name;
};

// TODO: Czy potrzebny jest SchemaComposite? Niewystarczy nam posiadanie samego Composite?
class SchemaComposite
 : virtual public SchemaNode, virtual public Composite, public inheritable_enable_shared_from_this<SchemaComposite> {
  public:
    virtual ~SchemaComposite();
    SchemaComposite(const String name, SharedPtr<Node> parent = nullptr, SharedPtr<Node> schema_node = nullptr);
    virtual void Accept(Visitor& visitor) override;
};
