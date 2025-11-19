///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   result.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Result type for error handling (extracted from device_session.h)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_RESULT_H
#define CBDEV_RESULT_H

#include <string>
#include <optional>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Result template for operation results
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
/// @brief Specialization for Result<void>
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

} // namespace cbdev

#endif // CBDEV_RESULT_H
