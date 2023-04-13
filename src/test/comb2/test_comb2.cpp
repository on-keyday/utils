#include <comb2/composite/number.h>
#include <comb2/basic/unicode.h>
#include <comb2/composite/string.h>
#include <comb2/basic/group.h>

int main() {
    utils::comb2::ops::test::check_unicode();
    constexpr auto c = utils::comb2::ops::str("a", utils::comb2::ops::lit('A'));
}