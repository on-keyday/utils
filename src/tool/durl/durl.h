/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cmdline/subcmd/cmdcontext.h>
#include <wrap/cout.h>
#include <json/convert_json.h>
#include <json/json_export.h>
#include <fnet/util/uri.h>

namespace durl {
    namespace subcmd = utils::cmdline::subcmd;
    namespace opt = utils::cmdline::option;
    extern utils::wrap::UtfOut& cout;
    extern utils::wrap::UtfOut& cerr;
    struct URIOption {
        bool punycode = false;
        bool path_urlenc = false;
        bool query_urlenc = false;
        bool strict_relative_path = true;
        bool detect_scheme_path_to_host_port = true;
        bool https_parse_prefix = false;
        bool from_json(auto&& js, auto flag) {
            JSON_PARAM_BEGIN(*this, js)
            FROM_JSON_OPT(punycode, "encode_scheme", flag)
            FROM_JSON_OPT(path_urlenc, "encode_path", flag)
            FROM_JSON_OPT(query_urlenc, "encode_query", flag)
            FROM_JSON_OPT(strict_relative_path, "strict_relative", flag)
            FROM_JSON_OPT(detect_scheme_path_to_host_port, "scheme_path_to_host_port", flag)
            FROM_JSON_OPT(https_parse_prefix, "https_prefix", flag)
            JSON_PARAM_END()
        }

        void bind(subcmd::RunCommand& cmd, bool enable_short = true) {
            auto& opt = cmd.option();
            auto ens = enable_short;
            opt.VarBool(&punycode,
                        ens ? "s,encode-scheme" : "encode-scheme", "encode scheme as punycode");
            opt.VarBool(&path_urlenc,
                        ens ? "p,encode-path" : "encode-path", "encode path as url encoding");
            opt.VarBool(&query_urlenc,
                        ens ? "q,encode-query" : "encode-query", "encode query as url encoding");
            opt.VarBool(&strict_relative_path,
                        ens ? "r,strict-relative" : "strict-relative", "disallow relative path fundamentally");
            opt.VarBool(&detect_scheme_path_to_host_port,
                        ens ? "d,scheme-path-to-host-port" : "scheme-path-to-host-port", "detect scheme:path as host:port ");
            opt.VarBool(&https_parse_prefix,
                        ens ? "S,https-prefix" : "https-prefix", "make default parse prefix https");
        }
    };

    struct HTTPOption {
        URIOption uri;
        constexpr HTTPOption() {
            uri.punycode = true;
            uri.path_urlenc = true;
            uri.query_urlenc = true;
        }
        bool from_json(auto&& js, auto flag) {
            if (!uri.from_json(js, flag)) {
                return false;
            }
        }

        void bind(subcmd::RunCommand& cmd) {
            uri.bind(cmd, false);
        }
    };

    extern URIOption uriopt;

    int http_request(subcmd::RunCommand& cmd);
    int uri_parse(subcmd::RunCommand& cmd);

    extern HTTPOption httpopt;

    struct GlobalOption {
        bool help = false;
        std::string ssl;
        std::string crypto;

        static int show_usage(subcmd::RunCommand& cmd) {
            cout << cmd.Usage(opt::ParseFlag::assignable_mode, true);
            return 1;
        }
        void bind(subcmd::RunContext& ctx) {
            auto& opt = ctx.option();
            opt.VarString(&ssl, "ssl", "set libssl location", "LIB");
            opt.VarString(&crypto, "crypto", "set libcrypto location", "LIB");
            bind_help(ctx);
            ctx.SetHelpRun(show_usage);
            auto req = ctx.SubCommand("req", http_request, "http request");
            bind_help(*req);
            httpopt.bind(*req);
            auto ps = ctx.SubCommand("parse", uri_parse, "uri parse");
            bind_help(*ps);
            uriopt.bind(*ps);
        }

        void bind_help(subcmd::RunCommand& ctx) {
            ctx.set_help_ptr(&help);
            ctx.option().VarBool(&help, "h,help", "show this help");
        }

        bool from_json(auto&& js, auto flag) {
            JSON_PARAM_BEGIN(*this, js)
            FROM_JSON_OPT(ssl, "ssl", flag)
            FROM_JSON_OPT(crypto, "crypto", flag)
            JSON_PARAM_END()
        }
    };

    extern GlobalOption global;
    namespace uri = utils::uri;
    using URI = uri::URI<std::string>;
    struct URIToJSON {
        URI uri;

        bool to_json(auto&& js) const {
            JSON_PARAM_BEGIN(*this, js)
            TO_JSON_PARAM(uri.scheme, "scheme")
            TO_JSON_PARAM(uri.authority, "authority")
            TO_JSON_PARAM(uri.userinfo, "userinfo")
            TO_JSON_PARAM(uri.hostname, "hostname")
            TO_JSON_PARAM(uri.port, "port")
            TO_JSON_PARAM(uri.path, "path")
            TO_JSON_PARAM(uri.query, "query")
            TO_JSON_PARAM(uri.fragment, "fragment")
            js["composition"] = uri::composition_uri(uri);
            JSON_PARAM_END()
        }
    };

    void printerr(subcmd::RunCommand& cmd, auto&&... args) {
        cerr << cmd.cuc() << ": error: ";
        (cerr << ... << args) << "\n";
    }

    using URIVec = std::vector<URIToJSON>;
    int do_request(URIVec& uris);

}  // namespace durl
