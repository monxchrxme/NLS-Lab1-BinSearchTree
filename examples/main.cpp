#include <tree_lab/ordered_set.hpp>
#include "student.hpp"

#include <algorithm>
#include <complex>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>

void print_heading(const char* title) {
    std::cout << '\n' << title << "\n------------------------------\n";
}

std::string read_line(const char* prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        throw std::runtime_error("Ввод завершён");
    }
    return line;
}

int read_int(const char* prompt) {
    const auto text = read_line(prompt);
    try {
        return ValueCodec<int>::decode(text);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Введите целое число без посторонних символов");
    }
}

int checked_int(long long value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        throw std::overflow_error("Результат не помещается в int");
    }
    return static_cast<int>(value);
}

template <typename Container>
Container read_container() {
    std::istringstream in(read_line("Целые числа через пробел (пустая строка — пустой контейнер): "));
    Container result;
    std::string token;
    std::size_t count = 0;
    while (in >> token) {
        if (++count > 20000) {
            throw std::invalid_argument("Для демонстрации допустимо не более 20000 чисел");
        }
        try {
            result.insert(ValueCodec<int>::decode(token));
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Ожидается список целых чисел через пробел");
        }
    }
    return result;
}

template <typename Container>
Container read_serialized(const char* prompt) {
    const auto text = read_line(prompt);
    try {
        return Container::from_string(text);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Некорректная строка: проверьте идентификатор, значения, потомков и порядок BST");
    }
}

template <typename T>
std::string format_value(const T& value) {
    std::ostringstream out;
    if constexpr (requires { out << value; }) {
        out << value;
        return out.str();
    } else {
        return ValueCodec<T>::encode(value);
    }
}

std::string format_value(const std::string& value) {
    std::string result = "«";
    for (char character : value) {
        switch (character) {
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            result += static_cast<unsigned char>(character) < 32 || character == 127 ? '?' : character;
        }
    }
    return result + "»";
}

std::string format_value(const Student& student) {
    return "ID: " + std::to_string(student.id()) + "; имя: " + format_value(student.name()) +
           "; дата рождения: " + format_value(student.birth_date());
}

template <typename Real>
std::string format_value(const std::complex<Real>& value) {
    if (value.imag() == 0) { return format_value(value.real()); }
    if (value.real() == 0) { return format_value(value.imag()) + "i"; }
    return format_value(value.real()) + (value.imag() < 0 ? " - " : " + ") +
           format_value(std::abs(value.imag())) + "i";
}

template <typename T, typename Compare>
void print_structure(const SearchTree<T, Compare>& tree) {
    if (tree.empty() || tree.size() > 20) { return; }
    struct DisplayNode {
        std::string label;
        std::size_t depth;
        TreeBranch branch;
    };
    DynamicArray<DisplayNode> nodes;
    tree.for_each_node([&](const T& value, std::size_t depth, TreeBranch branch) {
        nodes.push_back({format_value(value), depth, branch});
    });
    std::cout << "Структура (L — левый, R — правый потомок):\n";
    DynamicArray<bool> continues;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        auto next = i + 1;
        while (next < nodes.size() && nodes[next].depth > node.depth) { ++next; }
        const bool has_sibling = next < nodes.size() && nodes[next].depth == node.depth;
        continues.resize(node.depth + 1);
        continues[node.depth] = has_sibling;
        std::cout << "  ";
        for (std::size_t depth = 1; depth < node.depth; ++depth) {
            std::cout << (continues[depth] ? "│   " : "    ");
        }
        if (node.depth != 0) {
            std::cout << (has_sibling ? "├── " : "└── ")
                      << (node.branch == TreeBranch::left ? "L: " : "R: ");
        }
        std::cout << node.label << '\n';
    }
}

template <typename T, typename Compare>
void print_structure(const OrderedSet<T, Compare>&) {} // Sets expose their contents, not a tree shape.

