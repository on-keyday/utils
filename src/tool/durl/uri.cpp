/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "durl.h"

#include <json/to_string.h>
#include <fnet/util/punycode.h>
#include <fnet/util/urlencode.h>

namespace durl {

    URIOption uriopt;
    HTTPOption httpopt;

    uri::ParseError parse_uri(URI& uri, auto& url, URIOption& opt) {
        const char* parse_prefix = uriopt.https_parse_prefix ? "https://" : "http://";
        auto err = uri::parse_ex(uri, url);
        if (err != uri::ParseError::none) {
            url = parse_prefix + url;  // retry
            uri = {};
            err = uri::parse_ex(uri, url);
            if (err != uri::ParseError::none) {
                return err;
            }
            return uri::ParseError::none;
        }
        if ((utils::strutil::contains(uri.scheme, ".") ||
             uri.scheme == "localhost:") &&
            utils::number::is_number(uri.path) &&
            uri.hostname == "") {
            auto tmp = std::move(uri);
            url = parse_prefix + url;
            err = uri::parse_ex(uri, url);
            if (err != uri::ParseError::none) {
                uri = std::move(tmp);
            }
        }
        if (opt.strict_relative_path &&
            uri.hostname == "" &&
            !utils::strutil::starts_with(uri.path, "/") &&
            utils::strutil::contains(uri.path, "/")) {
            auto tmp = std::move(uri);
            url = parse_prefix + url;
            err = uri::parse_ex(uri, url);
            if (err != uri::ParseError::none) {
                uri = std::move(tmp);
            }
        }
        return uri::ParseError::none;
    }

    bool is_ascii(auto& str) {
        return utils::strutil::is_valid(str, true, utils::number::is_ascii_non_control<std::uint8_t>);
    }

    int uri_parse_impl(std::vector<URIToJSON>& urls, subcmd::RunCommand& cmd, URIOption& opt) {
        for (auto& url : cmd.arg()) {
            URI uri;
            if (auto err = parse_uri(uri, url, opt); err != uri::ParseError::none) {
                printerr(cmd, "failed to parse uri ", url, " : ", uri::err_msg(err));
                return -1;
            }
            if (opt.punycode && !utils::strutil::contains(uri.hostname, "xn--")) {
                std::string host;
                if (!utils::punycode::encode_host(uri.hostname, host)) {
                    printerr(cmd, "punycode encoding failed with", uri.hostname);
                    return -1;
                }
                uri.hostname = std::move(host);
            }
            if (opt.path_urlenc &&
                (!is_ascii(uri.path) ||
                 !utils::strutil::contains(uri.path, "%"))) {
                std::string path;
                if (!utils::urlenc::encode(uri.path, path, utils::urlenc::encodeURI())) {
                    printerr(cmd, "url encoding failed with ", uri.path);
                    return -1;
                }
                uri.path = std::move(path);
            }
            if (opt.query_urlenc &&
                (!is_ascii(uri.path) ||
                 !utils::strutil::contains(uri.query, "%"))) {
                std::string query;
                if (!utils::urlenc::encode(uri.query, query, utils::urlenc::encodeURI())) {
                    printerr(cmd, "url encoding failed with ", uri.path);
                    return -1;
                }
                uri.query = std::move(query);
            }
            urls.push_back({std::move(uri)});
        }
        return 0;
    }

    int uri_parse(subcmd::RunCommand& cmd) {
        URIVec urls;
        auto err = uri_parse_impl(urls, cmd, uriopt);
        if (err != 0) {
            return err;
        }
        cout << utils::json::to_string<std::string>(utils::json::convert_to_json<utils::json::OrderedJSON>(urls), utils::json::FmtFlag::unescape_slash) << "\n";
        return 0;
    }

    int http_request(subcmd::RunCommand& cmd) {
        URIVec urls;
        auto err = uri_parse_impl(urls, cmd, httpopt.uri);
        if (err != 0) {
            return err;
        }
        return 0;
    }
}  // namespace durl
