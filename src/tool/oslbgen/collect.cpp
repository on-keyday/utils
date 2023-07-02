/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "type.h"
#include <comb2/tree/simple_node.h>
#include "gen.h"

namespace oslbgen {

    Pos node_args(std::vector<std::string>& args, std::shared_ptr<node::Node>& tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        for (auto& c : g->children) {
            auto m = as_tok(c);
            if (!m) {
                return c->pos;
            }
            args.push_back(m->token);
        }
        return npos;
    }

    Pos node_spec(Spec& spec, std::shared_ptr<node::Node>& tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        auto t = as_tok(g->children[0]);
        if (!t) {
            return g->children[0]->pos;
        }
        spec.spec = t->token;
        if (g->children.size() == 2) {
            return node_args(spec.targets, g->children[1]);
        }
        spec.pos = tok->pos;
        return npos;
    }

    Pos node_strc(DataSet& db, std::shared_ptr<node::Node>& tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        Strc strc;
        std::vector<std::string> m;
        if (auto p = node_args(m, g->children[0]); !p.npos()) {
            return p;
        }
        if (m.size() != 1) {
            return g->pos;
        }
        strc.type = m[0];
        auto i = 1;
        if (g->children[i]->tag == k_spec) {
            if (auto p = node_spec(strc.spec, g->children[i]); !p.npos()) {
                return p;
            }
            i++;
        }
        auto u = as_tok(g->children[i]);
        if (!u) {
            return g->children[i]->pos;
        }
        strc.name = u->token;
        i++;
        u = as_tok(g->children[i]);
        if (!u) {
            return g->children[i]->pos;
        }
        strc.inner = u->token;
        strc.pos = tok->pos;
        db.dataset.push_back(std::move(strc));
        return npos;
    }

    Pos node_define(DataSet& db, std::shared_ptr<node::Node> tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        auto d = as_tok(g->children[0]);
        if (!d) {
            return d->pos;
        }
        Define def;
        def.macro_name = d->token;
        auto i = 1;
        if (g->children[i]->tag == k_args) {
            if (auto p = node_args(def.args, g->children[i]); !p.npos()) {
                return p;
            }
            def.is_fn_style = true;
            i++;
        }
        d = as_tok(g->children[i]);
        if (!d) {
            return d->pos;
        }
        def.token = d->token;
        def.pos = tok->pos;
        db.dataset.push_back(std::move(def));
        return npos;
    }

    Pos node_fn(DataSet& db, std::shared_ptr<node::Node> tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        auto i = 0;
        Fn fn;
        if (g->children[i]->tag == k_spec) {
            if (auto p = node_spec(fn.spec, g->children[i]); !p.npos()) {
                return p;
            }
            i++;
        }
        Pos p;
        auto get_tok = [&](auto& tk) {
            auto u = as_tok(g->children[i]);
            if (!u) {
                p = g->children[i]->pos;
                return false;
            }
            tk = u->token;
            i++;
            return true;
        };
        if (!get_tok(fn.fn_name) ||
            !get_tok(fn.args) ||
            !get_tok(fn.ret)) {
            return p;
        }
        fn.pos = tok->pos;
        db.dataset.push_back(std::move(fn));
        return npos;
    }

    Pos node_root(DataSet& db, std::shared_ptr<node::Node> tok) {
        auto g = as_group(tok);
        if (!g) {
            return tok->pos;
        }
        for (auto& c : g->children) {
#define SWITCH() \
    if (false) { \
    }
#define CASE(A) else if (c->tag == A)
            SWITCH()
            CASE(k_comment) {
                auto m = as_tok(c);
                if (!m) {
                    return c->pos;
                }
                db.dataset.push_back(Comment{.pos = m->pos, .comment = m->token});
            }
            CASE(k_preproc) {
                auto m = as_tok(c);
                if (!m) {
                    return c->pos;
                }
                db.dataset.push_back(Preproc{.pos = m->pos, .directive = m->token});
            }
            CASE(k_use) {
                auto m = as_group(c);
                if (!m) {
                    return c->pos;
                }
                auto u = as_tok(m->children[0]);
                if (!u) {
                    return c->pos;
                }
                db.dataset.push_back(Use{.pos = u->pos, .use = u->token});
            }
            CASE(k_alias) {
                auto m = as_group(c);
                if (!m) {
                    return c->pos;
                }
                auto u = as_tok(m->children[0]);
                if (!u) {
                    return c->pos;
                }
                auto u2 = as_tok(m->children[1]);
                if (!u2) {
                    return c->pos;
                }
                db.dataset.push_back(Alias{
                    .pos = m->pos,
                    .name = u->token,
                    .token = u2->token,
                });
            }
            CASE(k_strc) {
                if (auto p = node_strc(db, c); !p.npos()) {
                    return p;
                }
            }
            CASE(k_define) {
                if (auto p = node_define(db, c); !p.npos()) {
                    return p;
                }
            }
            CASE(k_setspec) {
                auto m = as_group(c);
                if (!m) {
                    return c->pos;
                }
                Spec spec;
                if (auto p = node_spec(spec, m->children[0]); !p.npos()) {
                    return p;
                }
                spec.pos = c->pos;
                db.dataset.push_back(std::move(spec));
            }
            CASE(k_fn) {
                if (auto p = node_fn(db, c); !p.npos()) {
                    return p;
                }
            }
        }
        return npos;
    }
}  // namespace oslbgen
