/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "game.h"
#include <platform/detect.h>
#include <filesystem>
#if defined(FUTILS_PLATFORM_WINDOWS)
#include <windows.h>
#define FOREIGN_LIBRARY_WINDOWS
#elif __has_include(<dlfcn.h>)
#include <dlfcn.h>
#define FOREIGN_LIBRARY_POSIX
#else
#define FOREIGN_LIBRARY_UNSUPPORTED
#endif
#include "foreign.h"
#include <cstring>

namespace foreign {
    using foreign_func = void (*)(ForeignCallback*);
#if defined(FOREIGN_LIBRARY_WINDOWS)
    HMODULE load_library_platform(const futils::wrap::path_string& path) {
        return LoadLibraryW(path.c_str());
    }

    foreign_func get_function_platform(void* handle, const std::string& name) {
        return (foreign_func)GetProcAddress((HMODULE)handle, name.c_str());
    }

    void unload_library_platform(HMODULE handle) {
        FreeLibrary(handle);
    }

#elif defined(FOREIGN_LIBRARY_POSIX)
    void* load_library_platform(const futils::wrap::path_string& path) {
        return dlopen(path.c_str(), RTLD_LAZY);
    }

    foreign_func get_function_platform(void* handle, const std::string& name) {
        return (foreign_func)dlsym(handle, name.c_str());
    }

    void unload_library_platform(void* handle) {
        dlclose(handle);
    }

#else

    void* load_library_platform(const futils::wrap::path_string& path) {
        return nullptr;
    }

    void unload_library_platform(void* handle) {}

#endif

    std::shared_ptr<ForeignLibrary> load_foreign(RuntimeState& state, std::u32string& name) {
        if (!state.foreign_data) {
            state.foreign_data = std::make_shared<ForeignData>();
        }
        auto path = futils::utf::convert<futils::wrap::path_string>(name);
        std::error_code ec;
        auto abs_path = io::fs::absolute(path, ec);
        if (ec) {
            state.special_object_result = false;
            state.special_object_error_reason = futils::utf::convert<std::u32string>(ec.message());
            return nullptr;
        }
        if (auto found = state.foreign_data->libraries.find(abs_path); found != state.foreign_data->libraries.end()) {
            return found->second;
        }
        auto plt_handle = load_library_platform(abs_path);
        if (!plt_handle) {
            state.special_object_result = false;
            state.special_object_error_reason = U"ライブラリの読み込みに失敗しました";
            return nullptr;
        }
        auto lib = std::make_shared<ForeignLibrary>();
        lib->path = std::move(path);
        lib->data = (void*)plt_handle;
        lib->unload = [plt_handle](void*) {
            unload_library_platform(plt_handle);
        };
        state.foreign_data->libraries[lib->path] = lib;
        state.special_object_result = true;
        state.special_object_error_reason = U"";
        return lib;
    }

    bool unload_foreign(RuntimeState& state, std::shared_ptr<ForeignLibrary>& lib) {
        if (!state.foreign_data || !lib) {
            return false;
        }
        // someone may still use the library
        // so only erase from the map for safety
        state.foreign_data->libraries.erase(lib->path);
        return true;
    }

    struct FuncWrapper {
        FuncDataAsync callback;
        AsyncCallback async_callback;
        std::weak_ptr<AsyncFuncChannel> chan;
        Value temporary_location;
    };

