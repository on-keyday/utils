/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/kernel.h>
#include <low/kernel/user.h>
#include <freestd/stdio.h>

void syscall_handler(struct trap_frame* f) {
    switch (f->a3) {
        case SYS_putchar: {
            f->a0 = putchar(f->a0);
            break;
        }
        case SYS_exit: {
            exit_current_process();
            // should not reach here
            break;
        }
        case SYS_getchar: {
            auto non_block = f->a0;
            while (1) {
                auto ret = getchar();
                if (ret < 0) {
                    if (non_block == 1) {
                        f->a0 = -1;
                        return;
                    }
                    else if (non_block) {
                        non_block--;
                    }
                    yield();
                }
                else {
                    f->a0 = ret;
                    return;
                }
            }
        }
        default: {
            PANIC("unknown syscall: %d", f->a3);
        }
    }
}
