/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/server/servcpp.h>
#include <fnet/http/client.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <thread/concurrent_queue.h>
#include <ranges>
#include <fnet/sockutil.h>
#include <fnet/util/uri_normalize.h>
#include <thread>

namespace futils::fnet::http {

    struct Destination;

    struct WaitInfo {
        std::shared_ptr<Destination> dest;
        WaitAddrInfo waiter;
        std::shared_ptr<Response::AbstractResponse> response;
    };

    using WaitDNSRequestQueue = thread::MultiProducerChannelBuffer<std::optional<WaitInfo>, glheap_allocator<WaitInfo>>;
    using CompletionQueue = thread::MultiProducerChannelBuffer<std::optional<DeferredCallback>, glheap_allocator<DeferredCallback>>;
    void range_to_uri(auto& uri, uri::URIRange range, futils::view::rvec raw) {
        uri::range_to_uri(uri, range, raw);
        if (uri.scheme.empty()) {
            uri.scheme = decltype(uri.scheme)("http");
        }
        if (uri.path.empty()) {
            uri.path = decltype(uri.path)("/");
        }
        if (uri.port.empty()) {
            uri.port = decltype(uri.port)(uri.scheme == "http" ? "80" : "443");
        }
    }

    struct Callback {
        DeferredCallback cb;
        bool written = false;
    };

    enum class ResponseState {
        pending,
        done,
        failed,
    };

    struct Response::AbstractResponse : std::enable_shared_from_this<AbstractResponse> {
        std::atomic<ResponseState> state = ResponseState::pending;
        error::Error err;
        uri::URI<flex_storage> uri;
        UserCallback cb;

        void handle_error(auto&& err) {
            this->err = std::forward<decltype(err)>(err);
            state = ResponseState::failed;
        }

        auto get_on_error() {
            return [resp = shared_from_this()](error::Error&& err) {
                resp->handle_error(std::move(err));
            };
        }
    };

    expected<void> Response::wait() {
        response->state.wait(ResponseState::pending);
        if (response->state.load() == ResponseState::failed) {
            return unexpect(response->err);
        }
        return {};
    }

    bool Response::done() const {
        return response->state.load() != ResponseState::pending;
    }

    struct Client::AbstractClient {
        slib::hash_map<flex_storage, std::shared_ptr<Destination>> clients;
        Socket udp_socket;
        std::shared_ptr<WaitDNSRequestQueue> dns_queue;
        std::shared_ptr<CompletionQueue> completion_queue;
        tls::TLSConfig tls_config;
    };

    struct Destination {
        std::weak_ptr<Client::AbstractClient> client;
        Socket tcp_socket;
        std::shared_ptr<http2::FrameHandler> h2_handler;

        tls::TLS tls;
        int recursion = 0;
        binary::flags_t<std::uint8_t, 1, 1> flags;
        bits_flag_alias_method(flags, 0, writing_async);  // writing job is running
        bits_flag_alias_method(flags, 1, reading_async);  // reading job is running
        std::vector<Callback> write_pending_callbacks;
        std::vector<Callback> read_pending_callbacks;
        flex_storage h2_writing_buffer;
        flex_storage tls_read_buffer;
        flex_storage tls_write_buffer;

        HTTP http;
        RequestWriter request_writer{http};
        ResponseReader response_reader{http};

        error::Error conn_err;
        bool transport_error = false;
        void handle_transport_error(auto&& err) {
            transport_error = true;
            conn_err = std::forward<decltype(err)>(err);
        }

        void handle_error(auto&& err) {
            conn_err = std::forward<decltype(err)>(err);
        }

        using SelfPtr = std::shared_ptr<Destination>;

        bool check_recursion_limit() {
            if (recursion > 10) {
                recursion = 0;
                return true;
            }
            recursion++;
            return false;
        }
        expected<Response> handle_request(std::shared_ptr<Destination>& self, uri::URIRange range, futils::uri::URI<futils::view::rvec> uri_, futils::view::rvec raw_uri, UserCallback cb);

        static void try_connect(error::Error&& from, AddrInfo& addr, WaitInfo& info);

