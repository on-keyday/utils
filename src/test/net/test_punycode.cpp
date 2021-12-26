/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/punycode.h"
#include "../../include/helper/pushbacker.h"
#include "../../include/helper/strutil.h"

// copiied from https://www.gnu.org/software/libidn/doxygen/punycode_8c_source.html#l00076

int punycode_decode(size_t input_length,
                    const char input[],
                    size_t *output_length,
                    std::uint32_t output[]) {
    std::uint32_t n, out, i, max_out, bias, oldi, w, k, digit, t;
    size_t b, j, in;

    /* Initialize the state: */

    n = utils::net::punycode::initial_n;
    constexpr auto maxint = (std::numeric_limits<int>::max)();
    out = i = 0;
    max_out = *output_length > maxint ? maxint
                                      : (std::uint32_t)*output_length;
    bias = utils::net::punycode::initial_bias;

    /* Handle the basic code points:  Let b be the number of input code */
    /* points before the last delimiter, or 0 if there is none, then    */
    /* copy the first b code points to the output.                      */

    for (b = j = 0; j < input_length; ++j)
        if (input[j] == '-')
            b = j;
    if (b > max_out)
        return 0;

    for (j = 0; j < b; ++j) {
        /*if (case_flags)
            case_flags[out] = flagged(input[j]);*/
        if (!utils::number::is_in_ascii_range(input[j]))
            return 0;
        output[out++] = input[j];
    }
    for (j = b + (b > 0); j < input_length; ++j)
        if (!utils::number::is_in_ascii_range(input[j]))
            return 0;

    /* Main decoding loop:  Start just after the last delimiter if any  */
    /* basic code points were copied; start at the beginning otherwise. */

    for (in = b > 0 ? b + 1 : 0; in < input_length; ++out) {
        /* in is the index of the next ASCII code point to be consumed, */
        /* and out is the number of code points in the output array.    */

        /* Decode a generalized variable-length integer into delta,  */
        /* which gets added to i.  The overflow checking is easier   */
        /* if we increase i as we go, then subtract off its starting */
        /* value at the end to obtain delta.                         */

        for (oldi = i, w = 1, k = utils::net::punycode::base_;; k += utils::net::punycode::base_) {
            if (in >= input_length)
                return 0;
            digit = utils::net::punycode::internal::decode_digit(input[in++]);
            if (digit >= utils::net::punycode::base_)
                return 0;
            if (digit > (maxint - i) / w)
                return 0;
            i += digit * w;
            t = k <= bias /* + tmin */ ? utils::net::punycode::t_min : /* +tmin not needed */
                    k >= bias + utils::net::punycode::t_max ? utils::net::punycode::t_max
                                                            : k - bias;
            if (digit < t)
                break;
            if (w > maxint / (utils::net::punycode::base_ - t))
                return 0;
            w *= (utils::net::punycode::base_ - t);
        }

        bias = utils::net::punycode::internal::calc_bias(i - oldi, out + 1, oldi == 0);

        /* i was supposed to wrap around from out+1 to 0,   */
        /* incrementing n each time, so we'll fix that now: */

        if (i / (out + 1) > maxint - n)
            return 0;
        n += i / (out + 1);
        if (n > 0x10FFFF || (n >= 0xD800 && n <= 0xDBFF))
            return 0;
        i %= (out + 1);

        /* Insert n at position i of the output: */

        /* not needed for Punycode: */
        /* if (basic(n)) return 0; */
        if (out >= max_out)
            return 0;
        /*
        if (case_flags) {
            memmove(case_flags + i + 1, case_flags + i, out - i);
            / Case of last ASCII code point determines case flag: *
        case_flags[i] = flagged(input[in - 1]);
    }
    */

        memmove(output + i + 1, output + i, (out - i) * sizeof *output);
        output[i++] = n;
    }

    *output_length = (size_t)out;
    /* cannot overflow because out <= old value of *output_length */
    return 0;
}

void test_punycode() {
    utils::helper::FixedPushBacker<char[50], 49> buf, buf2;
    utils::net::punycode::encode(u8"ウィキペディア", buf);
    assert(utils::helper::equal(buf.buf, "cckbak0byl6e"));
    size_t size = 49;
    utils::wrap::u32string v;
    v.resize(50);
    //punycode_decode(buf.count, buf.buf, &size, (std::uint32_t *)v.data());
    utils::net::punycode::decode(buf.buf, buf2);
}

int main() {
    test_punycode();
}
