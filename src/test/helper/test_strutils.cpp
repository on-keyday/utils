/*license*/

#include "../../include/helper/strutil.h"

constexpr bool test_endswith() {
    return utils::helper::ends_with("Hey Jude", " Jude");
}

void test_strutils() {
    constexpr bool result1 = test_endswith();
    constexpr bool result2 = utils::helper::sandwiched("(hello)", "(", ")");
}
