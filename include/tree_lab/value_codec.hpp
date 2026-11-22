#pragma once

#include <cmath>
#include <charconv>
#include <complex>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

// Specialize encode/decode for a type with its own text representation
template <typename T>
struct ValueCodec {
    static std::string encode(const T& value) {
        if constexpr (std::is_floating_point_v<T>) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("Non-finite value");
            }
        }
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<long double>::max_digits10) << value;
        if (!out) {
            throw std::invalid_argument("Cannot encode value");
        }
        return out.str();
    }

    static T decode(const std::string& text) {
        if constexpr (std::is_floating_point_v<T>) {
            T value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value)) {
                throw std::invalid_argument("Cannot decode finite floating-point value");
            }
            return value;
        }
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        T value{};
        if (!(in >> value)) {
            throw std::invalid_argument("Cannot decode value");
        }
        in >> std::ws;
        if (!in.eof()) {
            throw std::invalid_argument("Trailing characters in value");
        }
        if constexpr (std::is_floating_point_v<T>) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("Non-finite value");
            }
        }
        return value;
    }
};

template <>
struct ValueCodec<std::string> {
    static std::string encode(const std::string& value) { return value; }
    static std::string decode(const std::string& text) { return text; }
};

template <typename Real>
struct ValueCodec<std::complex<Real>> {
    static std::string encode(const std::complex<Real>& value) {
        return "(" + ValueCodec<Real>::encode(value.real()) + "," +
               ValueCodec<Real>::encode(value.imag()) + ")";
    }
    static std::complex<Real> decode(const std::string& text) {
        const auto comma = text.find(',');
        if (text.size() < 5 || text.front() != '(' || text.back() != ')' ||
            comma == std::string::npos || text.find(',', comma + 1) != std::string::npos) {
            throw std::invalid_argument("Expected complex value (real,imag)");
        }
        return {ValueCodec<Real>::decode(text.substr(1, comma - 1)),
                ValueCodec<Real>::decode(text.substr(comma + 1, text.size() - comma - 2))};
    }
};

inline void require_token_end(std::istream& in) {
    const auto next = in.peek();
    if (next != std::char_traits<char>::eof() &&
        !std::isspace(static_cast<char>(next), std::locale::classic())) {
        throw std::invalid_argument("Expected whitespace between tokens");
    }
}

inline void read_format(std::istream& in, const char* expected) {
    std::string actual;
    if (!(in >> actual) || actual != expected) {
        throw std::invalid_argument("Invalid format identifier");
    }
}

inline void require_text_end(std::istream& in) {
    // peek() at the end of the final token may already have set eofbit
    if (!in.eof()) {
        in >> std::ws;
    }
    if (!in.eof()) {
        throw std::invalid_argument("Trailing data");
    }
}

template <typename T>
std::optional<T> read_value(std::istream& in) {
    in >> std::ws;
    const auto next = in.peek();
    if (next == '#') {
        in.get();
        require_token_end(in);
        return std::nullopt;
    }
    if (next != '"') {
        throw std::invalid_argument("Expected quoted value or #");
    }
    std::string text;
    if (!(in >> std::quoted(text))) {
        throw std::invalid_argument("Unterminated quoted value");
    }
    require_token_end(in);
    return ValueCodec<T>::decode(text);
}

template <typename T>
void write_value(std::ostream& out, const T& value) {
    out << std::quoted(ValueCodec<T>::encode(value));
}
