// Result.h
#pragma once
#include <variant>
#include <string>
#include <optional>
#include <stdexcept>

template<typename T, typename E = std::string>
class Result {
public:
    static Result Ok(T value) {
        Result r;
        r.m_value = std::move(value);
        r.m_hasValue = true;
        return r;
    }

    static Result Err(E error) {
        Result r;
        r.m_error = std::move(error);
        r.m_hasValue = false;
        return r;
    }

    [[nodiscard]] bool IsOk() const noexcept { return m_hasValue; }
    [[nodiscard]] bool IsErr() const noexcept { return !m_hasValue; }

    T& Value()& {
        if (!m_hasValue) {
            throw std::runtime_error("Accessing error as value: " + GetError());
        }
        return *m_value;
    }

    const T& Value() const& {
        if (!m_hasValue) {
            throw std::runtime_error("Accessing error as value: " + GetError());
        }
        return *m_value;
    }

    T&& Value()&& {
        if (!m_hasValue) {
            throw std::runtime_error("Accessing error as value: " + GetError());
        }
        return std::move(*m_value);
    }

    E& Error()& {
        if (m_hasValue) {
            throw std::runtime_error("Accessing value as error");
        }
        return m_error;
    }

    const E& Error() const& {
        if (m_hasValue) {
            throw std::runtime_error("Accessing value as error");
        }
        return m_error;
    }

    [[nodiscard]] std::optional<T> TryGetValue() const {
        if (m_hasValue) return m_value;
        return std::nullopt;
    }

    [[nodiscard]] T ValueOr(T defaultValue) const {
        return m_hasValue ? *m_value : defaultValue;
    }

    [[nodiscard]] Result<void, E> DiscardValue() const {
        if (m_hasValue) {
            return Result<void, E>::Ok();
        }
        return Result<void, E>::Err(m_error);
    }

    [[nodiscard]] const E& GetError() const noexcept {
        return m_error;
    }

private:
    Result() = default;

    std::optional<T> m_value;
    E m_error{};
    bool m_hasValue = false;
};

template<typename E>
class Result<void, E> {
public:
    static Result Ok() {
        Result r;
        r.m_success = true;
        return r;
    }

    static Result Err(E error) {
        Result r;
        r.m_error = std::move(error);
        r.m_success = false;
        return r;
    }

    [[nodiscard]] bool IsOk() const noexcept { return m_success; }
    [[nodiscard]] bool IsErr() const noexcept { return !m_success; }

    E& Error()& {
        if (m_success) {
            throw std::runtime_error("Accessing success as error");
        }
        return m_error;
    }

    const E& Error() const& {
        if (m_success) {
            throw std::runtime_error("Accessing success as error");
        }
        return m_error;
    }

private:
    Result() = default;

    bool m_success = false;
    E m_error{};
};

template<typename T>
using StringResult = Result<T, std::string>;
using StringVoidResult = Result<void, std::string>;