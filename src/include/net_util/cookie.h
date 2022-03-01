/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// coolie - http coolie parser
// almost copied from socklib
#pragma once

#include "../wrap/lite/enum.h"
#include "date.h"
#include "../helper/strutil.h"
#include "../number/parse.h"
#include "../helper/appender.h"
#include "../number/to_string.h"
#include "../helper/equal.h"

namespace utils {
    namespace net {
        namespace cookie {
            enum class SameSite {
                default_mode,
                lax_mode,
                strict_mode,
                none_mode,
            };

            BEGIN_ENUM_STRING_MSG(SameSite, get_samesite)
            ENUM_STRING_MSG(SameSite::lax_mode, "Lax")
            ENUM_STRING_MSG(SameSite::strict_mode, "Strict")
            ENUM_STRING_MSG(SameSite::none_mode, "None")
            END_ENUM_STRING_MSG("")

            enum class CookieFlag {
                none,
                path_set = 0x1,
                domain_set = 0x2,
                not_allow_prefix_rule = 0x4,
            };

            DEFINE_ENUM_FLAGOP(CookieFlag);

            template <class String>
            struct Cookie {
                using string_t = String;
                string_t name;
                string_t value;

                string_t path;
                string_t domain;
                date::Date expires;

                int maxage = 0;
                bool secure = false;
                bool httponly = false;
                SameSite samesite = SameSite::default_mode;

                CookieFlag flag = CookieFlag::none;
            };

            enum class CookieError {
                none,
                no_cookie,
                no_keyvalue,
                no_attrkeyvalue,
                multiple_same_attr,
                unknown_samesite,
                invalid_url,
            };

            using CookieErr = wrap::EnumWrap<CookieError, CookieError::none, CookieError::no_cookie>;

            namespace internal {
                // hide old design pattern
                template <class String, template <class...> class Vec>
                struct CookieParser {
                    using string_t = String;
                    using cookie_t = Cookie<String>;

                    static void set_by_cookie_prefix(cookie_t& cookie) {
                        if (helper::starts_with(cookie.name, "__Secure-")) {
                            cookie.secure = true;
                        }
                        else if (helper::starts_with(cookie.name, "__Host-")) {
                            cookie.flag &= ~CookieFlag::domain_set;
                            cookie.path = "/";
                        }
                    }

                    static bool verify_cookie_prefix(cookie_t& cookie) {
                        if (helper::starts_with(cookie.name, "__Secure-")) {
                            if (!cookie.secure) {
                                cookie.flag |= CookieFlag::not_allow_prefix_rule;
                            }
                        }
                        else if (helper::starts_with(cookie.name, "__Host-")) {
                            if (cookie.path != "/" || any(cookie.flag & CookieFlag::domain_set)) {
                                cookie.flag |= CookieFlag::not_allow_prefix_rule;
                            }
                        }
                        return any(cookie.flag & CookieFlag::not_allow_prefix_rule);
                    }

