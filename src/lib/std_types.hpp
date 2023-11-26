#pragma once

#include <any>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <regex>
#include <set>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

// Keep it in alphabetical order
#define Deque std::deque
#define ForwardList std::forward_list
#define List std::list
#define Map std::map
#define MultiMap std::multimap
#define Pair std::pair
#define Queue std::queue
#define Set std::set
#define SharedPtr std::shared_ptr
#define Stack std::stack
#define WeakPtr std::weak_ptr
#define Vector std::vector

using Any = std::any;
using BadAnyCast = std::bad_any_cast;
using Regex = std::regex;
using String = std::string;
using StringView = std::string_view;

using UInt16 = std::uint16_t;

using Int64 = std::int64_t;

// FIXME: Use alias
#define AnyCast std::any_cast

static constexpr inline auto StringEnd() {
    return String::npos;
}