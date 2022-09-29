/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// native_socket - native socket interface
#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <memory>
#include "dll/dllh.h"
#include "errcode.h"
#include "heap.h"

namespace utils {
    namespace dnet {

        constexpr auto async_reserved = 0xf93f4928;
        enum AsyncMethod {
            am_recv,
            am_recvfrom,
            am_accept,
            am_connect,
        };

        struct AsyncHead {
            const std::uint32_t reserved = async_reserved;
            // mode represents async operation method which is processing now
            AsyncMethod method;
#ifdef _WIN32
            using get_ptrs_t =
                void (*)(AsyncHead* head, void** overlapped, void** wsbuf, int* bufcount,
                         void** sockfrom, int** fromlen, void** flags, std::uintptr_t** tmpsock);
            // get_ptrs gets object pointers
            // this function is windows spec
            get_ptrs_t get_ptrs;
#endif
            // start notifys operation start
            // if you use object as shared_ptr
            // you should increment count here
            void (*start)(AsyncHead* head, std::uintptr_t sock);
            // completion handle io completion
            // on windows, len contains bytes received
            // on linux, len is events bits
            // you should do operation in short time
            // to prevent blocking at wait_event
            // if you use object as shared_ptr
            // you should decrement count here
            void (*completion)(AsyncHead* head, size_t len);
            // canceled is called when get_ptrs is called and io does not start
            // if you use object as shared_ptr
            // you should decrement count here
            void (*canceled)(AsyncHead* head);
            // handling is current handling socket
            // you shouldn't use this; this is internal parameter
            std::uintptr_t handling;
        };

        // wait_event waits io completion until time passed
        // if event is processed function returns number of event
        // otherwise returns 0
        dnet_dll_export(int) wait_event(std::uint32_t time);

        enum MTUConfig {
            mtu_default,  // same as IP_PMTUDISC_WANT
            mtu_enable,   // same as IP_PMTUDISC_DO
            mtu_disable,  // same as IP_PMTUDISC_DONT
            mtu_ignore,   // same as IP_PMTUDISC_PROBE
        };

        // Socket is wrappper class of native socket
        struct dnet_class_export Socket {
            ~Socket();
            constexpr Socket(Socket&& sock)
                : sock(std::exchange(sock.sock, ~0)),
                  err(std::exchange(sock.err, 0)),
                  opt(std::exchange(sock.opt, nullptr)),
                  gc_(std::exchange(sock.gc_, nullptr)),
                  prevred(std::exchange(sock.prevred, 0)) {}
            Socket& operator=(Socket&& sock) {
                if (this == &sock) {
                    return *this;
                }
                this->~Socket();
                this->sock = std::exchange(sock.sock, ~0);
                err = std::exchange(sock.err, 0);
                opt = std::exchange(sock.opt, nullptr);
                gc_ = std::exchange(sock.gc_, nullptr);
                prevred = std::exchange(sock.prevred, 0);
                return *this;
            }

            bool connect(const void* addr /* = sockaddr*/, size_t len);
            bool write(const void* data, size_t len, int flag = 0);

            // read reads data from socket
            // if native read functions returns 0
            // readsize would be 0 and this function returns false
            bool read(void* data, size_t len, int flag = 0);
            bool writeto(const void* addr, int addrlen, const void* data, size_t len, int flag = 0);
            bool readfrom(void* addr, int* addrlen, void* data, size_t len, int flag = 0);
            [[nodiscard]] bool accept(Socket& to, void* addr, int* addrlen);
            using completion_accept_t = void (*)(void* user, Socket&& sock, int err);
            bool accept_async(completion_accept_t completion, void* user, void* data = nullptr, size_t len = 0);
            using completion_connect_t = void (*)(void* user, int err);
            bool connect_async(completion_connect_t completion, void* user, const void* addr, size_t len);
            bool bind(const void* addr /* = sockaddr*/, size_t len);
            bool listen(int back = 10);
            bool wait_readable(std::uint32_t sec, std::uint32_t usec);
            bool wait_writable(std::uint32_t sec, std::uint32_t usec);

