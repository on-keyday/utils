/*license*/

// number -  matching number implementation
// almost copied from commonlib
#pragma once

#include "../../wrap/lite/smart_ptr.h"

#include "context.h"

#include <cctype>

#include "../../helper/strutil.h"

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String>
            struct FloatReadPoint {
                using token_t = tknz::Token<String>;
                String str;
                wrap::shared_ptr<token_t> beforedot;
                wrap::shared_ptr<token_t> dot;
                wrap::shared_ptr<token_t> afterdot;
                wrap::shared_ptr<token_t> sign;
                wrap::shared_ptr<token_t> aftersign;
                size_t stack = 0;

                wrap::shared_ptr<token_t>& exists() {
                    if (beforedot) {
                        return beforedot;
                    }
                    else if (dot) {
                        return dot;
                    }
                    else if (afterdot) {
                        return afterdot;
                    }
                    else if (sign) {
                        return sign;
                    }
                    else {
                        return aftersign;
                    }
                }
            };

            template <class String, template <class...> class Vec>
            void get_floatpoint(Reader<String>& cr, wrap::shared_ptr<Element<String, Vec>>& v, FloatReadPoint<String>& pt) {
                while (true) {
                    auto e = cr.get();
                    if (!e) {
                        break;
                    }
                    if (!pt.dot && !pt.sign && e->has(".")) {
                        pt.dot = e;
                        pt.str += e->to_string();
                        cr.consume();
                        continue;
                    }
                    if (!pt.sign && (e->has("+") || e->has("-"))) {
                        pt.sign = e;
                        pt.str += e->to_string();
                        cr.consume();
                        continue;
                    }
                    if (!e->is(tknz::TokenKind::identifier)) {
                        break;
                    }
                    if (!pt.dot && !pt.sign && !pt.beforedot) {
                        pt.beforedot = e;
                    }
                    else if (pt.dot && !pt.afterdot) {
                        pt.afterdot = e;
                    }
                    else if (pt.sign && !pt.aftersign) {
                        pt.aftersign = e;
                    }
                    else {
                        break;
                    }
                    pt.str += e->to_string();
                    cr.consume();
                }
                pt.stack = cr.count;
            }

            bool check_int_str(auto& idv, int i, int base, int& allowed) {
                for (; i < idv.size(); i++) {
                    if (idv[i] < 0 || 0xff < idv[i]) {
                        allowed = i;
                        return true;
                    }
                    if (base == 16) {
                        if (!std::isxdigit(idv[i])) {
                            allowed = i;
                            return true;
                        }
                    }
                    else if (base == 8) {
                        if (!std::isdigit(idv[i]) || idv[i] == '8' || idv[i] == '9') {
                            allowed = i;
                            return true;
                        }
                    }
                    else if (base == 2) {
                        if (idv[i] != '0' && idv[i] != '1') {
                            allowed = i;
                            return true;
                        }
                    }
                    else {
                        if (!std::isdigit(idv[i])) {
                            allowed = i;
                            return true;
                        }
                    }
                }
                allowed = idv.size();
                return true;
            }

            template <class String, template <class...> class Vec>
            int parse_float(Context<String, Vec>& ctx, std::shared_ptr<Element<String, Vec>>& v, String& token, bool onlyint = false) {
                auto cr = ctx.r.from_current();
                FloatReadPoint<String> pt;
                auto report = [&](const auto& msg) {
                    ctx.err.packln("error:", msg);
                    ctx.errat = pt.exists();
                    ctx.errelement = v;
                };
                get_floatpoint(cr, v, pt);
                if (pt.str.size() == 0) {
                    report("expect number but not");
                    return 0;
                }
                token = pt.str;
                int base = 10;
                int i = 0;
                if (pt.dot && !pt.beforedot && !pt.afterdot) {
                    report("invalid float number format");
                    return 0;
                }
                if (helper::starts_with(pt.str, "0x") || helper::starts_with(pt.str, "0X")) {
                    if (pt.str.size() >= 3 && pt.str[2] == '.' && !pt.afterdot) {
                        report("invalid hex float fromat. token is " + pt.str);
                        return 0;
                    }
                    base = 16;
                    i = 2;
                }
                else if (helper::starts_with(pt.str, "0b") || helper::starts_with(pt.str, "0B")) {
                    base = 2;
                    i = 2;
                }
                else if (helper::starts_with(pt.str, "0o") || helper::starts_with(pt.str, "0O")) {
                    base = 8;
                    i = 2;
                }
                int allowed = false;
                internal::check_int_str(pt.str, i, base, allowed);
                if (pt.str.size() == allowed) {
                    if (!pt.beforedot || pt.dot || pt.sign) {
                        report("parser is broken");
                        return -1;
                    }
                    ctx.r.current = pt.beforedot->next;
                    ctx.r.count = pt.stack;
                    /*if (!callback(pt.exists(), r, pt.str, MatchingType::integer)) {
                    return -1;
                }*/
                    return 1;
                }
                if (base == 2 || base == 8 || onlyint) {
                    report("expect integer but token is " + pt.str);
                    return 0;
                }
                if (pt.str[allowed] == '.') {
                    if (!pt.dot) {
                        report("parser is broken");
                        return -1;
                    }
                    i = allowed + 1;
                }
                internal::check_int_str(pt.str, i, base, allowed);
                if (pt.str.size() == allowed) {
                    if (pt.sign) {
                        report("parser is broken");
                        return -1;
                    }
                    if (pt.afterdot) {
                        ctx.r.current = pt.afterdot->next;
                    }
                    else if (pt.dot) {
                        ctx.r.current = pt.dot->next;
                    }
                    else {
                        report("parser is broken");
                        return -1;
                    }
                    ctx.r.count = pt.stack;
                    /*if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                    return -1;
                }*/
                    return 1;
                }
                if (base == 16) {
                    if (pt.str[allowed] != 'p' && pt.str[allowed] != 'P') {
                        report("invalid hex float format. token is " + pt.str);
                        return 0;
                    }
                }
                else {
                    if (pt.str[allowed] != 'e' && pt.str[allowed] != 'E') {
                        report("invalid float format. token is " + pt.str);
                        return 0;
                    }
                }
                i = allowed + 1;
                if (pt.str.size() > i) {
                    if (pt.str[i] == '+' || pt.str[i] == '-') {
                        if (!pt.sign) {
                            report("parser is broken");
                            return -1;
                        }
                        i++;
                    }
                }
                if (pt.str.size() <= i) {
                    report("invalid float format. token is " + pt.str);
                    return 0;
                }
                internal::check_int_str(pt.str, i, 10, allowed);
                if (pt.str.size() != allowed) {
                    report("invalid float format. token is " + pt.str);
                    return 0;
                }
                if (pt.aftersign) {
                    ctx.r.current = pt.aftersign->next;
                }
                else if (pt.afterdot) {
                    ctx.r.current = pt.afterdot->next;
                }
                else if (pt.beforedot) {
                    ctx.r.current = pt.beforedot->next;
                }
                else {
                    report("parser is broken");
                    return -1;
                }
                ctx.r.count = pt.stack;
                /*if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                return -1;
            }*/
                return true;
            }
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
