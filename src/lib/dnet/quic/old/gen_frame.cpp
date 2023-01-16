/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

/*
#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/frame/crypto.h>
#include <dnet/quic/internal/gen_frames.h>
#include <dnet/bytelen_split.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {

            error::Error generate_ack_frames(QUICContexts* q) {
                auto wp = q->get_tmpbuf();
                int idx = 0;
                for (auto& unack : q->unacked) {
                    ack::ACKRangeVec vec;
                    if (!unack.gen_range(vec)) {
                        continue;
                    }
                    auto [ack, ok] = frame::convert_to_ACKFrame(vec);
                    if (!ok) {
                        return QUICError{.msg = "failed to generate ACKFrame"};
                    }
                    if (auto err = q->frames.push_front(ack::PacketNumberSpace(idx), ack)) {
                        return err;
                    }
                    unack.clear();
                    idx++;
                    wp.reset_offset();
                }
                return error::none;
            }

            error::Error generate_crypto_frames(QUICContexts* q) {
                ack::PacketNumberSpace map_crypto_to_ack[] = {
                    ack::PacketNumberSpace::initial,
                    ack::PacketNumberSpace::application,
                    ack::PacketNumberSpace::handshake,
                    ack::PacketNumberSpace::application,
                };
                for (auto i = 0; i < 4; i++) {
                    if (crypto::EncryptionLevel(i) == crypto::EncryptionLevel::early_data) {
                        continue;  // TODO(on-keyday): skip now
                    }
                    auto& hsdata = q->crypto.hsdata[i];
                    if (hsdata.remain()) {
                        auto data = hsdata.get();
                        auto base_offset = hsdata.offset;
                        error::Error oerr;
                        spliter::split(data, spliter::const_len_fn(200), [&](ByteLen b, size_t offset, size_t) {
                            if (auto err = q->frames.push(map_crypto_to_ack[i],
                                                          frame::CryptoFrame{
                                                              .offset = base_offset + offset,
                                                              .crypto_data = view::rvec(b.data, b.len),
                                                          })) {
                                oerr = std::move(err);
                                return false;
                            }
                            hsdata.consume(b.len);
                            return true;
                        });
                        if (oerr) {
                            return oerr;
                        }
                    }
                }
                return error::none;
            }

            error::Error generate_streams(QUICContexts* q) {
                /*
                for (auto id : q->stream_ids) {
                    auto add_frame = [&](auto&& fr) {
                        return q->frames.push(ack::PacketNumberSpace::application, fr);
                    };
                    q->streams.send().generate_stream(id, add_frame);
                }
return error::none;
}

error::Error generate_frames(QUICContexts* q) {
    if (auto err = generate_ack_frames(q)) {
        return err;
    }
    if (auto err = generate_crypto_frames(q)) {
        return err;
    }
    // TODO(on-keyday): add frame generation
    return error::none;
}

error::Error on_frame_render(QUICContexts* q, const frame::Frame& f, size_t packet_number) {
    *if (f.type.type() == FrameType::STREAM) {
        return q->streams.send().on_send_packet(packet_number, &f);
    }
    * /
        return error::none;
}

}  // namespace quic::handler
}  // namespace dnet
}  // namespace utils
*/