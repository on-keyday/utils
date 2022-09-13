/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// html - easy html boilerplate
#pragma once
#include "../net_util/uri_parse.h"
#include <string>

namespace utils {
    namespace dnet {
        namespace html {

            template <class To, class Render = std::wstring>
            auto render_funny_error_html(auto& method, auto& path, int code) {
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
        let text = "You visited {1} {0} \nbut received {2} {3}. \nThat's all we know.";
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
                uri::URI<To> url;
                uri::parse(url, path);
                auto phrase = utf::convert<Render>(net::h1value::reason_phrase(code));
                auto data = std::format(text, url.path, method, int(code), phrase);
                return utf::convert<To>(data);
            }

        }  // namespace html
    }      // namespace dnet
}  // namespace utils
