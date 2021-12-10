/*license*/

// stack - or and ref stack
#pragma once
#include "../make_parser/element.h"

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec>
            struct StackContext {
                size_t index;
                wrap::shared_ptr<Element<String, Vec>> element;
                Reader<String> r;
                Vec<wrap::shared_ptr<Element<String, Vec>>>* vec;
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
                    auto ret = std::move(stack.back());
                    stack.pop_back();
                    return std::move(ret);
                }
            };
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils