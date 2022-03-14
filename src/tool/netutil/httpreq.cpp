/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net_util/uri_normalize.h"
#include "../../include/net_util/uri.h"

using namespace utils;

namespace utils::net {
    bool to_json(const URI& uri, auto& json) {
        JSON_PARAM_BEGIN(uri, json)
        TO_JSON_PARAM(scheme, "scheme")
        TO_JSON_PARAM(has_double_slash, "has_double_slash")
        TO_JSON_PARAM(user, "user")
        TO_JSON_PARAM(password, "password")
        TO_JSON_PARAM(host, "host")
        TO_JSON_PARAM(port, "port")
        TO_JSON_PARAM(path, "path")
        TO_JSON_PARAM(query, "query")
        TO_JSON_PARAM(tag, "tag")
        TO_JSON_PARAM(other, "opaque")
        JSON_PARAM_END()
    }
}  // namespace utils::net

namespace netutil {
    wrap::string* cacert;
    bool* h2proto;
    bool* uricheck;
    bool* tidy;
    bool* show_encoded;

    net::NormalizeFlag normflag;

    void show_uri(wrap::vector<net::URI>& uri, wrap::vector<wrap::string>& raw) {
        auto js = json::convert_to_json<json::OrderedJSON>(uri);
        size_t idx = 0;
        for (auto& v : json::as_array(js)) {
            v["raw"] = raw[idx];
            if (any(normflag & ~net::NormalizeFlag::human_friendly) && *show_encoded) {
                auto& u = uri[idx];
                v[any(normflag & net::NormalizeFlag::human_friendly) ? "decoded" : "encoded"] = u.to_string();
            }
            idx++;
        }
        cout << json::to_string<wrap::string>(js, json::FmtFlag::last_line | json::FmtFlag::unescape_slash);
    }

    bool encode_uri(subcmd::RunCommand& ctx, wrap::vector<net::URI>& uris) {
        for (auto& uri : uris) {
            auto err = net::normalize_uri(uri, normflag);
            if (auto msg = error_msg(err)) {
                cout << ctx.cuc() << ": error: " << msg << "\n";
                return false;
            }
        }
        return true;
    }

    bool parse_uri(subcmd::RunCommand& ctx, wrap::vector<net::URI>& uris, bool use_on_http, bool tidy) {
        for (auto& v : ctx.arg()) {
            net::URI uri;
            net::rough_uri_parse(v, uri);
            if (use_on_http && uri.other.size()) {
                cout << ctx.cuc() << ": error: " << v << " is not parsable as url\n";
                return false;
            }
            if (tidy) {
                net::uri_tidy(uri);
            }
            uris.push_back(std::move(uri));
        }
        return true;
    }

    int preprocess_uri(subcmd::RunCommand& ctx, wrap::vector<net::URI>& uris) {
        if (!parse_uri(ctx, uris, true, true)) {
            return -1;
        }
        if (*verbose) {
            cout << "verbose url...\n";
            show_uri(uris, ctx.arg());
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
            else if (uri.scheme.size() && !uri.has_double_slash) {
                cout << ctx.cuc() << ": error: " << raw << ": invald url format; need // after scheme.\n";
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

    int uriparse(subcmd::RunCommand& ctx) {
        if (*help) {
            cout << ctx.Usage(mode);
            return 1;
        }
        wrap::vector<net::URI> uris;
        parse_uri(ctx, uris, false, *tidy);
        if (!encode_uri(ctx, uris)) {
            return -1;
        }
        show_uri(uris, ctx.arg());
        return 0;
    }

    void httpreq_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("httpreq", httpreq, "http request", "[option] <url>...");
        auto& opt = subcmd->option();
        common_option(*subcmd);
        cacert = opt.String<wrap::string>("c,cacert", "./cacert", "cacert file", "FILE");
        h2proto = opt.Bool("2,http2", false, "use http2 protocol");
        auto urps = ctx.SubCommand("uriparse", uriparse, "parse uri and output as json", "<uri>...");
        common_option(*urps);
        tidy = urps->option().Bool("t,tidy", false, "make parsed uri tidy");
        urps->option().VarFlagSet(&normflag, "H,encode-host", net::NormalizeFlag::host, "encode/decode host with punycode");
        urps->option().VarFlagSet(&normflag, "P,encode-path", net::NormalizeFlag::path, "encode/decode path with urlencode");
        urps->option().VarFlagSet(&normflag, "f,human-friendly", net::NormalizeFlag::human_friendly, "encode/decode human-friendly");
        show_encoded = urps->option().Bool("e,show-encoded", false, "show encoded/decoded url");
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
            if (!*quiet) {
                cout << ctx.cuc() << ": uri are all parsable\n";
            }
            return 1;
        }

        return -1;
    }
}  // namespace netutil