            using completion_t = void (*)(void* user, void* data, size_t len, size_t bufmax, int err);
            using completion_from_t = void (*)(void* user, void* data, size_t len, size_t bufmax, void* addr, size_t addrlen, int err);
            // read_async reads socket async
            // if custom opt value is set
            // argument will be ignored
            // if this function returns true async operation started and completion will be called
            // otherwise operation failed
            bool read_async(completion_t completion, void* user, void* data = nullptr, size_t datalen = 0);
            // read_from_async reads socket async
            // if custom opt value is set
            // argument will be ignored
            // if this function returns true async operation started and completion will be called
            // otherwise operation failed
            bool readfrom_async(completion_from_t completion, void* user, void* data = nullptr, size_t datalen = 0);
            constexpr void set_custom_opt(void* optv, void (*gc)(void*, std::uintptr_t)) {
                if (gc_) {
                    gc_(opt, sock);
                }
                opt = optv;
                gc_ = gc;
            }
            constexpr void* get_opt() const {
                return opt;
            }
            constexpr explicit operator bool() const {
                return sock != ~0;
            }

            bool block() const;
            constexpr int geterr() const {
                return err;
            }
            bool shutdown(int mode = 2 /*= both*/);
            constexpr int readsize() const {
                return prevred;
            }

            // get_option invokes getsockopt with getsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            bool get_option(int layer, int opt, T& t) {
                return get_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // get_option invokes getsockopt with setsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            bool set_option(int layer, int opt, T&& t) {
                return set_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // set_reuse_addr sets SO_REUSEADDR
            // if this is true,
            // on linux,   you can bind address in TIME_WAIT,CLOSE_WAIT,FIN_WAIT2 (this is windows default behaviour)
            // on windows, you can bind some sockets on same address, but
            // this behaviour has some security risks
            // see also https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
            // Japanese Docs: https://www.ymnet.org/diary/d/%E6%97%A5%E8%A8%98%E3%81%BE%E3%81%A8%E3%82%81/SO_EXCLUSIVEADDRUSE
            bool set_reuse_addr(bool resue);

            // set_exclusive_use sets SO_EXCLUSIVEUSE
            // this function works on windows.
            // see also set_reuse_addr behaviour description link
            // on linux, this function always return false
            bool set_exclusive_use(bool exclusive);

            // set_ipv6only sets IPV6_V6ONLY
            // if this is false,you can accept both ipv6 and ipv4
            // default value is different between linux(false) and windows(true)
            bool set_ipv6only(bool only);
            bool set_nodelay(bool no_delay);
            bool set_ttl(unsigned char ttl);
            bool set_mtu_discover(MTUConfig conf);
            std::int32_t get_mtu();

            // set_blocking calls ioctl(FIONBIO)
            [[deprecated]] void set_blocking(bool blocking);

           private:
            std::uintptr_t sock;
            int err;
            int prevred;
            void* opt;
            void (*gc_)(void* opt, std::uintptr_t sock);

            constexpr Socket(std::uintptr_t s)
                : sock(s), err(0), prevred(0), opt(nullptr), gc_(nullptr) {}
            bool get_option(int layer, int opt, void* buf, size_t size);
            bool set_option(int layer, int opt, const void* buf, size_t size);
            friend Socket make_socket(std::uintptr_t uptr);
            constexpr void set_err(int e) {
                err = e;
            }

           public:
            constexpr Socket()
                : Socket(std::uintptr_t(~0)){};

            // read_until_block calls read function until read returns false
            // callback is void(void)
            // if something is read, red would be true
            // otherwise false
            // read_until_block returns block() function result
            bool read_until_block(bool& red, void* data, size_t size, auto&& callback) {
                red = false;
                while (read(data, size)) {
                    callback();
                    red = true;
                }
                return block();
            }

