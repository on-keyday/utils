/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// make_syntaxc - make syntaxc implementation
// need to link libutils
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "syntaxc.h"

#include "../../wrap/light/string.h"
#include "../../wrap/light/vector.h"
#include "../../wrap/light/map.h"
#include "../../wrap/light/smart_ptr.h"

namespace utils {
    namespace syntax {
        template <class String = wrap::string, template <class...> class Vec = wrap::vector, template <class...> class Map = wrap::map>
        wrap::shared_ptr<SyntaxC<String, Vec, Map>> make_syntaxc() {
            return wrap::make_shared<SyntaxC<String, Vec, Map>>();
        }
        template <class String = wrap::string, template <class...> class Vec = wrap::vector>
        tknz::Tokenizer<String, Vec> make_tokenizer() {
            return tknz::Tokenizer<String, Vec>{};
        }

        template <class T, class String, template <class...> class Vec, class... Ctx>
        wrap::shared_ptr<tknz::Token<String>> tokenize_and_merge(const char*& err, Sequencer<T>& input, tknz::Tokenizer<String, Vec>& t, Ctx&&... ctx) {
            wrap::shared_ptr<tknz::Token<String>> ret;
            if (!t.tokenize(input, ret)) {
                return nullptr;
            }
            if (!tknz::merge(err, ret, ctx...)) {
                return nullptr;
            }
            return ret;
        }

        template <class Define, class Input, class String = wrap::string, template <class...> class Vec = wrap::vector, template <class...> class Map = wrap::map, class... Ctx>
        wrap::shared_ptr<tknz::Token<String>> default_parse(wrap::shared_ptr<SyntaxC<String, Vec, Map>>& c, Sequencer<Define>& def, Sequencer<Input>& input, Ctx&&... ctx) {
            if (!c) {
                return nullptr;
            }
            auto t = make_tokenizer<String, Vec>();
            auto m = c->make_tokenizer(def, t);
            if (!m) {
                return nullptr;
            }
            const char* err = nullptr;
            auto ret = tokenize_and_merge(err, input, t, ctx...);
            if (!ret) {
                c->error() << err;
            }
            return ret;
        }

        template <class String>
        Reader<String> make_reader(wrap::shared_ptr<tknz::Token<String>>& tok) {
            return Reader<String>(tok);
        }

#if !defined(UTILS_SYNTAX_NO_EXTERN_SYNTAXC)
        extern
#endif
            template struct DLL SyntaxC<wrap::string, wrap::vector, wrap::map>;
    }  // namespace syntax
}  // namespace  utils