    expr::EvalState call_foreign(RuntimeState& state, std::shared_ptr<ForeignLibrary>& lib, std::u32string& name, std::vector<Value>& args, Value& result, Callback& foreign_callback) {
        if (!state.foreign_data || !lib) {
            return state.eval_error(U"ライブラリが読み込まれていません");
        }
        if (!lib->data || !lib->unload) {
            return state.eval_error(U"ライブラリが読み込まれていません(bug)");
        }
        auto handle = lib->data;
        auto func = get_function_platform(handle, futils::utf::convert<std::string>(name).c_str());
        if (!func) {
            state.special_object_result = false;
            state.special_object_error_reason = U"関数" + name + U"が見つかりません";
            return expr::EvalState::normal;
        }
        struct InternalHolder {
            std::vector<Value>& args;
            Value& result;
            Value temporary_location;
            std::vector<Value> callback_arg;
            Value& callback_result;
            size_t callback_count;

            std::vector<std::string> strings;
            std::optional<Func> called_func;

            std::optional<std::u32string> abort_reason;

            std::shared_ptr<AsyncFuncChannel>& async_channel;
        };
        InternalHolder holder{
            .args = args,
            .result = result,
            .callback_result = foreign_callback.result,
            .callback_count = foreign_callback.count,
            .async_channel = state.async_callback_channel,
        };
        // clang-format off
        ForeignCallback cb{
            .data = (::ForeignContext*)(void*)&holder,
            .get_arg = [](::ForeignContext* data, size_t index) -> ValueData* {
                auto holder = (InternalHolder*)(void*)data;
                if (index >= holder->args.size()) {
                    return nullptr;
                }
                return (ValueData*)(void*)&holder->args[index];
            },
            .get_return_value_location = [](::ForeignContext* data) {
                auto holder = (InternalHolder*)(void*)data;
                return (ValueData*)(void*)&holder->result;
            },
            .set_boolean = [](ValueData* data, int value) {
                auto v = (Value*)(void*)data;
                *v =Value(Boolean{value!=0});
            },
            .set_number = [](ValueData* data, int64_t value) {
                auto v = (Value*)(void*)data;
                *v = Value(value);
            },
            .set_string = [](ValueData* data, const char* value) {
                auto v = (Value*)(void*)data;
                *v = Value(futils::utf::convert<std::u32string>(value));
            },
            .set_bytes = [](ValueData* data, const char* value, size_t size) {
                auto v = (Value*)(void*)data;
                *v = Value(Bytes{std::string(value, size)});
            },
            .set_array = [](ValueData* data, size_t size) {
                auto v = (Value*)(void*)data;
                *v = Value(ValueArray{std::vector<Value>(size)});
            },
            .set_object = [](ValueData* data) {
                auto v = (Value*)(void*)data;
                *v = Value(ValueMap{});
            },
            .set_foreign_object = [](ValueData* data, void* p_shared_ptr, int tag) {
                if(tag==0){
                    return;
                }
                auto v = (Value*)(void*)data;
                auto sh = (std::shared_ptr<void>*)p_shared_ptr;
                *v = Value(Foreign{ForeignType(tag),*sh});
            },
            .get_boolean = [](ValueData* data, int* value) -> int {
                auto v = (Value*)(void*)data;
                if (auto b = v->boolean()) {
                    *value = b->value ? 1 : 0;
                    return 1;
                }
                return 0;
            },
            .get_number = [](ValueData* data, int64_t* value) -> int {
                auto v = (Value*)(void*)data;
                if (auto n = v->number()) {
                    *value = *n;
                    return 1;
                }
                return 0;
            },
            .get_string = [](ValueData* data, char* value, size_t buf_size, size_t* size) -> int {
                auto v = (Value*)(void*)data;
                if (auto str = v->string()) {
                    auto str_ = futils::utf::convert<std::string>(*str);
                    if (str_.size() >= buf_size) {
                        if(size){
                            *size = str_.size();
                        }
                        return 0;
                    }
                    std::memcpy(value, str_.c_str(), str_.size());
                    if(size){
                        *size = str_.size();
                    }
                    return 1;
                }
                return 0;
            },
            .get_string_callback = [](ValueData* data,void* user, void (*callback)(const char*, size_t, void*)) -> int {
                auto v = (Value*)(void*)data;
                if (auto str = v->string()) {
                    auto str_ = futils::utf::convert<std::string>(*str);
                    callback(str_.c_str(), str_.size(), user);
                    return 1;
                }
                return 0;
            },
            .get_bytes = [](ValueData* data, char* value, size_t buf_size, size_t* size) -> int {
                auto v = (Value*)(void*)data;
                if (auto bytes = v->bytes()) {
                    if (bytes->data.size() >= buf_size) {
                        if(size){
                            *size = bytes->data.size();
                        }
                        return 0;
                    }
                    std::memcpy(value, bytes->data.c_str(), bytes->data.size());
                    if(size){
                        *size = bytes->data.size();
                    }
                    return 1;
                }
                return 0;
            },
            .get_bytes_callback = [](ValueData* data,void* user, void (*callback)(const char*, size_t, void*)) -> int {
                auto v = (Value*)(void*)data;
                if (auto bytes = v->bytes()) {
                    callback(bytes->data.c_str(), bytes->data.size(), user);
                    return 1;
                }
                return 0;
            },
            .get_foreign_object = [](ValueData* data, void* p_shared_ptr, int* tag) -> int {
                auto v = (Value*)(void*)data;
                auto sh = (std::shared_ptr<void>*)p_shared_ptr;
                if (auto f = v->foreign()) {
                    *sh = f->data;
                    *tag = (int)f->type;
                    return 1;
                }
                return 0;
            },
            .get_array_element = [](ValueData* data, size_t index, int may_append) -> ValueData* {
                auto v = (Value*)(void*)data;
                if (auto arr = v->array()) {
                    if (index >= arr->size()) {
                        if (may_append) {
                            arr->resize(index + 1);
                        } else {
                            return nullptr;
                        }
                    }
                    return (ValueData*)(void*)&(*arr)[index];
                }
                return nullptr;
            },
            .get_object_element = [](ValueData* data, const char* key, int may_create) -> ValueData* {
                auto v = (Value*)(void*)data;
                if (auto obj = v->object()) {
                    auto k = futils::utf::convert<std::u32string>(key);
                    if (auto found = obj->find(k); found != obj->end()) {
                        return (ValueData*)(void*)&found->second;
                    }
                    if (may_create) {
                        return (ValueData*)(void*)&obj->emplace(k, Value()).first->second;
                    }
                }
                return nullptr;
            },
            .get_func = [](ValueData* data) -> FuncData* {
                auto v = (Value*)(void*)data;
                if (auto f = v->func()) {
                    return (::FuncData*)(void*)f;
                }
                return nullptr;
            },
            .get_async_func = [](::ForeignContext* ctx,FuncData* data) {
                auto holder = (InternalHolder*)(void*)ctx;
                auto f = (Func*)(void*)data;
                auto n = new FuncWrapper();
                n->async_callback.func =*f;
                if(!holder->async_channel){
                    holder->async_channel = std::make_shared<AsyncFuncChannel>();
                }
                n->chan = holder->async_channel;
                n->callback.get_temporary = [](FuncDataAsync* data) -> ValueData* {
                    auto n = (FuncWrapper*)data;
                    return (ValueData*)(void*)&n->temporary_location;
                };
                n->callback.set_boolean = [](ValueData* data, int value) {
                    auto v = (Value*)(void*)data;
                    *v = Value(Boolean{value!=0});
                };
                n->callback.set_number = [](ValueData* data, int64_t value) {
                    auto v = (Value*)(void*)data;
                    *v = Value(value);
                };
                n->callback.set_string = [](ValueData* data,const char* str) {
                    auto v = (Value*)(void*)data;
                    *v = Value(futils::utf::convert<std::u32string>(str));
                };
                n->callback.set_bytes = [](ValueData* data,const char* str,size_t size) {
                    auto v = (Value*)(void*)data;
                    *v = Value(Bytes{std::string(str, size)});
                };
                n->callback.set_foreign_object = [](ValueData* data,void* p_shared_ptr,int tag) {
                    if(tag==0){
                        return;
                    }
                    auto v = (Value*)(void*)data;
                    auto sh = (std::shared_ptr<void>*)p_shared_ptr;
                    *v = Value(Foreign{ForeignType(tag),*sh});
                };
                n->callback.set_array = [](ValueData* data,size_t size) {
                    auto v = (Value*)(void*)data;
                    *v = Value(ValueArray{std::vector<Value>(size)});
                };
                n->callback.set_object = [](ValueData* data) {
                    auto v = (Value*)(void*)data;
                    *v = Value(ValueMap{});
                };
                n->callback.get_array_element = [](ValueData* data,size_t index,int may_append) -> ValueData* {
                    auto v = (Value*)(void*)data;
                    if (auto arr = v->array()) {
                        if (index >= arr->size()) {
                            if (may_append) {
                                arr->resize(index + 1);
                            } else {
                                return nullptr;
                            }
                        }
                        return (ValueData*)(void*)&(*arr)[index];
                    }
                    return nullptr;
                };
                n->callback.get_object_element = [](ValueData* data,const char* key,int may_create) -> ValueData* {
                    auto v = (Value*)(void*)data;
                    if (auto obj = v->object()) {
                        auto k = futils::utf::convert<std::u32string>(key);
                        if (auto found = obj->find(k); found != obj->end()) {
                            return (ValueData*)(void*)&found->second;
                        }
                        if (may_create) {
                            return (ValueData*)(void*)&obj->emplace(k, Value()).first->second;
                        }
                    }
                    return nullptr;
                };
                n->callback.push_arg = [](FuncDataAsync* data, ValueData* value) {
                    auto n = (FuncWrapper*)(void*)data;
                    n->async_callback.args.push_back(*(Value*)(void*)value);
                };
                n->callback.call = [](FuncDataAsync* data) {
                    auto n = (FuncWrapper*)(void*)data;
                    if(auto c=n->chan.lock()){
                        AsyncCallback cb;
                        cb.func = n->async_callback.func;
                        cb.args = std::move(n->async_callback.args);
                        c->send(std::move(cb));
                        return 1;
                    }
                    return 0;
                };
                n->callback.unload = [](FuncDataAsync* data) {
                    delete (FuncWrapper*)(void*)data;
                };
                return (FuncDataAsync*)&n->callback;
            },
            .get_temporary = [](::ForeignContext* data) -> ValueData* {
                auto holder = (InternalHolder*)(void*)data;
                return (ValueData*)(void*)&holder->temporary_location;
            },
            .push_arg = [](::ForeignContext* data, ValueData* value) {
                auto holder = (InternalHolder*)(void*)data;
                holder->callback_arg.push_back(*(Value*)(void*)value);
            },
            .call = [](::ForeignContext* data, FuncData* func) {
                auto holder = (InternalHolder*)(void*)data;
                holder->called_func = *(Func*)(void*)func;
            },
            .get_call_result = [](::ForeignContext* data) -> ValueData* {
                auto holder = (InternalHolder*)(void*)data;
                return (ValueData*)(void*)&holder->callback_result;
            },
            .abort = [](::ForeignContext* data, const char* reason) {
                auto holder = (InternalHolder*)(void*)data;
                holder->abort_reason = futils::utf::convert<std::u32string>(reason);
            },
            .get_callback_phase = [](::ForeignContext* data) -> size_t {
                auto holder = (InternalHolder*)(void*)data;
                return holder->callback_count;
            },
        };
        // clang-format on
        func(&cb);
        if (holder.abort_reason) {
            return state.eval_error(*holder.abort_reason);
        }
        if (holder.called_func) {
            foreign_callback.func = *holder.called_func;
            foreign_callback.args = std::move(holder.callback_arg);
            return expr::EvalState::call;
        }
        return expr::EvalState::normal;
    }

}  // namespace foreign
