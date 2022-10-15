/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// length - length expectation
#pragma once
#include "types.h"
#include "cast.h"

namespace utils {
    namespace quic {
        namespace frame {
            tsize length(Frame& f, bool length_consist) {
                tsize len = 0;
                bool enter = false;
#define I(V)                                \
    {                                       \
        enter = true;                       \
        auto tmp = varint::least_enclen(V); \
        if (tmp == 0) {                     \
            len = 0;                        \
            return;                         \
        }                                   \
        len += tmp;                         \
    }
#define B(V, LENGTH)         \
    enter = true;            \
    if (V.size() < LENGTH) { \
        len = 0;             \
        return;              \
    }
#define L(V, LENGTH)      \
    enter = true;         \
    if (length_consist) { \
        B(V, LENGTH)      \
    }                     \
    len += LENGTH;
                auto fp = &f;
                auto l1 = [](Frame*) {};
                len = 1;  // "type" parameter
                if_<Padding>(fp, l1);
                if_<Ping>(fp, l1);
                if_<Ack>(fp, [&](Ack* a) {
                    I(a->largest_ack)
                    I(a->ack_delay)
                    I(a->ack_range_count)
                    I(a->first_ack_range)
                    B(a->ack_ranges, a->ack_range_count)
                    for (tsize i = 0; i < a->ack_range_count; i++) {
                        I(a->ack_ranges[i].gap)
                        I(a->ack_ranges[i].ack_len)
                    }
                    if (a->type == types::ACK_ECN) {
                        I(a->ecn_counts.ect0_count)
                        I(a->ecn_counts.ect1_count)
                        I(a->ecn_counts.ecn_ce_count)
                    }
                });
                if_<ResetStream>(fp, [&](ResetStream* r) {
                    I(r->stream_id)
                    I(r->application_error_code)
                    I(r->final_size)
                });
                if_<StopSending>(fp, [&](StopSending* r) {
                    I(r->stream_id)
                    I(r->application_error_code)
                });
                if_<Crypto>(fp, [&](Crypto* c) {
                    I(c->offset)
                    I(c->length)
                    L(c->crypto_data, c->length)
                });
                if_<NewToken>(fp, [&](NewToken* n) {
                    I(n->token_length)
                    L(n->token, n->token_length)
                });
                if_<Stream>(fp, [&](Stream* s) {
                    auto bit = byte(s->type);
                    I(s->stream_id)
                    if (bit & OFF) {
                        I(s->offset)
                    }
                    if (bit & LEN) {
                        I(s->length)
                    }
                    L(s->stream_data, s->length)
                });
                if_<MaxData>(fp, [&](MaxData* d) {
                    I(d->max_data)
                });
                if_<MaxStreamData>(fp, [&](MaxStreamData* d) {
                    I(d->stream_id)
                    I(d->max_stream_data)
                });
                if_<MaxStreams>(fp, [&](MaxStreams* d) {
                    I(d->max_streams)
                });
                if_<DataBlocked>(fp, [&](DataBlocked* d) {
                    I(d->max_data)
                });
                if_<StreamDataBlocked>(fp, [&](StreamDataBlocked* d) {
                    I(d->stream_id)
                    I(d->max_stream_data)
                });
                if_<StreamsBlocked>(fp, [&](StreamsBlocked* d) {
                    I(d->max_streams)
                });
                if_<NewConnectionID>(fp, [&](NewConnectionID* d) {
                    I(d->seq_number)
                    I(d->retire_proior_to)
                    len += 1;  // d->length
                    L(d->connection_id, d->length)
                    len += sizeof(d->stateless_reset_token);
                });
                if_<RetireConnectionID>(fp, [&](RetireConnectionID* r) {
                    I(r->seq_number)
                });
                if_<PathChallange>(fp, [&](PathChallange* p) {
                    len += sizeof(p->data);
                });
                if_<PathResponse>(fp, [&](PathResponse* p) {
                    len += sizeof(p->data);
                });
                if_<ConnectionClose>(fp, [&](ConnectionClose* c) {
                    I(c->error_code)
                    if (c->type == types::CONNECTION_CLOSE_APP) {
                        I(tsize(c->frame_type))
                    }
                    I(c->reason_phrase_length)
                    L(c->reason_phrase, c->reason_phrase_length)
                });
                if_<HandshakeDone>(fp, l1);
                if_<Datagram>(fp, [&](Datagram* d) {
                    if (d->type == types::DATAGRAM_LEN) {
                        I(d->length)
                    }
                    L(d->data, d->length)
                });
                if (!enter) {
                    return 0;
                }
                return len;
            }
#undef I
#undef L
#undef B
        }  // namespace frame
    }      // namespace quic
}  // namespace utils
