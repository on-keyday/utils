/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>
#include <binary/writer.h>
#include "file.h"

namespace futils::file {

    namespace internal {
        template <class F>
        concept as_file =
            std::is_same_v<F, File> || std::is_same_v<F, File&> ||
            std::is_same_v<F, const File&>;

        static_assert(as_file<File> && as_file<File&>);

    }  // namespace internal

    template <class B, internal::as_file F = const File&>
    struct FileStream {
        F file;
        B buffer;
        FileError error;
        bool eof = false;

       private:
        static bool file_empty(void* ctx, size_t index) {
            auto self = static_cast<FileStream*>(ctx);
            const File& f = self->file;
            auto s = f.stat();
            if (!s) {
                self->error = s.error();
                return true;
            }
            if (s->mode.pipe() || s->mode.terminal() || s->mode.socket()) {
                return self->eof;
            }
            return self->eof || index >= s->size;
        }

        static void file_read(void* ctx, binary::ReadContract<byte> c) {
            auto req = c.least_requested();
            auto cur = c.buffer().size();
            auto next = c.least_new_buffer_size();
            auto self = static_cast<FileStream*>(ctx);
            const File& f = self->file;
            self->buffer.resize(next);
            auto buffer = view::wvec(self->buffer).substr(cur, req);
            auto read = f.read_file(buffer);
            if (!read || read->empty()) {
                self->eof = true;
                self->error = read.error();
                return;
            }
            self->buffer.resize(cur + read->size());
            c.set_new_buffer(self->buffer);
        }

        static void increase_buffer(void* ctx, binary::WriteContract<byte> c) {
            auto self = static_cast<FileStream*>(ctx);
            auto req = c.least_requested();
            auto cur = c.buffer().size();
            auto next = c.least_new_buffer_size();
            self->buffer.resize(next);
            c.set_new_buffer(self->buffer);
        }

        static void file_commit(void* ctx, binary::CommitContract<byte> c) {
            auto buf = c.buffer();
            auto to_commit = buf.substr(0, c.require_drop());
            auto self = static_cast<FileStream*>(ctx);
            auto written = self->file.write_file(to_commit);
            if (!written) {
                self->error = written.error();
                self->eof = true;
                return;
            }
            if (buf.size() == c.require_drop()) {
                c.set_new_buffer(self->buffer, c.require_drop());
                return;
            }
            c.direct_drop(c.require_drop());
            self->buffer.resize(c.buffer().size());
            c.replace_buffer(self->buffer);
        }

        static void finalize(void* ctx, binary::CommitContract<byte> c) {
            file_commit(ctx, c);
        }

        static void discard_buffer(void* ctx, binary::DiscardContract<byte> c) {
            auto self = static_cast<FileStream*>(ctx);
            c.direct_drop(c.require_drop());
            self->buffer.resize(c.buffer().size());
            c.replace_buffer(self->buffer);
        }

        static constexpr binary::ReadStreamHandler<byte> read_handler = {
            .empty = file_empty,
            .read = file_read,
            .discard = discard_buffer,
        };

        static constexpr binary::WriteStreamHandler<byte> write_handler = {
            .full = [](void* self, size_t) {
                auto fs = static_cast<FileStream*>(self);
                return fs->eof;
            },
            .write = increase_buffer,
            .commit = file_commit,
            .finalize = finalize,
        };

       public:
        static auto get_read_handler() {
            return &read_handler;
        }

        static auto get_write_handler() {
            return &write_handler;
        }
    };
}  // namespace futils::file
