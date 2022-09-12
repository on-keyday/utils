/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "parser.h"
#include "middle.h"
#include <helper/line_pos.h>
utils::Sequencer<const char*>* srcptr;
namespace minlc {
    void print_node(const std::shared_ptr<utils::minilang::MinNode>& node) {
        std::string w;
        srcptr->rptr = node->pos.begin;
        const auto len = node->pos.end - node->pos.begin;
        utils::helper::write_src_loc(w, *srcptr, 1);
        printe(w);
    }
}  // namespace minlc

namespace mc = minlc;

bool test_ok() {
    constexpr auto test = R"(
        mod test
        import (
            "std/fmt"
        )

        type Test struct {
            A string
            B int
        }

        fn voidfn(arg int,type Arg,arg2 char) () {

        }

        linkage "C" {

        fn printf(format *const char,...) int

        fn main(argc mut int,argv mut **char) int {
            /*argc,argv = argc,argv
            arg1 := argv[1]
            if arg1 != "" {
                fmt.Printf("%s",arg1)
            }
            */
            fnval := fn() int {
                return type<int> + type<int>
            }
            fnval()
            C.stdio.printf("%s","Hello World")
            return 0
        }

        }
    )";
    auto seq = utils::make_cpy_seq(test);
    srcptr = &seq;
    minlc::ErrC errc{};
    auto ok = mc::parse(seq, errc);
    if (!ok.first || !ok.second) {
        mc::printe("parse failed");
        return false;
    }
    mc::middle::M m;
    auto prog = mc::middle::minl_to_program(m, ok.second);
    for (auto& object : m.object_mapping) {
        minlc::printe(object.first->str);
        minlc::print_node(object.first);
    }
    return true;
}

int main(int argc, char** argv) {
    if (!test_ok()) {
        mc::printe("test code failed. library has bugs");
        return -1;
    }
}
