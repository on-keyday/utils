/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <wrap/cin.h>
#include <console/window.h>
#include <console/ansiesc.h>
#include <thread>
#include "save_data.h"
#include <file/file_stream.h>
#include <filesystem>
#include "game.h"
#include <signal.h>

struct Flags : futils::cmdline::templ::HelpOption {
    futils::wrap::path_string save_data_dir;
    bool make_embed_file = false;
    bool verify_embed_file = false;
    bool make_embed_before_start = false;
    std::string embed_file_location;
    std::string embed_index_location;
    std::string embed_dir_location;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&save_data_dir, "s,save-data-dir", "save data directory", "DIR");
        ctx.VarBool(&make_embed_file, "e,embed", "make embed file (development only)");
        ctx.VarBool(&verify_embed_file, "v,verify", "verify embed file (development only)");
        ctx.VarBool(&make_embed_before_start, "m,make-embed", "make embed file before start (development only)");
        ctx.VarString(&embed_file_location, "embed-file", "embed file location", "FILE");
        ctx.VarString(&embed_index_location, "embed-index", "embed index location", "FILE");
        ctx.VarString(&embed_dir_location, "embed-dir", "embed directory location", "DIR");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cin = futils::wrap::cin_wrap();
bool signaled = false;

struct Menu {
    save::UIState state = save::UIState::start;
    save::SaveData save_data;
    futils::wrap::path_string save_data_path;

    void show_title(TextController& text) {
        std::string buf;
        ansiesc::window_title(buf, "Hello, World! - とある世界の物語");
        cout << buf;
        text.clear_screen();
        text.layout.reset_layout(center_layout);
        text.write_title("Hello, World! - とある世界の物語");
        text.layout.reset_layout(text_layout);
    }

    void show_menu(TextController& text) {
        auto result = text.write_prompt(U"メニュー\n\n1. はじめる\n2. セーブデータを作る\n3. セーブデータを消す\n4. やめる", 0, 0);
        if (result == "1") {
            state = save::UIState::save_data_select;
        }
        else if (result == "2") {
            state = save::UIState::create_save_data;
        }
        else if (result == "3") {
            state = save::UIState::delete_save_data;
        }
        else if (result == "4") {
            state = save::UIState::ending;
        }
    }

    void create_save_data(TextController& text, Flags& flags) {
        fs::path save_data_dir{flags.save_data_dir};
        if (!fs::exists(save_data_dir)) {
            fs::create_directories(save_data_dir);
        }
        auto save_data_name = text.write_prompt(U"セーブデータ名を入力してください。(enterでキャンセル)", 0, 0);
        if (save_data_name == "") {
            state = save::UIState::confrontation;
            return;
        }
        save_data_name += ".hwrpg";
        auto res = futils::file::File::create((save_data_dir / save_data_name).u8string());
        if (!res) {
            text.write_story(U"セーブデータの作成に失敗しました。", 1);
            state = save::UIState::confrontation;
            return;
        }
        futils::file::FileStream<std::string> fs{*res};
        futils::binary::writer w{fs.get_write_handler(), &fs};
        save_data.version = 1;
        save_data.phase.name = "start";
        save_data.phase.len.value(5);
        if (auto err = save_data.encode(w)) {
            text.write_story(U"セーブデータの作成に失敗しました。", 0);
            state = save::UIState::confrontation;
            return;
        }
        save_data_name = save_data_name.substr(0, save_data_name.size() - 6);
        text.write_story(U"セーブデータ「" + futils::utf::convert<std::u32string>(save_data_name) + U"」を作成しました。", 0);
        state = save::UIState::confrontation;
    }