        static void on_connected(SelfPtr&& self, Socket&& sock, std::shared_ptr<Response::AbstractResponse> resp);

        static void do_tls_handshake(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp);

        Response get_response(uri::URIRange range, futils::view::rvec raw_uri, UserCallback cb) {
            auto r = std::allocate_shared<Response::AbstractResponse>(glheap_allocator<Response::AbstractResponse>{});
            range_to_uri(r->uri, range, raw_uri);
            uri::normalize_uri(r->uri, uri::NormalizeFlag::host | uri::NormalizeFlag::path);
            r->cb = std::move(cb);
            return Response{r};
        }

        static void dns_resolve_wait_thread(std::shared_ptr<WaitDNSRequestQueue> queue);
        static void running_completion_thread(std::shared_ptr<CompletionQueue> queue);

        static void init_http1(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp);
        static void init_http2(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp);
        static void request_http1(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp);
        static void request_http2(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp);

        static auto get_notify(SelfPtr self) {
            auto cl = self->client.lock();
            return [self, cl](DeferredCallback&& cb) {
                self->recursion = 0;
                cl->completion_queue->send(std::move(cb));
            };
        }
    };

    void Destination::running_completion_thread(std::shared_ptr<CompletionQueue> queue) {
        while (true) {
            auto q = queue->receive_wait();
            if (!q) {
                break;
            }
            if (!*q) {
                continue;
            }
            auto& target = **q;
            target();
        }
    }

    void Destination::dns_resolve_wait_thread(std::shared_ptr<WaitDNSRequestQueue> queue) {
        slib::list<WaitInfo> waiters;
        auto handle_wait_info = [&](WaitInfo& info) {
            auto addr = info.waiter.wait(1);
            if (!addr) {
                if (addr.error() != error::block) {
                    // error
                    info.response->handle_error(std::move(addr.error()));
                    return true;  // should remove
                }
                return false;
            }
            info.dest->try_connect(error::Error(), *addr, info);
            return true;  // should remove
        };
        auto do_handle = [&](auto& q) {
            if (!q) {
                return true;
            }
            if (!*q) {
                return false;
            }
            auto& target = **q;
            if (handle_wait_info(target)) {
                return true;
            }
            waiters.push_back(std::move(target));
            return true;
        };
        while (true) {
            waiters.remove_if(handle_wait_info);
            auto q = waiters.size() ? queue->receive() /*non blocking*/ : queue->receive_wait() /*blocking*/;
            if (!do_handle(q)) {
                break;
            }
        }
    }

    void Client::init_client(std::shared_ptr<Client::AbstractClient>& client) {
        if (!client) {
            client = std::allocate_shared<Client::AbstractClient>(glheap_allocator<Client::AbstractClient>{});
            client->completion_queue = std::allocate_shared<CompletionQueue>(glheap_allocator<CompletionQueue>{});
            client->dns_queue = std::allocate_shared<WaitDNSRequestQueue>(glheap_allocator<WaitDNSRequestQueue>{});
            std::thread{Destination::running_completion_thread, client->completion_queue}.detach();
            std::thread{Destination::dns_resolve_wait_thread, client->dns_queue}.detach();
        }
    }

    void Client::set_tls_config(tls::TLSConfig&& conf) {
        init_client(client);
        client->tls_config = std::move(conf);
    }

    expected<Response> Client::request(futils::view::rvec uri, UserCallback cb) {
        if (!cb) {
            return unexpect("request or response callback is null", futils::error::Category::app);
        }
        futils::uri::URIRange range;
        auto seq = futils::make_ref_seq(uri);
        auto err = futils::uri::parse_range_ex(range, seq);
        if (err != futils::uri::ParseError::none) {
            return unexpect(futils::uri::err_msg(err), futils::error::Category::app);
        }
        futils::uri::URI<futils::view::rvec> uri_;
        range_to_uri(uri_, range, uri);
        if (uri_.scheme != "http" && uri_.scheme != "https") {
            return unexpect("scheme is not http or https", futils::error::Category::app);
        }
        if (uri_.scheme == "https" && !client->tls_config) {
            return unexpect("tls config is not set", futils::error::Category::app);
        }
        flex_storage target;
        target.reserve(uri_.hostname.size() + uri_.port.size() + 1);
        if (!punycode::encode_host(uri_.hostname, target)) {
            return unexpect("punycode encoding failed", futils::error::Category::app);
        }
        target.push_back(':');
        target.append(uri_.port);
        init_client(client);
        auto exists = client->clients.find(target);
        if (exists == client->clients.end()) {
            auto dest = std::allocate_shared<Destination>(glheap_allocator<Destination>{});
            dest->client = client;
            client->clients[target] = dest;
            return dest->handle_request(dest, range, uri_, uri, std::move(cb));
        }
        return exists->second->handle_request(exists->second, range, uri_, uri, std::move(cb));
    }

