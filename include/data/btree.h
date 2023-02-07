//
// Created by Florian on 21.11.2022.
//

#pragma once

#include "features/optional.h"
#include "memory/heap.h"
#include "memory/mem.h"
#include "test/test.h"

template<typename T, size_t M>
struct btree_node {
    static constexpr size_t max_child_count = 2 * M;
    static constexpr size_t max_data_count = 2 * M - 1;
    T data[max_data_count];
    btree_node* children[max_child_count];
    size_t size; // number of elements in data

    size_t find_offset(const T& value) const {
        size_t offset = 0;
        while (offset < size && data[offset] < value) {
            offset++;
        }
        return offset;
    }
};

template<typename T>
struct btree {
    // 4096 = sizeof(size_t) + 2 * M * sizeof(void*) + (2 * M - 1) * sizeof(T);
    static constexpr size_t M = (4096 - sizeof(size_t) + sizeof(T)) / ((sizeof(void*) + sizeof(T)) * 2);
    using node = btree_node<T, M>;

    static_assert(sizeof(node) <= page_size, "btree::node too big");
    static_assert(sizeof(btree_node<T, M + 1>) > page_size, "btree::node too small");

    static Test::Result test() {
        btree<T> tree;
        for (T i = 0; i < T(4096); ++i) {
            tree.insert(i);
        }
        for (T i = 0; i < T(4096); ++i) {
            if (auto opt = tree.find(i)) {
                if (*opt != i) {
                    return Test::Result::failure("Wrong value");
                }
            } else {
                return Test::Result::failure("Could not find value");
            }
        }
        for (T i = 0; i < T(4096); ++i) {
            if (!tree.remove(i)) {
                return Test::Result::failure("Could not remove value");
            }
        }
        for (T i = 0; i < T(4096); ++i) {
            if (tree.find(i)) {
                return Test::Result::failure("Found removed value");
            }
        }
        return Test::Result::success();
    }

    node* m_root;
    size_t m_size;

    btree() : m_root(nullptr), m_size(0) {}
    btree(const btree& other) = delete;
    btree& operator=(const btree& other) = delete;
    btree(btree&& other) = delete;
    btree& operator=(btree&& other) = delete;
    ~btree() { delete m_root; }

    /**
     * @brief Iterates over the tree in order and calls callable for each element, if callable returns false, iteration stops
     * @tparam C Callable type (T -> bool)
     * @param callable the callable to call
     */
    template<typename C>
    auto iterate(C callable) {
        iter_impl(m_root, callable);
    }

    [[nodiscard]] optional<T> find(const T& cmp) const {
        node* node = m_root;
        while (node != nullptr) {
            auto off = node->find_offset(cmp);
            if (off < node->size && node->data[off] == cmp) {
                return node->data[off];
            } else {
                node = node->children[off];
            }
        }
        return {};
    }

    bool remove(const T& cmp) {
        auto res = remove_impl(cmp);
        if (res) {
            m_size--;
        }
        return res;
    }

    void insert(const T& cmp) {
        if (m_root == nullptr) {
            m_root = new node();
            m_root->data[0] = cmp;
            m_root->size = 1;
        } else if (m_root->size == node::max_data_count) {
            auto new_root = new node();
            auto old_root = m_root;
            m_root = new_root;
            new_root->size = 0;
            new_root->children[0] = old_root;
            split_child(new_root, 0);
            insert_non_full(new_root, cmp);
        } else {
            insert_non_full(m_root, cmp);
        }
        m_size++;
    }

    [[nodiscard]] inline size_t size() const { return m_size; }
    [[nodiscard]] inline bool empty() const { return m_size == 0; }

private:
    /**
     * @brief Iterates over the tree in order and calls callable for each element, if callable returns false, iteration stops
     * @tparam C Callable type (T -> bool)
     * @param node the node to start at
     * @param callable the callable to call
     */
    template<typename C>
    void iter_impl(node* node, C callable) {
        if (!node) return;
        for (size_t i = 0; i < node->size; i++) {
            iter_impl(node->children[i], callable);
            if(!callable(node->data[i])) return;
        }
        iter_impl(node->children[node->size], callable);
    };

    void insert_non_full(node* x, const T& cmp) {
        while (true) {
            auto i = x->size;
            if (x->children[0] == nullptr) {
                while (i > 0 && x->data[i - 1] > cmp) {
                    x->data[i] = std::move(x->data[i - 1]);
                    i--;
                }
                x->data[i] = cmp;
                x->size++;
                return;
            } else {
                while (i > 0 && x->data[i - 1] > cmp) {
                    i--;
                }
                if (x->children[i]->size == node::max_data_count) {
                    split_child(x, i);
                    if (x->data[i - 1] < cmp) {
                        i++;
                    }
                }
                x = x->children[i];
            }
        }
    }

