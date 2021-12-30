/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include "../../helper/deref.h"

namespace utils::platform::windows {
    struct Complete {
       private:
        struct interface__ {
            virtual void operator()(size_t size) = 0;

            virtual ~interface__() {}
        };

        template <class T__>
        struct implements__ : interface__ {
            T__ t_holder_;

            template <class V__>
            implements__(V__&& args)
                : t_holder_(std::forward<V__>(args)) {}

            void operator()(size_t size) override {
                auto t_ptr_ = utils::helper::deref(this->t_holder_);
                if (!t_ptr_) {
                    return (void)0;
                }
                return (*t_ptr_)(size);
            }
        };

        interface__* iface = nullptr;

       public:
        constexpr Complete() {}

        constexpr Complete(std::nullptr_t) {}

        template <class T__>
        Complete(T__&& t) {
            if (!utils::helper::deref(t)) {
                return;
            }
            iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
        }

        Complete(Complete&& in) {
            iface = in.iface;
            in.iface = nullptr;
        }

        Complete& operator=(Complete&& in) {
            delete iface;
            iface = in.iface;
            in.iface = nullptr;
            return *this;
        }

        explicit operator bool() const {
            return iface != nullptr;
        }

        ~Complete() {
            delete iface;
        }

        void operator()(size_t size) {
            return iface ? iface->operator()(size) : (void)0;
        }
    };

}  // namespace utils::platform::windows
