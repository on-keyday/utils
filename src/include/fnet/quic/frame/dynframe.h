/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// dynref - dynamic reference type
#pragma once
#include "base.h"
#include "../../storage.h"
#include <memory>

namespace futils {
    namespace fnet::quic::frame {
        struct DynFrame {
            virtual FrameType type(bool on_cast) const = 0;
            virtual bool render(binary::writer& w) const = 0;
            virtual size_t len(bool use_length_field) const = 0;
            virtual Frame* frame() = 0;
            virtual ~DynFrame() {}
        };

        template <class F, size_t count = F::rvec_field_count()>
        struct DynFrameImpl : DynFrame {
            flex_storage storage_;
            F frame_;

            Frame* frame() override {
                frame_.type = frame_.get_type(false);
                return &frame_;
            }

            FrameType type(bool on_cast) const override {
                return frame_.get_type(on_cast);
            }

            bool render(binary::writer& w) const override {
                return frame_.render(w);
            }

            size_t len(bool use_length_field) const override {
                return frame_.len(use_length_field);
            }

            virtual ~DynFrameImpl() {}
        };

        template <class F>
        struct DynFrameImpl<F, 0> : DynFrame {
            F frame_;

            Frame* frame() override {
                frame_.type = frame_.get_type(false);
                return &frame_;
            }

            FrameType type(bool on_cast) const override {
                return frame_.get_type(on_cast);
            }

            bool render(binary::writer& w) const override {
                return frame_.render(w);
            }

            size_t len(bool use_length_field) const override {
                return frame_.len(use_length_field);
            }

            virtual ~DynFrameImpl() {}
        };

        using DynFramePtr = std::unique_ptr<DynFrame, fnet::internal::Delete>;

        template <class F>
        inline std::unique_ptr<DynFrameImpl<std::decay_t<F>>, fnet::internal::Delete> makeDynFrame(F&& f) {
            using DF = std::decay_t<F>;
            using Dyn = DynFrameImpl<DF>;
            Dyn* ptr = new_from_global_heap<Dyn>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(Dyn), alignof(Dyn)));
            ptr->frame_ = std::move(f);
            if constexpr (DF::rvec_field_count() != 0) {
                size_t size = 0;
                ptr->frame_.visit_rvec([&](view::rvec& rvec) {
                    size += rvec.size();
                });
                ptr->storage_.reserve(size);
                ptr->frame_.visit_rvec([&](view::rvec& rvec) {
                    size_t cur = ptr->storage_.size();
                    ptr->storage_.append(rvec);
                    rvec = ptr->storage_.rvec().substr(cur, ptr->storage_.size());
                });
            }
            return std::unique_ptr<Dyn, fnet::internal::Delete>{ptr};
        }

    }  // namespace fnet::quic::frame
}  // namespace futils