    void split_child(node* x, size_t i) {
        auto left = x->children[i];
        auto right = new node();
        right->size = M - 1;
        for (size_t j = 0; j < M - 1; j++) {
            right->data[j] = std::move(left->data[j + M]);
            left->data[j + M] = {};
        }
        if (left->children[0] != nullptr) {
            for (size_t j = 0; j < M; j++) {
                right->children[j] = left->children[j + M];
                left->children[j + M] = {};
            }
        }
        left->size = M - 1;
        for (size_t j = x->size; j >= i + 1; j--) {
            x->children[j + 1] = x->children[j];
        }
        x->children[i + 1] = right;
        //for (int64_t j = x->size; j >= i; j--) {
        //    x->data[j - 1] = std::move(y->data[j - 1]);
        //}
        x->data[i] = std::move(left->data[M - 1]);
        left->data[M - 1] = {};
        x->size++;
    }

    bool remove_impl(const T& cmp) {
        if (m_root == nullptr) {
            return false;
        }
        node* p = m_root;
        node* q = nullptr;
        bool found = false;
        if (p->children[0] == nullptr) {
            for (size_t i = 0; i < p->size; ++i) {
                if (p->data[i] == cmp) {
                    if (p->size == 1) {
                        delete m_root;
                        m_root = nullptr;
                    } else {
                        for (size_t j = i; j < p->size - 1; ++j) {
                            p->data[j] = std::move(p->data[j + 1]);
                        }
                        p->data[p->size - 1] = {};
                        p->size--;
                    }
                    return true;
                }
            }
            return false;
        } else if (p->size == 1) {
            if (p->children[0]->size == p->children[1]->size && p->children[1]->size == M - 1) {
                p->data[M - 1] = std::move(p->data[0]);
                node* p1 = p->children[0];
                node* p2 = p->children[1];
                p->children[0] = p1->children[0];
                p->children[M] = p2->children[0];
                for (size_t i = 0; i < M - 1; ++i) {
                    p->data[i] = std::move(p1->data[i]);
                    p->children[i + 1] = p1->children[i + 1];
                    p->data[i + M] = std::move(p2->data[i]);
                    p->children[i + M + 1] = p2->children[i + 1];
                }
                p->size = 2 * M - 1;
                delete p1;
                delete p2;
            } else {
                if (cmp <= p->data[0]) {
                    if (p->children[0]->size == M - 1) {
                        shift_key_to_sibling(p, 1, false);
                    }
                    if (p->data[0] == cmp) {
                        q = p;
                        found = true;
                    }
                    p = p->children[0];
                } else {
                    if (p->children[1]->size == M - 1) {
                        shift_key_to_sibling(p, 1, true);
                    }
                    if (p->data[0] == cmp) {
                        q = p;
                        found = true;
                    }
                    p = p->children[1];
                }
            }
        }

        while (true) {
            if (p->children[0] == nullptr) {
                for (size_t i = 0; i < p->size; ++i) {
                    if (p->data[i] == cmp) {
                        for (size_t j = i; j < p->size - 1; ++j) {
                            p->data[j] = std::move(p->data[j + 1]);
                        }
                        p->data[p->size - 1] = {};
                        p->size--;
                        return true;
                    }
                }
                if (found) {
                    for (size_t i = 0; i < q->size; ++i) {
                        if (q->data[i] == cmp) {
                            q->data[i] = p->data[p->size - 1];
                            p->data[p->size - 1] = {};
                            p->size--;
                            return true;
                        }
                    }
                }
                return false;
            } else {
                size_t k;
                for (k = 0; k < p->size; ++k) {
                    if (cmp <= p->data[k]) {
                        break;
                    }
                }
                if (p->children[k]->size == M - 1) {
                    if (k == p->size) {
                        if (p->children[k - 1]->size == M - 1) {
                            merge_two_siblings(p, k - 1);
                        } else {
                            shift_key_to_sibling(p, k, true);
                        }
                    } else {
                        if (p->children[k + 1]->size == M - 1) {
                            merge_two_siblings(p, k + 1);
                        } else {
                            shift_key_to_sibling(p, k + 1, false);
                        }
                    }
                }
                for (size_t i = 0; i < p->size; ++i) {
                    if (p->data[i] == cmp) {
                        found = true;
                        q = p;
                        break;
                    }
                }
                for (k = 0; k < p->size; ++k) {
                    if (cmp <= p->data[k]) {
                        break;
                    }
                }
                p = p->children[k];
            }
        }
    }

