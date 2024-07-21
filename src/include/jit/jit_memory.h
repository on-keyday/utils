/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport_header.h>
#include <view/iovec.h>

namespace futils::jit {

    struct futils_DLL_EXPORT EditableMemory;

    struct futils_DLL_EXPORT ExecutableMemory {
       private:
        view::wvec memory;
        friend struct futils_DLL_EXPORT EditableMemory;
        constexpr ExecutableMemory(view::wvec memory)
            : memory(memory) {}

       public:
        constexpr ExecutableMemory()
            : memory() {}
        ~ExecutableMemory() {
            free();
        }

        constexpr ExecutableMemory(const ExecutableMemory&) = delete;

        constexpr ExecutableMemory(ExecutableMemory&& m)
            : memory(std::exchange(m.memory, view::wvec{})) {}

        ExecutableMemory& operator=(ExecutableMemory&& m) {
            if (this == &m) {
                return *this;
            }
            free();
            memory = std::exchange(m.memory, view::wvec{});
            return *this;
        }

        constexpr bool valid() const {
            return !memory.null();
        }

        void free();

        EditableMemory make_editable();

        constexpr futils::view::rvec get_memory() const {
            return memory;
        }

        template <class R, class... Args>
        constexpr auto as_function(size_t offset = 0) const {
            return reinterpret_cast<R (*)(Args...)>(const_cast<byte*>(memory.data() + offset));
        }
    };

    struct futils_DLL_EXPORT EditableMemory {
       private:
        view::wvec memory;
        friend struct futils_DLL_EXPORT ExecutableMemory;
        constexpr EditableMemory(view::wvec memory)
            : memory(memory) {}

       public:
        constexpr EditableMemory()
            : memory() {}

        ~EditableMemory() {
            free();
        }

        constexpr EditableMemory(const EditableMemory&) = delete;

        constexpr EditableMemory(EditableMemory&& m)
            : memory(std::exchange(m.memory, view::wvec{})) {}

        EditableMemory& operator=(EditableMemory&& m) {
            if (this == &m) {
                return *this;
            }
            free();
            memory = std::exchange(m.memory, view::wvec{});
            return *this;
        }

        static EditableMemory allocate(size_t size);

        constexpr bool valid() const {
            return !memory.null();
        }

        void free();

        constexpr futils::view::wvec get_memory() const {
            return memory;
        }

        ExecutableMemory make_executable();
    };

}  // namespace futils::jit
