/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


typedef struct VStack_tag {
    unsigned char* hi;
    unsigned char* lo;
    size_t sp;
    void (*cur)(VStack_tag*);
} VStack;

struct stack_sys {
    int vstack_func1_block0_b_4524fac3dbefc4b4 = 2;
    int vstack_func1_block0_a_4524fdc3dbefc9cd = 1;
};

struct argument_sys {
    int vstack_func0_c_4cd7205b3cb939b8;
    int vstack_func0_b_4cd7215b3cb93b6b;
    int vstack_func0_a_4cd7225b3cb93d1e;
    int ret_space;
    void (*ret)(VStack*);
};
void vstack_add_b95b8812988e3f8f_0(VStack* vstack);
void vstack_main_cb7f6bb82e9817d1_0(VStack* vstack);
void vstack_main_cb7f6bb82e9817d1_1(VStack* vstack);

void vstack_add_b95b8812988e3f8f_0(VStack* vstack) {
    ((argument_sys*)(vstack->hi - vstack->sp - sizeof(argument_sys)))->ret_space =
        ((argument_sys*)(vstack->hi - vstack->sp - sizeof(argument_sys)))->vstack_func0_a_4cd7225b3cb93d1e +
        ((argument_sys*)(vstack->hi - vstack->sp - sizeof(argument_sys)))->vstack_func0_b_4cd7215b3cb93b6b +
        ((argument_sys*)(vstack->hi - vstack->sp - sizeof(argument_sys)))->vstack_func0_c_4cd7205b3cb939b8;
    vstack->cur = ((argument_sys*)(vstack->hi - vstack->sp - sizeof(argument_sys)))->ret;
}

void vstack_main_cb7f6bb82e9817d1_0(VStack* vstack) {
    ((stack_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys)))->vstack_func1_block0_a_4524fdc3dbefc9cd = 1;
    ((stack_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys)))->vstack_func1_block0_b_4524fac3dbefc4b4 = 2;
    ((argument_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys) - sizeof(argument_sys)))
        ->vstack_func0_a_4cd7225b3cb93d1e =
        ((stack_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys)))
            ->vstack_func1_block0_a_4524fdc3dbefc9cd;
    ((argument_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys) - sizeof(argument_sys)))
        ->vstack_func0_b_4cd7215b3cb93b6b =
        ((stack_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys)))
            ->vstack_func1_block0_b_4524fac3dbefc4b4;
    ((argument_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys) - sizeof(argument_sys)))
        ->vstack_func0_b_4cd7215b3cb93b6b = 3;
    ((argument_sys*)(vstack->hi - vstack->sp - sizeof(stack_sys) - sizeof(argument_sys)))
        ->ret = vstack_main_cb7f6bb82e9817d1_1;
    vstack->sp += sizeof(stack_sys);
    vstack->cur = vstack_add_b95b8812988e3f8f_0;
}

void vstack_main_cb7f6bb82e9817d1_1(VStack* vstack) {
    vstack->sp -= sizeof(stack_sys);
}
