#include <tree_lab/search_tree.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

using BenchmarkClock = std::chrono::steady_clock;

struct Options {
    std::size_t max_n = 1000000;
    std::size_t repeats = 3;
    std::size_t sorted_max = 20000;
    std::size_t memory_mib = 512;
    std::uint32_t seed = 42;
    double max_seconds = 5.0;
    std::string output = "results/benchmark.csv";
};

std::size_t parse_size(const std::string& text) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("Expected nonnegative integer: " + text);
    }
    std::size_t used = 0;
    const auto value = std::stoull(text, &used);
    if (used != text.size() || value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Integer out of range");
    }
    return static_cast<std::size_t>(value);
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") {
            std::cout << "tree_lab_benchmark [--max-n 1000000] [--repeats 3] [--seed 42]\n"
                         "  [--sorted-max 20000] [--memory-mib 512] [--max-seconds 5]\n"
                         "  [--output results/benchmark.csv]\n"
                         "Sizes: 10000, 20000, 100000, ... plus max-n (up to 100000000).\n"
                         "Time limit applies separately to each batch; skipped/partial rows are marked.\n";
            std::exit(0);
        }
        if (++i >= argc) { throw std::invalid_argument("Missing value for " + key); }
        const std::string value = argv[i];
        if (key == "--output") { options.output = value; }
        else if (key == "--max-seconds") { options.max_seconds = ValueCodec<double>::decode(value); }
        else if (key == "--max-n") { options.max_n = parse_size(value); }
        else if (key == "--repeats") { options.repeats = parse_size(value); }
        else if (key == "--sorted-max") { options.sorted_max = parse_size(value); }
        else if (key == "--memory-mib") { options.memory_mib = parse_size(value); }
        else if (key == "--seed") {
            const auto seed = parse_size(value);
            if (seed > std::numeric_limits<std::uint32_t>::max()) { throw std::invalid_argument("Seed too large"); }
            options.seed = static_cast<std::uint32_t>(seed);
        } else { throw std::invalid_argument("Unknown option: " + key); }
    }
    if (options.max_n < 10000 || options.max_n > 100000000 || options.repeats == 0 ||
        options.repeats > 100 || options.memory_mib == 0 || options.memory_mib > 1048576 ||
        options.max_seconds <= 0 || options.max_seconds > 3600 || options.sorted_max > 100000000) {
        throw std::invalid_argument("Invalid limits (max-n: 10000..100000000, repeats: 1..100)");
    }
    return options;
}

struct Measurement {
    std::size_t operations = 0;
    double seconds = 0;
    std::string status = "ok";
};

template <typename Function>
Measurement measure(std::size_t count, double max_seconds, Function&& function) {
    Measurement result;
    const auto start = BenchmarkClock::now();
    for (std::size_t i = 0; i < count; ++i) {
        function(i);
        ++result.operations;
        // One clock read per 1024 operations keeps measurement overhead small.
        if (result.operations % 1024 == 0) {
            result.seconds = std::chrono::duration<double>(BenchmarkClock::now() - start).count();
            if (result.seconds >= max_seconds && result.operations < count) {
                result.status = "timeout";
                break;
            }
        }
    }
    result.seconds = std::chrono::duration<double>(BenchmarkClock::now() - start).count();
    return result;
}

void write_row(std::ostream& out, const Options& options, std::size_t n, const char* order,
               std::size_t repeat, std::uint32_t seed, const char* operation,
               const Measurement& measurement) {
#ifdef NDEBUG
    constexpr const char* build = "release";
#else
    constexpr const char* build = "debug";
#endif
#ifdef __clang__
    const char* compiler = "Clang " __clang_version__;
#elif defined(__GNUC__)
    const char* compiler = "GCC " __VERSION__;
#else
    const char* compiler = "other";
#endif
    out << n << ',' << order << ',' << repeat << ',' << seed << ',' << operation << ','
        << measurement.operations << ',' << std::setprecision(10) << measurement.seconds << ','
        << measurement.status << ',' << options.max_n << ',' << options.repeats << ','
        << options.sorted_max << ',' << options.memory_mib << ',' << options.max_seconds << ','
        << std::quoted(compiler) << ',' << build << '\n';
    out.flush();
    if (!out) { throw std::runtime_error("Cannot write CSV"); }
}

