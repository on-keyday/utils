/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread/concurrent_queue.h>
#include <console/window.h>
#include <console/ansiesc.h>
#include <wrap/cout.h>
#include <wrap/cin.h>
#include <filesystem>
#include <thread>

extern futils::wrap::UtfOut& cout;
extern futils::wrap::UtfIn& cin;
extern bool signaled;

namespace io {
    template <class T>
    using AsyncChannel = futils::thread::MultiProducerChannelBuffer<T, std::allocator<T>>;
    constexpr auto text_layout = futils::console::make_layout_data<80, 10>(2);
    constexpr auto center_layout = futils::console::make_layout_data<80, 10>(4);
    namespace ansiesc = futils::console::escape;
    namespace fs = std::filesystem;

    enum class IORequestType {
        prompt,
        story,
        end_io,
        set_layout,
        cancel_input,
        clear_screen,
    };

    struct ShowParams {
        size_t delay = 100;
        size_t after_wait = 1000;
        size_t delay_offset = 0;
    };

    struct IORequest {
        IORequestType type;
        std::u32string text;
        ShowParams show_params;
    };

    struct IOResponse {
        IORequestType type;
        std::u32string result;
    };

    struct IOSharedState {
        std::shared_ptr<AsyncChannel<IORequest>> request_channel;
        size_t waiting_io_response = 0;
        std::shared_ptr<AsyncChannel<IOResponse>> response_channel;
        std::atomic<bool> io_thread_running = false;
        futils::console::Window layout;
    };

    struct IORequester {
        IORequester(std::shared_ptr<IOSharedState> shared)
            : shared_state(shared) {
        }

        void request(IORequest req) {
            shared_state->request_channel->send(std::move(req));
        }

        std::optional<IOResponse> receive() {
            return shared_state->response_channel->receive();
        }

        std::optional<IOResponse> receive_wait() {
            return shared_state->response_channel->receive_wait();
        }

       private:
        friend struct TextController;
        std::shared_ptr<IOSharedState> shared_state;
    };

    struct IOResponder {
        IOResponder(std::shared_ptr<IOSharedState> shared)
            : shared_state(shared) {
            shared->io_thread_running = true;
            shared->io_thread_running.notify_one();
        }

        ~IOResponder() {
            shared_state->io_thread_running = false;
            shared_state->io_thread_running.notify_one();
        }

        void respond(IOResponse res) {
            shared_state->response_channel->send(std::move(res));
        }

        std::optional<IORequest> receive() {
            return shared_state->request_channel->receive();
        }

        std::optional<IORequest> receive_wait() {
            return shared_state->request_channel->receive_wait();
        }

       private:
        std::shared_ptr<IOSharedState> shared_state;
    };

    enum class TimerState {
        start,
        waiting_delay,
        show_text,
        after_wait,
        completed,
        setup_prompt,
        waiting_input,
    };

    struct IOTimer {
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        TimerState state = TimerState::completed;
        IORequestType type = IORequestType::story;
        ShowParams params;
        std::u32string text;
        size_t text_index = 0;

        void reset(IORequest req) {
            state = TimerState::start;
            params = req.show_params;
            text = req.text;
            text_index = params.delay_offset;
            type = req.type;
        }

        constexpr bool is_running() const {
            return state != TimerState::completed;
        }
    };

    struct TextController {
       private:
        IORequester requester;

        static void io_thread(std::shared_ptr<IOSharedState> s) {
            IOResponder responder{s};
            auto cleanup_io = futils::helper::defer([&]() {
                cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
            });
            IOTimer timer;
            std::vector<IORequest> queue;
            std::u32string input;
            while (true) {
                if (!timer.is_running()) {
                    if (queue.size()) {
                        timer.reset(queue.front());
                        queue.erase(queue.begin());
                    }
                    else {
                        auto req = responder.receive_wait();
                        if (!req) {
                            return;
                        }
                        if (req->type == IORequestType::end_io) {
                            return;
                        }
                        if (req->type == IORequestType::set_layout) {
                            if (req->text == U"center") {
                                s->layout.reset_layout(center_layout);
                            }
                            else {
                                s->layout.reset_layout(text_layout);
                            }
                            continue;
                        }
                        if (req->type == IORequestType::clear_screen) {
                            clear_screen_impl();
                            continue;
                        }
                        timer.reset(*req);
                    }
                }
                write_story_impl(s->layout, timer);
                if (timer.state == TimerState::waiting_input) {
                    if (read_input_nonblock(s->layout, input, timer.text)) {
                        while (input.size() && (input.back() == U'\n' || input.back() == U'\r')) {
                            input.pop_back();
                        }
                        responder.respond(IOResponse{.type = IORequestType::prompt, .result = input});
                        input.clear();
                        hide_cursor();
                        cout << ansiesc::cursor(ansiesc::CursorMove::up, 1);
                        timer.state = TimerState::completed;
                    }
                }
            }
        }

       public:
        void set_layout(std::u32string_view name) {
            requester.request(IORequest{.type = IORequestType::set_layout, .text = std::u32string(name)});
        }

       private:
        static void write_text(futils::console::Window& window, futils::view::rvec data) {
            std::string buffer;
            futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            window.draw(w, data);
            cout << w.written();
        }
        static void move_back(futils::console::Window& window) {
            cout << ansiesc::cursor(ansiesc::CursorMove::up, window.dy());
            cout << ansiesc::cursor(ansiesc::CursorMove::from_left, 0);
        }

