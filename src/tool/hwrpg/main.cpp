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
struct Flags : futils::cmdline::templ::HelpOption {
    futils::wrap::path_string save_data_dir;
    bool make_embed_file = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&save_data_dir, "s,save-data-dir", "save data directory", "DIR", futils::cmdline::option::CustomFlag::required);
        ctx.VarBool(&make_embed_file, "e,embed", "make embed file (development only)");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cin = futils::wrap::cin_wrap();
constexpr auto text_layout = futils::console::make_layout_data<80, 10>(2);
constexpr auto center_layout = futils::console::make_layout_data<80, 10>(4);
namespace ansiesc = futils::console::escape;
namespace fs = std::filesystem;
struct TextController {
    futils::console::Window layout;
    void write_text(futils::view::rvec data) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        layout.draw(w, data);
        cout << w.written();
    }

    void write_title(futils::view::rvec title) {
        layout.reset_layout(center_layout);
        write_text(title);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void move_back() {
        cout << ansiesc::cursor(ansiesc::CursorMove::up, layout.dy());
    }

    auto write_story(std::u32string_view text, size_t delay = 100, size_t after_wait = 1000) {
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        move_back();
        layout.reset_layout(text_layout);
        write_text("");
        if (delay == 0) {
            move_back();
            write_text(futils::utf::convert<std::string>(text));
            std::this_thread::sleep_for(std::chrono::milliseconds(after_wait));
            return;
        }
        std::string output;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        for (size_t i = 0; i < text.size(); i++) {
            move_back();
            output = futils::utf::convert<std::string>(text.substr(0, i + 1));
            write_text(output);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(after_wait));
    };

    auto show_cursor() {
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
    }

    auto write_prompt(std::u32string_view text, size_t delay = 100, size_t after_wait = 1000) {
        write_story(text, delay, after_wait);
        cout << ansiesc::erase(ansiesc::EraseTarget::line, ansiesc::EraseMode::back_of_cursor);
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
        cout << "> ";
        std::string answer;
        cin >> answer;
        while (!answer.empty() && (answer.back() == '\n' || answer.back() == '\r')) {
            answer.pop_back();
        }
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        cout << ansiesc::cursor(ansiesc::CursorMove::up, 1);
        return answer;
    };
};

struct Menu {
    UIState state = UIState::start;
    SaveData save_data;
    void show_title(TextController& text) {
        std::string buf;
        ansiesc::window_title(buf, "Hello, World! - とある世界の物語");
        cout << buf;
        text.write_title("Hello, World! - とある世界の物語");
    }

    void show_menu(TextController& text) {
        auto result = text.write_prompt(U"メニュー\n\n1. はじめる\n2. セーブデータを作る\n3. セーブデータを消す\n4. やめる", 0, 0);
        if (result == "1") {
            state = UIState::save_data_select;
        }
        else if (result == "2") {
            state = UIState::create_save_data;
        }
        else if (result == "3") {
            state = UIState::delete_save_data;
        }
        else if (result == "4") {
            state = UIState::ending;
        }
    }

    void create_save_data(TextController& text, Flags& flags) {
        fs::path save_data_dir{flags.save_data_dir};
        if (!fs::exists(save_data_dir)) {
            fs::create_directories(save_data_dir);
        }
        auto save_data_name = text.write_prompt(U"セーブデータ名を入力してください。(enterでキャンセル)", 0, 0);
        if (save_data_name == "") {
            state = UIState::confrontation;
            return;
        }
        save_data_name += ".hwrpg";
        auto res = futils::file::File::create((save_data_dir / save_data_name).u8string());
        if (!res) {
            text.write_story(U"セーブデータの作成に失敗しました。", 1);
            state = UIState::confrontation;
            return;
        }
        futils::file::FileStream<std::string> fs{*res};
        futils::binary::writer w{fs.get_write_handler(), &fs};
        save_data.version = 1;
        save_data.phase = StoryPhase::prologue;
        save_data.location = Location::hello_island;
        if (auto err = save_data.encode(w)) {
            text.write_story(U"セーブデータの作成に失敗しました。", 0);
            state = UIState::confrontation;
            return;
        }
        save_data_name = save_data_name.substr(0, save_data_name.size() - 6);
        text.write_story(U"セーブデータ「" + futils::utf::convert<std::u32string>(save_data_name) + U"」を作成しました。", 0);
        state = UIState::confrontation;
    }

