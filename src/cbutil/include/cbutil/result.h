///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   result.h
///
/// @brief  Result type for error handling without exceptions
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBUTIL_RESULT_H
#define CBUTIL_RESULT_H

#include <optional>
#include <string>

namespace cbutil {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Result template for operations that return a value or an error
///
template<typename T>
class Result {
public:
    static Result<T> ok(T value) {
        Result<T> r;
        r.m_ok = true;
        r.m_value = std::move(value);
        return r;
    }

    static Result<T> error(const std::string& msg) {
        Result<T> r;
        r.m_ok = false;
        r.m_error = msg;
        return r;
    }

    [[nodiscard]] bool isOk() const { return m_ok; }
    [[nodiscard]] bool isError() const { return !m_ok; }

    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    bool m_ok = false;
    std::optional<T> m_value;
    std::string m_error;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Specialization for operations that succeed or fail without returning a value
///
template<>
class Result<void> {
public:
    static Result<void> ok() {
        Result<void> r;
        r.m_ok = true;
        return r;
    }

    static Result<void> error(const std::string& msg) {
        Result<void> r;
        r.m_ok = false;
        r.m_error = msg;
        return r;
    }

    [[nodiscard]] bool isOk() const { return m_ok; }
    [[nodiscard]] bool isError() const { return !m_ok; }
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    bool m_ok = false;
    std::string m_error;
};

} // namespace cbutil

#endif // CBUTIL_RESULT_H
