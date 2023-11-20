#include "topo_sort.hpp"

Graph::Node::Node(std::string_view name)
 : _name { name } {
}

void Graph::Node::addEdge(SharedPtr<Graph::Node> node) {
    _edges.push_back(node);
}

const std::string& Graph::Node::name() const {
    return _name;
}

std::list<SharedPtr<Graph::Node>>& Graph::Node::edges() {
    return _edges;
}

bool Graph::Node::operator==(const Graph::Node& another) const {
    return _name == another._name;
}

std::optional<std::string> Graph::depResolve(SharedPtr<Graph::Node> node,
    std::list<SharedPtr<Graph::Node>>& resolved, std::list<SharedPtr<Graph::Node>>& unresolved) {
    auto findNode = [&node](const SharedPtr<Graph::Node>& n) {
        return n->name() == node->name(); 
    };

    unresolved.push_back(node);
    for (auto& edge : node->edges()) {
        auto findEdge = [&edge](const SharedPtr<Graph::Node>& n) {
            return n->name() == edge->name(); 
        };
        auto node_it = std::find_if(resolved.begin(), resolved.end(), findNode);
        // edge not in resolved
        if (node_it == resolved.end()) {
            node_it = std::find_if(unresolved.begin(), unresolved.end(), findEdge);
            // edge in unresolved
            if (node_it != unresolved.end()) {
                return "Circular reference detected: " + node->name() + " -> " + edge->name();
            }

            if (auto err = Graph::depResolve(edge, resolved, unresolved)) {
                return err;
            }
        }
    }

    resolved.push_back(node);
    unresolved.remove_if(findNode);

    return std::nullopt;
}

std::optional<std::string> makeGraphNode(std::map<std::string, std::set<std::string>>& adj_list, Graph::Node& node) {
    // std::cout << "Node name: " << node.name() << std::endl;
    auto adj_it = adj_list.find(node.name());
    if (adj_it == adj_list.end()) {
            // std::cerr << "Empty adjacency list\n";
        //     auto edge = std::make_shared<Graph::Node>("");
        //     node.addEdge(edge);
        //     makeGraphNode(adj_list, *edge);
            return std::nullopt;
    }

    for (auto& edge_name : adj_it->second) { // go through all node adjacency
        // std::cout << "Edge: " << edge_name << std::endl;
        auto edge = std::make_shared<Graph::Node>(edge_name);
        node.addEdge(edge);
        // Break cyclic
        auto edge_adj_it = adj_list.find(edge_name);
        if ((edge_adj_it == adj_list.end())
            || (edge_adj_it->second.find(node.name()) == edge_adj_it->second.end())) {
            if (auto err = makeGraphNode(adj_list, *edge)) {
                return err;
            }
        }
        else {
            return "Detected cyclic dependency: " + edge_name + " -> " + node.name();
        }
    }

    return std::nullopt;
}

