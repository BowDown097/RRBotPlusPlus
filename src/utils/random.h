#pragma once
#include <limits>
#include <ranges>
#include <sodium.h>

namespace RR
{
    namespace utility
    {
        template<std::integral T>
        T random(T min, T max)
        {
            return min + randombytes_uniform(max - min);
        }

        template<std::floating_point T>
        T random(T min, T max)
        {
            constexpr int shift = std::min(std::numeric_limits<T>::digits, std::numeric_limits<double>::digits);
            uint64_t value;
            randombytes_buf(&value, sizeof(value));
            T flt = (T)(value >> 11) / (T)(1ull << shift);
            return min + flt * (max - min);
        }

        template<typename T> requires std::integral<T> || std::floating_point<T>
        T random(T max) { return random((T)0, max); }

        decltype(auto) randomElement(std::ranges::range auto&& range)
        {
            if constexpr (std::ranges::random_access_range<decltype(range)>)
                return range[random(std::ranges::size(range))];
            else if constexpr (std::ranges::sized_range<decltype(range)>)
                return *std::ranges::next(std::ranges::begin(range), random(std::ranges::size(range)));
            else
                return *std::ranges::next(std::ranges::begin(range), random(std::ranges::distance(range)));
        }
    }
}