    void transfer_call(std::shared_ptr<Destination> self, auto&& callback) {
        auto cb = alloc_deferred_callback(callback);
        if (!cb) {
            self->handle_error(error::memory_exhausted);
            callback();
            return;
        }
        auto client = self->client.lock();
        if (!client) {
            self->handle_error(error::Error("client is already destroyed", error::Category::lib));
            callback();
            return;
        }
        client->completion_queue->send(std::move(cb));
    }

    expected<Response> Destination::handle_request(std::shared_ptr<Destination>& dest, uri::URIRange range, futils::uri::URI<futils::view::rvec> uri_, futils::view::rvec raw_uri, UserCallback cb) {
        auto cl = client.lock();
        // non existent client
        if (!tcp_socket) {
            auto waiter = resolve_address(uri_.hostname, uri_.port, sockattr_tcp());
            if (!waiter) {
                return unexpect(waiter.error());
            }
            auto solved = waiter->wait(1);
            if (!solved) {
                if (solved.error() != error::block) {
                    return unexpect(solved.error());
                }
                auto resp = get_response(range, raw_uri, std::move(cb));
                auto info = WaitInfo{dest, std::move(*waiter), resp.response};
                cl->dns_queue->send(std::move(info));
                return resp;
            }
            auto resp = get_response(range, raw_uri, std::move(cb));
            auto t = WaitInfo{dest, {}, resp.response};
            try_connect(error::Error(), *solved, t);
            return resp;
        }
        auto resp = get_response(range, raw_uri, std::move(cb));
        auto c = alloc_deferred_callback([dest, resp = resp.response]() {
            if (dest->h2_handler) {
                init_http2(dest, resp);
            }
            else {
                auto push_pending = [&](auto&& callback) {
                    auto dc = alloc_deferred_callback(std::move(callback));
                    if (!dc) {
                        resp->handle_error(error::memory_exhausted);
                        return;
                    }
                    dest->write_pending_callbacks.push_back({.cb = std::move(dc)});
                };
                auto callback = [=] {
                    if (dest->h2_handler) {
                        init_http2(dest, resp);
                    }
                    else {
                        init_http1(dest, resp);
                    }
                };
                if (dest->tls && dest->tls.in_handshake()) {  // now, handshake operation is running
                    push_pending(std::move(callback));
                }
                else {
                    if (!dest->writing_async() && !dest->reading_async()) {
                        init_http1(dest, resp);
                    }
                    else {
                        push_pending(std::move(callback));
                    }
                }
            }
        });
        if (!c) {
            resp.response->handle_error(error::memory_exhausted);
            return resp;
        }
        cl->completion_queue->send(std::move(c));
        return resp;
    }

    void add_error(error::Error& err, error::Error&& e) {
        if (!err) {
            err = std::move(e);
        }
        else {
            err = error::ErrList{std::move(e), err};
        }
    }

    struct ConnectCallback {
        WaitInfo info;
        AddrInfo addr;
        error::Error err;

        void operator()(Socket&& sock, NotifyResult&& r) {
            if (!r.value()) {
                add_error(err, std::move(r.value().error()));
                info.dest->try_connect(std::move(err), addr, info);
                return;
            }
            info.dest->on_connected(std::move(info.dest), std::move(sock), info.response);
        }
    };

