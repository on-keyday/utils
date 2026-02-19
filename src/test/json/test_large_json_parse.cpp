/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <json/constructor.h>
#include <file/file_view.h>
#include <json/json_export.h>
#include <json/to_string.h>
#include <json/parse.h>
#include <testutil/timer.h>
#include <wrap/cout.h>
#include <vector>
#include <numeric>

struct Record {
    using dur_t = std::chrono::milliseconds;
    dur_t init_time;
    dur_t parser_time;
    dur_t to_string_time;
    dur_t parse_time;
    dur_t total_time;
    size_t max_stack_size;
    size_t max_stack_size2;
};

struct DecoderObserver {
    std::uint64_t node_count = 0;
    std::uint64_t scope_count = 0;
    bool next_int_is_node_count = false;
    bool next_int_is_scope_count = false;
    bool next_array_is_node = false;
    bool next_array_is_scope = false;
    void observe(futils::json::ElementType t, auto& s) {
        if (t == futils::json::ElementType::key_string) {
            if (s.size() != 4) {
                return;
            }
            if (s.back().get_holder().init_as_string() == "node_count") {
                next_int_is_node_count = true;
            }
            else if (s.back().get_holder().init_as_string() == "scope_count") {
                next_int_is_scope_count = true;
            }
            else if (s.back().get_holder().init_as_string() == "node") {
                next_array_is_node = true;
            }
            else if (s.back().get_holder().init_as_string() == "scope") {
                next_array_is_scope = true;
            }
        }
        else if (t == futils::json::ElementType::integer) {
            if (next_int_is_node_count) {
                node_count = *s.back().get_holder().as_numi();
                next_int_is_node_count = false;
            }
            else if (next_int_is_scope_count) {
                scope_count = *s.back().get_holder().as_numi();
                next_int_is_scope_count = false;
            }
        }
        else if (t == futils::json::ElementType::array) {
            if (next_array_is_node) {
                next_array_is_node = false;
                s.back().get_holder().init_as_array().reserve(node_count);
            }
            else if (next_array_is_scope) {
                next_array_is_scope = false;
                s.back().get_holder().init_as_array().reserve(scope_count);
            }
        }
    }
};

int main() {
    namespace json = futils::json;
    constexpr int iterations = 10;
    std::vector<Record> records;
    auto& cout = futils::wrap::cout_wrap();
    for (int i = 0; i < iterations; ++i) {
        cout << "Iteration: " << i + 1 << "\n";
        futils::test::Timer t;
        futils::file::View f;
        f.open("./src/test/json/sample.json").value();
        auto init_time = t.next_step();
        json::BytesLikeReader<futils::view::rvec> r{futils::view::rvec(f)};
        r.size = r.bytes.size();
        futils::json::JSONConstructor<json::JSON, json::StaticStack<15, json::JSON>, DecoderObserver> c;
        json::GenericConstructor<decltype(r)&, std::vector<json::ParseStateDetail>, decltype(c)&> g{r, c};
        json::Parser<decltype(g)&> p{g};
        p.skip_space();
        auto result = p.parse();
        auto s = p.state.state();
        assert(result == json::ParseResult::end);
        auto js = std::move(c.stack.back());
        auto parser_time = t.next_step();
        auto out = json::to_string<std::string>(js);
        auto to_string_time = t.next_step();
        if (i == 0) {
            cout << out << "\n";
        }
        // auto js2 = json::parse<json::JSON>(futils::view::rvec(f));
        auto parse_time = t.next_step();

        records.push_back({init_time,
                           parser_time,
                           to_string_time,
                           parse_time,
                           init_time + parser_time + to_string_time + parse_time,
                           c.max_stack_size,
                           g.max_stack_size});
    }

    Record avg = {};
    for (const auto& rec : records) {
        avg.init_time += rec.init_time;
        avg.parser_time += rec.parser_time;
        avg.to_string_time += rec.to_string_time;
        avg.parse_time += rec.parse_time;
        avg.total_time += rec.total_time;
        avg.max_stack_size += rec.max_stack_size;
        avg.max_stack_size2 += rec.max_stack_size2;
    }

    avg.init_time /= iterations;
    avg.parser_time /= iterations;
    avg.to_string_time /= iterations;
    avg.parse_time /= iterations;
    avg.total_time /= iterations;
    avg.max_stack_size /= iterations;
    avg.max_stack_size2 /= iterations;

    cout << "Average init time: " << avg.init_time.count() << "ms\n";
    cout << "Average parser time: " << avg.parser_time.count() << "ms\n";
    cout << "Average to_string time: " << avg.to_string_time.count() << "ms\n";
    cout << "Average parse time: " << avg.parse_time.count() << "ms\n";
    cout << "Average total time: " << avg.total_time.count() << "ms\n";
    cout << "Average max stack size: " << avg.max_stack_size << "\n";
    cout << "Average max stack size2: " << avg.max_stack_size2 << "\n";
}