    void show_save_files(TextController& text, Flags& flags, bool delete_mode = false) {
        fs::path save_data_dir{flags.save_data_dir};
        fs::directory_entry entry{save_data_dir};
        if (!entry.exists()) {
            text.write_story(U"セーブデータが見つかりません。", 0);
            state = save::UIState::confrontation;
            return;
        }
        std::vector<fs::path> save_files;
        auto collect_save_files = [&] {
            save_files.clear();
            for (auto& file : fs::directory_iterator{save_data_dir}) {
                if (file.is_regular_file() && file.path().extension() == ".hwrpg") {
                    save_files.push_back(file.path());
                }
            }
        };
        collect_save_files();
        if (save_files.empty()) {
            text.write_story(U"セーブデータが見つかりません。", 0);
            state = save::UIState::confrontation;
            return;
        }
        constexpr auto prefix = U"セーブデータを選択してください。\n\n";
        size_t selected_range = 0;
        auto select_3_file = [&] {
            std::u32string files;
            for (size_t i = selected_range; i < selected_range + 3; i++) {
                if (i < save_files.size()) {
                    files += futils::number::to_string<std::u32string>(i + 1) + U". " + save_files[i].filename().replace_extension("").u32string() + U"\n";
                }
            }
            if (selected_range != 0) {
                files += U"p. 前へ\n";
            }
            if (selected_range + 3 < save_files.size()) {
                files += U"n. 次へ\n";
            }
            files += U"c. キャンセル\n";
            return files;
        };
        while (true) {
            auto x = text.write_prompt(prefix + select_3_file(), 0, 0);
            if (x == "c") {
                state = save::UIState::confrontation;
                return;
            }
            if (x == "p") {
                if (selected_range >= 3) {
                    selected_range -= 3;
                }
                continue;
            }
            if (x == "n") {
                if (selected_range + 3 < save_files.size()) {
                    selected_range += 3;
                }
                continue;
            }
            size_t selected = x[0] - U'1';
            if (selected < save_files.size()) {
                auto& file = save_files[selected];
                if (delete_mode) {
                    file = fs::canonical(file);
                    auto name = file.filename().replace_extension("").u32string();
                    auto p = text.write_prompt(U"セーブデータ「" + name + U"」を削除しますか?(y/n)", 0, 0);
                    if (p == "y") {
                        if (!fs::remove(file)) {
                            text.write_story(U"セーブデータ「" + name + U"」の削除に失敗しました。", 0);
                        }
                        else {
                            text.write_story(U"セーブデータ「" + name + U"」を削除しました。", 0);
                            collect_save_files();
                            if (save_files.empty()) {
                                text.write_story(U"セーブデータが見つかりません。", 0);
                                state = save::UIState::confrontation;
                                return;
                            }
                        }
                    }
                    continue;
                }
                auto res = futils::file::File::open(file.u8string(), futils::file::O_READ_DEFAULT);
                if (!res) {
                    text.write_story(U"セーブデータの読み込みに失敗しました。", 0);
                    continue;
                }
                futils::file::FileStream<std::string> fs{*res};
                futils::binary::reader r{fs.get_read_handler(), &fs};
                if (auto err = save_data.decode(r)) {
                    text.write_story(U"セーブデータの読み込みに失敗しました。", 0);
                    continue;
                }
                save_data_path = futils::utf::convert<futils::wrap::path_string>(file.u8string());
                state = save::UIState::game_start;
                return;
            }
        }
    }

    bool run(TextController& text, Flags& flags) {
        switch (state) {
            case save::UIState::start:
                show_title(text);
                state = save::UIState::confrontation;
                break;
            case save::UIState::confrontation:
                show_menu(text);
                break;
            case save::UIState::create_save_data:
                create_save_data(text, flags);
                break;
            case save::UIState::save_data_select:
                show_save_files(text, flags);
                break;
            case save::UIState::delete_save_data:
                show_save_files(text, flags, true);
                break;
            case save::UIState::game_start:
                text.write_story(U"ゲームを開始します....", 1);
                return false;
            case save::UIState::ending:
                text.write_story(U"バイバイ!", 1, 0);
                return false;
            default:
                text.write_story(U"エラーが発生しました。", 1);
                return false;
        }
        return true;
    }
};

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    auto target_dir = "./src/tool/hwrpg/data/";
    auto embed_file = "./src/tool/hwrpg/embed_file.dat";
    auto embed_index = "./src/tool/hwrpg/embed_file.idx";
    if (!flags.embed_file_location.empty()) {
        embed_file = flags.embed_file_location.c_str();
    }
    if (!flags.embed_index_location.empty()) {
        embed_index = flags.embed_index_location.c_str();
    }
    if (!flags.embed_dir_location.empty()) {
        target_dir = flags.embed_dir_location.c_str();
    }
    if (flags.make_embed_file) {
        return make_embed(target_dir, embed_file, embed_index);
    }
    if (flags.verify_embed_file) {
        return verify_embed(embed_file, embed_index);
    }
    if (flags.make_embed_before_start) {
        auto res = make_embed(target_dir, embed_file, embed_index);
        if (res != 0) {
            return res;
        }
    }
    if (flags.save_data_dir.empty()) {
        cout << "save data directory is not specified\n";
        return 1;
    }
    TextController text{futils::console::Window::create(text_layout, 80, 10).value()};
    Menu menu;
    while (menu.run(text, flags)) {
        ;
    }
    int res = 0;
    if (menu.state == save::UIState::game_start) {
        while (true) {
            auto gr = run_game(text, menu.save_data, embed_file, embed_index, menu.save_data_path);
            if (gr == GameEndRequest::reload) {
                unload_embed();
                auto tmp = make_embed(target_dir, embed_file, embed_index);
                if (tmp != 0) {
                    return tmp;  // cannot reload
                }
                text.clear_screen();
                text.write_story(U"再読み込みします....", 1);
                continue;
            }
            // gr is 1: exit, 2: failure, 3: interrupt, 4: reload
            // so 1, 2, 3, 4 -> 0, 1, 2, 3
            res = static_cast<int>(gr) - 1;
            break;
        }
    }
    text.show_cursor();
    return res;
}

void notify_signal(int sig) {
    signaled = true;
    signal(sig, notify_signal);
}

int main(int argc, char** argv) {
    Flags flags;
    signal(SIGINT, notify_signal);
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