template <typename Container>
void print_container(const Container& container) {
    std::cout << "Элементов: " << container.size() << '\n';
    if (container.empty()) {
        std::cout << "Контейнер пуст.\n";
        return;
    }
    std::cout << "Значения (ЛКП):\n  ";
    std::size_t printed = 0;
    container.for_each([&](const auto& value) {
        if (printed++ >= 100) { return; }
        if constexpr (std::is_arithmetic_v<std::remove_cvref_t<decltype(value)>>) {
            std::cout << format_value(value) << ' ';
        } else {
            if (printed > 1) { std::cout << "\n  "; }
            std::cout << format_value(value);
        }
    });
    if (container.size() > 100) { std::cout << "\n  ... (показаны первые 100)"; }
    std::cout << '\n';
    print_structure(container);
}

template <typename Container>
void print_result(const Container& source, const Container& result) {
    std::cout << "Исходный контейнер:\n";
    print_container(source);
    std::cout << "\nРезультат:\n";
    print_container(result);
    std::cout << "Исходный контейнер не изменён.\n";
}

template <typename Container>
Container generate_container() {
    const int count = read_int("Количество попыток вставки (0..20000): ");
    const int lower = read_int("Минимальное значение: ");
    const int upper = read_int("Максимальное значение: ");
    const int seed = read_int("Seed (целое число): ");
    const int order = read_int("Порядок: 0 — случайный, 1 — по возрастанию, 2 — по убыванию: ");
    if (count < 0 || count > 20000 || lower > upper || order < 0 || order > 2) {
        throw std::invalid_argument("Некорректные параметры генерации");
    }
    std::mt19937 generator(static_cast<std::uint32_t>(seed));
    std::uniform_int_distribution<int> distribution(lower, upper);
    DynamicArray<int> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) { values.push_back(distribution(generator)); }
    if (order != 0) { std::sort(values.begin(), values.end()); }
    if (order == 2) { std::reverse(values.begin(), values.end()); }
    std::cout << "Сгенерированные данные: ";
    for (std::size_t i = 0; i < std::min<std::size_t>(values.size(), 100); ++i) {
        std::cout << values[i] << ' ';
    }
    if (values.size() > 100) { std::cout << "..."; }
    std::cout << '\n';
    Container result;
    for (int value : values) { result.insert(value); }
    std::cout << "Попыток вставки: " << count
              << "\nУникальных значений: " << result.size()
              << "\nПовторов: " << values.size() - result.size() << '\n';
    return result;
}

void print_traversals(const SearchTree<int>& tree) {
    print_heading("ОБХОДЫ ДЕРЕВА");
    if (tree.empty()) { std::cout << "Дерево пусто.\n"; return; }
    const char* const names[]{"КЛП", "КПЛ", "ЛКП", "ЛПК", "ПКЛ", "ПЛК", "BFS (в ширину)"};
    for (std::size_t i = 0; i < std::size(names); ++i) {
        std::cout << names[i] << ": ";
        std::size_t count = 0;
        tree.for_each([&](int value) { if (count++ < 100) { std::cout << value << ' '; } },
                      static_cast<TraversalOrder>(i));
        if (count > 100) { std::cout << "..."; }
        std::cout << '\n';
    }
}

void print_container_menu(bool is_tree) {
    print_heading(is_tree ? "БИНАРНОЕ ДЕРЕВО ПОИСКА — BST<int>" : "МНОЖЕСТВА — OrderedSet<int>");
    std::cout << "Работа с данными\n"
                 "  1. Ручной ввод   2. Генерация   3. Просмотр   12. Очистка\n"
                 "Базовые операции\n"
                 "  4. Вставка       5. Удаление    6. Поиск";
    if (is_tree) { std::cout << "   13. Все обходы"; }
    std::cout << "\nФункциональные операции\n"
                 "  7. map           8. where       9. reduce (сумма)\n";
    if (is_tree) {
        std::cout << "Поддеревья\n"
                     "  14. Извлечь      15. Проверить вхождение\n";
    } else {
        std::cout << "Операции над множествами\n"
                     "  13. Объединение, пересечение, разность, включение и равенство\n";
    }
    std::cout << "Сериализация\n"
                 "  10. Сохранить в строку   11. Прочитать строку\n";
    std::cout << "\n99. Показать меню    0. Назад\n";
}

