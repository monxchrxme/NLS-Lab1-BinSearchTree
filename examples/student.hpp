#pragma once

#include <tree_lab/value_codec.hpp>

#include <string>
#include <utility>

class Student {
private:
    int id_{0};
    std::string name_;
    std::string birth_date_;

public:
    Student() = default;

    Student(int id, std::string name, std::string birth_date)
        : id_(id), name_(std::move(name)), birth_date_(std::move(birth_date)) {
        if (id_ <= 0 || name_.empty() || birth_date_.empty()) {
            throw std::invalid_argument("Student requires positive id, name and birth date");
        }
    }

    int id() const noexcept { return id_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& birth_date() const noexcept { return birth_date_; }

    friend bool operator<(const Student& left, const Student& right) {
        return left.id_ < right.id_;
    }
    friend bool operator==(const Student&, const Student&) = default;
};

// Key ordering is by id; the codec preserves every field, including spaces/quotes
template <>
struct ValueCodec<Student> {
    static std::string encode(const Student& student) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << student.id() << ' ' << std::quoted(student.name()) << ' '
            << std::quoted(student.birth_date());
        return out.str();
    }
    static Student decode(const std::string& text) {
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        int id = 0;
        if (!(in >> id)) {
            throw std::invalid_argument("Invalid student id");
        }
        auto name = read_value<std::string>(in);
        auto birth_date = read_value<std::string>(in);
        if (!name || !birth_date) {
            throw std::invalid_argument("Missing student field");
        }
        require_text_end(in);
        return Student(id, std::move(*name), std::move(*birth_date));
    }
};
