/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net_util/uri_normalize.h"

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
    bool to_json(const UriWithTag& wtag, auto& json) {
        return to_json(wtag.uri, json);
    }
    wrap::string* cacert;
    bool* h2proto;
    bool* uricheck;
    bool* tidy;
    bool* show_encoded;
    bool* show_timer;

    net::NormalizeFlag normflag;
    TagFlag tagflag;

    void show_uri(auto& uri, wrap::vector<wrap::string>& raw, auto&& cb) {
        auto js = json::convert_to_json<json::OrderedJSON>(uri);
        size_t idx = 0;
        for (auto& v : json::as_array(js)) {
            v["raw"] = raw[idx];
            if (any(normflag & ~net::NormalizeFlag::human_friendly) && *show_encoded) {
                auto& u = cb(uri[idx]);
                v[any(normflag & net::NormalizeFlag::human_friendly) ? "decoded" : "encoded"] = u.to_string();
            }
            idx++;
        }
        cout << json::to_string<wrap::string>(js, json::FmtFlag::last_line | json::FmtFlag::unescape_slash);
    }

    void parse_uri(wrap::string& v, net::URI& uri, bool tidy) {
        net::rough_uri_parse(v, uri);
        if (tidy) {
            net::uri_tidy(uri);
        }
    }

    bool preprocese_a_uri(wrap::internal::Pack&& cout, wrap::string cuc, wrap::string& raw, UriWithTag& utag, UriWithTag& pretag) {
        auto& uri = utag.uri;
        auto& prev = pretag.uri;
        parse_uri(raw, uri, true);
        if (auto msg = error_msg(net::normalize_uri(uri, normflag))) {
            cout << cuc << ": " << raw << ": error: " << msg << "\n";
            return false;
        }
        if (uri.other.size()) {
            cout << cuc << ": error: " << raw << " is not parsable as http url\n";
            return false;
        }
        if (uri.user.size() || uri.password.size()) {
            cout << cuc << ": " << raw << ": error: user and password are not settable for http url\n";
            return false;
        }

        if (!uri.scheme.size()) {
            uri.scheme = prev.scheme;
        }
        else if (uri.scheme != "http" && uri.scheme != "https") {
            cout << cuc << ": error: " << raw << ": uri scheme " << uri.scheme << " is not surpported\n";
            return false;
        }
        else if (uri.scheme.size() && !uri.has_double_slash) {
            cout << cuc << ": error: " << raw << ": invald url format; need // after scheme.\n";
            return false;
        }
        if (uri.port.size()) {
            std::uint16_t port = 0;
            if (!number::parse_integer(uri.port, port)) {
                cout << cuc << ": error: " << raw << ": port number is not acceptable.\n";
                return false;
            }
        }
        if (!uri.host.size()) {
            if (!prev.host.size()) {
                cout << cuc << ": error: " << raw << ": no host name is provided. need least one host name.\n";
                return false;
            }
            uri.host = prev.host;
            uri.port = prev.port;
        }
        else {
            prev.host = uri.host;
            prev.port = uri.port;
        }
        if (!uri.path.size()) {
            uri.path = "/";
        }
        else if (uri.path[0] != '/') {
            auto tmp = std::move(uri.path);
            uri.path = prev.path;
            if (!helper::ends_with(uri.path, "/")) {
                uri.path.push_back('/');
            }
            uri.path += tmp;
        }
        utag.tagcmd.flag = tagflag;
        if (uri.tag.size() && helper::sandwiched(uri.tag, "(", ")")) {
            wrap::string err;
            if (!parse_tagcommand(uri.tag, utag.tagcmd, err)) {
                cout << cuc << ": tagparse: error: " << err;
                return false;
            }
        }
        uri.has_double_slash = true;
        return true;
    }

    int uriparse(subcmd::RunCommand& ctx) {
        if (*help) {
            cout << ctx.Usage(mode);
            return 1;
        }
        wrap::vector<net::URI> uris;
        for (auto& v : ctx.arg()) {
            net::URI uri;
            parse_uri(v, uri, *tidy);
            auto err = error_msg(net::normalize_uri(uri, normflag));
            if (err) {
                cout << ctx.cuc() << ": error: " << err << "\n";
            }
            uris.push_back(std::move(uri));
        }
        show_uri(uris, ctx.arg(), [](auto& v) -> net::URI& {
            return v;
        });
        return 0;
    }

    void httpreq_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("httpreq", httpreq, "http request", "[option] <url>...");
        auto& opt = subcmd->option();
        common_option(*subcmd);
        cacert = opt.String<wrap::string>("c,cacert", "./cacert", "cacert file", "FILE");
        h2proto = opt.Bool("2,http2", false, "use http2 protocol");
        opt.VarFlagSet(&tagflag, "r,redirect", TagFlag::redirect, "auto redirect");
        show_timer = opt.Bool("t,timer", false, "show timer");
        auto urps = ctx.SubCommand("uriparse", uriparse, "parse uri and output as json", "<uri>...");
        common_option(*urps);
        tidy = urps->option().Bool("t,tidy", false, "make parsed uri tidy");
        urps->option().VarFlagSet(&normflag, "H,encode-host", net::NormalizeFlag::host, "encode/decode host with punycode");
        urps->option().VarFlagSet(&normflag, "P,encode-path", net::NormalizeFlag::path, "encode/decode path with urlencode");
        urps->option().VarFlagSet(&normflag, "f,human-friendly", net::NormalizeFlag::human_friendly, "encode/decode human-friendly");
        show_encoded = urps->option().Bool("e,show-encoded", false, "show encoded/decoded url");
    }

    int preprocess_uri(subcmd::RunCommand& ctx, wrap::vector<UriWithTag>& uris) {
        normflag = net::NormalizeFlag::host | net::NormalizeFlag::path;
        UriWithTag prev;
        prev.uri.scheme = "http";
        prev.uri.path = "/";
        for (size_t i = 0; i < ctx.arg().size(); i++) {
            UriWithTag uri;
            wrap::internal::Pack pack;
            if (!preprocese_a_uri(pack.pack(), ctx.cuc(), ctx.arg()[i], uri, prev)) {
                cout << pack.pack();
                return -1;
            }
            uris.push_back(std::move(uri));
        }
        if (*verbose) {
            cout << "--- verbose log begin ---\nuri parsed:\n";
            show_uri(uris, ctx.arg(), [](UriWithTag& tag) -> net::URI& {
                return tag.uri;
            });
            cout << "--- verbose log end ---\n";
        }
        return 0;
    }

    int httpreq(subcmd::RunCommand& ctx) {
        if (ctx.arg().size() == 0) {
            cout << ctx.cuc() << ": require url\n";
            return 2;
        };
        wrap::vector<UriWithTag> uris;
        auto err = preprocess_uri(ctx, uris);
        if (err != 0) {
            return err;
        }

        return http_do(ctx, uris);
    }
}  // namespace netutil
