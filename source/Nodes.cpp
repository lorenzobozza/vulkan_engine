//
//  Nodes.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 20/09/23.
//

#include "Nodes.hpp"
#include "Log.hpp"


Node::Tree::Tree(Primitive::Map& priMap) : m_Primitives(priMap) {
    Node root("_root");
    root.cacheMatrix = glm::toMat4(glm::quat(0.f, 1.f, 0.f, 0.f)); // glTF -> Vulkan, PI rotation about X axis
    root.invalidateCache = false;
    add(root);
}

uint32_t Node::Tree::add(const Node& n, uint32_t parent) {
    if (parent == UINT32_MAX) {
        nodes.emplace_back(n);
        if (n.parent >= 0 && (uint32_t)n.parent < nodes.size())
            nodes[n.parent].children.push_back((uint32_t)nodes.size() - 1);
        return (uint32_t)nodes.size() - 1;
    } else if (parent < nodes.size()) {
        nodes.emplace_back(n);
        nodes.back().parent = parent;
        nodes[parent].children.push_back((uint32_t)nodes.size() - 1);
        return (uint32_t)nodes.size() - 1;
    }
    return UINT32_MAX;
}
void Node::Tree::pop(uint32_t node) {
    if (node < nodes.size()) {
        int32_t parent = nodes[node].parent;
        uint32_t child = 0;
        for (auto it = nodes[parent].children.begin(); it < nodes[parent].children.end(); ++it, ++child) {
            if (nodes[parent].children[child] == node) { nodes[parent].children.erase(it); break; }
        }
        nodes.erase(nodes.begin() + node);
        // TODO: Fix Leaking dangling nodes
    }
}

void Node::Tree::invalidateCacheHierarchy(uint32_t node) {
    if (node < nodes.size() && node > 0) {
        nodes[node].invalidateCache = true;
        for (uint32_t child : nodes[node].children) {
            invalidateCacheHierarchy(child);
        }
    }
}

void Node::Tree::computeTransformMatrix(uint32_t node) {
    if (node < nodes.size() && node > 0) {
        Node& n = nodes[node];
        if (n.invalidateCache) {
            uint32_t parent = n.parent;
            computeTransformMatrix(parent);
            n.cacheMatrix = nodes[parent].cacheMatrix * n.matrix * (glm::translate(glm::mat4(1.0), n.transl) * glm::toMat4(n.quat) * glm::scale(glm::mat4(1.0), n.scale));
            n.invalidateCache = false;
            for (Primitive::id_t p : n.primitives) {
                m_Primitives.at(p).transform.matrix = n.cacheMatrix;
            }
        }
    }
}