    void Destination::try_connect(error::Error&& err_from, AddrInfo& addr, WaitInfo& info) {
        auto err = std::move(err_from);
        while (addr.next()) {
            auto sockaddr = addr.sockaddr();
            auto res = fnet::make_socket(sockaddr.attr);
            if (!res) {
                add_error(err, std::move(res.error()));
                continue;
            }
            res->connect_ex_loopback_optimize(sockaddr.addr);
            auto callback = async_connect_then(
                sockaddr.addr,
                ConnectCallback{
                    .info = std::move(info),
                    .addr = std::move(addr),
                    .err = std::move(err),
                });
            auto result = res->connect_async(callback, true);
            if (!result) {
                auto do_del = futils::helper::defer([&] {
                    callback->del(callback);
                });
                // restore state
                addr = std::move(callback->fn.addr);
                info = std::move(callback->fn.info);
                err = std::move(callback->fn.err);
                add_error(err, std::move(result.error()));
                continue;
            }
            // anyway, handled by ConnectCallback, so return
            return;
        }
        if (!err) {
            err = error::Error("no address to try", error::Category::app);
        }
        // no more addr to try, so handle error
        info.response->handle_error(std::move(err));
    }

    void Destination::on_connected(SelfPtr&& self, Socket&& sock, std::shared_ptr<Response::AbstractResponse> resp) {
        self->tcp_socket = std::move(sock);
        auto cl = self->client.lock();
        if (!cl) {
            resp->handle_error(error::Error("client is already destroyed", error::Category::lib));
            return;
        }
        auto callback = [=] {
            if (resp->uri.scheme == "https") {
                auto t = tls::create_tls_with_error(cl->tls_config);
                if (!t) {
                    resp->handle_error(std::move(t.error()));
                    return;
                }
                if (resp->uri.hostname.size() > 255) {
                    resp->handle_error(error::Error("hostname too long (maybe bug!)", error::Category::app));
                    return;
                }
                char buf[256];
                futils::view::copy(futils::view::wvec(buf, 256), resp->uri.hostname);
                buf[resp->uri.hostname.size()] = 0;
                auto res = t->set_hostname(buf);
                if (!res) {
                    resp->handle_error(std::move(res.error()));
                    return;
                }
                self->tls = std::move(*t);
                do_tls_handshake(std::move(self), std::move(resp));
                return;
            }
            init_http1(std::move(self), std::move(resp));
        };
        auto cb = alloc_deferred_callback(std::move(callback));
        if (!cb) {
            resp->handle_error(error::memory_exhausted);
            return;
        }
        cl->completion_queue->send(std::move(cb));
    }

    void do_tls_read(std::shared_ptr<Destination> self, auto&& add_data, auto&& then, bool anyway_then = false) {
        socket_read_deferred(
            std::move(self->tcp_socket),
            [self](auto&& d) {
                self->tls_read_buffer.append(d);
            },
            Destination::get_notify(self),
            [self, then, add_data](auto&&) {
                while (self->tls_read_buffer.size() > 0) {
                    while (self->tls_read_buffer.size() > 0) {
                        auto res = self->tls.provide_tls_data(self->tls_read_buffer);
                        if (!res && tls::isTLSBlock(res.error())) {
                            break;  // block so continue processing
                        }
                        if (!res) {
                            self->handle_error(std::move(res.error()));
                            then(self);
                            return;
                        }
                        self->tls_read_buffer.shift_front(self->tls_read_buffer.size() - res->size());
                    }
                    auto res = self->tls.read_until_block(add_data);
                    if (!res) {
                        self->handle_error(std::move(res.error()));
                        then(self);
                        return;
                    }
                }
                then(self);
            },
            [self, then](auto&& err) {
                self->handle_transport_error(std::move(err));
                then(self);
            },
            {}, anyway_then);
    }

    void do_tls_write(std::shared_ptr<Destination> self, auto&& then) {
        if (self->check_recursion_limit()) {
            transfer_call(std::move(self), [self, then] {
                do_tls_write(std::move(self), then);
            });
            return;
        }
        auto res = self->tls.receive_tls_data_until_block([&](auto&& d) {
            self->tls_write_buffer.append(d);
        });
        if (!res) {
            self->handle_error(std::move(res.error()));
            then(self);
            return;
        }
        socket_write_deferred(
            std::move(self->tcp_socket), self->tls_write_buffer,
            Destination::get_notify(self),
            [self, then](auto&&) {
                self->tls_write_buffer.clear();
                then(self);
            },
            [self, then](auto&& err) {
                self->handle_transport_error(std::move(err));
                then(self);
            });
    }

