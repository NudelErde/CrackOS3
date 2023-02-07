//
// Created by Florian on 20.11.2022.
//

#pragma once

template<typename T>
struct interval {
    T left;
    T right;

    [[nodiscard]] auto size() const {
        return right - left;
    }

    [[nodiscard]] bool valid() const {
        return left < right;
    }

    [[nodiscard]] bool intersects(const interval<T>& other) const {
        return !(other.right < left || right < other.left);
    }
};