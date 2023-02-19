/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "durl.h"
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <fnet/plthead.h>
#include <thread>

namespace durl {
    namespace fnet = utils::fnet;
    struct AddrWait {
        URIToJSON uri;
        fnet::WaitAddrInfo wait;
    };

    std::atomic_uint32_t reqcounter;

    struct Context {
        std::atomic_uint32_t reqcounter;
    };

    void request_message(URI& uri, auto&&... msg) {}

    struct Request {
        URI uri;
    };

    void wait_connect(Request req, fnet::AddrInfo& info, fnet::Socket& conn) {
        while (true) {
            if (auto err = conn.wait_writable(0, 1000)) {
                if (!fnet::isSysBlock(err)) {
                    request_message(req.uri, "failed to connect to ", info.sockaddr().addr.to_string<std::string>(), ". last error is ", err.error<std::string>());
                    return;
                }
                continue;
            }
            break;
        }
    }

    // on thread
    void resolved(Request& req, fnet::AddrInfo& info) {
        fnet::error::Error lasterr;
        while (info.next()) {
            auto addr = info.sockaddr();
            auto sock = fnet::make_socket(addr.attr);
            if (!sock) {
                lasterr = fnet::error::Error("make_socket failed");
                continue;
            }
            auto err = sock.connect(addr.addr);
            if (err && !fnet::isSysBlock(err)) {
                lasterr = std::move(err);
                continue;
            }
        }
        info.next();  // seek to top
        request_message(req.uri, "failed to connect to ", info.sockaddr().addr.to_string<std::string>(), ". last error is ", lasterr.error<std::string>());
        reqcounter--;
    }

    void request(Request req) {
        auto& uri = req.uri;
        uri::tidy(uri);
        if (uri.scheme == "") {
            uri.scheme = httpopt.uri.https_parse_prefix ? "https" : "http";
        }
        if (uri.scheme != "http" && uri.scheme != "https") {
            request_message(uri, "unexpected scheme ", uri.scheme, ". expect https or http");
            reqcounter--;
            return;
        }
        if (uri.port == "") {
            if (uri.scheme == "http") {
                uri.port = "80";
            }
            else if (uri.scheme == "https") {
                uri.port = "443";
            }
        }
        fnet::SockAddr addr{};
        auto [wait, err] = fnet::resolve_address(uri.hostname, uri.port, {.socket_type = SOCK_STREAM});
        if (err) {
            request_message(uri, "resolve address failed: ", err.error<std::string>());
            reqcounter--;
            return;
        }
        std::thread(
            [=](fnet::WaitAddrInfo wait, Request req) {
                fnet::AddrInfo info;
                while (true) {
                    if (auto err = wait.wait(info, 1)) {
                        if (err == fnet::error::block) {
                            continue;
                        }
                        request_message(req.uri, "resolve address failed ", err.error<std::string>());
                        reqcounter--;
                        return;
                    }
                    break;
                }
                resolved(req, info);
            },
            std::move(wait), std::move(req))
            .detach();
    }

    int do_request(URIVec& uris) {
        for (auto& uri : uris) {
            reqcounter++;
            request(Request{std::move(uri.uri)});
        }
        return 0;
    }
}  // namespace durl
