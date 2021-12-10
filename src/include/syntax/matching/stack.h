/*license*/

// stack - or and ref stack
#pragma once
#include "../make_parser/element.h"
#include <cassert>

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec>
            struct StackContext {
                size_t index = 0;
                wrap::shared_ptr<Element<String, Vec>> element;
                Reader<String> r;
                Vec<wrap::shared_ptr<Element<String, Vec>>>* vec = nullptr;
                bool on_repeat = false;
            };

            template <class String, template <class...> class Vec>
            struct Stack {
                Vec<StackContext<String, Vec>> stack;

                void push(StackContext<String, Vec>&& v) {
                    stack.push_back(std::move(v));
                }

                StackContext<String, Vec> pop() {
                    assert(stack.size());
                    auto ret = std::move(current());
                    stack.pop_back();
                    return std::move(ret);
                }

                StackContext<String, Vec>& current() {
                    return stack[stack.size() - 1];
                }

                bool end_group() {
                    return current().vec == nullptr || current().index >= current().vec->size();
                }
            };
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
