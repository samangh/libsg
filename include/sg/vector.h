#pragma once

#include <vector>

namespace sg::vector {

template <typename T>
void append(std::vector<T> &base, const std::vector<T>& to_add) {
    base.insert(std::end(base), std::begin(to_add), std::end(to_add));
}

/**
 * @brief Move contents of one vector to the end of another.
 *
 * There is no point in using this for basic types where a move is
 * equivalnet to copy (e.g. ini, double). But for more complex types
 * this is useful.
 */
template <typename T>
void append(std::vector<T> &base, std::vector<T>&& to_add) {
    base.insert(std::end(base), make_move_iterator(to_add.begin()), make_move_iterator(to_add.end()));
}

} // namespace sg::vector
