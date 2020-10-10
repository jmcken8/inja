// Copyright (c) 2019 Pantor. All rights reserved.

#ifndef INCLUDE_INJA_FUNCTION_STORAGE_HPP_
#define INCLUDE_INJA_FUNCTION_STORAGE_HPP_

#include <vector>
#include <ostream>
#include <utility>

#include "bytecode.hpp"
#include "string_view.hpp"


namespace inja {

using json = nlohmann::json;

using Arguments = std::vector<const json*>;
using ValueCallbackFunction = std::function<json(Arguments& args)>;
using StreamingCallbackFunction = std::function<void(std::ostream& s, Arguments& args)>;

struct CallbackContainer {
  enum class Type {VALUE, STREAMING, _};
  union Callback {
    Callback() {}
    Callback(const ValueCallbackFunction& function) : value(function) {}
    Callback(const StreamingCallbackFunction& function) : streaming(function) {}
    ~Callback() {}
    ValueCallbackFunction value;
    StreamingCallbackFunction streaming;
  };
  CallbackContainer() : type(Type::_) {}
  CallbackContainer(const ValueCallbackFunction& function) : type(Type::VALUE), callback(function) {}
  CallbackContainer(const StreamingCallbackFunction& function) : type(Type::STREAMING), callback(function) {}
  ~CallbackContainer() {
    switch(type) {
      case Type::VALUE:
        callback.value.~ValueCallbackFunction(); return;
      case Type::STREAMING:
        callback.streaming.~StreamingCallbackFunction(); return;
      case Type::_: return;
    }
  }
  CallbackContainer(const CallbackContainer& other) : type(other.type) {
    switch(other.type) {
      case Type::VALUE:
        new(&callback) Callback(other.callback.value); return;
      case Type::STREAMING:
        new(&callback) Callback(other.callback.streaming); return;
      case Type::_: return;
    }
  }
  CallbackContainer(CallbackContainer&& other) noexcept : type(other.type) {
    switch(other.type)
    {
      case Type::VALUE:
        new(&callback.value) ValueCallbackFunction(std::move(other.callback.value)); return;
      case Type::STREAMING:
        new(&callback.streaming) StreamingCallbackFunction(std::move(other.callback.streaming)); return;
      case Type::_: return;
    }
  }
  CallbackContainer& operator=(const CallbackContainer& other) {
    return *this = CallbackContainer(other);
  }
  CallbackContainer& operator=(const CallbackContainer&& other) noexcept {
    type = other.type;
    switch(other.type) {
      case Type::VALUE:
        new(&callback.value) ValueCallbackFunction(std::move(other.callback.value)); return *this;
      case Type::STREAMING:
        new(&callback.streaming) StreamingCallbackFunction(std::move(other.callback.streaming)); return *this;
      case Type::_: return *this;
    }
  }
  operator bool()
  {
    return type != Type::_;
  }

  Type type;
  Callback callback;
};

/*!
 * \brief Class for builtin functions and user-defined callbacks.
 */
class FunctionStorage {
 public:
  void add_builtin(nonstd::string_view name, unsigned int num_args, Bytecode::Op op) {
    auto& data = get_or_new(name, num_args);
    data.op = op;
  }

  template<typename T>
  void add_callback(nonstd::string_view name, unsigned int num_args, const T& function) {
    auto& data = get_or_new(name, num_args);
    data.function = function;
  }

  Bytecode::Op find_builtin(nonstd::string_view name, unsigned int num_args) const {
    if (auto ptr = get(name, num_args)) {
      return ptr->op;
    }
    return Bytecode::Op::Nop;
  }

  CallbackContainer find_callback(nonstd::string_view name, unsigned int num_args) const {
    if (auto ptr = get(name, num_args)) {
      return ptr->function;
    }
    return {};
  }

 private:
  struct FunctionData {
    unsigned int num_args {0};
    Bytecode::Op op {Bytecode::Op::Nop}; // for builtins
    CallbackContainer function; // for callbacks
    FunctionData() : function() {}
  };

  FunctionData& get_or_new(nonstd::string_view name, unsigned int num_args) {
    auto &vec = m_map[static_cast<std::string>(name)];
    for (auto &i: vec) {
      if (i.num_args == num_args) {
        return i;
      }
    }
    vec.emplace_back();
    vec.back().num_args = num_args;
    return vec.back();
  }

  const FunctionData* get(nonstd::string_view name, unsigned int num_args) const {
    auto it = m_map.find(static_cast<std::string>(name));
    if (it == m_map.end()) {
      return nullptr;
    }

    for (auto &&i: it->second) {
      if (i.num_args == num_args) {
        return &i;
      }
    }
    return nullptr;
  }

  std::map<std::string, std::vector<FunctionData>> m_map;
};

}

#endif  // INCLUDE_INJA_FUNCTION_STORAGE_HPP_
