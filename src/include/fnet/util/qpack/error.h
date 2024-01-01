/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils {
    namespace qpack {
        enum class QpackError {
            none,
            input_length,
            output_length,
            undefined_instruction,
            encoding_error,
            unexpected_error,
            large_capacity,
            small_capacity,
            large_field,
            invalid_static_table_ref,
            invalid_dynamic_table_abs_ref,
            no_entry_for_index,
            invalid_increment,
            invalid_required_insert_count,
            invalid_index,
            invalid_post_base_index,
            invalid_stream_id,
            no_entry_exists,
            dynamic_ref_exists,
            negative_base,
            head_of_blocking,
        };
    }
}  // namespace futils