    void show_save_files(TextController& text, Flags& flags, bool delete_mode = false) {
        fs::path save_data_dir{flags.save_data_dir};
        fs::directory_entry entry{save_data_dir};
        if (!entry.exists()) {
            text.write_story(U"セーブデータが見つかりません。", 0);
            state = UIState::confrontation;
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
            state = UIState::confrontation;
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
                state = UIState::confrontation;
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
                                state = UIState::confrontation;
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
                state = UIState::game_start;
                return;
            }
        }
    }

    bool run(TextController& text, Flags& flags) {
        switch (state) {
            case UIState::start:
                show_title(text);
                state = UIState::confrontation;
                break;
            case UIState::confrontation:
                show_menu(text);
                break;
            case UIState::create_save_data:
                create_save_data(text, flags);
                break;
            case UIState::save_data_select:
                show_save_files(text, flags);
                break;
            case UIState::delete_save_data:
                show_save_files(text, flags, true);
                break;
            case UIState::game_start:
                text.write_story(U"ゲームを開始します....", 1);
                return false;
            case UIState::ending:
                text.write_story(U"バイバイ!", 1, 0);
                return false;
            default:
                text.write_story(U"エラーが発生しました。", 1);
                return false;
        }
        return true;
    }
};

void set_varint_len(Varint& v, std::uint64_t len) {
    if (len <= 0x3F) {
        v.prefix(0);
        v.value(len);
    }
    else if (len <= 0x3FFF) {
        v.prefix(1);
        v.value(len);
    }
    else if (len <= 0x3FFFFFFF) {
        v.prefix(2);
        v.value(len);
    }
    else {
        v.prefix(3);
        v.value(len);
    }
}

int make_embed() {
    const auto target_dir = "./src/tool/hwrpg/data/";
    const auto embed_file = "./src/tool/hwrpg/embed_file.dat";
    const auto embed_index = "./src/tool/hwrpg/embed_file.idx";
    auto embed_file_data = futils::file::File::create(embed_file);
    if (!embed_file_data) {
        cout << "failed to create " << embed_file << embed_file_data.error().error<std::string>() << "\n";
        return 1;
    }
    auto embed_index_data = futils::file::File::create(embed_index);
    if (!embed_index_data) {
        cout << "failed to create " << embed_index << embed_index_data.error().error<std::string>() << "\n";
        return 1;
    }
    futils::file::FileStream<std::string> efs{*embed_file_data}, ifs{*embed_index_data};
    futils::binary::writer ew{efs.get_direct_write_handler(), &efs}, iw{ifs.get_direct_write_handler(), &ifs};
    auto root_path = fs::path{target_dir};
    auto entries = fs::recursive_directory_iterator{root_path};
    std::uint64_t offset = 0;
    std::uint64_t file_count = 0;
    for (auto entry : entries) {
        if (entry.is_regular_file()) {
            auto path = entry.path();
            auto data = futils::file::File::open(path.u8string(), futils::file::O_READ_DEFAULT);
            if (!data) {
                cout << "failed to open " << path.u8string() << data.error().error<std::string>() << "\n";
                continue;
            }
            auto res = data->mmap(futils::file::r_perm);
            if (!res) {
                cout << "warning: failed to mmap " << path.u8string() << res.error().error<std::string>() << "\n";
                continue;
            }
            auto read_view = res->read_view();
            if (read_view.empty()) {
                cout << "warning: failed to read " << path.u8string() << "\n";
                continue;
            }
            auto relative_path = fs::relative(path, root_path);
            auto u8path = relative_path.u8string();
            EmbedFileIndex file;
            set_varint_len(file.name.len, u8path.size());
            file.name.name = futils::view::rvec{u8path};
            set_varint_len(file.len, read_view.size());
            set_varint_len(file.offset, offset);
            offset += read_view.size();
            if (auto err = file.encode(iw)) {
                cout << "fatal: failed to encode " << path.u8string() << err.error<std::string>() << "\n";
                return 1;
            }
            if (!ew.write(read_view)) {
                cout << "fatal: failed to write " << path.u8string() << "\n";
                return 1;
            }
            file_count++;
        }
    }
    if (!futils::binary::write_num(iw, file_count, true)) {
        cout << "fatal: failed to write file count\n";
        return 1;
    }
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.make_embed_file) {
    }
    TextController text{futils::console::Window::create(text_layout, 80, 10).value()};
    Menu menu;
    while (menu.run(text, flags)) {
        ;
    }
    text.show_cursor();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
