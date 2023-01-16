/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <deprecated/cnet/mem.h>
#include <wrap/light/smart_ptr.h>
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

            Error mem_write(Stopper stop, CNet* ctx, MemoryIO* io, Buffer<const char>* buf) {
                if (!io->io) {
                    return consterror{"mem: target memory buffer not exists"};
                }
                buf->proced = io->io->write(buf->ptr, buf->size);
                if (buf->size != 0 && buf->proced == 0) {
                    return consterror{"mem: failed to write to buffer"};
                }
                return nil();
            }

            Error mem_read(Stopper stop, CNet* ctx, MemoryIO* io, Buffer<char>* buf) {
                auto lock = io->link.lock();
                if (!lock) {
                    return consterror{"mem: failed to get memory buffer"};
                }
                buf->proced = lock->read(buf->ptr, buf->size);
                return nil();
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