                    template <class Cookies>
                    static CookieErr parse_cookie(const string_t& raw, Cookies& cookies) {
                        auto data = helper::split<string_t, Vec>(raw, "; ");
                        for (auto& v : data) {
                            auto keyval = helper::split<string_t, Vec>(v, "=", 1);
                            if (keyval.size() != 2) {
                                return CookieError::no_keyvalue;
                            }
                            auto cookie = cookie_t{.name = keyval[0], .value = keyval[1]};
                            set_by_cookie_prefix(cookie);
                            cookies.push_back(cookie);
                        }
                        return true;
                    }
                    template <class URL, class Raw>
                    static CookieErr parse(const Raw& raw, cookie_t& cookie, const URL& url) {
                        if (!url.host.size() || !url.path.size()) {
                            return CookieError::invalid_url;
                        }
                        auto data = helper::split<string_t, Vec>(raw, "; ");
                        if (data.size() == 0) {
                            return CookieError::no_cookie;
                        }
                        auto keyval = helper::split<string_t, Vec>(data[0], "=", 1);
                        if (keyval.size() != 2) {
                            return CookieError::no_keyvalue;
                        }

                        cookie.name = keyval[0];
                        cookie.value = keyval[1];
                        constexpr auto cmp = [](auto& a, auto& b) {
                            return helper::equal(a, b, helper::ignore_case());
                        };
                        for (auto& attr : data) {
                            if (cmp(attr, "HttpOnly")) {
                                if (cookie.httponly) {
                                    return CookieError::multiple_same_attr;
                                }
                                cookie.httponly = true;
                            }
                            else if (cmp(attr, "Secure")) {
                                if (cookie.secure) {
                                    return CookieError::multiple_same_attr;
                                }
                                cookie.secure = true;
                            }
                            else {
                                auto elm = helper::split<string_t, Vec>(attr, "=", 1);
                                if (elm.size() != 2) {
                                    return CookieError::no_attrkeyvalue;
                                }
                                if (cmp(elm[0], "Expires")) {
                                    if (cookie.expires != date::Date{}) {
                                        return CookieError::multiple_same_attr;
                                    }
                                    date::internal::DateParser<string_t, Vec>::replace_to_parse(elm[1]);
                                    if (!date::encode(elm[1], cookie.expires)) {
                                        cookie.expires = date::invalid_date;
                                    }
                                }
                                else if (cmp(elm[0], "Path")) {
                                    if (cookie.path.size()) {
                                        return CookieError::multiple_same_attr;
                                    }
                                    cookie.path = elm[1];
                                    cookie.flag |= CookieFlag::path_set;
                                }
                                else if (cmp(elm[0], "Domain")) {
                                    if (cookie.domain.size()) {
                                        return CookieError::multiple_same_attr;
                                    }
                                    cookie.domain = elm[1];
                                    cookie.flag |= CookieFlag::domain_set;
                                }
                                else if (cmp(elm[0], "SameSite")) {
                                    if (cookie.samesite != SameSite::default_mode) {
                                        return CookieError::multiple_same_attr;
                                    }
                                    if (cmp(elm[1], "Strict")) {
                                        cookie.samesite = SameSite::strict_mode;
                                    }
                                    else if (cmp(elm[1], "Lax")) {
                                        cookie.samesite = SameSite::lax_mode;
                                    }
                                    else if (cmp(elm[1], "None")) {
                                        cookie.samesite = SameSite::none_mode;
                                    }
                                    else {
                                        return CookieError::unknown_samesite;
                                    }
                                }
                                else if (cmp(elm[0], "Max-Age")) {
                                    if (cookie.maxage != 0) {
                                        return CookieError::multiple_same_attr;
                                    }
                                    number::parse_integer(elm[1], cookie.maxage);
                                }
                            }
                        }
                        verify_cookie_prefix(cookie);
                        if (!any(cookie.flag & CookieFlag::path_set)) {
                            cookie.path = url.path;
                        }
                        if (!any(cookie.flag & CookieFlag::domain_set)) {
                            cookie.domain = url.host;
                        }
                        return CookieError::none;
                    }

                    template <class Header, class URL, class Cookies>
                    static CookieErr parse_set_cookie(Header& header, Cookies& cookies, const URL& url) {
                        for (auto& h : header) {
                            if (helper::equal(get<0>(h), "Set-Cookie", helper::ignore_case())) {
                                cookie_t cookie;
                                auto err = parse(get<1>(h), cookie, url);
                                if (!err) return err;
                                cookies.push_back(std::move(cookie));
                            }
                        }
                        return true;
                    }
                };

                enum class PathDomainState {
                    none,
                    same_path = 0x01,
                    sub_path = 0x02,
                    same_origin = 0x10,
                    sub_origin = 0x20,
                };

                DEFINE_ENUM_FLAGOP(PathDomainState)

                template <class String>
                struct CookieWriter {
                    using string_t = String;
                    using cookie_t = Cookie<string_t>;

                   private:
                    static bool check_path_and_domain(cookie_t& info, cookie_t& cookie) {
                        if (any(cookie.flag & CookieFlag::domain_set)) {
                            if (info.domain.find(cookie.domain) == ~0) {
                                return false;
                            }
                        }
                        else {
                            if (info.domain != cookie.domain) {
                                return false;
                            }
                        }
                        if (any(cookie.flag & CookieFlag::path_set)) {
                            if (info.path.find(cookie.path) != 0) {
                                return false;
                            }
                        }
                        else {
                            if (info.path != cookie.path) {
                                return false;
                            }
                        }
                        return true;
                    }

