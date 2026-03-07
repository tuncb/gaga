#pragma once

#include <cassert>
#include <utility>
#include <variant>

namespace tl {

template <typename E>
class unexpected {
public:
    constexpr explicit unexpected(E error) : error_(std::move(error)) {}

    [[nodiscard]] constexpr const E& value() const& { return error_; }
    [[nodiscard]] constexpr E& value() & { return error_; }
    [[nodiscard]] constexpr E&& value() && { return std::move(error_); }

private:
    E error_;
};

template <typename E>
unexpected(E) -> unexpected<E>;

template <typename T, typename E>
class expected {
public:
    expected(const T& value) : storage_(value) {}
    expected(T&& value) : storage_(std::move(value)) {}
    expected(const unexpected<E>& error) : storage_(error.value()) {}
    expected(unexpected<E>&& error) : storage_(std::move(error.value())) {}

    [[nodiscard]] bool has_value() const { return std::holds_alternative<T>(storage_); }
    [[nodiscard]] explicit operator bool() const { return has_value(); }

    [[nodiscard]] T& value() & {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T& value() const& {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] E& error() & {
        assert(!has_value());
        return std::get<E>(storage_);
    }

    [[nodiscard]] const E& error() const& {
        assert(!has_value());
        return std::get<E>(storage_);
    }

private:
    std::variant<T, E> storage_;
};

template <typename E>
class expected<void, E> {
public:
    expected() = default;
    expected(const unexpected<E>& error) : has_value_(false), error_(error.value()) {}
    expected(unexpected<E>&& error) : has_value_(false), error_(std::move(error.value())) {}

    [[nodiscard]] bool has_value() const { return has_value_; }
    [[nodiscard]] explicit operator bool() const { return has_value(); }

    void value() const { assert(has_value_); }

    [[nodiscard]] E& error() & {
        assert(!has_value_);
        return error_;
    }

    [[nodiscard]] const E& error() const& {
        assert(!has_value_);
        return error_;
    }

private:
    bool has_value_ = true;
    E error_{};
};

}  // namespace tl
