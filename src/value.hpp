#pragma once

#include "lib/types.hpp"

#include <any>

using Any = std::any;
using Number = int32_t;

class Value {
 public:
  enum class Type {
    BOOL,
    NUMBER,
    STRING,
    REF // if leafref then attach_on_update
  };

  Value() = delete;
  Value(const Type type) : _type { type } { }
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

 private:
  Type _type;
  Any _holder;
};