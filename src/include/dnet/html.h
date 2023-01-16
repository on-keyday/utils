/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// html - easy html boilerplate
#pragma once
#include "../net_util/uri_parse.h"
#include <string>
#include <format>
#include <memory>

namespace utils {
    namespace dnet {
        namespace html {

            template <class To, class Render = std::wstring>
            auto render_funny_error_html(auto&& method, auto&& path, int code) {
                constexpr auto text =
                    LR"a(<!DOCTYPE HTML>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
h1 {{
    margin-bottom: 1px;
}}
h3 {{
    margin-top: 1px;
}}
</style>
</head>
<body>
<div>
<h4 id="method"><br></h4>
<h3 id="face">🧐</h3>
</div>
</body>
<script>
'use strict'
const action = ()=>{{
window.COUNTER=100;
window.TIMER = window.setInterval(function(){{
    let element=window.document.getElementById("face");
    element.style.fontSize=String(window.COUNTER)+"%"
    element.style.margin="1px";
    window.COUNTER+=10;
    if(window.COUNTER>=5000) {{
        window.clearInterval(window.TIMER)
        let meth = window.document.getElementById("method");
        meth.style.fontSize="200%";
        meth.style.margin="1px";
        let text = "You visited {1} {0} \nbut received an error, {2} {3}. \nThat's all we know.";
        let add_count=0;
        window.TIMER = window.setInterval(()=>{{
            if(meth.innerText=="\n"){{
                meth.innerText="";
            }}
            if(text[add_count]=='\n'){{
                meth.innerText+="\n ";
            }}
            else if(text[add_count]==' '){{

            }}
            else {{
                if(add_count!=0&& text[add_count-1]==' '){{
                    meth.innerText+=" "+text[add_count];
                }}
                else{{
                    meth.innerText+=text[add_count];
                }}
            }}
            add_count++;
            if(add_count==text.length){{
                meth.innerText=text;
                window.clearInterval(window.TIMER);
            }}
        }},100);
        
    }}
}},10);
}};
window.addEventListener("load",action);
</script>
</html>
)a";
                uri::URI<Render> url;
                uri::parse(url, path);
                auto phrase = utf::convert<Render>(net::h1value::reason_phrase(code));
                auto data = std::format(text, url.path, utf::convert<Render>(method), code, phrase);
                return utf::convert<To>(data);
            }

            enum ElementType {
                text,
                tag,
                comment,
            };

            struct Element {
                const ElementType type = ElementType::text;
                std::weak_ptr<Element> parent;
                std::weak_ptr<Element> prev;
                std::shared_ptr<Element> child;
                std::shared_ptr<Element> next;
                std::string text;
                constexpr Element() {}

               protected:
                constexpr Element(ElementType t)
                    : type(t) {}
            };

            struct Attribute {
                std::string name;
                std::string value;
                std::shared_ptr<Attribute> next;
            };

            struct TagElement : Element {
                constexpr TagElement()
                    : Element(ElementType::tag) {}
                std::shared_ptr<Attribute> attr;
            };

            struct CommentElement : Element {
                constexpr CommentElement()
                    : Element(ElementType::comment) {
                }
            };

            struct ParseContext {
                std::shared_ptr<Element> root;
                std::shared_ptr<TagElement> current_tag;
                std::shared_ptr<Element> current;
            };

