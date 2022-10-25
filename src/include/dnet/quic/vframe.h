/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// vframe - boxed frame
#pragma once
#include "frame_util.h"
#include <memory>
#include "../dll/allocator.h"
#include "../../helper/defer.h"

namespace utils {
    namespace dnet {
        namespace quic {
            template <class F>
            struct RawVFrame;

            // VFrame is Frame on heap.
            // derived classes have overhead to memory management area
            struct VFrame {
               protected:
                FrameType type_;
                template <class T>
                friend std::shared_ptr<RawVFrame<T>> make_vframe(const T& frame);

               public:
                FrameType type() const {
                    return type_;
                }
            };

            // RawVFrame is derived from VFrame
            // F type parameter is must be derived from Frame
            template <class F>
            struct RawVFrame : VFrame {
               private:
                ByteLen raw_data;
                F frame_;
                static_assert(std::is_base_of_v<Frame, F>, "must be derived from Frame");
                template <class T>
                friend std::shared_ptr<RawVFrame<T>> make_vframe(const T& frame);

               public:
                ~RawVFrame() {
                    if (raw_data.data) {
                        free_normal(raw_data.data, DNET_DEBUG_MEMORY_LOCINFO(true, raw_data.len));
                    }
                }

                F* frame() const {
                    return &frame_;
                }
            };

            // make_vframe makes VFrame
            // if you want to cast Frame by Frame.type, use make_vframe_ex
            template <class F>
            std::shared_ptr<RawVFrame<F>> make_vframe(const F& frame) {
                ByteLen raw;
                auto flen = frame.frame_len();
                raw.data = (byte*)alloc_normal(flen, DNET_DEBUG_MEMORY_LOCINFO(true, flen));
                if (!raw.data) {
                    return nullptr;
                }
                raw.len = flen;
                auto r = helper::defer([&] {
                    free_normal(raw.data, DNET_DEBUG_MEMORY_LOCINFO(true, flen));
                });
                std::shared_ptr<RawVFrame<F>> d =
                    std::allocate_shared<RawVFrame<F>, glheap_allocator<RawVFrame<F>>>(
                        glheap_allocator<RawVFrame<F>>{});
                WPacket wp{raw};
                if (!frame.render(wp)) {
                    return nullptr;
                }
                if (wp.overflow) {
                    return nullptr;
                }
                if (!d->frame_.parse(wp.b)) {
                    return nullptr;
                }
                d->type_ = frame.type.type();
                d->raw_data = raw;
                r.cancel();
                return d;
            }

            // make_vframe_ex makes VFrame with typed
            // if frame.type is known Type, invoke make_vframe with sepecific type
            inline std::shared_ptr<VFrame> make_vframe_ex(const Frame& frame) {
                return frame_type_to_Type(frame.type.type(), [&](const auto& serv) -> std::shared_ptr<VFrame> {
                    return make_vframe(static_cast<decltype(serv)>(frame));
                });
            }

            template <class F>
            std::shared_ptr<RawVFrame<F>> vframe_cast(const std::shared_ptr<VFrame>& vf) {
                if (!vf) {
                    return nullptr;
                }
                return frame_type_to_Type(vf->type(), [&](auto serv) -> std::shared_ptr<RawVFrame<F>> {
                    if constexpr (std::is_same_v<decltype(serv), F>) {
                        return std::static_pointer_cast<RawVFrame<F>>(vf);
                    }
                    return nullptr;
                });
            }

            constexpr auto vframe_vec(auto& vec, FrameType* errtype) {
                return [&, errtype](auto& fr, bool err) {
                    if (err) {
                        if (errtype) {
                            *errtype = fr.type.type();
                        }
                        return false;
                    }
                    auto vf = make_vframe(fr);
                    if (!vf) {
                        if (errtype) {
                            *errtype = fr.type.type();
                        }
                        return false;
                    }
                    vec.push_back(std::move(vf));
                    return true;
                };
            }

            constexpr auto frame_vec(auto& vec, FrameType* errtype) {
                return [&, errtype](auto& fr, bool err) {
                    if (err) {
                        if (errtype) {
                            *errtype = fr.type.type();
                        }
                        return false;
                    }
                    using F = std::remove_cvref_t<decltype(fr)>;
                    auto frame = std::allocate_shared<F>(glheap_allocator<F>{});
                    *frame = fr;
                    vec.push_back(std::move(frame));
                    return true;
                };
            }

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
