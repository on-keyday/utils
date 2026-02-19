/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <comb2/composite/comment.h>
#include <comb2/composite/string.h>
#include <comb2/composite/range.h>
#include <comb2/basic/logic.h>
#include <comb2/basic/group.h>
#include <comb2/lexctx.h>
#include <comb2/seqrange.h>
#include <view/concat.h>
#include <binary/reader.h>
#include <binary/writer.h>

namespace futils::number::hex {
    namespace parser {
        using namespace futils::comb2;
        using ops::operator|, ops::operator&, ops::operator*, ops::operator-, ops::str, ops::eos;
        namespace cps = composite;

        constexpr auto comment = cps::shell_comment;
        constexpr auto hex = cps::hex;
        constexpr auto space = cps::space | cps::tab | cps::eol;

        constexpr auto hex_tag = "hex";

        constexpr auto text = *(comment | space | str(hex_tag, hex)) & eos;

        namespace test {
            constexpr auto check_hex() {
                auto seq = futils::make_ref_seq("1234567890abcdefABCDEF # comment");
                return text(seq, 0, 0) == Status::match;
            }

            static_assert(check_hex());
        }  // namespace test

        template <class Buf>
        struct HexReader : LexContext<> {
            Buf buf;
            void end_string(Status res, auto&& tag, auto&& seq, Pos pos) {
                LexContext::end_string(res, tag, seq, pos);
                if (res == Status::match) {
                    seq_range_to_string(buf, seq, pos);
                }
            }
        };
    }  // namespace parser

    template <class B, class T>
    bool read_hex(B& data, T&& input) {
        auto seq = futils::make_ref_seq(input);
        auto ctx = parser::HexReader<B>{};
        if (auto res = parser::text(seq, ctx, 0); res == parser::Status::match) {
            if (ctx.buf.size() % 2 == 0) {
                for (auto i = 0; i < ctx.buf.size(); i += 2) {
                    futils::byte c = ctx.buf[i];
                    futils::byte d = ctx.buf[i + 1];
                    auto n = futils::number::number_transform[c] << 4 | futils::number::number_transform[d];
                    data.push_back(n);
                }
                return true;
            }
        }
        return false;
    }

    struct remain_buffer {
        bool has_remain = false;
        char buf[1];

        constexpr size_t size() const noexcept {
            return has_remain ? 1 : 0;
        }

        constexpr decltype(auto) begin() const noexcept {
            return buf + 0;
        }

        constexpr decltype(auto) end() const noexcept {
            return buf + size();
        }

        constexpr decltype(auto) operator[](size_t i) const noexcept {
            return buf[i];
        }
    };

    template <class B, class TmpB = B, class T>
    bool read_hex_stream(B& data, T&& new_data, remain_buffer& remain_buffer) {
        auto c = futils::view::make_concat(remain_buffer, new_data);
        auto ctx = parser::HexReader<TmpB>{};
        auto seq = futils::make_ref_seq(c);
        if (auto res = parser::text(seq, ctx, 0); res == parser::Status::match) {
            for (auto i = 0; i < ctx.buf.size() / 2; i++) {
                futils::byte c = ctx.buf[i << 1];
                futils::byte d = ctx.buf[(i << 1) + 1];
                auto n = futils::number::number_transform[c] << 4 | futils::number::number_transform[d];
                data.push_back(n);
            }
            if (ctx.buf.size() % 2) {
                remain_buffer.has_remain = true;
                remain_buffer.buf[0] = ctx.buf[ctx.buf.size() - 1];
            }
            else {
                remain_buffer.has_remain = false;
            }
            return true;
        }
        return false;
    }

    template <class B, class C = byte, class Src = binary::basic_reader<C>&, class TmpB = B>
    struct HexFilter {
        Src src;
        size_t rate = 1024;
        B buffer;
        remain_buffer remain;
        bool decode_error = false;

       private:
        static constexpr bool empty(void* ctx, size_t offset) {
            auto filter = static_cast<HexFilter*>(ctx);
            return filter->buffer.size() == offset && filter->src.empty();
        }

        static constexpr void read(void* ctx, binary::ReadContract<C> c) {
            HexFilter* filter = static_cast<HexFilter*>(ctx);
            if (filter->decode_error) {
                return;
            }
            auto new_buf = c.least_new_buffer_size();
            while (filter->buffer.size() < new_buf) {
                auto best = filter->src.read_best_direct(filter->rate);
                if (!read_hex_stream<B, TmpB>(filter->buffer, best, filter->remain)) {
                    filter->decode_error = true;
                    break;
                }
                if (best.size() < filter->rate) {
                    break;
                }
            }
            c.set_new_buffer(filter->buffer);
        }

        static constexpr void discard(void* ctx, binary::DiscardContract<C> c) {
            HexFilter* filter = static_cast<HexFilter*>(ctx);
            if (filter->decode_error) {
                return;
            }
            auto req = c.require_drop();
            c.direct_drop(req);
            filter->buffer.resize(c.buffer().size());
            c.replace_buffer(filter->buffer);
            filter->src.discard();  // also discard the source
        }

        static constexpr binary::ReadStreamHandler<C> handler{
            .empty = empty,
            .read = read,
            .discard = discard,
        };

       public:
        static constexpr const binary::ReadStreamHandler<C>* get_read_handler() {
            return &handler;
        }
    };

    namespace test {

        inline void test_hex_filter() {
            constexpr auto hex_text = R"(
                # this data is 24 bytes long
                0123456789abcdefABCDEF
                0123456789abcdefABCDEF
            )";
            constexpr auto bad_text = R"(
                # this data is 24 bytes long
                he can still read this
            )";
            struct TmpBuffer {
                byte buffer[1024];
                size_t size_ = 0;
                constexpr size_t size() const noexcept {
                    return size_;
                }

                byte* data() noexcept {
                    return buffer;
                }

                constexpr void push_back(byte c) {
                    buffer[size_++] = c;
                }

                constexpr byte operator[](size_t i) const noexcept {
                    return buffer[i];
                }

                constexpr byte& operator[](size_t i) noexcept {
                    return buffer[i];
                }

                constexpr void resize(size_t s) noexcept {
                    size_ = s;
                }
            };
            binary::reader r{hex_text};
            HexFilter<TmpBuffer> filter{r};
            binary::reader hex_reader{filter.get_read_handler(), &filter};
            auto [_, ok] = hex_reader.read_direct(20);
            if (!ok) {
                throw "error";
            }
            hex_reader.discard();
            hex_reader.read_direct(2);
            if (!hex_reader.empty()) {
                throw "error";
            }
            hex_reader.discard();
            r.reset_buffer(bad_text);
            auto [__, ok2] = hex_reader.read_direct(20);
            if (ok2) {
                throw "error";
            }
            if (!filter.decode_error) {
                throw "error";
            }
        }
    }  // namespace test

}  // namespace futils::number::hex
