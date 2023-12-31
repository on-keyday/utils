/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"
#include "../../../helper/defer.h"
#include "../../../view/span.h"

namespace utils {
    namespace fnet::quic::qlog {
        enum class VantagePointType {
            client,
            server,
            network,
            unknown,
        };

        struct Object {
            bool begin = false;
        };
#define OBJECT(field, required)               \
    if (!cb(field, Object{true}, required)) { \
        return true;                          \
    }                                         \
    const auto d_##__LINE__ = helper::defer([&] { cb(field, Object{false}); });
#define ENDOBJECT() return true;
#define CB(field, required)             \
    if (!cb(#field, field, required)) { \
        return false;                   \
    }
#define APPLY(field, required)            \
    if (!field.apply(#field, required)) { \
        return false;                     \
    }

        class VantagePoint {
            view::rvec name;
            VantagePointType type = VantagePointType::unknown;
            VantagePointType flow = VantagePointType::unknown;

            constexpr bool apply(const char* field, bool required, auto&& cb) {
                OBJECT(field, required)
                CB(name, false)
                CB(type, true)
                CB(flow, false)
                ENDOBJECT()
            }
        };

        struct TraceError {
            view::rvec error_description;
            view::rvec uri;
            VantagePoint vantage_point;

            constexpr bool apply(const char* field, auto&& cb) {
                OBJECT(field)
                CB(error_description, true)
                CB(uri, false)
                APPLY(vantage_point, false)
                ENDOBJECT()
            }
        };

        struct URI {
            view::rvec uri;
        };

        struct Configuration {
            double time_offset = 0;
            view::rspan<URI> original_uris;

            constexpr bool apply(const char* field, bool required, auto&& cb, auto&& custom_fields) {
                OBJECT(field, required)
                CB(time_offset, true)
                CB(original_uris, true)
                if (!custom_fields.apply(nullptr, false, cb)) {
                    return false;
                }
                ENDOBJECT()
            }
        };

        enum class TimeFormat {
            unspec,
        };

        struct ProtocolType {
            view::rvec type;
        };

        struct CommonFields {
            TimeFormat time_format = TimeFormat::unspec;
            double referrence_time = 0;
            view::rspan<ProtocolType> protocol_type;
            view::rvec group_id;

            constexpr bool apply(const char* field, bool required, auto&& cb, auto&& text) {
                OBJECT(field, required)
                CB(time_format, false)
                CB(referrence_time, false)
                CB(protocol_type, false)
                CB(group_id, false)
                if (!text.apply(nullptr, false, cb)) {
                    return false;
                }
                ENDOBJECT()
            }
        };

        struct TraceHeader {
            view::rvec title;
            view::rvec description;
            Configuration configuration;
            VantagePoint vantage_point;
            CommonFields common_fields;

            constexpr bool apply(const char* field, bool required, auto&& cb, auto&& custom_fields, auto&& text) {
                OBJECT(field, required)
                CB(title, false)
                CB(description, false)
                if (!configuration.apply("configuration", false, cb, custom_fields)) {
                    return false;
                }
                APPLY(vantage_point, false)
                if (!common_fields.apply("common_fields", false, cb, text)) {
                    return false;
                }
                ENDOBJECT()
            }
        };

        struct Summary {
            std::uint32_t trace_count = 0;
            std::uint64_t max_duration = 0;
            float max_outgoing_loss_rate = 0;
            std::uint64_t total_event_count = 0;
            std::uint64_t error_count = 0;
        };

        struct QLogFile {
            view::rvec qlog_version;
            view::rvec qlog_format;
            view::rvec title;
            view::rvec description;
            Summary summary;
        };
#undef OBJECT
#undef ENDOBJECT
#undef CB
#undef APPLY
    }  // namespace fnet::quic::qlog
}  // namespace utils
