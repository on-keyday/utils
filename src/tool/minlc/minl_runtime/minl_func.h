/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_func - minilang function base
#pragma once
#include <cstdlib>

// minlrt is minilang runtime base code
// for the future, minlc will generate runtime code in itself.
// these are stub
namespace minlrt {

    /* for example if
        fn stub_test(argc int,argv **char) fn() bool {
            return fn() bool {
                return argv[argc]==null;
            }
        }

        fn invoke(argc int,argv **char) int {
            fnval:=stub_test(argc,argv)
            result := fnval()
            return result
        }
    would be transpiled*/

    // common in same signature function type
    typedef struct {
        bool (*fn)(void*);
        void (*gc)(void*, unsigned int*);
        unsigned int ref = 0;
    } minl_fn;

    // each captured function type
    typedef struct {
        bool (*fn)(void*);
        void (*gc)(void*, unsigned int*);
        unsigned int ref = 0;
        int argc;
        char** argv;
    } minl_captured;

    inline bool inner_stub_test(void* captured) {
        minl_captured* data = (minl_captured*)captured;
        return data->argv[data->argc] == (char*)0;
    }

    inline void minl_fn_free(void* p, unsigned int* ref) {
        if (0 == --*ref) {
            free(p);
        }
    }

    inline minl_fn* capture_inner_stub_test(int argc, char** argv) {
        minl_captured* captured = (minl_captured*)malloc(sizeof(minl_captured));
        captured->argc = argc;
        captured->argv = argv;
        captured->fn = inner_stub_test;
        captured->gc = minl_fn_free;
        captured->ref = 1;
        return (minl_fn*)captured;
    }

    inline minl_fn* stub_test(int argc, char** argv) {
        return capture_inner_stub_test(argc, argv);
    }

    inline int invoke(int argc, char** argv) {
        minl_fn* fnval = stub_test(argc, argv);
        bool result = fnval->fn(fnval);
        fnval->gc(fnval, &fnval->ref);
        return result;
    }
}  // namespace minlrt
