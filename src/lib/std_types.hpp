/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

// This file should include only aliases to types from std:: namespace because it is included 
// in a lot of files so it should not be changed too frequently.

#include <any>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <optional>
#include <vector>

// Keep it in alphabetical order
template<class T> using Deque = std::deque<T>;
// FIXME: Conflict with class Duration in ABSL library
// #define Duration std::chrono::duration
template<class T> using ForwardList = std::forward_list<T>;
template<class T> using List = std::list<T>;
template<class Mutex> using LockGuard = std::lock_guard<Mutex>;
template<class Key, class T> using Map = std::map<Key, T>;
template<class Key, class T> using MultiMap = std::multimap<Key, T>;
template<class T> using Optional = std::optional<T>;
template<class T1, class T2> using Pair = std::pair<T1, T2>;
template<class T> using Queue = std::queue<T>;
template<class... MutexTypes> using ScopedLock = std::scoped_lock<MutexTypes...>;
template<class T> using Set = std::set<T>;
template<class T> using SharedPtr = std::shared_ptr<T>;
template<class T> using Stack = std::stack<T>;
template<class T> using UniqueLock = std::unique_lock<T>;
template<class T> using UniquePtr = std::unique_ptr<T>;
template<class T> using WeakPtr = std::weak_ptr<T>;
template<class T> using Vector = std::vector<T>;

using Any = std::any;
#define Array std::array
using BadAnyCast = std::bad_any_cast;
using ConditionVariable = std::condition_variable;
// using JThread = std::jthread;
#define MakeSharedPtr std::make_shared
#define MakeUniquePtr std::make_unique
using Mutex = std::mutex;
#define NullOpt std::nullopt
using Regex = std::regex;
using RuntimeError = std::runtime_error;
using String = std::string;
using StringStream = std::stringstream;
using StringView = std::string_view;
using Thread = std::thread;

using Byte = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt64 = std::uint64_t;

using Int64 = std::int64_t;

// FIXME: Use alias
#define AnyCast std::any_cast

static constexpr inline auto StringEnd() {
    return String::npos;
}