           private:
            static void* internal_alloc(size_t s, DebugInfo info);  // wrappper of get_rawbuf
            static void internal_free(void*, DebugInfo info);       // wrapper of free_rawbuf

           public:
            // this function is wrapper of raw read_async function
            // when completion event occured, this function recieve data
            // and try reading more data as it can
            // fn is called when data recieved with fn(std::move(object))
            // object is continuation context should contains Socket instance. this must be move assignable
            // get_sock is called with get_sock(object) and should returns Socket&
            // add_data is called with add_data(object,(const char*)data,(size_t)len)
            // if read_async started this function returns true
            // otherwise returns false and err would be set
            bool read_async(auto fn, auto&& object, auto get_sock, auto add_data) {
                struct ObjectHolder {
                    std::decay_t<decltype(fn)> fn;
                    std::decay_t<decltype(get_sock)> get;
                    std::decay_t<decltype(add_data)> add;
                    std::remove_cvref_t<decltype(object)> obj;
                };
                auto pobj = internal_alloc(sizeof(ObjectHolder), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ObjectHolder)));
                if (!pobj) {
                    err = no_resource;
                    return false;
                }
                ObjectHolder* obj = new (pobj) ObjectHolder{
                    std::move(fn),
                    std::move(get_sock),
                    std::move(add_data),
                    std::move(object),
                };
                auto cb = [](void* pobj, void* data, size_t size, size_t buf, int err) {
                    ObjectHolder* obj = static_cast<ObjectHolder*>(pobj);
                    auto pass = std::move(obj->obj);
                    struct R {
                        ObjectHolder* h;
                        ~R() {
                            h->~ObjectHolder();
                            internal_free(h, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(*h)));
                        };
                    } releaser{obj};
                    obj->get(pass).set_err(err);
                    obj->add(pass, static_cast<const char*>(data), size);
                    if (size == buf) {
                        Socket& sock = obj->get(pass);
                        bool red = false;
                        sock.read_until_block(red, data, buf, [&] {
                            obj->add(pass, (char*)data, sock.readsize());
                        });
                    }
                    obj->fn(std::move(pass));
                };
                if (!obj->get(obj->obj).read_async(cb, obj)) {
                    object = std::move(obj->obj);  // return pass
                    internal_free(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ObjectHolder)));
                    return false;
                }
                return true;
            }

            // connect_async wraps origina connect_async
            bool connect_async(auto&& fn, auto&& object, auto get_sock, void* addr, int addrlen) {
                struct ObjectHolder {
                    std::decay_t<decltype(fn)> fn;
                    std::decay_t<decltype(get_sock)> get;
                    std::remove_cvref_t<decltype(object)> obj;
                };
                auto pobj = internal_alloc(sizeof(ObjectHolder), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ObjectHolder)));
                if (!pobj) {
                    err = no_resource;
                    return false;
                }
                ObjectHolder* obj = new (pobj) ObjectHolder{
                    std::move(fn),
                    std::move(get_sock),
                    std::move(object),
                };
                auto cb = [](void* pobj, int err) {
                    ObjectHolder* obj = static_cast<ObjectHolder*>(pobj);
                    auto pass = std::move(obj->obj);
                    struct R {
                        ObjectHolder* h;
                        ~R() {
                            h->~ObjectHolder();
                            internal_free(h, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(*h)));
                        };
                    } releaser{obj};
                    obj->get(pass).set_err(err);
                    obj->fn(std::move(pass));
                };
                if (!obj->get(obj->obj).connect_async(cb, obj, addr, addrlen)) {
                    object = std::move(obj->obj);  // return pass
                    internal_free(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ObjectHolder)));
                    return false;
                }
                return true;
            }
        };

        // make_socket creates socket object
        // before this function calls
        // you mustn't call any other functions
        // if you call any function
        // that is undefined behaviour
        // socket is always non-blocking
        // if you want blocking socket, call Socket::set_blocking explicit
        dnet_dll_export(Socket) make_socket(int address_family, int socket_type, int protocol);
    }  // namespace dnet
}  // namespace utils
