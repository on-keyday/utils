/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cmdline/template/parse_and_err.h>
#include <env/env_sys.h>
#include <fnet/dll/dll_path.h>
struct Flags : futils::cmdline::templ::HelpOption {
    std::string port;
    std::string secure_port;
    std::string quic_port;
    bool only_secure = false;
    bool cleartext_http2 = false;
    bool quic = false;
    bool prior_http3_alt_svc = false;
    bool single_thread = false;
    bool memory_debug = false;
    bool verbose = false;
    bool bind_public = false;
    bool no_input = false;
    std::string public_key;
    std::string private_key;
    futils::wrap::path_string libssl;
    futils::wrap::path_string libcrypto;
    std::string key_log;
    bool ssl = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        load_env();
        ctx.VarString(&port, "p,port", "port number (used for http) (default:8091)", "PORT");
        ctx.VarString(&secure_port, "P,secure-port", "secure port (used for https) number (default:8092)", "PORT");
        ctx.VarString(&quic_port, "Q,quic-port", "quic port number (used for http3) (default:8092)", "PORT");
        ctx.VarBool(&only_secure, "only-secure", "launch only secure server (must specify --ssl)");
        ctx.VarBool(&cleartext_http2, "h2c", "launch http2 cleartext server");
        ctx.VarBool(&quic, "http3", "launch http3 server");
        ctx.VarBool(&prior_http3_alt_svc, "prior-http3-alt-svc", "prioritize http3 alt-svc than http2");
        ctx.VarBool(&single_thread, "single", "single thread mode");
        ctx.VarBool(&memory_debug, "memory-debug", "add memory debug hook");
        ctx.VarBool(&verbose, "verbose", "verbose log");
        ctx.VarString(&public_key, "public-key", "public key file", "FILE");
        ctx.VarString(&private_key, "private-key", "private key file", "FILE");
        ctx.VarString(&libssl, "libssl", "libssl path", "FILE");
        ctx.VarString(&libcrypto, "libcrypto", "libcrypto path", "FILE");
        ctx.VarBool(&ssl, "ssl", "enable ssl");
        ctx.VarBool(&bind_public, "bind-public", "bind to public address (default: only localhost)");
        ctx.VarString(&key_log, "key-log", "key log file", "FILE");
        ctx.VarBool(&no_input, "no-input", "no input mode");
    }
#ifdef _WIN32
#define SUFFIX ".dll"
#else
#define SUFFIX ".so"
#endif
    void load_env() {
        auto env = futils::env::sys::env_getter();
        env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl" SUFFIX));
        env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto" SUFFIX));
        env.get_or(public_key, "FNET_PUBLIC_KEY", "cert.pem");
        env.get_or(private_key, "FNET_PRIVATE_KEY", "private.pem");
        verbose = env.get<std::string>("FNET_VERBOSE_LOG") == "1";
        bind_public = env.get<std::string>("FNET_BIND_PUBLIC") == "1";
        env.get_or(port, "FNET_PORT", "8091");
        env.get_or(secure_port, "FNET_SECURE_PORT", "8092");
        env.get_or(quic_port, "FNET_QUIC_PORT", "8092");
        env.get_or(key_log, "FNET_KEY_LOG", "");
    }
};
void log(futils::wrap::internal::Pack p);

void key_log(futils::view::rvec line, void*);