    void Destination::do_tls_handshake(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp) {
        auto res = self->tls.connect();
        if (!res && !tls::isTLSBlock(res.error())) {
            resp->handle_error(std::move(res.error()));
            return;
        }
        if (res) {  // handshake completed
            auto alpn = self->tls.get_selected_alpn();
            if (alpn == "h2") {
                request_http2(std::move(self), std::move(resp));
            }
            else if (!alpn || alpn == "http/1.1") {
                request_http1(std::move(self), std::move(resp));
            }
            else {
                resp->handle_error(error::Error("unsupported alpn", error::Category::app));
            }
            return;
        }
        do_tls_write(std::move(self), [resp](auto&& self) {
            do_tls_read(
                std::move(self),
                [](auto&&) { assert(false); },
                [resp](auto&& self) { do_tls_handshake(std::move(self), std::move(resp)); });
        });
    }

    void read_input(std::shared_ptr<Destination> self, auto&& add_data, auto&& then) {
        self->reading_async(true);
        if (self->check_recursion_limit()) {
            transfer_call(std::move(self), [self, add_data, then] {
                read_input(std::move(self), add_data, then);
            });
            return;
        }
        if (self->tls) {
            do_tls_read(
                self,
                add_data,
                [self, then](auto&&) {
                    self->reading_async(false);
                    then(self);
                });
        }
        else {
            socket_read_deferred(
                std::move(self->tcp_socket),
                [add_data](auto&& d) {
                    add_data(d);
                },
                Destination::get_notify(self),
                [self, then](auto&&) {
                    self->reading_async(false);
                    then(self);
                },
                [self, then](auto&& err) {
                    self->handle_transport_error(std::move(err));
                    then(self);
                });
        }
    }

    // if data.size() == 0, then it will only call `then` and return
    void write_output(std::shared_ptr<Destination> self, auto&& add_data, futils::view::rvec data, auto&& then) {
        self->writing_async(true);
        if (self->check_recursion_limit()) {  // move to completion
            transfer_call(std::move(self), [self, add_data, data, then] {
                write_output(std::move(self), add_data, data, then);
            });
            return;
        }
        if (self->tls) {
            while (data.size() > 0) {
                auto res = self->tls.write(data);
                if (!res && tls::isTLSBlock(res.error())) {
                    do_tls_write(self, [then, data, add_data](auto&& self) {
                        if (self->conn_err) {
                            then(self);
                            return;
                        }
                        if (!self->reading_async()) {
                            self->reading_async(true);
                            do_tls_read(
                                std::move(self),
                                add_data,
                                [then, add_data, data](auto&& self) {
                                    if (self->conn_err) {
                                        then(self);
                                        return;
                                    }
                                    self->reading_async(false);
                                    write_output(std::move(self), add_data, data, then);
                                },
                                true);
                            return;
                        }
                        write_output(std::move(self), add_data, data, then);
                    });
                    return;
                }
                if (!res) {
                    self->handle_error(std::move(res.error()));
                    then(self);
                    return;
                }
                data = *res;
            }
            do_tls_write(
                self,
                [self, then](auto&&) {
                    self->writing_async(false);
                    then(self);
                });
        }
        else {
            socket_write_deferred(
                std::move(self->tcp_socket), data,
                Destination::get_notify(self),
                [self, then](auto&&) {
                    self->writing_async(false);
                    then(self);
                },
                [self, then](auto&& err) {
                    self->handle_transport_error(std::move(err));
                    then(self);
                });
        }
    }

    void Destination::init_http1(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp) {
        if (!self->http.http1()) {
            self->http = http::HTTP(http1::HTTP1{});
        }
        request_http1(std::move(self), std::move(resp));
    }

    void http2_read_write_loop(std::shared_ptr<Destination> self);

