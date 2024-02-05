/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stdint.h>
#include <freestd/stddef.h>
#ifdef __cplusplus
extern "C" {
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

struct sbiret {
    long error;
    long value;
};

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid);

void futils_kernel_panic(const char* file, int line, const char* fmt, ...);

#define PANIC(...) futils_kernel_panic(__FILE__, __LINE__, __VA_ARGS__)

void kernel_exception_entry(void);

typedef uint32_t paddr_t;
typedef uint32_t vaddr_t;

#define PAGE_SIZE 4096

EXTERN_C paddr_t page_alloc(size_t n_page);

struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                     \
    (([] {                                                \
        unsigned long __tmp;                              \
        __asm__ volatile("csrr %0, " #reg : "=r"(__tmp)); \
        return __tmp;                                     \
    })())

#define WRITE_CSR(reg, value)                               \
    do {                                                    \
        uint32_t __tmp = (value);                           \
        __asm__ volatile("csrw " #reg ", %0" ::"r"(__tmp)); \
    } while (0)

struct process {
    int pid;
    int state;
    vaddr_t sp;
    vaddr_t* page_table;
    uint8_t stack[8192];
};

void yield();
void switch_context(vaddr_t* prev_sp, vaddr_t* next_sp);
void process_init();
struct process* create_kernel_process(void (*entry)());
struct process* create_user_process(const void* image, size_t size);

#define SATP_SV32 (1 << 31)
#define PAGE_V (1 << 0)
#define PAGE_R (1 << 1)
#define PAGE_W (1 << 2)
#define PAGE_X (1 << 3)
#define PAGE_U (1 << 4)
void map_page(uint32_t* table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags);

#define USER_BASE 0x1000000

#define SSTATUS_SPIE (1 << 5)

#define SCAUSE_ECALL 8

void syscall_handler(struct trap_frame* f);

void exit_current_process();

#ifdef __cplusplus
namespace futils::low::kernel {
    using ::sbi_call;
    using ::sbiret;
}  // namespace futils::low::kernel
}
#endif
