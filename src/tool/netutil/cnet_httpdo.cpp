/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "subcommand.h"
#include <thread>
#include <thread/channel.h>
#include <cnet/tcp.h>
#include <cnet/ssl.h>
#include <cnet/http2.h>
#include <net/http/http_headers.h>

namespace netutil {
    namespace wrap = utils::wrap;
    namespace th = utils::thread;
    namespace cnet = utils::cnet;
    namespace net = utils::net;
    namespace h1 = utils::net::h1header;
    namespace hlp = utils::helper;
    namespace num = utils::number;

    struct Flags {
        std::int64_t timeout = -1;
        wrap::string cert;
        wrap::map<wrap::string, wrap::string> h;
    };

    using Header = wrap::multimap<wrap::string, wrap::string>;

    struct Done {
        UriWithTag uri;
        wrap::string msg;
        wrap::string layer;
        std::int64_t code = 0;

        h1::StatusCode status;
        Header h;

        cnet::CNet* pool = nullptr;
        int httpver = 0;
    };

    bool http2(cnet::CNet* client, th::SendChan<Done>& msg, UriWithTag& req, bool ack = false) {
        auto h2ctx = cnet::http2::create_client();
        cnet::set_lowlevel_protocol(h2ctx, client);
        bool sent_header = false;
        std::int32_t id = 0;
        Header response;
        wrap::string data;
        cnet::http2::set_frame_read_callback(h2ctx, [&](cnet::http2::Frames* frames) {
            namespace h2 = cnet::http2;
            auto write_header = [&] {
                if (ack && !sent_header) {
                    Header h;
                    h2::set_request(h, req.uri.host_port(), req.uri.path_query());
                    h2::write_header(frames, h, id);
                    sent_header = true;
                }
            };
            write_header();
            bool res = true;
            for (auto& fr : frames) {
                h2::default_proc(frames, fr, h2::DefaultProc::all);
                if (h2::is_settings_ack(fr)) {
                    ack = true;
                }
                write_header();
                if (sent_header) {
                    if (fr->type == net::http2::FrameType::header &&
                        fr->flag & net::http2::Flag::end_headers) {
                        h2::read_header(frames, response, fr->id);
                    }
                    if (fr->type == net::http2::FrameType::data &&
                        fr->flag & net::http2::Flag::end_stream) {
                        h2::read_data(frames, data, fr->id);
                    }
                    if (fr->id == id && h2::is_close_stream_signal(fr)) {
                        res = false;
                    }
                }
            }
            return res;
        });
        auto res = cnet::http2::poll_frame(h2ctx);
        if (!res) {
            msg << Done{
                .uri = std::move(req),
                .msg = "failed to read frame",
                .layer = "http2",
                .code = -1,
            };
            return false;
        }
        msg << Done{
            .uri = std::move(req),
            .msg = "http2 succeeded",
            .layer = "http2",
            .code = 0,
            .h = std::move(response),
            .pool = h2ctx,
            .httpver = 2,
        };
        return true;
    }

    bool http1(cnet::CNet* client, th::SendChan<Done>& msg, UriWithTag& req, Flags& flags) {
        wrap::string buf;
        bool res = h1::render_request(
            buf, req.tagcmd.method, req.uri.path_query(), flags.h,
            [](auto&& keyval) {
                constexpr auto valid = h1::default_validator();
                if (!valid(keyval) ||
                    hlp::equal(keyval.first, "host", hlp::ignore_case()) ||
                    hlp::equal(keyval.first, "content-length", hlp::ignore_case())) {
                    cout << "warning: header " << keyval.first << " is ignored\n";
                    return false;
                }
                return true;
            },
            false,
            [&](auto& str) {
                hlp::appends(str, "Host: ", req.uri.host_port(), "\r\n");
            });
        if (!res) {
            msg << Done{
                .uri = std::move(req),
                .msg = "failed to render header",
                .layer = "http",
                .code = -1,
            };
            return false;
        }
        size_t w = 0;
        if (!cnet::write(client, buf.data(), buf.size(), &w)) {
            msg << Done{
                .uri = std::move(req),
                .msg = "failed to write request",
                .layer = "transport",
                .code = cnet::get_error(client),
            };
            return false;
        }
        buf.clear();
        h1::StatusCode status;
        Header response;
        wrap::string body;
        bool transport = false;
        res = h1::read_response<wrap::string>(buf, status, response, body, [&](auto& seq, size_t require, bool end) {
            num::Array<1024, char> tmp;
            while (tmp.size() == 0) {
                if (!cnet::read(client, tmp.buf, tmp.capacity(), &tmp.i)) {
                    transport = true;
                    return false;
                }
                hlp::append(buf, tmp);
                if (tmp.size() < tmp.capacity() || end) {
                    break;
                }
            }
            return true;
        });
        if (!res) {
            msg << Done{
                .uri = std::move(req),
                .msg = "failed to read header",
                .layer = transport ? "transport" : "http",
                .code = cnet::get_error(client),

            };
            return false;
        }
        msg << Done{
            .uri = std::move(req),
            .msg = "http1 succeeded",
            .layer = "http",
            .code = 0,
            .status = status,
            .h = std::move(response),
            .pool = client,
            .httpver = 1,
        };
        return true;
    }

    void request(th::RecvChan<UriWithTag> uri, th::SendChan<Done> msg, Flags flags) {
        UriWithTag req;
        uri.set_blocking(true);
        while (uri >> req) {
            cnet::CNet* client = cnet::tcp::create_client();
            const char* port = req.uri.port.size() ? req.uri.port.c_str() : req.uri.scheme.c_str();
            cnet::tcp::set_hostport(client, req.uri.host.c_str(), port);
            cnet::tcp::set_ip_version(client, req.tagcmd.ipver);
            cnet::tcp::set_connect_timeout(client, flags.timeout);
            cnet::tcp::set_recieve_timeout(client, flags.timeout);
            if (!cnet::open(client)) {
                msg << Done{
                    .uri = std::move(req),
                    .msg = "failed to open connection of tcp",
                    .layer = "tcp",
                    .code = cnet::get_error(client),
                };
                continue;
            }
            bool res = false;
            if (req.uri.scheme == "https") {
                auto ssl = cnet::ssl::create_client();
                cnet::set_lowlevel_protocol(ssl, client);
                cnet::ssl::set_alpn(ssl, "\x02h2\x08http/1.1");
                cnet::ssl::set_certificate_file(ssl, flags.cert.c_str());
                cnet::ssl::set_host(ssl, req.uri.host.c_str());
                cnet::ssl::set_timeout(ssl, flags.timeout);
                if (!cnet::open(ssl)) {
                    msg << Done{
                        .uri = std::move(req),
                        .msg = "failed to open connection of tls",
                        .layer = "tls",
                        .code = cnet::get_error(ssl),
                    };
                    continue;
                }
                auto alpn = cnet::ssl::get_alpn_selected(ssl);
                if (!alpn || alpn[1] == 't') {
                    res = http1(client, msg, req, flags);
                }
                else {
                    res = http2(client, msg, req);
                }
            }
            else {
                res = http1(client, msg, req, flags);
            }
            if (!res) {
                cnet::delete_cnet(client);
            }
        }
    }

    int http_do(subcmd::RunCommand& ctx, wrap::vector<UriWithTag>& uris) {
        Flags flags;
        flags.timeout = ctx.option().value_or_not<std::int64_t>("timeout", -1);
        auto [w, r] = th::make_chan<UriWithTag>();
        auto [mw, mr] = th::make_chan<Done>();
        for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
            std::thread(request, r, mw, Flags{flags});
        }
    }
}  // namespace netutil
