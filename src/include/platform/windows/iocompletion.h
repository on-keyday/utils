/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include<cstddef>
#include"../../helper/deref.h"

#ifndef NOVTABLE__
#ifdef _WIN32
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif

namespace utils {
namespace platform {
namespace windows {
struct CCBImpl {

   private:
    struct vtable__t {
        void (*call)(void* this__, void* ol, size_t com) = nullptr;
    };

    template<class T__v>
    struct vtable__instance__ {
       private:
        constexpr vtable__instance__() = default;
        using this_type = std::remove_pointer_t<decltype(utils::helper::deref(std::declval<std::decay_t<T__v>&>()))>;

        static void call(void* this__, void* ol, size_t com) {
            return static_cast<this_type*>(this__)->call(ol, com);
        }

       public:
        static const vtable__t* instantiate() noexcept {
            static vtable__t instance{
                &vtable__instance__::call,
            };
            return &instance;
        }
    };

   public:
    struct vtable__interface__ {
       private:
        void* this__ = nullptr;
        const vtable__t* vtable__ = nullptr;
       public:
        constexpr vtable__interface__() = default;

        explicit operator bool() const {
             return this__!=nullptr&&vtable__!=nullptr;
        }

        const vtable__t* to_c_style_vtable() const {
            return vtable__;
        }

        void* to_c_style_this() const {
            return this__;
        }

        template<class T__v>
        vtable__interface__(T__v&& v__)
            :this__(static_cast<void*>(utils::helper::deref(v__))),vtable__(vtable__instance__<T__v>::instantiate()){}

        void call(void* ol, size_t com) {
            return vtable__->call(this__, ol, com);
        }

    };


   private:

    struct NOVTABLE__ interface__ {
        virtual void call(void* ol, size_t com) = 0;
        virtual vtable__interface__ vtable__get__() const noexcept = 0;
        virtual interface__* move__(void* __storage_box) = 0;

        virtual ~interface__() = default;
    };

    template<class T__>
    struct implements__ : interface__ {
        T__ t_holder_;

        template<class V__>
        implements__(V__&& args)
            :t_holder_(std::forward<V__>(args)){}

        void call(void* ol, size_t com) override {
            auto t_ptr_ = utils::helper::deref(this->t_holder_);
            if (!t_ptr_) {
                return (void)0;
            }
            return t_ptr_->call(ol, com);
        }

        vtable__interface__ vtable__get__() const noexcept override {
            return vtable__interface__(const_cast<T__&>(t_holder_));
        }

        interface__* move__(void* __storage_box) override {
           using gen_type = implements__<T__>;
           if constexpr (sizeof(gen_type) <= sizeof(void*)*3&&
                         alignof(gen_type) <= alignof(std::max_align_t)&&
                         std::is_nothrow_move_constructible<T__>::value) {
               return new(__storage_box) implements__<T__>(std::move(t_holder_));
           }
           else {
               return nullptr;
           }
        }

    };

    union {
        char __storage_box[sizeof(void*)*(1+(3))]{0};
        std::max_align_t __align_of;
        struct {
            void* __place_holder[3];
            interface__* iface;
        };
    };

    template<class T__>
    void new___(T__&& v) {
        interface__* p = nullptr;
        using decay_T__ = std::decay_t<T__>;
        using gen_type= implements__<decay_T__>;
        if constexpr (sizeof(gen_type) <= sizeof(void*)*3&&
                      alignof(gen_type) <= alignof(std::max_align_t)&&
                      std::is_nothrow_move_constructible<decay_T__>::value) {
            p = new (__storage_box) gen_type(std::forward<T__>(v));
        }
        else {
            p = new gen_type(std::forward<T__>(v));
        }
        iface = p;
    }

    bool is_local___() const {
        return static_cast<const void*>(__storage_box)==static_cast<const void*>(iface);
    }

    void delete___() {
        if(!iface)return;
        if(!is_local___()) {
            delete iface;
        }
        else {
            iface->~interface__();
        }
        iface=nullptr;
    }

   public:
    constexpr CCBImpl(){}

    constexpr CCBImpl(std::nullptr_t){}

    template <class T__>
    CCBImpl(T__&& t) {
        static_assert(!std::is_same<std::decay_t<T__>,CCBImpl>::value,"can't accept same type");
        if(!utils::helper::deref(t)){
            return;
        }
        new___(std::forward<T__>(t));
    }

    CCBImpl(CCBImpl&& in) noexcept {
        // reference implementation: MSVC std::function
        if (in.is_local___()) {
            iface = in.iface->move__(__storage_box);
            in.delete___();
        }
        else {
            iface = in.iface;
            in.iface = nullptr;
        }
    }

    CCBImpl& operator=(CCBImpl&& in) noexcept {
        if(this==std::addressof(in))return *this;
        delete___();
        // reference implementation: MSVC std::function
        if (in.is_local___()) {
            iface = in.iface->move__(__storage_box);
            in.delete___();
        }
        else {
            iface = in.iface;
            in.iface = nullptr;
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return iface != nullptr;
    }

    bool operator==(std::nullptr_t) const noexcept {
        return iface == nullptr;
    }
    
    ~CCBImpl() {
        delete___();
    }

    void call(void* ol, size_t com) {
        return iface?iface->call(ol, com):(void)0;
    }

    vtable__interface__ get_self_vtable() const noexcept {
         return iface?iface->vtable__get__():vtable__interface__{};
    }

    template<class T__v>
    static vtable__interface__ get_vtable(T__v& v) noexcept {
         return vtable__interface__(v);
    }

    CCBImpl(const CCBImpl&) = delete;

    CCBImpl& operator=(const CCBImpl&) = delete;

    CCBImpl(CCBImpl&) = delete;

    CCBImpl& operator=(CCBImpl&) = delete;

};

using CCBInvoke = decltype(std::declval<CCBImpl>().get_self_vtable());

} // namespace windows
} // namespace platform
} // namespace utils