    void transmit_http2_request(std::shared_ptr<Destination> self, auto&& callback) {
        auto cb = alloc_deferred_callback([self, callback] {
            callback(self);
        });
        if (!cb) {
            self->handle_transport_error(error::memory_exhausted);
            callback(self);
            return;
        }
        self->write_pending_callbacks.push_back({std::move(cb)});
        if (!self->writing_async()) {
            http2_read_write_loop(self);
        }
    }

    // this function is handling http2 read/write loop
    void http2_read_write_loop(std::shared_ptr<Destination> self) {
        if (self->transport_error) {
            return;  // no more loop because of transport error
        }
        if (!self->writing_async()) {
            // first, we have to call deferred callbacks
            for (size_t i = 0; i < self->write_pending_callbacks.size(); i++) {
                auto& cb = self->write_pending_callbacks[i];
                if (cb.written) {
                    cb.cb();
                }
                else {
                    self->write_pending_callbacks.erase(self->write_pending_callbacks.begin(), self->write_pending_callbacks.begin() + i);
                    break;
                }
            }
            // then, check if we have to write
            if (self->h2_handler->get_write_buffer().size() > 0) {
                self->h2_writing_buffer = self->h2_handler->get_write_buffer().take_buffer();
                for (auto& cb : self->write_pending_callbacks) {
                    cb.written = true;  // mark as written (for next loop)
                }
                // do write (this will call next read_write_loop)
                write_output(self, [self](auto&& d) { self->h2_handler->add_data(d); }, self->h2_writing_buffer, [](auto&& self) { http2_read_write_loop(self); });
                return;
            }
        }
        if (!self->reading_async()) {  // no reading job is running
                                       // so start reading
            read_input(
                self,
                [self](auto&& d) { self->h2_handler->add_data(d); },
                [](std::shared_ptr<Destination> self) {
                    if (self->conn_err && !self->transport_error) {
                        if (auto code = self->conn_err.as<http2::Error>()) {
                            self->h2_handler->send_goaway(int(code->code));
                        }
                        else {
                            self->h2_handler->send_goaway(int(http2::H2Error::internal));
                        }
                        if (!self->writing_async()) {
                            transmit_http2_request(std::move(self), [](std::shared_ptr<Destination> self) {
                                if (self->tls) {
                                    auto res = self->tls.shutdown();
                                    if (!res) {
                                        self->handle_error(std::move(res.error()));
                                        return;
                                    }
                                    do_tls_write(std::move(self), [](auto&&) {});
                                    return;
                                }
                            });
                        }
                        return;
                    }
                    http2_read_write_loop(self);  // start next loop
                });
        }
    }

    void Destination::init_http2(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp) {
        if (!self->h2_handler) {
            self->h2_handler = std::allocate_shared<http2::FrameHandler>(glheap_allocator<http2::FrameHandler>(), http2::HTTP2Role::client);
            auto err = self->h2_handler->send_settings({.enable_push = false});
            if (err) {
                resp->handle_error(std::move(err));
                return;
            }
            auto client = self->client.lock();
            if (!client) {
                resp->handle_error(error::Error("client is already destroyed", error::Category::lib));
                return;
            }
            for (auto&& cb : self->write_pending_callbacks) {
                client->completion_queue->send(std::move(cb.cb));
            }
            self->write_pending_callbacks.clear();
            http2_read_write_loop(self);
        }
        auto stream = self->h2_handler->open();
        if (!stream) {
            resp->handle_error(error::Error("failed to open stream", error::Category::app));
            return;
        }
        stream->get_or_set_application_data<Response::AbstractResponse>([&](auto&& c, auto&& set) {
            set(resp);
            return resp;
        });
        request_http2(std::move(self), std::move(resp));
    }

