#include "value.hpp"

#include <array>
#include <iostream>
#include <charconv>
#include <sstream>
#include <string_view>
#include <system_error>
#include <typeindex>
#include <typeinfo>
#include <type_traits>

Value::Value(const Value& value) {
    _type = value._type;
    _holder = value._holder;
}

bool Value::has_value() const {
  return _holder.has_value();
}

String Value::to_string() const {
  std::ostringstream convert;
  switch (_type) {
    case Type::BOOL: {
      convert << std::boolalpha << get_bool();
      break;
    }
    case Type::NUMBER: {
      std::array<char, 20> str;
      auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), get_number());
      if (ec != std::errc()) {
        std::cerr << "Failed to convert number " << get_number() << " into string\n";
      }

      convert << StringView(str.data(), ptr - str.data());
      break;
    }
    case Type::STRING: {
      convert << get_string();
      break;
    }
    default: {
      convert << "";
    }
  }

  return convert.str();
}

#define DEFINE_VALUE_METHOD(NAME_SUFFIX, ENUM_VALUE_TYPE, REAL_TYPE) \
  bool Value::is_##NAME_SUFFIX() const { return _holder.has_value() && (std::type_index(_holder.type()) == std::type_index(typeid(REAL_TYPE))); } \
  REAL_TYPE Value::get_##NAME_SUFFIX() const { return std::any_cast< REAL_TYPE >(_holder); } \
  bool Value::set_##NAME_SUFFIX(const REAL_TYPE val) { if (_type != ENUM_VALUE_TYPE) return false; _holder = val; return true; }

DEFINE_VALUE_METHOD(bool, Value::Type::BOOL, bool)
DEFINE_VALUE_METHOD(number, Value::Type::NUMBER, Number)
DEFINE_VALUE_METHOD(string, Value::Type::STRING, String)
// bool Value::has_bool() const { return _holder.has_value() && (std::type_index(_holder.type()) == std::type_index(typeid(bool))); }
// bool Value::get_bool() const { return std::any_cast<bool>(_holder); }
// bool Value::set_bool(bool val) { if (_type != Type::BOOL) return false; _holder = val; }