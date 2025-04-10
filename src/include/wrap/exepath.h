/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport_header.h>
#include <helper/pushbacker.h>
#include <wrap/light/string.h>

namespace futils {
    namespace wrap {
        futils_DLL_EXPORT void STDCALL get_exepath(helper::IPushBacker<> pb);

        template <class Out = wrap::string>
        Out get_exepath() {
            Out out;
            get_exepath(out);
            return out;
        }
    }  // namespace wrap
}  // namespace futils