    void http1_read_response(std::shared_ptr<Destination> self, std::shared_ptr<Response::AbstractResponse> resp) {
        if (self->check_recursion_limit()) {
            transfer_call(std::move(self), [self, resp] {
                http1_read_response(std::move(self), std::move(resp));
            });
            return;
        }
        if (self->conn_err) {
            resp->handle_error(self->conn_err);
            return;
        }
        self->response_reader.reset(self->http);
        size_t current_input_size = self->http.http1()->get_input().size();
        resp->cb.do_response(resp->uri, self->response_reader);
        if (auto err = self->response_reader.get_error()) {
            if (!self->response_reader.is_resumable()) {
                resp->handle_error(std::move(err));
                return;
            }
        }
        self->http.http1()->adjust_input();
        if (self->http.http1()->read_ctx.on_no_body_semantics()) {
            resp->state = ResponseState::done;
            if (self->read_pending_callbacks.empty()) {
                return;
            }
            // enqueue index 0 request
            auto& cb = self->read_pending_callbacks[0];
            auto client = self->client.lock();
            if (!client) {
                resp->handle_error(error::Error("client is already destroyed", error::Category::lib));
                return;
            }
            client->completion_queue->send(std::move(cb.cb));
            self->read_pending_callbacks.erase(self->read_pending_callbacks.begin());
            return;
        }
        // now, input still has some data
        if (!self->response_reader.is_called()) {
            http1_read_response(std::move(self), std::move(resp));
            return;
        }
        read_input(
            self, [self](auto&& d) { self->http.http1()->add_input(d); },
            [resp](auto&& self) {
                http1_read_response(std::move(self), std::move(resp));
            });
    }

    void Destination::request_http1(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp) {
        if (self->check_recursion_limit()) {
            transfer_call(std::move(self), [self, resp] {
                request_http1(std::move(self), std::move(resp));
            });
            return;
        }
        if (self->conn_err) {
            resp->handle_error(self->conn_err);
            return;
        }
        self->request_writer.reset(self->http);
        resp->cb.do_request(resp->uri, self->request_writer);
        if (auto err = self->request_writer.get_error()) {
            resp->handle_error(std::move(err));
            return;
        }
        auto data = self->http.http1()->get_output();
        auto was_fin = self->request_writer.is_fin();
        write_output(
            self,
            [self](auto&& d) { self->http.http1()->add_input(d); }, data,
            [resp, was_fin](SelfPtr self) {
                self->http.http1()->clear_output();
                if (!was_fin) {
                    request_http1(std::move(self), std::move(resp));
                    return;
                }
                if (!self->write_pending_callbacks.empty()) {
                    auto client = self->client.lock();
                    if (!client) {
                        resp->handle_error(error::Error("client is already destroyed", error::Category::lib));
                        return;
                    }
                    // enqueue index 0 request
                    auto& cb = self->write_pending_callbacks[0];
                    client->completion_queue->send(std::move(cb.cb));
                    self->write_pending_callbacks.erase(self->write_pending_callbacks.begin());
                }
                if (!self->reading_async()) {
                    http1_read_response(std::move(self), std::move(resp));
                }
                else {
                    auto cb = alloc_deferred_callback([self, resp = std::move(resp)] {
                        http1_read_response(std::move(self), std::move(resp));
                    });
                    if (!cb) {
                        resp->handle_error(error::memory_exhausted);
                        return;
                    }
                    self->read_pending_callbacks.push_back({.cb = std::move(cb)});
                }
            });
    }

    void Destination::request_http2(SelfPtr self, std::shared_ptr<Response::AbstractResponse> resp) {
        if (self->check_recursion_limit()) {
            transfer_call(std::move(self), [self, resp] {
                request_http2(std::move(self), std::move(resp));
            });
            return;
        }
        if (self->conn_err) {
            resp->handle_error(self->conn_err);
            return;
        }
        auto stream = self->http.http2()->get_stream();
        self->http = http2::HTTP2(stream);  // restore http2
        self->request_writer.reset(self->http);
        resp->cb.do_request(resp->uri, self->request_writer);
        if (auto err = self->request_writer.get_error()) {
            resp->handle_error(std::move(err));
            return;
        }
        auto was_fin = self->request_writer.is_fin();
        auto callback = [=](SelfPtr self) {
            if (!was_fin) {
                request_http2(std::move(self), std::move(resp));
                return;
            }
        };
        transmit_http2_request(std::move(self), callback);
    }

}  // namespace futils::fnet::http
