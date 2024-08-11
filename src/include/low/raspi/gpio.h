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
        constexpr auto sys_rio_bank0_offset = 0x00010000;
        constexpr auto sys_rio_reg_out_offset = 0x0000;
        constexpr auto sys_rio_reg_oe_offset = 0x0004;
        constexpr auto sys_rio_reg_sync_in_offset = 0x0008;
        constexpr auto rio_rw_offset = 0x0000;
        constexpr auto rio_xor_offset = 0x1000;
        constexpr auto rio_set_offset = 0x2000;
        constexpr auto rio_clr_offset = 0x3000;
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

        constexpr auto gpio_pads_offset(size_t gpio_number) {
            return pads_bank0_offset + 4 + (gpio_number * 4);
        }

        constexpr auto sys_rio_out_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_out_offset;
        }

        constexpr auto sys_rio_sync_in_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_sync_in_offset;
        }

        constexpr auto sys_rio_out_set_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_out_offset + rio_set_offset;
        }

        constexpr auto sys_rio_out_clr_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_out_offset + rio_clr_offset;
        }

        constexpr auto sys_rio_oe_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_oe_offset;
        }

        constexpr auto sys_rio_oe_set_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_oe_offset + rio_set_offset;
        }

        constexpr auto sys_rio_oe_clr_offset() {
            return sys_rio_bank0_offset + sys_rio_reg_oe_offset + rio_clr_offset;
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
            rpi::GPIO gpio_;

            constexpr GPIO(rpi::GPIO&& gpio) noexcept
                : gpio_(std::move(gpio)) {}

           public:
            static GPIO open() noexcept {
                return GPIO{rpi::GPIO::open(block_size)};
            }

            constexpr explicit operator bool() const noexcept {
                return static_cast<bool>(this->gpio_);
            }

            StatusRegister status(size_t gpio_number) noexcept {
                return StatusRegister{gpio_[gpio_status_offset(gpio_number) / 4]};
            }

            ControlRegister control(size_t gpio_number) noexcept {
                return ControlRegister{gpio_[gpio_ctrl_offset(gpio_number) / 4]};
            }

            void control(ControlRegister control, size_t gpio_number) noexcept {
                gpio_[gpio_ctrl_offset(gpio_number) / 4] = control.flags_1_.as_value();
            }

            VoltageSelect voltage() noexcept {
                return VoltageSelect{gpio_[pads_bank0_offset / 4]};
            }

            GPIORegister gpio(size_t gpio_number) noexcept {
                return GPIORegister{gpio_[gpio_pads_offset(gpio_number) / 4]};
            }

            void gpio(GPIORegister gpio, size_t gpio_number) noexcept {
                this->gpio_[gpio_pads_offset(gpio_number) / 4] = gpio.flags_4_.as_value();
            }

            GPIOs rio_out() noexcept {
                return GPIOs{gpio_[sys_rio_out_offset() / 4]};
            }

            GPIOs rio_sync_in() noexcept {
                return GPIOs{gpio_[sys_rio_sync_in_offset() / 4]};
            }

            GPIOs rio_out_enable() noexcept {
                return GPIOs{gpio_[sys_rio_oe_offset() / 4]};
            }

           private:
            static constexpr bool check_mask(GPIOs rio) noexcept {
                auto v = rio.flags_2_.as_value();
                if (rio.reserved()) {
                    return false;
                }
                if (v == 0) {
                    return false;
                }
                if (v & (v - 1)) {  // check if v is a power of 2
                    return false;
                }
                return true;
            }

           public:
            bool rio_out_set(GPIOs rio) noexcept {
                if (!check_mask(rio)) {
                    return false;
                }
                gpio_[sys_rio_out_set_offset() / 4] = rio.flags_2_.as_value();
            }

            bool rio_out_clr(GPIOs rio) noexcept {
                if (!check_mask(rio)) {
                    return false;
                }
                gpio_[sys_rio_out_clr_offset() / 4] = rio.flags_2_.as_value();
            }

            bool rio_out_enable_set(GPIOs rio) noexcept {
                if (!check_mask(rio)) {
                    return false;
                }
                gpio_[sys_rio_oe_set_offset() / 4] = rio.flags_2_.as_value();
            }

            bool rio_out_enable_clr(GPIOs rio) noexcept {
                if (!check_mask(rio)) {
                    return false;
                }
                gpio_[sys_rio_oe_clr_offset() / 4] = rio.flags_2_.as_value();
            }
        };

    }  // namespace rp1
}  // namespace futils::low::rpi
