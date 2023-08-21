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
#include <fnet/connect.h>

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
        std::thread(
            [=](Request req) {
                auto ok = fnet::connect(req.uri.hostname, req.uri.port, fnet::sockattr_tcp());
                if (!ok) {
                    request_message(req.uri, "cannot connect ", ok.error().error<std::string>());
                }
                auto& sock = ok->first;
            },
            std::move(req))
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
