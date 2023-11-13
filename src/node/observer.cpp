#include "observer.hpp"

#include "xpath.hpp"

#include <iostream>

bool ISubject::attach(SharedPtr<Node> node, std::map<String, WeakPtr<Node>>& node_by_xpath) {
   std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " > Adding node " << node->getName() << " to observing" << std::endl;
    if (!node) {
        std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
        return false;
    }

    auto xpath = XPath::to_string2(node);
    if (xpath.empty()) {
        std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
        return false;
    }

    std::cout << " By xpath: " << xpath << std::endl;
    if (!std::dynamic_pointer_cast<IObserver>(node)) {
        std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
        return false;
    }

    if (!node_by_xpath.insert({ xpath, node }).second) {
      std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
      return false;
    }

    return true;
}

bool ISubject::detach(String xpath, std::map<String, WeakPtr<Node>>& node_by_xpath) {
    auto it = node_by_xpath.find(xpath);
    if (it == node_by_xpath.end()) {
        std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
        return false;
    }

    node_by_xpath.erase(xpath);
    return true;
}

bool ISubject::attachOnUpdate(SharedPtr<Node> node) {
    return attach(node, m_node_by_xpath_on_update);
}

bool ISubject::detachOnUpdate(String xpath) {
    return detach(xpath, m_node_by_xpath_on_update);
}

bool ISubject::attachOnDelete(SharedPtr<Node> node) {
    return attach(node, m_node_by_xpath_on_delete);
}

bool ISubject::detachOnDelete(String xpath) {
    return detach(xpath, m_node_by_xpath_on_delete);
}

// bool ISubject::notifyOnUpdate(SharedPtr<Node> node,
//     Value old_value, Value new_value, const bool dry_run) {
//     for (auto [_, observer] : m_node_by_xpath_on_update) {
//         if (!std::dynamic_pointer_cast<IObserver>(observer)->onUpdate(node, old_value, new_value)) {
//             std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
//             return false;
//         }
//     }

//     return true;
// }

bool ISubject::notifyOnUpdate(SharedPtr<Node> node) {
  std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << std::endl;
    ForwardList<String> nodes_no_longer_valid;
    for (auto [xpath, node_observer] : m_node_by_xpath_on_update) {
      std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << std::endl;
        auto observer = node_observer.lock();
        if (!observer) {
            nodes_no_longer_valid.push_front(xpath);
            continue;
        }

        if (!std::dynamic_pointer_cast<IObserver>(observer)->onUpdate(node)) {
            std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
            return false;
        }
    }

    for (auto& xpath : nodes_no_longer_valid) {
        m_node_by_xpath_on_update.erase(xpath);
    }

    return true;
}

bool ISubject::notifyOnDelete(SharedPtr<Node> node) {
  std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << std::endl;
    ForwardList<String> nodes_no_longer_valid;
    for (auto [xpath, node_observer] : m_node_by_xpath_on_delete) {
        auto observer = node_observer.lock();
        if (!observer) {
            nodes_no_longer_valid.push_front(xpath);
            continue;
        }
        std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "] Call onDelete() for " << std::dynamic_pointer_cast<Node>(observer)->getName() << " -> " << (std::dynamic_pointer_cast<Node>(observer)->getParent() ? std::dynamic_pointer_cast<Node>(observer)->getParent()->getName() : "no parent") << " -> " << xpath << std::endl;
        if (!std::dynamic_pointer_cast<IObserver>(observer)->onDelete(node)) {
            std::cout << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "]" << " ERROR" << std::endl;
            return false;
        }
    }

    for (auto& xpath : nodes_no_longer_valid) {
        m_node_by_xpath_on_delete.erase(xpath);
    }

    return true;
}