    void shift_key_to_sibling(node* p, size_t k, bool shift_right) {
        if(!shift_right) {
            // shift key from p[k] to p[k-1]
            // example:
            // prev:          [  20  ]
            //               /        \
            //            [10]         [30, 40]
            //           /    \       /   |    \
            //        [5]     [15]  [25] [35]  [50]
            //
            // after:        [  30  ]
            //              /        \
            //      [10, 20]          [40]
            //     /   |    \        /    \
            //  [5]   [15]  [25]  [35]    [50]
            node* left = p->children[k - 1]; // [10]
            node* right = p->children[k]; // [30, 40]
            left->data[left->size] = std::move(p->data[k - 1]); // [10, 20]
            left->size++;
            p->data[k - 1] = std::move(right->data[0]); // [30]
            for (size_t i = 0; i < right->size - 1; ++i) {
                right->data[i] = std::move(right->data[i + 1]);
            }
            right->data[right->size - 1] = {};
            right->size--;
            if (left->children[0] != nullptr) {
                left->children[left->size] = right->children[0];
                for (size_t i = 0; i < right->size; ++i) {
                    right->children[i] = right->children[i + 1];
                }
                right->children[right->size] = nullptr;
            }
        } else {
            // shift key from p[k-1] to p[k]
            // example:
            // prev:          [  30  ]
            //               /        \
            //            [10, 20]     [40]
            //           /   |    \    /    \
            //        [5]   [15]  [25] [35]  [50]
            //
            // after:        [  20  ]
            //              /        \
            //          [10]          [30, 40]
            //         /    \        /   |    \
            //      [5]     [15]  [25] [35]  [50]
            node* left = p->children[k - 1]; // [10, 20]
            node* right = p->children[k]; // [40]
            for (size_t i = right->size; i > 0; --i) {
                right->data[i] = std::move(right->data[i - 1]);
            }
            right->data[0] = std::move(p->data[k - 1]); // [20, 30]
            right->size++;
            p->data[k - 1] = std::move(left->data[left->size - 1]); // [20]
            left->data[left->size - 1] = {};
            left->size--;
            if (left->children[0] != nullptr) {
                for (size_t i = right->size; i > 0; --i) {
                    right->children[i] = right->children[i - 1];
                }
                right->children[0] = left->children[left->size];
                left->children[left->size] = nullptr;
            }
        }
    }

    void merge_two_siblings(node* p, size_t k) {
        k--;
        node* p1 = p->children[k];
        node* p2 = p->children[k + 1];
        p1->data[M - 1] = std::move(p->data[k]);
        for (size_t i = 0; i < M - 1; ++i) {
            p1->data[i + M] = std::move(p2->data[i]);
            p1->children[i + M] = p2->children[i];
        }
        p1->children[2 * M - 1] = p2->children[M - 1];
        p1->size = 2 * M - 1;
        for (size_t i = k; i < p->size - 1; ++i) {
            p->data[i] = std::move(p->data[i + 1]);
            p->children[i + 1] = p->children[i + 2];
        }
        p->children[p->size] = p->children[p->size + 1];
        p->data[p->size - 1] = {};
        p->children[p->size] = nullptr;
        p->size--;
        delete p2;
    }
};

namespace btree_impl {

template<typename K, typename V>
struct pair {
    K key;
    V value;
    bool operator==(const pair& other) const { return key == other.key; }
    bool operator!=(const pair& other) const { return key != other.key; }
    bool operator<(const pair& other) const { return key < other.key; }
    bool operator>(const pair& other) const { return key > other.key; }
    bool operator<=(const pair& other) const { return key <= other.key; }
    bool operator>=(const pair& other) const { return key >= other.key; }
};

}// namespace btree_impl

template<typename K, typename V>
struct btree_map : public btree<btree_impl::pair<K,V>>{
    [[nodiscard]] optional<V> find(const K& cmp) const {
        auto res = btree<btree_impl::pair<K,V>>::find(btree_impl::pair<K,V>{cmp, V{}});
        if(!res) {
            return {};
        }
        return res->value;
    }
    void insert(const K& key, const V& value) {
        btree<btree_impl::pair<K,V>>::insert(btree_impl::pair<K,V>{key, value});
    }
    /**
     * @brief Iterates over the tree in order and calls callable for each element, if callable returns false, iteration stops
     * @tparam C Callable type K,V -> bool
     * @param callable the callable to call
     */
    template<typename C>
    void iterate_kv(C callable) {
        btree<btree_impl::pair<K,V>>::iterate([&callable](const btree_impl::pair<K,V>& p) -> bool{
            return callable(p.key, p.value);
        });
    }
    bool remove(const K& key) {
        return btree<btree_impl::pair<K,V>>::remove(btree_impl::pair<K,V>{key, V{}});
    }
};