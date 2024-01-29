/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/freestd_macro.h>
#ifdef __cplusplus
extern "C" {
#endif

void FUTILS_FREESTD_STDC(exit)(int status);

#ifdef __cplusplus
}
namespace futils::freestd {
    using ::FUTILS_FREESTD_STDC(exit);
}  // namespace futils::freestd
#endif
