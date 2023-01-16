/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "midtoenv.h"

namespace minlc {
    namespace mtoenv {
        namespace mid = middle;

        void hlt(MtoEnv& m) {
            m.out.push_back(ipret::ist_hlt);  // halt
        }
        void expr_to_env(MtoEnv& m, sptr<mid::Expr>& expr);

        void binary_to_env(MtoEnv& m, sptr<mid::BinaryExpr>& bin) {
            expr_to_env(m, bin->left);
            m.out.push_back(ipret::ist_push);
            m.out.push_back(0xC0 | 0x00);  // len=8, regnum=0
            expr_to_env(m, bin->right);
            m.out.push_back(ipret::ist_trsf);
            m.out.push_back(0x0);  // from_reg=0
            m.out.push_back(0x1);  // to_reg=1
            m.out.push_back(ipret::ist_pop);
            m.out.push_back(0xC0 | 0x00);  // len=8,regnum=0
            ipret::byte ist = 0, mask = 0;
            bool swp = false;
            auto& str = bin->node->str;
            auto set_mask = [&](auto ist_, auto _1, auto _2, auto _3, auto _4) {
                if (str == _1 || str == _2 || str == _3 || str == _4) {
                    ist = ist_;
                    if (str == _1) {
                        mask = 0x0;
                    }
                    else if (str == _2) {
                        mask = 0x40;
                    }
                    else if (str == _3) {
                        mask = 0x80;
                    }
                    else {
                        mask = 0xC0;
                    }
                    return true;
                }
                return false;
            };
            if (set_mask(ipret::ist_4artop, "+", "-", "*", "/") ||
                set_mask(ipret::ist_modbit, "%", "&", "|", "^") ||
                set_mask(ipret::ist_shifteq, "<<", ">>", "==", "!=") ||
                set_mask(ipret::ist_cmplogi, "<", "<=", "&&", "||")) {
                // nothing to do
            }
            else if (set_mask(ipret::ist_cmplogi, ">", ">=", "", "")) {
                swp = true;
            }
            else {
                return hlt(m);  // halt operation
            }
            m.out.push_back(ist);
            if (swp) {
                m.out.push_back(mask | 0x1);  // op = mask,leftreg=1
                m.out.push_back(0x0);         // rightreg=0
            }
            else {
                m.out.push_back(mask | 0x0);  // op = mask,leftreg=0
                m.out.push_back(0x1);         // rightreg=1
            }
            m.out.push_back(0x0);  // targetreg=0
            return;
        }

        void expr_to_env(MtoEnv& m, sptr<mid::Expr>& expr) {
            if (expr->obj_type == mid::mk_binary) {
                auto bin = std::static_pointer_cast<mid::BinaryExpr>(expr);
                return binary_to_env(m, bin);
            }
            if (expr->obj_type == mid::mk_expr) {
                if (auto num = mi::is_Number(expr->node)) {
                    if (num->is_float) {
                        return hlt(m);  // halt operation
                    }
                    size_t key = 0;
                    if (!utils::number::prefix_integer(num->str, key)) {
                        return hlt(m);
                    }
                    auto len = ipret::len_by_num(key);
                    auto lenmask = ipret::len_to_mask(len);
                    m.out.push_back(ipret::ist_load);
                    m.out.push_back(lenmask | 0x0);  // len=len,regnum=0
                    ipret::byte buf[8];
                    ipret::Vec v;
                    v.area = buf;
                    v.limit = sizeof(buf);
                    v.ptr = 0;
                    ipret::store_l(v, len, key);
                    m.out.append((const char*)buf, len);
                    return;
                }
            }
            return hlt(m);
        }

        void func_to_env(MtoEnv& m, sptr<mid::Func>& fn) {
        }

    }  // namespace mtoenv
}  // namespace minlc
