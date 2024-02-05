/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "long_packet.h"

namespace futils {
    namespace fnet::quic::packet {
        struct RetryPacket : LongPacketBase {
            view::rvec retry_token;
            byte retry_integrity_tag[16]{};

            static constexpr PacketType get_packet_type() {
                return PacketType::Retry;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, PacketType::Retry) &&
                       r.read(retry_token, r.remain().size() - 16) &&
                       r.read(retry_integrity_tag);
            }

            constexpr size_t len() const noexcept {
                return LongPacketBase::len() +
                       retry_token.size() +
                       16;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return render_with_pnlen(w, PacketType::Retry, 1) &&
                       w.write(retry_token) &&
                       w.write(retry_integrity_tag);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return LongPacketBase::rvec_field_count() +
                       1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                LongPacketBase::visit_rvec(cb);
                cb(retry_token);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                LongPacketBase::visit_rvec(cb);
                cb(retry_token);
            }
        };

        // rfc 9001 5.8. Retry Packet Integrity
        // this is not derived from Packet class
        struct RetryPseudoPacket {
            byte origDstIDlen = 0;  // only parse
            view::rvec origDstID;
            LongPacketBase long_packet;
            view::rvec retry_token;

            constexpr size_t len() const noexcept {
                return 1 + origDstID.size() + long_packet.len() + retry_token.size();
            }

            constexpr bool render(binary::writer& w) const {
                if (origDstID.size() > 0xff) {
                    return false;
                }
                return w.write(origDstID.size(), 1) &&
                       w.write(origDstID) &&
                       long_packet.render(w) &&
                       w.write(retry_token);
            }

            constexpr void from_retry_packet(view::rvec origdst, const RetryPacket& retry) {
                origDstID = origdst;
                long_packet = retry;
                retry_token = retry.retry_token;
            }
        };

        namespace test {
            constexpr bool check_retry() {
                RetryPacket retry;
                byte tok[] = {'h', 'e', 'l', 'o', 'n'};
                retry.version = 1;
                retry.retry_token = tok;
                byte tag[] = "fuzakeruna nemu";
                view::copy(retry.retry_integrity_tag, tag);
                byte data[100];
                binary::writer w{data};
                bool ok = retry.render(w);
                if (!ok) {
                    return false;
                }
                binary::reader r{w.written()};
                if (!retry.parse(r)) {
                    return false;
                }
                ok = retry.retry_token == tok &&
                     retry.retry_integrity_tag == view::rvec(tag);
                if (!ok) {
                    return false;
                }
                RetryPseudoPacket pseduo;
                pseduo.from_retry_packet(tok, retry);
                w.reset();
                return pseduo.render(w);
            }

            static_assert(check_retry());
        }  // namespace test

    }  // namespace fnet::quic::packet
}  // namespace futils
