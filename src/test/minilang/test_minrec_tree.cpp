/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <minilang/minl.h>
#include <minilang/minlfunc.h>
#include <minilang/minlctrl.h>
#include <minilang/minlprog.h>
#include <minilang/minlstat.h>

void test_minrec_tree() {
    namespace mi = utils::minilang;
    constexpr auto text = "assign :=2,object.com.alpha.beta.ganma.epsiron";
    auto seq = utils::make_cpy_seq(mi::CView(text, utils::strlen(text)));
    constexpr auto stat = mi::stat_default;
    constexpr auto stats = mi::make_stats(
        mi::make_funcexpr_primitive(stat, mi::typesig_default), mi::until_eof_or_not_matched(stat));
    auto tree = stats(seq, mi::null_errc);
    assert(tree);
}

int main() {
    test_minrec_tree();
}