        static bool write_story_impl(futils::console::Window& window, IOTimer& timer) {
            if (!timer.is_running()) {
                return false;
            }
            auto now = std::chrono::steady_clock::now();
            while (true) {
                switch (timer.state) {
                    case TimerState::start: {
                        move_back(window);
                        timer.start_time = now;
                        if (timer.params.delay_offset == 0 && timer.params.delay == 0) {
                            auto output = futils::utf::convert<std::string>(timer.text);
                            write_text(window, output);
                            timer.state = timer.params.after_wait != 0         ? TimerState::after_wait
                                          : timer.type == IORequestType::story ? TimerState::completed
                                                                               : TimerState::setup_prompt;
                            break;
                        }
                        if (timer.params.delay_offset == 0) {
                            write_text(window, "");  // to show clear window
                        }
                        timer.state = TimerState::waiting_delay;
                        break;
                    }
                    case TimerState::waiting_delay:
                    case TimerState::after_wait: {
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timer.start_time).count();
                        auto threshold = timer.state == TimerState::waiting_delay ? timer.params.delay : timer.params.after_wait;
                        if (elapsed >= threshold) {
                            if (timer.state == TimerState::waiting_delay) {
                                timer.state = TimerState::show_text;
                                timer.start_time = now;
                            }
                            else {
                                timer.state = timer.type == IORequestType::story ? TimerState::completed : TimerState::setup_prompt;
                                break;
                            }
                        }
                        else {
                            return false;
                        }
                        break;
                    }
                    case TimerState::show_text: {
                        auto output = futils::utf::convert<std::string>(timer.text.substr(0, timer.text_index + 1));
                        move_back(window);
                        write_text(window, output);
                        timer.text_index++;
                        if (timer.text_index >= timer.text.size()) {
                            if (timer.params.after_wait == 0) {
                                timer.state = timer.type == IORequestType::story ? TimerState::completed : TimerState::setup_prompt;
                                break;
                            }
                            else {
                                timer.state = TimerState::after_wait;
                                timer.start_time = now;
                            }
                        }
                        else {
                            timer.state = TimerState::waiting_delay;
                            timer.start_time = now;
                        }
                        break;
                    }
                    case TimerState::setup_prompt: {
                        write_prompt_header(U"> ");
                        show_cursor();
                        timer.state = TimerState::waiting_input;
                        return true;
                    }
                    case TimerState::completed: {
                        return true;
                    }
                    default:
                        return false;
                }
            }
        }

        static bool read_input_nonblock(futils::console::Window& window, std::u32string& out, std::u32string_view prompt_msg) {
            futils::wrap::path_string buf = futils::utf::convert<futils::wrap::path_string>(out);
            futils::wrap::InputState state;
            state.non_block = true;
            state.no_echo = true;
            auto terminated = futils::wrap::input(buf, &state);
            out = futils::utf::convert<std::u32string>(buf);
            if (state.buffer_update) {
                /*
                clear_screen_impl();
                IOTimer tmp_timer;
                tmp_timer.reset(IORequest{.type = IORequestType::story, .text = std::u32string(prompt_msg), .show_params = ShowParams{.delay = 0, .after_wait = 0, .delay_offset = 0}});
                write_story_impl(window, tmp_timer);
                */
                std::erase(out, U'\r');
                std::erase(out, U'\n');
                cout << ansiesc::cursor(ansiesc::CursorMove::from_left, 0);
                cout << ansiesc::erase(ansiesc::EraseTarget::line, ansiesc::EraseMode::front_of_cursor);
                write_prompt_header(U"> ");
                cout << out;
            }
            if (state.ctrl_c) {
                signaled = true;
            }
            return terminated;
        }

        static void show_cursor() {
            cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
        }

        static void hide_cursor() {
            cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        }

        static void clear_screen_impl() {
            cout << ansiesc::erase(ansiesc::EraseTarget::screen, ansiesc::EraseMode::all);
            cout << ansiesc::cursor_abs(0, 0);
        }

        static void write_prompt_header(std::u32string_view text) {
            cout << ansiesc::erase(ansiesc::EraseTarget::line, ansiesc::EraseMode::back_of_cursor);
            cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
            cout << text;
        }

       public:
        void clear_screen() {
            requester.request(IORequest{.type = IORequestType::clear_screen});
        }

        void write_story(std::u32string text, size_t delay = 100, size_t after_wait = 1000, size_t delay_offset = 0) {
            requester.request(IORequest{.type = IORequestType::story, .text = std::move(text), .show_params = ShowParams{
                                                                                                   .delay = delay,
                                                                                                   .after_wait = after_wait,
                                                                                                   .delay_offset = delay_offset,
                                                                                               }});
        }

        std::string write_prompt(std::u32string text, size_t delay = 100, size_t after_wait = 1000, size_t delay_offset = 0) {
            requester.request(IORequest{.type = IORequestType::prompt, .text = std::move(text), .show_params = ShowParams{
                                                                                                    .delay = delay,
                                                                                                    .after_wait = after_wait,
                                                                                                    .delay_offset = delay_offset,
                                                                                                }});
            auto res = requester.receive_wait();
            if (!res) {
                return "";
            }
            return futils::utf::convert<std::string>(res->result);
        }

        void finish() {
            requester.request(IORequest{.type = IORequestType::end_io});
            requester.shared_state->io_thread_running.wait(true);
        }

        TextController(futils::console::Window window)
            : requester{std::make_shared<IOSharedState>()} {
            requester.shared_state->layout = std::move(window);
            requester.shared_state->request_channel = std::make_shared<AsyncChannel<IORequest>>();
            requester.shared_state->response_channel = std::make_shared<AsyncChannel<IOResponse>>();
            std::thread{io_thread, requester.shared_state}.detach();
            requester.shared_state->io_thread_running.wait(false);
        }
    };
}  // namespace io
