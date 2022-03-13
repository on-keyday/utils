/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net_util/uri.h"

using namespace utils;

namespace netutil {
    wrap::string* cacert;
    bool* h2proto;
    bool* uricheck;
    void httpreq_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("httpreq", httpreq, "http request", "[option] <url>...");
        auto& opt = subcmd->option();
        common_option(*subcmd);
        cacert = opt.String<wrap::string>("c,cacert", "./cacert", "cacert file", "FILE");
        h2proto = opt.Bool("2,http2", false, "use http2 protocol");
        uricheck = opt.Bool("u,uri-check", false, "check url whether it's parsable");
    }

    int preprocess_uri(subcmd::RunCommand& ctx, wrap::vector<net::URI>& uris) {
        for (auto& v : ctx.arg()) {
            net::URI uri;
            net::rough_uri_parse(v, uri);
            if (uri.other.size()) {
                cout << ctx.cuc() << ": error: " << v << " is not parsable as url\n";
                return -1;
            }
            uris.push_back(std::move(uri));
        }
        net::URI prev;
        prev.scheme = "http";
        for (size_t i = 0; i < uris.size(); i++) {
            auto& uri = uris[i];
            auto& raw = ctx.arg()[i];
            if (!uri.scheme.size()) {
                uri.scheme = prev.scheme;
            }
            else if (uri.scheme != "http" && uri.scheme != "https") {
                cout << ctx.cuc() << ": error: " << raw << ": uri scheme " << uri.scheme << " is not surpported\n";
                return -1;
            }
            if (!uri.host.size()) {
                if (!prev.host.size()) {
                    cout << ctx.cuc() << ": error: " << raw << ": no host name is provided. need least one host name.\n";
                    return -1;
                }
            }
            else {
                prev.host = uri.host;
            }
            if (!uri.path.size()) {
                uri.path = "/";
            }
        }
        return 0;
    }

    int httpreq(subcmd::RunCommand& ctx) {
        if (*help) {
            cout << ctx.Usage(mode);
            return 1;
        }
        if (ctx.arg().size() == 0) {
            cout << ctx.cuc() << ": require url\n";
            return 2;
        };
        wrap::vector<net::URI> uris;
        auto err = preprocess_uri(ctx, uris);
        if (err != 0) {
            return err;
        }
        if (*uricheck) {
            cout << ctx.cuc() << ": uri are all parsable\n";
            return 1;
        }
        return -1;
    }
}  // namespace netutil