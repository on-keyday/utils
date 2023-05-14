/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../../std/list.h"
#include <memory>

namespace utils {
    namespace fnet::quic::stream {

        template <class T = std::shared_ptr<void>, class RecvT = T>
        struct UserCallbackArgType {
            using accept_uni = T;
            using accept_bidi = T;
            using open_uni = T;
            using open_bidi = T;
            using recv = RecvT;
        };

        template <class A, class B, class C, class D, class E>
        struct UserCallbackArgType4 {
            using accept_uni = A;
            using accept_bidi = B;
            using open_uni = C;
            using open_bidi = D;
            using recv = E;
        };

        template <class ConnLock,
                  class CallbackArgType = UserCallbackArgType<std::shared_ptr<void>>,
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

            using callback_arg = CallbackArgType;
        };

    }  // namespace fnet::quic::stream
}  // namespace utils
