
// Code generated by ifacegen (https://github.com/on-keyday/utils) DO NOT EDIT.

#pragma once
#include <string>
#include <helper/deref.h>

namespace utils::net {
    struct Hey {
       private:
        struct interface {
            virtual std::string hey(const std::string& name) = 0;
            virtual std::string hey(const char* name) = 0;
            virtual bool is_boy() const = 0;

            virtual ~interface() {}
        };

        template <class T>
        struct implement : interface {
            T t_holder_;

            template <class... Args>
            implement(Args&&... args)
                : t(std::forward<Args>(args)...) {}

            std::string hey(const std::string& name) override {
                auto t_ptr_ = utils::helper::deref(this->t_holder_) if (!t_ptr_) {
                    return {};
                }
                return t_ptr_->hey(name);
            }

            std::string hey(const char* name) override {
                auto t_ptr_ = utils::helper::deref(this->t_holder_) if (!t_ptr_) {
                    return {};
                }
                return t_ptr_->hey(name);
            }

            bool is_boy() const override {
                auto t_ptr_ = utils::helper::deref(this->t_holder_) if (!t_ptr_) {
                    return false;
                }
                return t_ptr_->is_boy();
            }
        };
        interface* iface = nullptr;

       public:
        template <class T>
        Hey(T&& t){
            iface = new implement<std::decay_t<T>>(std::forward<T>(t))}

        Hey(Hey&& in) {
            iface = in.iface;
            in.iface = nullptr;
        }

        Hey& operator=(Hey&& in) {
            delete iface;
            iface = in.iface;
            in.iface = nullptr;
            return *this;
        }

        operator bool() const {
            return iface != nullptr;
        }

        std::string hey(const std::string& name) {
            return iface ? iface->hey(name) : {};
        }

        std::string hey(const char* name) {
            return iface ? iface->hey(name) : {};
        }

        bool is_boy() const {
            return iface ? iface->is_boy() : false;
        }
    };

}  // namespace utils::net