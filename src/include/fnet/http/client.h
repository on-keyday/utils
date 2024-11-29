/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fnet/server/servh.h>
#include "http.h"
#include <fnet/util/uri.h>
#include <fnet/tls/config.h>

namespace futils::fnet::http {

    struct fnetserv_class_export Response {
        struct AbstractResponse;

       private:
        friend struct Destination;
        std::shared_ptr<AbstractResponse> response;
        Response(std::shared_ptr<AbstractResponse> r)
            : response(r) {}

       public:
        expected<void> wait();
        bool done() const;
    };

    using URI = uri::URI<flex_storage>;

    using RequestCallback = void (*)(void*, const URI& uri, RequestWriter& w);
    using ResponseCallback = void (*)(void*, const URI& uri, ResponseReader& r);

    struct UserCallback {
       private:
        void* c;
        void (*destroy)(void*);
        RequestCallback request;
        ResponseCallback response;

        constexpr void do_destroy() {
            if (destroy) {
                destroy(c);
                c = nullptr;
                destroy = nullptr;
            }
        }

       public:
        constexpr UserCallback()
            : c(nullptr), destroy(nullptr), request(nullptr), response(nullptr) {}
        constexpr UserCallback(void* c, void (*destroy)(void*), RequestCallback request, ResponseCallback response)
            : c(c), destroy(destroy), request(request), response(response) {}

        constexpr UserCallback(UserCallback&& x)
            : c(std::exchange(x.c, nullptr)), destroy(std::exchange(x.destroy, nullptr)), request(std::exchange(x.request, nullptr)), response(std::exchange(x.response, nullptr)) {}

        constexpr UserCallback& operator=(UserCallback&& x) {
            if (this == &x) {
                return *this;
            }
            do_destroy();
            c = std::exchange(x.c, nullptr);
            destroy = std::exchange(x.destroy, nullptr);
            request = std::exchange(x.request, nullptr);
            response = std::exchange(x.response, nullptr);
            return *this;
        }

        constexpr ~UserCallback() noexcept {
            if (destroy) {
                destroy(c);
            }
        }

        explicit operator bool() const noexcept {
            return request && response;
        }

        void do_request(const URI& uri, RequestWriter& w) {
            request(c, uri, w);
        }

        void do_response(const URI& uri, ResponseReader& r) {
            response(c, uri, r);
        }
    };

    struct fnetserv_class_export Client {
       private:
        struct AbstractClient;
        friend struct Destination;

        std::shared_ptr<AbstractClient> client;

        void init_client(std::shared_ptr<AbstractClient>& client);

       public:
        expected<Response> request(futils::view::rvec uri, UserCallback cb);

        void set_tls_config(tls::TLSConfig&& conf);
    };

}  // namespace futils::fnet::http
