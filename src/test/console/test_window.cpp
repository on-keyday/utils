/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <console/window.h>
#include <wrap/cout.h>
#include <wrap/cin.h>
#include <console/ansiesc.h>
#include <thread>

int main() {
    auto& cout = futils::wrap::cout_wrap();
    auto& cin = futils::wrap::cin_wrap();
    auto text_layout = futils::console::make_layout_data<80, 10>(2);
    auto center_layout = futils::console::make_layout_data<80, 10>(4);
    auto layout = futils::console::Window::create(text_layout, 80, 10).value();
    auto write_text = [&](futils::view::rvec data) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        layout.draw(w, data);
        cout << w.written();
    };
    namespace ansiesc = futils::console::escape;
    layout.reset_layout(center_layout);
    write_text("Hello, World! - とある世界の物語");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    cout << ansiesc::erase(ansiesc::EraseTarget::line, ansiesc::EraseMode::back_of_cursor);
    auto move_back = [&] {
        cout << ansiesc::cursor(ansiesc::CursorMove::up, layout.dy());
    };

    auto write_story = [&](futils::console::Window& layout, std::u32string_view text) {
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        move_back();
        layout.reset_layout(text_layout);
        write_text("");
        std::string output;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (size_t i = 0; i < text.size(); i++) {
            move_back();
            output = futils::utf::convert<std::string>(text.substr(0, i + 1));
            write_text(output);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };
    auto write_prompt = [&](futils::console::Window& layout, std::u32string_view text) {
        write_story(layout, text);
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
    write_story(layout, U"Hello, World! こんにちは世界!\nあなたが幸せでありますように\nアーメン");
    write_story(layout, U"これはとある世界のお話。あるところに、とある村がありました。");
    write_story(layout, U"村には、魔王が住んでいました。");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto answer = write_prompt(layout, U"対決しますか?(y/n)");
    if (answer == "y") {
        write_story(layout, U"勇者は魔王に立ち向かいました。");
    }
    else {
        write_story(layout, U"勇者は魔王に立ち向かうことをやめました。");
    }
    auto ending = write_prompt(layout, U"お話はどうなるでしょうか?(good/bad)");
    if (ending == "good") {
        write_story(layout, U"お話は幸せな結末を迎えました。");
    }
    else {
        write_story(layout, U"お話は悲しい結末を迎えました。");
    }
}