template <typename Container>
void run_container() {
    Container container;
    constexpr bool is_tree = std::is_same_v<Container, SearchTree<int>>;
    print_container_menu(is_tree);
    while (true) {
        try {
            const int command = read_int(is_tree ? "\nBST [99 — меню, 0 — назад] > "
                                                  : "\nМножество [99 — меню, 0 — назад] > ");
            if (command == 0) { return; }
            switch (command) {
            case 99: print_container_menu(is_tree); break;
            case 1:
                print_heading("РУЧНОЙ ВВОД");
                container = read_container<Container>(); print_container(container); break;
            case 2:
                print_heading("ГЕНЕРАЦИЯ");
                container = generate_container<Container>(); print_container(container); break;
            case 3: print_heading("СОДЕРЖИМОЕ"); print_container(container); break;
            case 4: {
                const int value = read_int("Добавить: ");
                std::cout << (container.insert(value) ? "Добавлено.\n" : "Уже существует.\n");
                break;
            }
            case 5: {
                const int value = read_int("Удалить: ");
                std::cout << (container.erase(value) ? "Удалено.\n" : "Не найдено.\n");
                break;
            }
            case 6: {
                const int value = read_int("Найти: ");
                std::cout << (container.find(value) ? "Найдено\n" : "Не найдено\n");
                break;
            }
            case 7: {
                print_heading("MAP — a*x+b");
                const int multiplier = read_int("map: множитель a для a*x+b: ");
                const int offset = read_int("map: слагаемое b: ");
                auto result = container.map([&](int x) {
                    return checked_int(static_cast<long long>(multiplier) * x + offset);
                });
                print_result(container, result);
                break;
            }
            case 8: {
                print_heading("WHERE — отбор по интервалу");
                const int lower = read_int("where: нижняя граница (включительно): ");
                const int upper = read_int("where: верхняя граница (включительно): ");
                if (lower > upper) { throw std::invalid_argument("Границы перепутаны"); }
                auto result = container.where([&](int x) { return x >= lower && x <= upper; });
                print_result(container, result);
                break;
            }
            case 9:
                print_heading("REDUCE — сумма элементов");
                print_container(container);
                std::cout << "reduce: сумма = " << container.reduce([](int x, long long acc) {
                    if ((x > 0 && acc > std::numeric_limits<long long>::max() - x) ||
                        (x < 0 && acc < std::numeric_limits<long long>::min() - x)) {
                        throw std::overflow_error("Переполнение суммы");
                    }
                    return acc + x;
                }, 0LL) << '\n';
                break;
            case 10:
                print_heading("СЕРИАЛИЗАЦИЯ — строка для копирования");
                std::cout << container.to_string() << '\n'; break;
            case 11: {
                print_heading("ЧТЕНИЕ ИЗ СТРОКИ");
                container = read_serialized<Container>("Строка сериализации: ");
                print_container(container);
                break;
            }
            case 12: container.clear(); std::cout << "Контейнер очищен.\n"; break;
            default:
                if constexpr (is_tree) {
                    if (command == 13) {
                        print_traversals(container);
                    } else if (command == 14) {
                        print_heading("ИЗВЛЕЧЕНИЕ ПОДДЕРЕВА");
                        const int root = read_int("Корень поддерева: ");
                        auto result = container.subtree(root);
                        print_result(container, result);
                    } else if (command == 15) {
                        print_heading("ВХОЖДЕНИЕ ПОДДЕРЕВА");
                        auto candidate = read_serialized<Container>("Поддерево в формате TREE1: ");
                        std::cout << "Исходное дерево:\n"; print_container(container);
                        std::cout << "\nКандидат:\n"; print_container(candidate);
                        std::cout << (container.contains_subtree(candidate) ? "Входит\n" : "Не входит\n");
                    } else { throw std::invalid_argument("Неизвестная команда"); }
                } else {
                    if (command != 13) { throw std::invalid_argument("Неизвестная команда"); }
                    print_heading("ОПЕРАЦИИ НАД МНОЖЕСТВАМИ");
                    std::cout << "Введите второе множество B.\n";
                    auto other = read_container<Container>();
                    std::cout << "\nA:\n"; print_container(container);
                    std::cout << "\nB:\n"; print_container(other);
                    std::cout << "\nA | B — объединение:\n"; print_container(container | other);
                    std::cout << "\nA & B — пересечение:\n"; print_container(container & other);
                    std::cout << "\nA - B — разность:\n"; print_container(container - other);
                    std::cout << "\nB - A — разность:\n"; print_container(other - container);
                    std::cout << "\nA <= B: " << (container <= other ? "да" : "нет")
                              << "; A >= B: " << (container >= other ? "да" : "нет")
                              << "; A == B: " << (container == other ? "да" : "нет") << '\n'
                              << "Исходные множества не изменены.\n";
                }
            }
        } catch (const std::out_of_range&) {
            std::cout << "Ошибка: корень поддерева не найден.\n";
        } catch (const std::exception& error) {
            if (std::cin.eof()) { return; }
            std::cout << "Ошибка: " << error.what() << '\n';
        }
    }
}

