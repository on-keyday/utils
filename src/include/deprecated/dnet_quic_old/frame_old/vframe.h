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
#include "../../../dll/allocator.h"
#include "../../../../helper/defer.h"
#include "../../../boxbytelen.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {
            template <class F>
            struct RawVFrame;

            // VFrame is Frame on heap.
            // derived classes have overhead to memory management area
            struct VFrame {
               protected:
                FrameType type_;
                template <class T, template <class...> class Ptr>
                friend Ptr<RawVFrame<T>> make_vframe_base(const T& frame, auto&&);

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
                BoxByteLen raw_data;
                F frame_;
                static_assert(std::is_base_of_v<Frame, F>, "must be derived from Frame");
                template <class T, template <class...> class Ptr>
                friend Ptr<RawVFrame<T>> make_vframe_base(const T& frame, auto&&);

               public:
                const F* frame() const {
                    return &frame_;
                }
            };

            template <class F, template <class...> class Ptr>
            Ptr<RawVFrame<F>> make_vframe_base(const F& frame, auto&& make_) {
                size_t enbox = frame.visit_bytelen(nop_visitor());
                Ptr<RawVFrame<F>> d = make_();
                if (!d) {
                    return nullptr;
                }
                d->type_ = frame.type.type();
                d->frame_ = frame;
                if (enbox) {
                    d->raw_data = BoxByteLen{enbox};
                    if (!d->raw_data.valid()) {
                        return nullptr;
                    }
                    WPacket wp{d->raw_data.unbox()};
                    auto res = d->frame_.visit_bytelen([&](ByteLen& b) {
                        b = wp.copy_from(b);
                    });
                    if (res != enbox) {
                        return nullptr;
                    }
                }
                return d;
            }

            // make_vframe makes VFrame
            // if you want to cast Frame by Frame.type, use make_vframe_ex
            template <class F>
            std::shared_ptr<RawVFrame<F>> make_vframe(const F& frame) {
                return make_vframe_base<F, std::shared_ptr>(frame, []() {
                    return std::allocate_shared<RawVFrame<F>>(glheap_allocator<RawVFrame<F>>{});
                });
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

            bool vframe_apply(const std::shared_ptr<VFrame>& vf, auto&& cb) {
                if (!vf) {
                    return false;
                }
                return frame_type_to_Type(vf->type(), [&](auto serv) {
                    auto p = std::static_pointer_cast<RawVFrame<decltype(serv)>>(vf);
                    if constexpr (internal::has_logical_not<decltype(cb(p))>) {
                        return false == !cb(p);
                    }
                    cb(p);
                    return true;
                });
            }

            // bool cb(spec-Frame* frame)
            bool vframe_apply_lite(const auto& vf, auto&& cb) {
                if (!vf) {
                    return false;
                }
                return frame_type_to_Type(vf->type(), [&](auto serv) {
                    auto p = static_cast<RawVFrame<decltype(serv)>*>(std::to_address(vf));
                    if constexpr (internal::has_logical_not<decltype(cb(p))>) {
                        return false == !cb(p);
                    }
                    cb(p);
                    return true;
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

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils
