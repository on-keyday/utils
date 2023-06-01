/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../../std/list.h"
#include "impl/send_buffer.h"
#include "impl/recv_buffer.h"
#include <memory>

namespace utils {
    namespace fnet::quic::stream {

        template <class T = std::shared_ptr<void>, class RecvT = T>
        struct UserConnCallbackArgType {
            using accept_uni = T;
            using accept_bidi = T;
            using open_uni = T;
            using open_bidi = T;
            using recv = RecvT;
        };

        template <class A, class B, class C, class D, class E>
        struct UserConnCallbackArgType5 {
            using accept_uni = A;
            using accept_bidi = B;
            using open_uni = C;
            using open_bidi = D;
            using recv = E;
        };

        template <class SendBuf = impl::SendBuffer<>, class RecvBuf = impl::FragmentSaver<>>
        struct UserStreamHandlerType {
            using send_buf = SendBuf;
            using recv_buf = RecvBuf;
        };

        template <class ConnLock,
                  class StreamHandler = UserStreamHandlerType<>,
                  class ConnCBArgType = UserConnCallbackArgType<std::shared_ptr<void>>,
                  class SendStreamLock = ConnLock,
                  class RecvStreamLock = ConnLock,
                  template <class...> class RetransmitQue = slib::list,
                  template <class...> class StreamMap = slib::hash_map>
        struct TypeConfig {
            using conn_lock = ConnLock;
            using send_stream_lock = SendStreamLock;
            using recv_stream_lock = RecvStreamLock;
            template <class T>
            using retransmit_que = RetransmitQue<T>;
            template <class K, class V>
            using stream_map = StreamMap<K, V>;

            using conn_cb_arg = ConnCBArgType;
            using stream_handler = StreamHandler;
        };

    }  // namespace fnet::quic::stream
}  // namespace utils
