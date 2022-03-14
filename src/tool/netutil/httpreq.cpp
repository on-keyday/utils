/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net_util/uri.h"
#include "../../include/net_util/punycode.h"
#include "../../include/net_util/urlencode.h"

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

    enum class EncodeFlag {
        host = 0x1,
        path = 0x2,
    };

    DEFINE_ENUM_FLAGOP(EncodeFlag);
    EncodeFlag* encflag;

    void show_uri(wrap::vector<net::URI>& uri, wrap::vector<wrap::string>& raw) {
        auto js = json::convert_to_json<json::OrderedJSON>(uri);
        size_t idx = 0;
        for (auto& v : json::as_array(js)) {
            js.abegin();
            v["raw"] = raw[idx];
            idx++;
        }
        cout << json::to_string<wrap::string>(js, json::FmtFlag::last_line | json::FmtFlag::unescape_slash);
    }

    bool encode_uri(subcmd::RunCommand& ctx, wrap::vector<net::URI>& uris) {
        for (auto& uri : uris) {
            if (any(*encflag & EncodeFlag::host)) {
                wrap::string encoded;
                if (!net::punycode::encode_host(uri.host, encoded)) {
                    cout << ctx.cuc() << ": error: failed to encode host\n";
                }
                uri.host = std::move(encoded);
            }
            if (any(*encflag & EncodeFlag::path)) {
                wrap::string encoded;
                if (!net::urlenc::encode(uri.path, encoded, net::urlenc::encodeURIComponent())) {
                    cout << ctx.cuc() << ": error: failed to encode path\n";
                    return false;
                }
                uri.path = std::move(encoded);
                if (!net::urlenc::encode(uri.query, encoded, net::urlenc::encodeURI())) {
                    cout << ctx.cuc() << ": error: failed to encode path\n";
                    return false;
                }
                uri.query = std::move(uri.query);
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
        encflag = urps->option().FlagSet("H,encode-host", EncodeFlag::host, "encode host with punycode");
        urps->option().VarFlagSet(encflag, "P,encode-path", EncodeFlag::path, "encode path with urlencode");
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
