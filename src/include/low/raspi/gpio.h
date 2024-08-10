/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <binary/flags.h>
#include "gpio_bit.h"

namespace futils::low::rpi {

    struct GPIO {
       private:
        futils::view::swap_wvec gpio_map;

        constexpr GPIO(futils::view::wvec gpio_map) noexcept
            : gpio_map(gpio_map) {}

       public:
        constexpr GPIO() noexcept = default;
        constexpr GPIO(GPIO&&) noexcept = default;
        static GPIO open(size_t block_size) noexcept;

        constexpr explicit operator bool() const noexcept {
            return !this->gpio_map.w.null();
        }

        void free() noexcept;

        ~GPIO() noexcept {
            this->free();
        }

        volatile uint32_t& operator[](size_t index) noexcept {
            return reinterpret_cast<volatile uint32_t*>(this->gpio_map.w.data())[index];
        }
    };

    namespace rp1 {
        // https://github.com/raspberrypi/utils/blob/master/pinctrl/gpiochip_rp1.c#L503
        constexpr auto block_size = 0x30000;

        // https://github.com/raspberrypi/utils/blob/master/pinctrl/gpiochip_rp1.c#L11
        constexpr auto io_bank0_offset = 0;
        constexpr auto pads_bank0_offset = 0x00020000;
        constexpr auto pull_none = 0;
        constexpr auto pull_down = 1;
        constexpr auto pull_up = 2;
        constexpr auto pull_mask = 3;

        constexpr auto max_gpio = 27;

        // https://datasheets.raspberrypi.com/rp1/rp1-peripherals.pdf
        constexpr auto gpio_status_offset(size_t gpio_number) {
            return gpio_number * 8;
        }

        constexpr auto gpio_ctrl_offset(size_t gpio_number) {
            return gpio_number * 8 + 4;
        }

        constexpr auto interrupt = 0x100;
        enum class Interrupt {
            proc0,
            proc1,
            pcie,
        };

        constexpr auto interrupt_enable_offset(Interrupt interrupt) {
            return 0x104 + static_cast<size_t>(interrupt) * 12;
        }

        constexpr auto interrupt_force_offset(Interrupt interrupt) {
            return 0x108 + static_cast<size_t>(interrupt) * 12;
        }

        constexpr auto interrupt_status_offset(Interrupt interrupt) {
            return 0x10c + static_cast<size_t>(interrupt) * 12;
        }

        struct GPIO {
           private:
            rpi::GPIO gpio;

            constexpr GPIO(rpi::GPIO&& gpio) noexcept
                : gpio(std::move(gpio)) {}

           public:
            static GPIO open() noexcept {
                return GPIO{rpi::GPIO::open(block_size)};
            }

            constexpr explicit operator bool() const noexcept {
                return static_cast<bool>(this->gpio);
            }

            StatusRegister status(size_t gpio_number) noexcept {
                return StatusRegister{gpio[gpio_status_offset(gpio_number) / 4]};
            }

            ControlRegister control(size_t gpio_number) noexcept {
                return ControlRegister{gpio[gpio_ctrl_offset(gpio_number) / 4]};
            }

            void control(ControlRegister control, size_t gpio_number) noexcept {
                gpio[gpio_ctrl_offset(gpio_number) / 4] = control.flags_1_.as_value();
            }

            VoltageSelect voltage() noexcept {
                return VoltageSelect{gpio[pads_bank0_offset / 4]};
            }

            GPIORegister gpio(size_t gpio_number) noexcept {
                return GPIORegister{gpio[(pads_bank0_offset / 4) + 1 + gpio_number]};
            }
        };

    }  // namespace rp1
}  // namespace futils::low::rpi
