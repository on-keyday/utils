/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <timer/time.h>
#include <timer/to_string.h>
#include <timer/clock.h>

int main() {
    futils::timer::test::test_format();
    auto epoch_time = futils::timer::unix_epoch.to_time(futils::timer::unix_epoch) +
                      futils::timer::Time{9 * futils::timer::sec_per_hour, 0};
    auto epoch = futils::timer::unix_epoch.from_time(epoch_time);
    auto now = epoch.from_time(futils::timer::utc_clock.now());
}
