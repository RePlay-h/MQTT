#ifndef MQTT_CONTAINERS_TRIE_H_
#define MQTT_CONTAINERS_TRIE_H_

#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <string>
#include <memory>
#include <fstream>
#include <ranges>
#include <map>

namespace tree {

    static std::vector<std::string> split(std::string_view path) {
        std::vector<std::string> path_copy;

        std::string_view::iterator it1 = begin(path);
        std::string_view::iterator it2 = std::find(it1, end(path), '/');

        while (it2 != end(path)) {
            path_copy.push_back(std::string{ it1, it2 });
            it1 = it2 + 1;
            it2 = std::find(it1, end(path), '/');
        }

        path_copy.push_back(std::string{ it1, it2 });

        return path_copy;
    }

    template<class T>
    struct Node;

    template<class T>
    class trie {

    public:
        trie() {
            node_ = std::make_unique<Node<T>>();
        }

        //Functions for removing an element from a tree
        template<class It>
        void insert(It it, It end_it, const T& data) { //Copy an element in the tree
            if (it == end_it) {
                node_->data_ = data;
                return;
            }

            node_->children_[*it].insert(next(it), end_it, data); //descending deeper into the tree
        }

        template<class It>
        void insert(It it, It end_it, const T&& data) { //Move an element in the tree
            if (it == end_it) {
                node_->data_ = std::move(data);
                return;
            }

            node_->children_[*it].insert(it + 1, end_it, std::move(data)); //descending deeper into the tree
            
        }

        void insert(std::string path, const T& data) {

            std::vector<std::string> path_copy = split(path);

            insert(begin(path_copy), end(path_copy), data);
        }

        //get element at specified path
        template<class It>
        T& get(It it, It end_it) {
            if (it == end_it) {
                return node_->data_;
            }
            return node_->children_[*it].get(next(it), end_it); //descending deeper into the tree
        }

        T& get(const std::string& path) {

            std::vector<std::string> path_copy = split(path);

            return get(begin(path_copy), end(path_copy));
        }

        //remove elemetn from tree
        template<class It>
        void remove(It it, It end_it) {
            if (it + 1 == end_it) {
                node_->children_[*it].node_->children_.clear();
                node_->children_.erase(*it);
                return;
            }

            return node_->children_[*it].remove(next(it), end_it); //descending deeper into the tree
        }

        void remove(std::string path) {

            std::vector<std::string> path_copy = split(path);

            return remove(begin(path_copy), end(path_copy));
        }

        void print(std::ostream& os) {
            if (!node_) {
                return;
            }

            for (auto& [path, node] : node_->children_) {
                os << path << ' ' << node.node_->data_ << '\n';
                node.print(os);

            }
        }

        std::map<std::string, trie<T>>* get_node(const std::string &path) {
            
            std::vector<std::string> path_copy = split(path);

            tree::trie<T> *tr = this;

            for(const auto& piece_of_top : path_copy) {
                tr = &tr->node_->children_[piece_of_top];
            }

            return &tr->node_->children_;

        }


        bool empty() {
            return node_;
        }
    private:
        std::unique_ptr<Node<T>> node_;
    };

    template<class T>
    struct Node {
        std::map<std::string, trie<T>> children_;
        T data_;
    };
}

#endif // !MQTT_CONTAINERS_TRIE_H_

