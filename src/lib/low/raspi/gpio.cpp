/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/detect.h>
#include <low/raspi/gpio.h>
#ifdef FUTILS_PLATFORM_LINUX
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <helper/defer.h>

namespace futils::low::rpi {

    int try_open_gpiomem() noexcept {
        const auto fd = ::open("/dev/gpiomem", O_RDWR | O_SYNC);
        if (fd >= 0) {
            return fd;
        }
        for (auto i = 0; i < 4; i++) {
            char buf[] = "/dev/gpiomem0";
            buf[sizeof(buf) - 2] = '0' + i;
            const auto fd = ::open(buf, O_RDWR | O_SYNC);
            if (fd >= 0) {
                return fd;
            }
        }
        return -1;
    }

    constexpr auto periph_base = 0x20000000;
    constexpr auto io_bank_base_address = 0x400d0000;

    GPIO GPIO::open(size_t block_size) noexcept {
        const auto gpio_fd = try_open_gpiomem();
        if (gpio_fd < 0) {
            return GPIO{};
        }
        const auto d = futils::helper::defer([&] {
            ::close(gpio_fd);
        });
        auto ptr = mmap(nullptr, block_size, PROT_READ | PROT_WRITE, MAP_SHARED, gpio_fd, 0);
        if (ptr == MAP_FAILED) {
            return GPIO{};
        }
        return GPIO{futils::view::wvec{static_cast<byte*>(ptr), block_size}};
    }

    void GPIO::free() noexcept {
        if (this->gpio_map.w.null()) {
            return;
        }
        auto m = this->gpio_map.release();
        munmap(m.data(), m.size());
    }

}  // namespace futils::low::rpi
#else
namespace futils::low::rpi {
    // stubs
    GPIO GPIO::open() noexcept {
        return GPIO{};
    }

    void GPIO::free() noexcept {
    }
}  // namespace futils::low::rpi
#endif
