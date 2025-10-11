#pragma once

#include <ranges>
#include <vector>

namespace DSREquipmentSwap
{
    template <typename T>
    bool contains(const std::vector<T>& vec, const T& item)
    {
        return std::ranges::find(vec, item) != vec.end();
    }
} // namespace DSREquipmentSwap