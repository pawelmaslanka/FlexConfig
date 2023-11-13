#pragma once

#include "lib/types.hpp"

class Node;

class IObserver {
 public:
  virtual ~IObserver() = default;
  // onPreUpdate: dry_run = true
  // onUpdate: dry_run = false
  // virtual bool onUpdate(SharedPtr<Node> node, Value old_value, Value new_value, const bool dry_run = false) = 0;
  virtual bool onUpdate(SharedPtr<Node> node) = 0;
  virtual bool onDelete(SharedPtr<Node> node) = 0;
};

class ISubject {
  public:
    ISubject() = default;
    virtual ~ISubject() = default;
    virtual bool attachOnUpdate(SharedPtr<Node> node);
    virtual bool detachOnUpdate(String xpath);
    virtual bool attachOnDelete(SharedPtr<Node> node);
    virtual bool detachOnDelete(String xpath);
    // notifyOnPreUpdate: dry_run = true
    // notifyOnUpdate: dry_run = false
    // virtual bool notifyOnUpdate(SharedPtr<Node> node,
    //     Value old_value, Value new_value, const bool dry_run = false);
    virtual bool notifyOnUpdate(SharedPtr<Node> node);
    virtual bool notifyOnDelete(SharedPtr<Node> node);

  protected:
    std::map<String, WeakPtr<Node>> m_node_by_xpath_on_update;
    std::map<String, WeakPtr<Node>> m_node_by_xpath_on_delete; // TODO: SharedPtr<Node> -> WeakPtr<Node>

    bool attach(SharedPtr<Node> node, std::map<String, WeakPtr<Node>>& node_by_xpath);
    bool detach(String xpath, std::map<String, WeakPtr<Node>>& node_by_xpath);
};
