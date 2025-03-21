/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "lib/std_types.hpp"
#include "node/node.hpp"

#include <any>

using Any = std::any;
using Number = int32_t;

// TODO: Replace Value with std::variant
class Value {
 public:
  enum class Type {
    BOOL,
    NUMBER,
    STRING,
    STRING_ARRAY,
    NODE_ARRAY,
    REF // if leafref then attach_onUpdate
  };

  Value() = delete;
  Value(const Type type);
  Value(const Value& value);

  bool has_value() const;
  Type type() const { return _type; }
  String to_string() const;

  bool is_bool() const;
  bool get_bool() const;
  bool set_bool(const bool val);

  bool is_number() const;
  Number get_number() const;
  bool set_number(const Number val);

  bool is_string() const;
  String get_string() const;
  bool set_string(const String val);

  bool is_string_array() const;
  Vector<String> get_string_array() const;
  bool add_item(const String val);

  bool is_node_array() const;
  Vector<SharedPtr<Node>> get_node_array() const;
  bool add_item(SharedPtr<Node> val);

 private:
  Type _type;
  Any _holder;
};
