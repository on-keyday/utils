/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by binred (https://github.com/on-keyday/utils)

#pragma once
#include <cstdint>
#include <string>
#include "error_type.h"
#include "../../wrap/lite/smart_ptr.h"

namespace utils {
    namespace net {
        namespace http2 {
            struct Frame {
                int len = 0;
                FrameType type;
                Flag flag = Flag::none;
                std::int32_t id;
            };

            template <class Output>
            H2Error encode(const Frame& input, Output& output) {
                output.write(input.len, 3);
                output.write(input.type);
                output.write(input.flag);
                if (!(input.id >= 0)) {
                    return H2Error::internal;
                }
                output.write(input.id);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, Frame& output) {
                if (!input.read(output.len, 3)) {
                    return H2Error::read_len;
                }
                if (!input.read(output.type)) {
                    return H2Error::read_type;
                }
                if (!input.read(output.flag)) {
                    return H2Error::read_flag;
                }
                if (!input.read(output.id)) {
                    return H2Error::read_id;
                }
                if (!(output.id >= 0)) {
                    return H2Error::internal;
                }
                return H2Error::none;
            }

            struct Priority {
                std::uint32_t depend = 0;
                std::uint8_t weight = 0;
            };

            template <class Output>
            H2Error encode(const Priority& input, Output& output) {
                output.write(input.depend);
                output.write(input.weight);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, Priority& output) {
                if (!input.read(output.depend)) {
                    return H2Error::read_depend;
                }
                if (!input.read(output.weight)) {
                    return H2Error::read_weight;
                }
                return H2Error::none;
            }

            struct DataFrame : Frame {
                std::uint8_t padding = 0;
                std::string data;
                std::string pad;
            };

            template <class Output>
            H2Error encode(const DataFrame& input, Output& output) {
                if (!(input.id != 0)) {
                    return H2Error::protocol;
                }
                if (!(input.type == FrameType::data)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (input.flag & Flag::padded) {
                    output.write(input.padding);
                }
                output.write(input.data, input.len - input.padding);
                output.write(input.pad, input.padding);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, DataFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::data)) {
                        return H2Error::unknown;
                    }
                }
                if (!(output.id != 0)) {
                    return H2Error::protocol;
                }
                if (output.flag & Flag::padded) {
                    if (!input.read(output.padding)) {
                        return H2Error::read_padding;
                    }
                }
                if (!input.read(output.data, output.len - output.padding)) {
                    return H2Error::read_data;
                }
                if (!input.read(output.pad, output.padding)) {
                    return H2Error::read_padding;
                }
                return H2Error::none;
            }

            struct HeaderFrame : Frame {
                std::uint8_t padding = 0;
                Priority priority;
                std::string data;
                std::string pad;
            };

            template <class Output>
            H2Error encode(const HeaderFrame& input, Output& output) {
                if (!(input.type == FrameType::header)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (input.flag & Flag::padded) {
                    output.write(input.padding);
                }
                if (input.flag & Flag::priority) {
                    if (auto e__ = encode(input.priority, output); H2Error::none != e__) {
                        return e__;
                    }
                }
                output.write(input.data, input.len - input.padding);
                output.write(input.pad, input.padding);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, HeaderFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::header)) {
                        return H2Error::unknown;
                    }
                }
                if (output.flag & Flag::padded) {
                    if (!input.read(output.padding)) {
                        return H2Error::read_padding;
                    }
                }
                if (output.flag & Flag::priority) {
                    if (auto e__ = decode(input, output.priority); H2Error::none != e__) {
                        return e__;
                    }
                }
                if (!input.read(output.data, output.len - output.padding)) {
                    return H2Error::read_data;
                }
                if (!input.read(output.pad, output.padding)) {
                    return H2Error::read_padding;
                }
                return H2Error::none;
            }

            struct PriorityFrame : Frame {
                Priority priority;
            };

            template <class Output>
            H2Error encode(const PriorityFrame& input, Output& output) {
                if (!(input.len == 5)) {
                    return H2Error::unknown;
                }
                if (!(input.type == FrameType::priority)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (auto e__ = encode(input.priority, output); H2Error::none != e__) {
                    return e__;
                }
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, PriorityFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::priority)) {
                        return H2Error::unknown;
                    }
                }
                if (!(output.len == 5)) {
                    return H2Error::unknown;
                }
                if (auto e__ = decode(input, output.priority); H2Error::none != e__) {
                    return e__;
                }
                return H2Error::none;
            }

            struct RstStreamFrame : Frame {
                std::uint32_t code;
            };

            template <class Output>
            H2Error encode(const RstStreamFrame& input, Output& output) {
                if (!(input.len == 4)) {
                    return H2Error::unknown;
                }
                if (!(input.type == FrameType::rst_stream)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                output.write(input.code);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, RstStreamFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::rst_stream)) {
                        return H2Error::unknown;
                    }
                }
                if (!(output.len == 4)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.code)) {
                    return H2Error::read_code;
                }
                return H2Error::none;
            }

            struct SettingsFrame : Frame {
                std::string setting;
                Dummy dummy;
            };

            template <class Output>
            H2Error encode(const SettingsFrame& input, Output& output) {
                if (!((input.len % 6) == 0)) {
                    return H2Error::frame_size;
                }
                if (!(input.id == 0)) {
                    return H2Error::protocol;
                }
                if (!(input.type == FrameType::settings)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if ((input.flag & Flag::ack) == 0) {
                    output.write(input.setting, input.len);
                }
                if (!(((input.flag & Flag::ack) == 0) || (input.len == 0))) {
                    return H2Error::frame_size;
                }
                output.write(input.dummy);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, SettingsFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::settings)) {
                        return H2Error::unknown;
                    }
                }
                if (!((output.len % 6) == 0)) {
                    return H2Error::frame_size;
                }
                if (!(output.id == 0)) {
                    return H2Error::protocol;
                }
                if ((output.flag & Flag::ack) == 0) {
                    if (!input.read(output.setting, output.len)) {
                        return H2Error::read_data;
                    }
                }
                if (!input.read(output.dummy)) {
                    return H2Error::unknown;
                }
                if (!(((output.flag & Flag::ack) == 0) || (output.len == 0))) {
                    return H2Error::frame_size;
                }
                return H2Error::none;
            }

            struct PushPromiseFrame : Frame {
                std::uint8_t padding = 0;
                std::int32_t promise;
                std::string data;
                std::string pad;
            };

            template <class Output>
            H2Error encode(const PushPromiseFrame& input, Output& output) {
                if (!(input.type == FrameType::push_promise)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (input.flag & Flag::padded) {
                    output.write(input.padding);
                }
                if (!(input.promise > 0)) {
                    return H2Error::internal;
                }
                output.write(input.promise);
                output.write(input.data, input.len - input.padding);
                output.write(input.pad, input.padding);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, PushPromiseFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::push_promise)) {
                        return H2Error::unknown;
                    }
                }
                if (output.flag & Flag::padded) {
                    if (!input.read(output.padding)) {
                        return H2Error::unknown;
                    }
                }
                if (!input.read(output.promise)) {
                    return H2Error::internal;
                }
                if (!(output.promise > 0)) {
                    return H2Error::internal;
                }
                if (!input.read(output.data, output.len - output.padding)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.pad, output.padding)) {
                    return H2Error::unknown;
                }
                return H2Error::none;
            }

            struct PingFrame : Frame {
                std::uint64_t opeque;
            };

            template <class Output>
            H2Error encode(const PingFrame& input, Output& output) {
                if (!(input.len == 8)) {
                    return H2Error::unknown;
                }
                if (!(input.type == FrameType::ping)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (!(input.id == 0)) {
                    return H2Error::unknown;
                }
                output.write(input.opeque);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, PingFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::ping)) {
                        return H2Error::unknown;
                    }
                }
                if (!(output.len == 8)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.opeque)) {
                    return H2Error::unknown;
                }
                if (!(output.id == 0)) {
                    return H2Error::unknown;
                }
                return H2Error::none;
            }

            struct GoAwayFrame : Frame {
                std::int32_t id;
                std::uint32_t code;
                std::string data;
            };

            template <class Output>
            H2Error encode(const GoAwayFrame& input, Output& output) {
                if (!(input.type == FrameType::goaway)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (!(input.id >= 0)) {
                    return H2Error::unknown;
                }
                output.write(input.id);
                output.write(input.code);
                output.write(input.data, input.len - 8);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, GoAwayFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::goaway)) {
                        return H2Error::unknown;
                    }
                }
                if (!input.read(output.id)) {
                    return H2Error::unknown;
                }
                if (!(output.id >= 0)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.code)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.data, output.len - 8)) {
                    return H2Error::unknown;
                }
                return H2Error::none;
            }

            struct WindowUpdateFrame : Frame {
                std::int32_t increment;
            };

            template <class Output>
            H2Error encode(const WindowUpdateFrame& input, Output& output) {
                if (!(input.len == 4)) {
                    return H2Error::unknown;
                }
                if (!(input.type == FrameType::window_update)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                if (!(input.increment > 0)) {
                    return H2Error::unknown;
                }
                output.write(input.increment);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, WindowUpdateFrame& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::window_update)) {
                        return H2Error::unknown;
                    }
                }
                if (!(output.len == 4)) {
                    return H2Error::unknown;
                }
                if (!input.read(output.increment)) {
                    return H2Error::unknown;
                }
                if (!(output.increment > 0)) {
                    return H2Error::unknown;
                }
                return H2Error::none;
            }

            struct Continuation : Frame {
                std::string data;
            };

            template <class Output>
            H2Error encode(const Continuation& input, Output& output) {
                if (!(input.type == FrameType::continuous)) {
                    return H2Error::unknown;
                }
                if (auto e__ = encode(static_cast<const Frame&>(input), output); H2Error::none != e__) {
                    return e__;
                }
                output.write(input.data, input.len);
                return H2Error::none;
            }

            template <class Input>
            H2Error decode(Input&& input, Continuation& output, bool base_set = false) {
                if (!base_set) {
                    if (auto e__ = decode(input, static_cast<Frame&>(output)); H2Error::none != e__) {
                        return e__;
                    }
                    if (!(output.type == FrameType::continuous)) {
                        return H2Error::unknown;
                    }
                }
                if (!input.read(output.data, output.len)) {
                    return H2Error::unknown;
                }
                return H2Error::none;
            }

            template <class Output>
            H2Error encode(const Frame* input, Output& output) {
                if (input == nullptr) return H2Error::unknown;
                if ((*input).type == FrameType::data) {
                    return encode(static_cast<const DataFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::header) {
                    return encode(static_cast<const HeaderFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::priority) {
                    return encode(static_cast<const PriorityFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::rst_stream) {
                    return encode(static_cast<const RstStreamFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::settings) {
                    return encode(static_cast<const SettingsFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::push_promise) {
                    return encode(static_cast<const PushPromiseFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::ping) {
                    return encode(static_cast<const PingFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::goaway) {
                    return encode(static_cast<const GoAwayFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::window_update) {
                    return encode(static_cast<const WindowUpdateFrame&>(*input), output);
                }
                else if ((*input).type == FrameType::continuous) {
                    return encode(static_cast<const Continuation&>(*input), output);
                }
                else {
                    return encode(*input, output);
                }
            }

            template <class Input>
            H2Error decode(Input&& input, wrap::shared_ptr<Frame>& output) {
                Frame judgement;
                if (auto e__ = decode(input, judgement); H2Error::none != e__) {
                    return e__;
                }
                if (judgement.type == FrameType::data) {
                    auto p = wrap::make_shared<DataFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::header) {
                    auto p = wrap::make_shared<HeaderFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::priority) {
                    auto p = wrap::make_shared<PriorityFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::rst_stream) {
                    auto p = wrap::make_shared<RstStreamFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::settings) {
                    auto p = wrap::make_shared<SettingsFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::push_promise) {
                    auto p = wrap::make_shared<PushPromiseFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::ping) {
                    auto p = wrap::make_shared<PingFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::goaway) {
                    auto p = wrap::make_shared<GoAwayFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::window_update) {
                    auto p = wrap::make_shared<WindowUpdateFrame>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else if (judgement.type == FrameType::continuous) {
                    auto p = wrap::make_shared<Continuation>();
                    p->len = std::move(judgement.len);
                    p->type = std::move(judgement.type);
                    p->flag = std::move(judgement.flag);
                    p->id = std::move(judgement.id);
                    if (auto e__ = decode(input, *p, true); H2Error::none != e__) {
                        return e__;
                    }
                    output = p;
                    return H2Error::none;
                }
                else {
                    return H2Error::unknown;
                }
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils
