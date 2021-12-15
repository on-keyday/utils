/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// option_result - option parsed result
#pragma once

#include "option.h"

#include "../wrap/lite/smart_ptr.h"

namespace utils {
    namespace cmdline {

        template <class String, template <class...> class Vec>
        struct OptionResult {
            Option<String, Vec>* base = nullptr;
            wrap::shared_ptr<Option<String, Vec>> value;
        };

        template <class String, template <class...> class Vec, template <class...> class MultiMap>
        struct OptionResultSet {
            MultiMap<String, OptionResult<String, Vec>> result;

            void emplace(const String& v, OptionResult<String, Vec>&& in) {
                result.emplace(v, std::move(in));
            }

            bool exists(const String& name) {
                return result.find(name) != result.end();
            }
        };

    }  // namespace cmdline
}  // namespace utils