                    static bool check_expires(cookie_t& info, cookie_t& cookie) {
                        time_t now = ::time(nullptr), prevtime = 0;
                        date::Date nowdate;
                        date::internal::TimeConvert::from_time_t(now, nowdate);
                        date::internal::TimeConvert::to_time_t(prevtime, info.expires);
                        if (cookie.maxage) {
                            if (cookie.maxage <= 0) {
                                return false;
                            }
                            if (prevtime + cookie.maxage < now) {
                                return false;
                            }
                        }
                        else if (cookie.expires != date::Date{} && cookie.expires != date::invalid_date) {
                            if (cookie.expires - nowdate <= 0) {
                                return false;
                            }
                        }
                        return true;
                    }

                   public:
                    template <class Cookies>
                    static bool write(string_t& towrite, cookie_t& info, Cookies& cookies) {
                        if (!info.domain.size() || !info.path.size() || info.expires == date::Date{} ||
                            info.expires == date::invalid_date) {
                            return false;
                        }

                        for (size_t i = 0; i < cookies.size(); i++) {
                            auto del_cookie = [&] {
                                cookies.erase(cookies.begin() + i);
                                i--;
                            };
                            cookie_t& cookie = cookies[i];
                            if (cookie.secure && !info.secure) {
                                continue;
                            }
                            if (!check_expires(info, cookie)) {
                                del_cookie();
                                continue;
                            }
                            if (!check_path_and_domain(info, cookie)) {
                                continue;
                            }
                            if (towrite.size()) {
                                helper::append(towrite, "; ");
                            }
                            helper::append(towrite, cookie.name);
                            towrite.push_back('=');
                            helper::append(towrite, cookie.value);
                        }
                        return towrite.size() != 0;
                    }

                    template <class Cookies>
                    static bool write_set_cookie(string_t& towrite, cookie_t& cookie) {
                        helper::append(towrite, cookie.name);
                        towrite.push_back('=');
                        helper::append(towrite, cookie.value);
                        if (cookie.maxage) {
                            helper::append(towrite, "; MaxAge=");
                            number::to_string(towrite, cookie.maxage);
                        }
                        else if (cookie.expires != date::Date{}) {
                            if (cookie.expires == date::invalid_date) {
                                helper::append(towrite, "; Expires=-1");
                            }
                            else {
                                helper::append(towrite, "; Expires=");
                                date::decode(cookie.expires, towrite);
                            }
                        }

                        if (cookie.domain.size()) {
                            helper::append(towrite, "; Domain=");
                            helper::append(towrite, cookie.domain);
                        }
                        if (cookie.path.size()) {
                            helper::append(towrite, "; Path=");
                            helper::append(towrite, cookie.path);
                        }
                        if (cookie.secure) {
                            helper::append(towrite, "; Secure");
                        }
                        if (cookie.httponly) {
                            helper::append(towrite, "; HttpOnly");
                        }
                        if (cookie.samesite != SameSite::default_mode) {
                            helper::append(towrite, "; SameSite=");
                            helper::append(towrite, get_samesite(cookie.samesite));
                        }
                        return true;
                    }
                };
            }  // namespace internal

            template <class In, class String, class URL, template <class...> class Vec = wrap::vector>
            CookieErr parse(In&& src, Cookie<String>& out, const URL& url) {
                return internal::CookieParser<String, Vec>::parse(src, out, url);
            }

            template <class String = wrap::string, template <class...> class Vec = wrap::vector, class Header, class Cookies, class URL>
            CookieErr parse_set_cookie(Header&& src, Cookies& cookies, const URL& url) {
                return internal::CookieParser<String, Vec>::parse_set_cookie(src, cookies, url);
            }

            template <class String, class Cookies>
            CookieErr render_cookie(String& out, Cookie<String>& info, Cookies& cookies) {
                return internal::CookieWriter<String>::write(out, info, cookies);
            }

        }  // namespace cookie

    }  // namespace net
}  // namespace utils
