#ifndef ENUM_ARRAY_H
#define ENUM_ARRAY_H

#include <array>
#include <type_traits>

template<typename T, typename V, std::size_t Size = static_cast<std::size_t>(T::Count)>
class EnumArray {
public:
    using value_type = V;
    using size_type = std::size_t;
    using reference = value_type&;
    using const_reference = const value_type&;

    constexpr reference operator[](T index) {
        return data_[static_cast<size_type>(index)];
    }

    constexpr const_reference operator[](T index) const {
        return data_[static_cast<size_type>(index)];
    }

    constexpr size_type size() const { return Size; }

private:
    std::array<V, Size> data_;
};

#endif // ENUM_ARRAY_H 