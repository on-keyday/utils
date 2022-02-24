/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// damage - calc damage
#pragma once
#include <wrap/lite/lite.h>

namespace play {
    namespace damage {
        using string = utils::wrap::string;
        template <class T>
        using shared_ptr = utils::wrap::shared_ptr<T>;
        template <class T>
        using vector = utils::wrap::vector<T>;
        struct Target;

        struct Damage {
            size_t damage;
        };

        struct Ability {
            string name;
            virtual void process_damage(Target& from, Target& to, Damage& dam) {}
        };

        struct Weapon {
            string name;
        };

        struct Target {
            string name;
            shared_ptr<Weapon> weapon;
        };
    }  // namespace damage
}  // namespace play
