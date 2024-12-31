/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <file/gzip/huffman.h>
#include <file/gzip/deflate.h>
#include <wrap/cout.h>
#include <number/to_string.h>

int main() {
    namespace huf = futils::file::gzip::huffman;
    constexpr auto text = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja" lang="ja">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<meta http-equiv="X-UA-Compatible" content="IE=edge" />
<meta http-equiv="Content-Script-Type" content="text/javascript" />
<meta http-equiv="Content-Style-Type" content="text/css" />
<meta name="format-detection" content="telephone=no" />



<link rel="shortcut icon" href="https://static.syosetu.com/view/images/narou.ico?psawph" />
<link rel="icon" href="https://static.syosetu.com/view/images/narou.ico?psawph" />
<link rel="apple-touch-icon-precomposed" href="https://static.syosetu.com/view/images/apple-touch-icon-precomposed.png?ojjr8x" />

<link href="https://api.syosetu.com/writernovel/275389.Atom" rel="alternate" type="application/atom+xml" title="Atom" />

<link rel="stylesheet" type="text/css" href="https://static.syosetu.com/novelview/css/reset.css?piu6zr" media="screen,print" />
<link rel="stylesheet" type="text/css" href="https://static.syosetu.com/view/css/lib/jquery.hina.css?oyb9lo" media="screen,print" />

<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/view/css/lib/tippy.css?ov2lia" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/novelview/css/siori_toast_pc.css?q6lspt" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/view/css/lib/remodal.css?oqe20g" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/view/css/lib/remodal-default_pc-theme.css?r9whxq" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/novelview/css/remodal_pc.css?rfnxgm" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/novelview/css/p_novelview-pc.css?r6m0tn" />
<link rel="stylesheet" type="text/css" media="all" href="https://static.syosetu.com/novelview/css/kotei.css?rm1k73" />



<script type="text/javascript"><!--
var domain = 'syosetu.com';
//--></script>

<script type="text/javascript" src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
<script type="text/javascript" src="https://static.syosetu.com/view/js/lib/jquery.hina.js?rq7apb"></script>
<script type="text/javascript" src="https://static.syosetu.com/view/js/global.js?rq7apb"></script>
<script type="text/javascript" src="https://static.syosetu.com/view/js/char_count.js?rgso7v"></script>
<script type="text/javascript" src="https://static.syosetu.com/novelview/js/novelview.js?rmgcvd"></script>

<script type="text/javascript" src="https://static.syosetu.com/view/js/lib/jquery.readmore.js?o7mki8"></script>
<script type="text/javascript" src="https://static.syosetu.com/view/js/lib/tippy.min.js?oqe1mv"></script>
<script type="text/javascript" src="https://static.syosetu.com/novelview/js/lib/jquery.raty.js?q6lspt"></script>
<script type="text/javascript" src="https://static.syosetu.com/novelview/js/novel_bookmarkmenu.js?riuqjo"></script>
<script type="text/javascript" src="https://static.syosetu.com/novelview/js/novel_point.js?q6lspt"></script>
<script type="text/javascript" src="https://static.syosetu.com/view/js/lib/remodal.min.js?oqe1mv"></script>
<script type="text/javascript" src="https://static.syosetu.com/novelview/js/novel_good.js?r6m0tn"></script>

<script type="text/javascript">
var microadCompass = microadCompass || {};
microadCompass.queue = microadCompass.queue || [];
</script>
<script type="text/javascript" charset="UTF-8" src="//j.microad.net/js/compass.js" onload="new microadCompass.AdInitializer().initialize();" async></script>


<!--
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
 xmlns:dc="http://purl.org/dc/elements/1.1/">

<rdf:Description
rdf:about="https://ncode.syosetu.com/n0566gy/
dc:identifier="https://ncode.syosetu.com/n0566gy/"
 />
</rdf:RDF>
-->

</head>
<body onload="initRollovers();">

<a id="pageBottom" href="#footer">↓</a>

<div id="novel_header">
<ul id="head_nav">
<li id="login">
<a href="https://ssl.syosetu.com/login/input/"><span class="attention">ログイン</span></a>
</li>
<li><a href="https://ncode.syosetu.com/novelview/infotop/ncode/n0566gy/">小説情報</a></li>
<li><a href="https://novelcom.syosetu.com/impression/list/ncode/1800386/no/10/">感想</a></li>
<li><a href="https://novelcom.syosetu.com/novelreview/list/ncode/1800386/">レビュー</a></li>
<li><a href="https://pdfnovels.net/n0566gy/" class="menu" target="_blank">縦書きPDF</a></li>
<li class="booklist">

<span class="button_bookmark logout">ブックマークに追加</span>

<input type="hidden" class="js-bookmark_url" value="https://syosetu.com/favnovelmain/addajax/favncode/1800386/no/10/?token=2c96c9786ef35b6b6b3e05d766a38590">
</li>



<li>
<a
style="border-left:none;"

href="https://twitter.com/intent/tweet?text=%E7%8E%8B%E5%AD%90%E6%A7%98%E3%82%92%E5%90%88%E6%B3%95%E7%9A%84%E3%81%AB%E6%AE%B4%E3%82%8A%E3%81%9F%E3%81%84%E3%80%80%E9%80%A3%E8%BC%89%E7%89%88%E3%80%8E%E5%85%84%E3%81%A8%E5%BC%9F1%E3%80%8F&amp;url=https%3A%2F%2Fncode.syosetu.com%2Fn0566gy%2F10%2F&amp;hashtags=narou%2CnarouN0566GY">

<img src="https://static.syosetu.com/novelview/img/Twitter_logo_blue.png?nrkioy" class="menu" style="margin-left:7px; margin-right:7px;" height="20px" />
</a>
<script>!function(d,s,id){var js,fjs=d.getElementsByTagName(s)[0],p=/^http:/.test(d.location)?'http':'https';if(!d.getElementById(id)){js=d.createElement(s);js.id=id;js.src=p+'://platform.twitter.com/widgets.js';fjs.parentNode.insertBefore(js,fjs);}}(document, 'script', 'twitter-wjs');</script>
</li>

</ul>
<div id="novelnavi_right">
<div class="toggle" id="menu_on">表示調整</div>
<div class="toggle_menuclose" id="menu_off">閉じる</div>

<div class="novelview_navi">
<img src="https://static.syosetu.com/novelview/img/novelview_on.gif?n7nper" width="100" height="20" style="cursor:pointer;" onclick="sasieclick(true);" id="sasieflag" alt="挿絵表示切替ボタン" />
<script type="text/javascript"><!--
sasieinit();
//--></script>

<div class="color">
▼配色<br />
<label><input type="radio" value="0" name="colorset" />指定なし(作者設定優先)</label><br />
<label><input type="radio" value="1" name="colorset" />標準設定</label><br />
<label><input type="radio" value="2" name="colorset" />ブラックモード</label><br />
<label><input type="radio" value="3" name="colorset" />ブラックモード2</label><br />
<label><input type="radio" value="4" name="colorset" />通常[1]</label><br />
<label><input type="radio" value="5" name="colorset" />通常[2]</label><br />
<label><input type="radio" value="6" name="colorset" />シンプル</label><br />
<label><input type="radio" value="7" name="colorset" />おすすめ設定</label>
</div>

▼行間
<ul class="novelview_menu">
<li><input type="text" value="180" name="lineheight" size="2" style="text-align:right;" readonly="readonly" />％</li>
<li><a href="javascript:void(0);" name="lineheight_inc" class="size">+</a></li>
<li><a href="javascript:void(0);" name="lineheight_dec" class="size">-</a></li>
<li><a href="javascript:void(0);" name="lineheight_reset">リセット</a></li>
</ul>

<div class="size">
▼文字サイズ
<ul class="novelview_menu">
<li><input type="text" value="100" name="fontsize" size="2" style="text-align:right;" readonly="readonly" />％</li>
<li><a href="javascript:void(0);" name="fontsize_inc" class="size">+</a></li>
<li><a href="javascript:void(0);" name="fontsize_dec" class="size">-</a></li>
<li><a href="javascript:void(0);" name="fontsize_reset">リセット</a></li>
</ul>
</div>

<script type="text/javascript"><!--
changeButtonView();
//--></script>

▼メニューバー<br />
<label><input type="checkbox" name="fix_menu_bar" />追従</label>
<span id="menu_off_2">×閉じる</span>

</div><!--novelview_navi-->
</div><!-- novelnavi_right -->
</div><!--novel_header-->

<div id="container">



<div class="box_announce_bookmark announce_bookmark">
ブックマーク機能を使うには<a href="https://ssl.syosetu.com/login/input/" target="_blank">ログイン</a>してください。
</div>




<div class="toaster bookmarker_addend" id="toaster_success">
<strong>しおりの位置情報を変更しました</strong>
</div>
<div class="toaster bookmarker_error" id="toaster_error">
<strong>エラーが発生しました</strong><br />
<div class="text-right">
<a href="#" class="toaster_close">閉じる</a>
</div>
</div>

<div class="narou_modal" data-remodal-id="add_bookmark">
<div class="close js-add_bookmark_modal_close"></div>

<div class="scroll">
<p class="fav_noveltitle js-add_bookmark_title"></p>
<p class="mes">ブックマークに追加しました</p>

<div class="favnovelmain_update">
<h3>設定</h3>
<p>
<input type="checkbox" name="isnotice" value="1" />更新通知
<span class="left10 js-isnoticecnt">0/400</span>
</p>

<p>
<label><input type="radio" name="jyokyo" class="bookmark_jyokyo" value="2" checked="checked" />公開</label>
<label><input type="radio" name="jyokyo" class="bookmark_jyokyo left10" value="1" />非公開</label>
</p>

<div class="toaster bookmarker_addend js-bookmark_save_toaster" id="bookmark_save_toaster">
<strong>設定を保存しました</strong>
</div>

<div class="toaster bookmarker_error js-bookmark_save_err_toaster">
<strong>エラーが発生しました</strong>
<div id="bookmark_save_errmsg" class="js-bookmark_save_errmsg"></div>
<div class="text-right">
<a href="#" class="toaster_close">閉じる</a>
</div>
</div>

<p>
カテゴリ
<select name="categoryid" class="js-category_select"></select>
</p>
<div class="favnovelmain_bkm_memo">
<label>メモ</label>
<div class="favnovelmain_bkm_memo_content">
<input type="text" maxlength="" class="js-bookmark_memo">
<span class="favnovelmain_bkm_memo_content_help-text">※文字以内</span>
</div>
</div>

<input type="button" class="button js-bookmark_setting_submit" value="設定を保存" />
<input type="hidden" class="js-bookmark_setting_url" value="https://syosetu.com/favnovelmain/updateajax/" />
<input type="hidden" class="js-bookmark_setting_useridfavncode" value="" />
<input type="hidden" class="js-bookmark_setting_xidfavncode" value="" />
<input type="hidden" class="js-bookmark_setting_token" value="" />
</div>
<!-- favnovelmain_update -->

<a href="https://syosetu.com/favnovelmain/list/" class="button js-modal_bookmark_btn">ブックマークへ移動</a>
<input type="hidden" value="//syosetu.com/favnovelmain/list/" class="js-base_bookmark_url">
</div><!-- scroll -->
</div>
<!-- narou_modal -->

    <!-- Global site tag (gtag.js) - Google Analytics -->
    <script async src="https://www.googletagmanager.com/gtag/js?id=G-1TH9CF4FPC"></script>
    <script>
        window.dataLayer = window.dataLayer || [];
        function gtag(){dataLayer.push(arguments);}
        gtag('js', new Date());

        gtag('config', 'G-1TH9CF4FPC');
        gtag('config', 'UA-3637811-4');
    </script>

                <script>
            gtag('config', 'G-2YQV7PZTL9');
        </script>
    


<script type="text/javascript" src="//d-cache.microad.jp/js/td_sn_access.js"></script>
<script type="text/javascript">
  microadTd.SN.start({})
</script>




</body></html>

)";
    // auto tree = huf::gen_tree(text);
    huf::CanonicalTable<256, huf::CodeMap, huf::UnwrapCodeMap> table;
    // huf::tree_to_table(tree, table);
    size_t add = 0;

    for (auto c : futils::view::rvec(text)) {
        add += table.map(c)->code.bits;
    }
    double mean = 0;
    auto txt = futils::view::rvec(text);

    auto print = [&](huf::Code code) {
        auto c = code.literal;
        std::string str;
        futils::number::to_string(str, code.literal);
        str.push_back(' ');
        auto& cout = futils::wrap::cout_wrap();
        cout << str;
        if (code.bits < 10) {
            cout << " ";
        }
        cout << code.bits << " ";
        str.clear();
        code.write(str, '1', '0');
        cout << str << "\n";
    };
    for (auto i = 0; i < table.index; i++) {
        print(table.codes[i].code);
        mean += table.codes[i].code.bits * double(table.codes[i].appear) / txt.size();
    }
    const auto base_bit = txt.size() * 8;
    futils::wrap::cout_wrap() << "base: " << base_bit << " encoded: " << add << " enc/base: " << double(add) / base_bit
                             << " mean: " << mean << "\n";

    auto [h, d] = futils::file::gzip::deflate::make_deflate_fixed_huffman();
    for (auto i = 0; i < h.index; i++) {
        print(h.codes[i]);
    }
    for (auto i = 0; i < d.index; i++) {
        print(d.codes[i]);
    }
}
