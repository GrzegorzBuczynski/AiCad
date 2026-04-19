#pragma once

#include <utility>
#include <variant>

#if __has_include(<expected>)
#include <expected>
#endif

#if !defined(__cpp_lib_expected)
namespace std {

template <typename E>
class unexpected {
public:
    explicit unexpected(E error)
        : error_(std::move(error)) {}

    [[nodiscard]] E& error() {
        return error_;
    }

    [[nodiscard]] const E& error() const {
        return error_;
    }

private:
    E error_;
};

template <typename T, typename E>
class expected {
public:
    expected(const T& value)
        : has_value_(true), storage_(value) {}

    expected(T&& value)
        : has_value_(true), storage_(std::move(value)) {}

    expected(const unexpected<E>& error)
        : has_value_(false), storage_(error.error()) {}

    expected(unexpected<E>&& error)
        : has_value_(false), storage_(std::move(error.error())) {}

    [[nodiscard]] bool has_value() const {
        return has_value_;
    }

    [[nodiscard]] explicit operator bool() const {
        return has_value();
    }

    [[nodiscard]] T& value() {
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T& value() const {
        return std::get<T>(storage_);
    }

    [[nodiscard]] E& error() {
        return std::get<E>(storage_);
    }

    [[nodiscard]] const E& error() const {
        return std::get<E>(storage_);
    }

private:
    bool has_value_ = true;
    std::variant<T, E> storage_{};
};

}  // namespace std
#endif
