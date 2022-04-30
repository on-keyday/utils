/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/mem.h"
#include "../../include/wrap/light/smart_ptr.h"
#include <mutex>

namespace utils {
    namespace cnet {
        namespace mem {
            struct LockedIO {
                MemoryBuffer* b;
                std::mutex mu;

                size_t write(const void* m, size_t s) {
                    std::lock_guard _{mu};
                    return append(b, m, s);
                }

                size_t read(void* m, size_t s) {
                    std::lock_guard _{mu};
                    return remove(b, m, s);
                }

                ~LockedIO() {
                    delete_buffer(b);
                }
            };

            wrap::shared_ptr<LockedIO> make_locked(MemoryBuffer* b) {
                auto lock = wrap::make_shared<LockedIO>();
                lock->b = b;
                return lock;
            }

            struct MemoryIO {
                wrap::shared_ptr<LockedIO> io;
                wrap::weak_ptr<LockedIO> link;
            };

            bool mem_settings(CNet* ctx, MemoryIO* io, std::int64_t key, void* value) {
                if (key == io_pair) {
                    auto p = static_cast<LockPair*>(value);
                    if (p->set) {
                        io->link = p->io;
                    }
                    else {
                        p->io = io->io;
                    }
                }
                else if (key == mem_buffer_set) {
                    auto mbuf = static_cast<MemoryBuffer*>(value);
                    io->io = make_locked(mbuf);
                }
                else {
                    return false;
                }
                return true;
            }

            bool mem_write(CNet* ctx, MemoryIO* io, Buffer<const char>* buf) {
                if (!io->io) {
                    return false;
                }
                buf->proced = io->io->write(buf->ptr, buf->size);
                if (buf->size != 0 && buf->proced == 0) {
                    return false;
                }
                return true;
            }

            bool mem_read(CNet* ctx, MemoryIO* io, Buffer<char>* buf) {
                if (!io->io) {
                    return false;
                }
                auto lock = io->link.lock();
                if (!lock) {
                    return false;
                }
                buf->proced = lock->read(buf->ptr, buf->size);
                return true;
            }

            CNet* STDCALL create_mem() {
                ProtocolSuite<MemoryIO> proto{
                    .write = mem_write,
                    .read = mem_read,
                    .settings_ptr = mem_settings,
                    .deleter = [](MemoryIO* p) { delete p; },
                };
                return cnet::create_cnet(CNetFlag::final_link, new MemoryIO{}, proto);
            }

        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