            template <class T>
            int parse_comment(ParseContext& ctx, Sequencer<T>& seq, bool last_input) {
                auto add_comment = [](std::string&& str) -> std::shared_ptr<Element> {
                    auto elm = std::make_shared<CommentElement>();
                };
                auto start = seq.rptr;
                if (seq.seek_if("<!--")) {
                    // comment
                    // special case
                    if (seq.seek_if("->")) {
                        add_comment("<!--->");
                        return 1;
                    }
                    else if (seq.seek_if(">")) {
                        add_comment("<!-->");
                        return 1;
                    }
                    std::string text = "<!--";
                    while (true) {
                        if (seq.eos()) {
                            if (last_input) {
                                break;
                            }
                            seq.rptr = start;
                            return -1;  // suspend
                        }
                        if (seq.seek_if("-->")) {
                            text.append("-->");
                            break;
                        }
                        else if (seq.seek_if("--!>")) {
                            text.append("--!>");
                            break;
                        }
                        if (last_input) {
                            auto ptr = seq.rptr;
                            if (seq.seek_if("--!") && seq.eos()) {
                                text.append("--!");
                                break;
                            }
                            seq.rptr = ptr;
                            if (seq.seek_if("--") && seq.eos()) {
                                text.append("--");
                                break;
                            }
                            seq.rptr = ptr;
                            if (seq.seek_if("-") && seq.eos()) {
                                text.append("-");
                                break;
                            }
                            seq.rptr = ptr;
                        }
                        text.push_back(seq.current());
                        seq.consume();
                    }
                    add_comment(std::move(text));
                    return 1;
                }
                else if (seq.seek_if("<!") || seq.seek_if("<?") || seq.seek_if("</")) {
                    std::string text;
                    text.push_back('<');
                    text.push_back(seq.current(-1));
                    while (true) {
                        if (seq.eos()) {
                            if (last_input) {
                                break;
                            }
                            seq.rptr = start;
                            return -1;
                        }
                        if (seq.seek_if(">")) {
                            text.push_back('>');
                            break;
                        }
                        text.push_back(seq.current());
                        seq.consume();
                    }
                    add_comment(std::move(text));
                    return 1;
                }
                return 0;
            }

            template <class T>
            int parse_tag(ParseContext& ctx, Sequencer<T>& seq, bool last_input) {
                auto start = seq.rptr;
                if (!seq.seek_if("<")) {
                    return 0;
                }
                if (!number::is_alpha(seq.current())) {
                    seq.rptr = seq.rptr;
                    return 0;
                }
                std::string tagname;
                while (true) {
                    if (seq.eos()) {
                        if (last_input) {
                            return -1;  // suspend
                        }
                        seq.rptr = start;
                        return 0;
                    }
                    if (seq.current() == '/') {
                        if (!seq.match("/>")) {
                            seq.rptr = start;
                            return 0;
                        }
                        break;
                    }
                    if (seq.current() == '>') {
                        break;
                    }
                    auto c = seq.current();
                    if (c == '\t' || c == ' ' || c == '\n' || c == '\r') {
                        break;
                    }
                    tagname.push_back(helper::to_lower(seq.current()));
                    seq.consume();
                }
                auto element = std::make_shared<TagElement>();
                element->text = tagname;
                while (true) {
                    if (seq.seek_if("/")) {
                    }
                }
                element->parent = ctx.current_tag;
                if (ctx.current_tag && !ctx.current_tag->child) {
                    ctx.current_tag->child = element;
                }
                element->prev = ctx.current;
                if (ctx.current) {
                    ctx.current->next = element;
                }
                ctx.current_tag = element;
                ctx.current = nullptr;
                return true;
            }

            template <class T>
            bool parse(ParseContext& ctx, Sequencer<T>& seq, bool last_input) {
                while (!seq.eos()) {
                    auto start = seq.eos();
                    if (ctx.current_tag) {
                        if (!seq.seek_if("</")) {
                            goto COMMENT;
                        }
                        if (!seq.seek_if(ctx.current_tag->text)) {
                            goto COMMENT;
                        }
                        while (seq.consume_if(' ') || seq.consume_if('\t') ||
                               seq.consume_if('\n') || seq.consume_if('\r')) {
                        }
                        if (!seq.seek_if(">")) {
                            if (last_input) {
                                goto COMMENT;
                            }
                            if (seq.eos()) {
                                seq.rptr = start;
                                return true;  // suspend
                            }
                        }
                        ctx.current_tag = std::static_pointer_cast<TagElement>(ctx.current_tag->parent.lock());
                    }
                COMMENT:
                    if (auto state = parse_comment(ctx, seq, last_input)) {
                        if (state == -1) {
                            return true;  // suspend
                        }
                        continue;
                    }
                    if (seq.seek_if("<")) {
                    }
                TEXT:
                }
            }
        }  // namespace html
    }      // namespace dnet
}  // namespace utils