struct ComplexCompare {
    bool operator()(const std::complex<double>& a, const std::complex<double>& b) const {
        return std::tuple(a.real(), a.imag()) < std::tuple(b.real(), b.imag());
    }
};

template <typename Container>
void show_type_example(const char* title, const char* ordering, const Container& container) {
    print_heading(title);
    std::cout << ordering << "\n\n";
    print_container(container);
    while (true) {
        try {
            const int command = read_int("\n1 — показать сериализацию, 0 — к выбору типа > ");
            if (command == 0) { return; }
            if (command != 1) { throw std::invalid_argument("Неизвестная команда"); }
            print_heading("СЕРИАЛИЗАЦИЯ — строка для копирования");
            std::cout << container.to_string() << '\n';
        } catch (const std::exception& error) {
            if (std::cin.eof()) { return; }
            std::cout << "Ошибка: " << error.what() << '\n';
        }
    }
}

void show_types() {
    while (std::cin) {
        print_heading("ДРУГИЕ ТИПЫ ДАННЫХ");
        std::cout << "1. double          Вещественные числа\n"
                     "2. std::string     Строки\n"
                     "3. Student         Пользовательский тип\n"
                     "4. std::complex    Комплексные числа\n\n"
                     "0. Назад\n";
        try {
            switch (read_int("> ")) {
            case 0: return;
            case 1:
                show_type_example("BST<double>", "Порядок: по возрастанию чисел.",
                                  SearchTree<double>{0.1, 2.5, -3.0});
                break;
            case 2:
                show_type_example("OrderedSet<std::string>", "Порядок: лексикографический (std::less).",
                                  OrderedSet<std::string>{"строка с пробелами", "#", "кавычка\""});
                break;
            case 3: {
                const SearchTree<Student> students{
                    Student(2, "Иван Иванов", "2005-04-12"), Student(1, "Анна Петрова", "2006-01-03")};
                const auto restored = SearchTree<Student>::from_string(students.to_string());
                show_type_example("BST<Student>", "Порядок: по ID студента. Пример восстановлен из строки.", restored);
                break;
            }
            case 4:
                show_type_example("BST<std::complex<double>>",
                                  "Компаратор: сначала действительная часть, затем мнимая.",
                                  SearchTree<std::complex<double>, ComplexCompare>{{2, 1}, {1, 3}, {2, -1}});
                break;
            default: throw std::invalid_argument("Неизвестная команда");
            }
        } catch (const std::exception& error) {
            if (std::cin.eof()) { return; }
            std::cout << "Ошибка: " << error.what() << '\n';
        }
    }
}

int main() {
    std::cout << "Лабораторная 1. BST и множество.\n"
                 "Операции map/where и извлечение поддерева показывают независимый результат.\n";
    while (std::cin) {
        try {
            print_heading("ГЛАВНОЕ МЕНЮ");
            const int command = read_int("1. Бинарное дерево поиска (BST<int>)\n"
                                         "2. Множества (OrderedSet<int>)\n"
                                         "3. Демонстрация других типов данных\n\n0. Выход\n> ");
            if (command == 0) { return 0; }
            if (command == 1) { run_container<SearchTree<int>>(); }
            else if (command == 2) { run_container<OrderedSet<int>>(); }
            else if (command == 3) { show_types(); }
            else { std::cout << "Неизвестная команда\n"; }
        } catch (const std::exception& error) {
            if (std::cin.eof()) { return 0; }
            std::cout << "Ошибка: " << error.what() << '\n';
        }
    }
    return 0;
}
