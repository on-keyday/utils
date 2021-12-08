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

        template <class String>
        struct FloatReadPoint {
            using token_t = tknz::Token<String>;
            String str;
            wrap::shared_ptr<String> beforedot;
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
            if (!any(v->attr & Attribute::adjacent)) {
                cr.Read();
            }
            while (true) {
                auto e = cr.Get();
                if (!e) {
                    break;
                }
                if (!pt.dot && !pt.sign && e->has_(".")) {
                    pt.dot = e;
                    pt.str += e->to_string();
                    cr.Consume();
                    continue;
                }
                if (!pt.sign && (e->has_("+") || e->has_("-"))) {
                    pt.sign = e;
                    pt.str += e->to_string();
                    cr.Consume();
                    continue;
                }
                if (!e->is_(tknz::TokenKind::identifier)) {
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
                cr.Consume();
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
        int parse_float(Context<String, Vec>& ctx, std::shared_ptr<Element<String, Vec>>& v) {
            auto cr = ctx.r.FromCurrent();
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
            int allowed = false;
            check_int_str(pt.str, i, base, allowed);
            if (pt.str.size() == allowed) {
                if (!pt.beforedot || pt.dot || pt.sign) {
                    report(&r, pt.exists(), v, "parser is broken");
                    return -1;
                }
                r.current = pt.beforedot->get_next();
                r.count = pt.stack;
                if (!callback(pt.exists(), r, pt.str, MatchingType::integer)) {
                    return -1;
                }
                return 1;
            }
            if (pt.str[allowed] == '.') {
                if (!pt.dot) {
                    report(&r, pt.exists(), v, "parser is broken");
                    return -1;
                }
                i = allowed + 1;
            }
            check_int_str(pt.str, i, base, allowed);
            if (pt.str.size() == allowed) {
                if (pt.sign) {
                    report(&r, pt.exists(), v, "parser is broken");
                    return -1;
                }
                if (pt.afterdot) {
                    r.current = pt.afterdot->get_next();
                }
                else if (pt.dot) {
                    r.current = pt.dot->get_next();
                }
                else {
                    report(&r, pt.exists(), v, "parser is broken");
                    return -1;
                }
                r.count = pt.stack;
                if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                    return -1;
                }
                return 1;
            }
            if (base == 16) {
                if (pt.str[allowed] != 'p' && pt.str[allowed] != 'P') {
                    report(&r, pt.exists(), v, "invalid hex float format. token is " + pt.str);
                    return 0;
                }
            }
            else {
                if (pt.str[allowed] != 'e' && pt.str[allowed] != 'E') {
                    report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                    return 0;
                }
            }
            i = allowed + 1;
            if (pt.str.size() > i) {
                if (pt.str[i] == '+' || pt.str[i] == '-') {
                    if (!pt.sign) {
                        report(&r, pt.exists(), v, "parser is broken");
                        return -1;
                    }
                    i++;
                }
            }
            if (pt.str.size() <= i) {
                report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                return 0;
            }
            check_int_str(pt.str, i, 10, allowed);
            if (pt.str.size() != allowed) {
                report(&r, pt.exists(), v, "invalid float format. token is " + pt.str);
                return 0;
            }
            if (pt.aftersign) {
                r.current = pt.aftersign->get_next();
            }
            else if (pt.afterdot) {
                r.current = pt.afterdot->get_next();
            }
            else if (pt.beforedot) {
                r.current = pt.beforedot->get_next();
            }
            else {
                report(&r, pt.exists(), v, "parser is broken");
                return -1;
            }
            r.count = pt.stack;
            if (!callback(pt.exists(), r, pt.str, MatchingType::number)) {
                return -1;
            }
            return true;
        }
    }  // namespace syntax
}  // namespace utils