void run_experiment(std::ostream& out, const Options& options, std::size_t n,
                    bool sorted, std::size_t repeat) {
    const char* order = sorted ? "sorted" : "random";
    const auto seed = options.seed + static_cast<std::uint32_t>(repeat);
    std::string skip;
    // Conservative estimate: shared node, control block, allocator rounding and input array: 96 B/key.
    // It is a planning limit, not a process RSS guarantee.
    if (n > options.memory_mib * 1024ULL * 1024ULL / 96ULL) { skip = "skipped_memory"; }
    else if (sorted && n > options.sorted_max) { skip = "skipped_sorted_limit"; }
    if (!skip.empty()) {
        for (const char* operation : {"insert", "find", "erase"}) {
            write_row(out, options, n, order, repeat, seed, operation, {0, 0, skip});
        }
        return;
    }
    std::cout << "n=" << n << " order=" << order << " repeat=" << repeat << std::endl;
    DynamicArray<int> values;
    SearchTree<int> tree;
    const char* current_operation = "insert";
    try {
        values.resize(n);
        std::iota(values.begin(), values.end(), 0);
        std::mt19937 generator(seed);
        if (!sorted) { std::shuffle(values.begin(), values.end(), generator); }
        auto inserted = measure(n, options.max_seconds, [&](std::size_t i) { tree.insert(values[i]); });
        write_row(out, options, n, order, repeat, seed, "insert", inserted);
        if (inserted.status != "ok") {
            for (const char* operation : {"find", "erase"}) {
                write_row(out, options, n, order, repeat, seed, operation, {0, 0, "skipped_incomplete_insert"});
            }
            return;
        }
        // Both search and erase use a shuffled permutation, including the sorted tree.
        std::shuffle(values.begin(), values.end(), generator);
        current_operation = "find";
        std::size_t hits = 0;
        auto found = measure(n, options.max_seconds, [&](std::size_t i) { hits += tree.contains(values[i]); });
        if (hits != found.operations) { throw std::runtime_error("Search validation failed"); }
        write_row(out, options, n, order, repeat, seed, "find", found);
        current_operation = "erase";
        std::size_t erased_count = 0;
        auto erased = measure(n, options.max_seconds, [&](std::size_t i) { erased_count += tree.erase(values[i]); });
        if (erased_count != erased.operations || tree.size() != n - erased.operations) {
            throw std::runtime_error("Erase validation failed");
        }
        write_row(out, options, n, order, repeat, seed, "erase", erased);
    } catch (const std::bad_alloc&) {
        bool remaining = false;
        for (const char* operation : {"insert", "find", "erase"}) {
            remaining = remaining || std::string(operation) == current_operation;
            if (remaining) {
                write_row(out, options, n, order, repeat, seed, operation, {0, 0, "allocation_failed"});
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto parent = std::filesystem::path(options.output).parent_path();
        if (!parent.empty()) { std::filesystem::create_directories(parent); }
        std::ofstream out(options.output);
        if (!out) { throw std::runtime_error("Cannot open output file"); }
        out << "n,order,repeat,seed,operation,operations,seconds,status,max_n,repeats,sorted_max,memory_mib,max_seconds,compiler,build\n";
        DynamicArray<std::size_t> sizes{10000};
        if (options.max_n >= 20000) { sizes.push_back(20000); }
        for (std::size_t n = 100000; n <= options.max_n; n *= 10) { sizes.push_back(n); }
        if (sizes.back() != options.max_n) { sizes.push_back(options.max_n); }
        for (auto n : sizes) {
            for (bool sorted : {false, true}) {
                for (std::size_t repeat = 0; repeat < options.repeats; ++repeat) {
                    run_experiment(out, options, n, sorted, repeat);
                }
            }
        }
        std::cout << "CSV: " << options.output << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
