/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/kernel.h>
#include <freestd/stdio.h>
#include <freestd/string.h>
#define PROCS_MAX 8
#define PROCESS_STATE_DEAD 0
#define PROCESS_STATE_READY 1
#define PROCESS_STATE_EXIT 2
process process_list[PROCS_MAX];
struct process* current_process;
struct process* idle_process;

extern char __kernel_base[], __free_ram_end[];

struct process* pickup_free_process() {
    for (int i = 0; i < PROCS_MAX; i++) {
        if (process_list[i].state == PROCESS_STATE_DEAD) {
            process_list[i].state = PROCESS_STATE_READY;
            process_list[i].pid = i + 1;
            return &process_list[i];
        }
    }
    return nullptr;
}

struct process* create_process_internal(void (*entry)()) {
    struct process* proc = pickup_free_process();
    if (proc == nullptr) {
        PANIC("Out of process");
    }
    auto sp = (vaddr_t*)&proc->stack[sizeof(proc->stack)];
    *--sp = 0;               // s11
    *--sp = 0;               // s10
    *--sp = 0;               // s9
    *--sp = 0;               // s8
    *--sp = 0;               // s7
    *--sp = 0;               // s6
    *--sp = 0;               // s5
    *--sp = 0;               // s4
    *--sp = 0;               // s3
    *--sp = 0;               // s2
    *--sp = 0;               // s1
    *--sp = 0;               // s0
    *--sp = (vaddr_t)entry;  // ra
    proc->sp = (vaddr_t)sp;
    proc->page_table = (vaddr_t*)page_alloc(1);
    for (paddr_t p = (paddr_t)__kernel_base;
         p < (paddr_t)__free_ram_end; p += PAGE_SIZE) {
        map_page(proc->page_table, p, p, PAGE_R | PAGE_W | PAGE_X);
    }
    return proc;
}

EXTERN_C struct process* create_kernel_process(void (*entry)()) {
    if (!idle_process) {
        PANIC("process not initialized");
    }
    return create_process_internal(entry);
}

__attribute__((naked)) void user_entry() {
    __asm__ volatile("csrw sepc, %0\n" : : "r"(USER_BASE));
    __asm__ volatile("csrw sstatus, %0\n" : : "r"(SSTATUS_SPIE));
    __asm__ volatile("sret\n");
}

void dummy_user_entry() {
    PANIC("reached");
}

EXTERN_C struct process* create_user_process(const void* image, size_t size) {
    if (!idle_process) {
        PANIC("process not initialized");
    }
    auto proc = create_process_internal(user_entry);
    for (vaddr_t vaddr = 0; vaddr < size; vaddr += PAGE_SIZE) {
        paddr_t page = page_alloc(1);
        memcpy((void*)page, (uint8_t*)image + vaddr, PAGE_SIZE);
        map_page(proc->page_table, USER_BASE + vaddr, page, PAGE_R | PAGE_W | PAGE_X | PAGE_U);
    }
    return proc;
}

EXTERN_C void process_init() {
    for (int i = 0; i < PROCS_MAX; i++) {
        process_list[i].state = PROCESS_STATE_DEAD;
    }
    idle_process = create_process_internal(nullptr);
    idle_process->pid = -1;
    current_process = idle_process;
}

EXTERN_C void exit_current_process() {
    current_process->state = PROCESS_STATE_EXIT;
    yield();
    PANIC("unexpected return from exited process");
}

EXTERN_C void yield() {
    if (!idle_process) {
        PANIC("process not initialized");
    }

    struct process* next = idle_process;
    for (int i = 0; i < PROCS_MAX; i++) {
        auto index = (current_process->pid + i) % PROCS_MAX;
        if (index < 0 || index >= PROCS_MAX) {
            continue;  // idle process
        }
        auto proc = &process_list[index];
        if (proc->state == PROCESS_STATE_READY &&
            proc->pid > 0) {
            next = proc;
            break;
        }
    }
    if (next == current_process) {
        return;  // no need to switch
    }
    auto prev = current_process;
    current_process = next;

    __asm__ volatile(
        "sfence.vma\n"
        "csrw satp, %0\n"
        "sfence.vma\n"
        : : "r"(SATP_SV32 | (((vaddr_t)next->page_table) / PAGE_SIZE)));
    // save kernel stack pointer
    __asm__ volatile("csrw sscratch, %0\n" : : "r"(&next->stack[sizeof(next->stack)]));

    switch_context(&prev->sp, &next->sp);
}
