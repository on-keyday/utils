/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// make_syntaxc - make syntaxc implementation
// need to link libutils
#pragma once
#include "syntaxc.h"

#include "../../wrap/lite/string.h"
#include "../../wrap/lite/vector.h"
#include "../../wrap/lite/map.h"
#include "../../wrap/lite/smart_ptr.h"

namespace utils {
    namespace syntax {
        template <class String = wrap::string, template <class...> class Vec = wrap::vector, template <class...> class Map = wrap::map>
        wrap::shared_ptr<SyntaxC<String, Vec, Map>> make_syntaxc() {
            return wrap::make_shared<SyntaxC<String, Vec, Map>>();
        }
#if !defined(UTILS_SYNTAX_NO_EXTERN_SYNTAXC)
        extern template struct SyntaxC<wrap::string, wrap::vector, wrap::map>;
#endif
    }  // namespace syntax
}  // namespace  utils