// TODO: First run delete_depends(), then run update_depends()
std::optional<std::string> run_update_op(SharedPtr<Map<String, Set<String>>> cmds, std::list<String>& ordered_cmds) {
    if (!cmds) {
        return "Not pass data!";
    }
    // std::cout << "Elements: " << cmds->size() << std::endl;
    // TODO: First sort by operation to delete and then operation to update
    std::map<std::string, std::set<std::string>> adj_list; // adjacency list
    // Build adjacency list
    for (auto& cmd : *cmds) {
        // std::cout << "Build adjacency list for " << cmd.first << std::endl;
        auto comp = cmd.first;
        auto depends = cmd.second;
        // std::cout << comp << ": " << depends.size() << std::endl;
        if (depends.empty()) {
            if (adj_list.find(comp) == adj_list.end()) {
                adj_list.emplace(comp, std::set<std::string>{});
            }

            continue;
        }

        for (auto& dep : depends) {
            // std::cout << comp << " -> " << dep << std::endl;
            auto it = adj_list.find(comp);
            if (it != adj_list.end()) {
                it->second.insert(dep);
            }
            else {
                auto insert_it = adj_list.emplace(comp, std::set<std::string>{});
                insert_it.first->second.insert(dep);
            }
        }
    }

    // std::cout << "Adjacency list size: " << adj_list.size() << std::endl;
    std::list<SharedPtr<Graph::Node>> graph_nodes;
    for (auto& node : adj_list) {
        graph_nodes.emplace_front(std::make_shared<Graph::Node>(node.first));
        if (auto err = makeGraphNode(adj_list, *graph_nodes.front())) {
            return err;
        }
    }

    std::list<SharedPtr<Graph::Node>> resolved, unresolved;
    for (auto& node : graph_nodes) {
        // std::cout << "Resolving " << node->name() << std::endl;
        if (auto err = Graph::depResolve(node, resolved, unresolved)) {
            return err;
        }
    }

    // std::cout << "Dependency resolved:\n";
    for (const auto& res : resolved) {
        // std::cout << res->name() << std::endl;
    }

    // Merge result from all "trees".
    // Remove duplicated nodes.
    // Keep "resolved order" of nodes.
    for (auto& res : resolved) {
        auto it = adj_list.find(res->name());
        if (it != adj_list.end()) {
                // std::cout << "Resolved " << res->name() << std::endl;
                // Check if there is already xpath which is parent of processing xpath
                std::list<String>::iterator find_it = std::find_if(ordered_cmds.begin(), ordered_cmds.end(), [&res](String& node_xpath) -> bool {
                        if ((node_xpath.find(res->name()) != std::string::npos)
                                        && (res->name().size() < node_xpath.size())) {
                                // std::cout << "Found shorter xpath " << res->name() << " than " << node_xpath << std::endl;
                                return true;
                        }

                        return false;
                });

                // Put parent before its child
                ordered_cmds.emplace(find_it, res->name());
                adj_list.erase(it);
        }
    }

    // std::cout << "Dependency resolved2:\n";
    for (const auto& res : ordered_cmds) {
        // std::cout << res << std::endl;
    }

    // TODO: Check if this step won't mess dependencies
    // NOTE: Unfortunately, the following algorithm causes incorrect ordering (e.g. /interface/ethernet/eth-2_1):
    /*
        /platform
        /platform/port
        /platform/port/eth-1
        /platform/port/eth-1/breakout-mode
        /platform/port/eth-2
        /platform/port/eth-2/breakout-mode
        /interface
        /interface/ethernet
        /interface/ethernet/eth-1
        /interface/ethernet/eth-1/speed
        /interface/aggregate-ethernet
        /interface/aggregate-ethernet/ae-1
        /interface/aggregate-ethernet/ae-1/members
        /interface/aggregate-ethernet/ae-1/members/eth-2_1
        /interface/ethernet/eth-2_1
        /vlan
        /vlan/id
        /vlan/id/2
        /vlan/id/2/members
        /vlan/id/2/members/eth-1
        /protocol
    */
    // Find last xpath which is a substring of currently processing xpath and put it after that xpath
    // Got: /interface/ethernet/eth-2 /interface/aggregate-ethernet /interface/aggregate-ethernet/ae-1 /interface/ethernet/eth-2/speed
    // Expected: /interface/ethernet/eth-2 /interface/ethernet/eth-2/speed /interface/aggregate-ethernet /interface/aggregate-ethernet/ae-1
    // ForwardList<String> corrected_ordered_cmds {};
    // if (ordered_cmds.size() > 0) {
    //     corrected_ordered_cmds.push_front(*ordered_cmds.begin());
    //     ordered_cmds.erase(ordered_cmds.begin());
    //     ForwardList<String>::iterator longest_xpath = corrected_ordered_cmds.begin();
    //     for (auto& item : ordered_cmds) {
    //         for (auto it = corrected_ordered_cmds.begin(); it != corrected_ordered_cmds.end(); ++it) {
    //             if (item.find(*it) != String::npos) {
    //                 if (longest_xpath->size() < it->size()) {
    //                     // std::cout << item << " is longer than " << *it << std::endl;
    //                     longest_xpath = it;
    //                 }
    //             }
    //         }

    //         longest_xpath = corrected_ordered_cmds.insert_after(longest_xpath, item);
    //     }

    //     ordered_cmds.clear();
    //     for (auto& item : corrected_ordered_cmds) {
    //         ordered_cmds.push_back(item);
    //     }
    // }

    // std::cout << "Resolved " << ordered_cmds.size() << " items:\n";
    // for (auto& r : ordered_cmds) {
    //     std::cout << r << " ";
    // }

    // std::cout << std::endl;
    return std::nullopt;
}
