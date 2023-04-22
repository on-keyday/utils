
enum class WakeResult {
    continue_,
    succeeded,
    failed,
};

struct Waker2 {
   private:
    std::weak_ptr<Waker2> self;
    using implfn = WakeResult (*)(Waker2* self, WakerState state, void* external_arg);
    implfn impl_call = nullptr;
    std::atomic<WakerState> state = WakerState::reset;

    bool try_get_turn(WakerState expect, WakerState next) {
        for (;;) {
            WakerState e = expect;
            if (!state.compare_exchange_strong(e, next)) {
                if (e == next) {
                    continue;
                }
                return false;
            }
            return true;
        }
    }

    bool wakeup_impl(void* arg, bool cancel) {
        if (!try_get_turn(WakerState::sleep, WakerState::waking)) {
            return false;
        }
        auto pin = get_self();
        assert(pin.get() == this);
        auto w = impl_call(this, cancel ? WakerState::canceling : WakerState::waking, arg);
        if (cancel && w != WakeResult::continue_) {
            state.store(WakerState::sleep);
        }
        auto c = impl_call;
        state.store(WakerState::woken);
        // now maybe unsafe by race condition
        // usage of this is, for example, enque waker to execution buffer
        c(pin.get(), cancel ? WakerState::canceled : WakerState::woken, arg);
        return true;
    }

   protected:
    constexpr Waker2(implfn fn)
        : impl_call(fn) {}

    void set_weak(const std::shared_ptr<Waker2>& w) {
        assert(w.get() == this);
        self = w;
    }

    std::shared_ptr<Waker2> get_self() {
        return self.lock();
    }

    template <class T>
    static void type() {}
    struct Reflect {
        void (*type)();
        const void* ptr = nullptr;
        bool is_const = false;
    };

    struct VisitArgs {
        void* ctx;
        void (*arg)(void* ctx, Reflect&, size_t idx);
    };

   public:
    Waker2(Waker2&&) = delete;

    // is_waiting reports whether state is WakerState::sleep
    bool is_waiting() const {
        return state == WakerState::sleep;
    }

    // is_ready reports whether state is WakerState::woken
    bool is_ready() const {
        return state == WakerState::woken;
    }

    bool is_complete() const {
        return state == WakerState::done;
    }

    // wakeup change state to WakerState::woken
    // previous state MUST be WakerState::sleep
    // call callback with WakerState::waking and WakerState::woken
    // callback with WakerState::woken may be in race condition
    // arg is implementation depended argument
    // this function is block if other thread call wakeup() or cancel() in same state
    bool wakeup(void* arg = nullptr) {
        return wakeup_impl(arg, false);
    }

    // cancel may change state to WakerState::woken
    // previous state MUST be WakerState::sleep
    // call callback with WakerState::canceling and may call it with WakerState::canceled
    // this cancel mechanism is implementation dependent
    // even if cancel() is called, run() can be called
    // if internal callback returns <= 0 at WakerState::canceling, wakup() and cancel() also can be called again
    // if callback returns <= 0 at WakerState::canceling and implementation wants user call cancel() only once,
    // implementation should mark this Waker as canceled over Waker::data
    bool cancel(void* arg = nullptr) {
        return wakeup_impl(arg, true);
    }

    // run change state to WakerState::done
    // previous state MUST be WakerState::woken
    // call callback with WakerState::running
    // arg is implementation depended argument
    bool run(void* arg = nullptr) {
        WakerState s = WakerState::woken;
        if (!state.compare_exchange_strong(s, WakerState::running)) {
            return false;
        }
        auto pin = get_self();
        impl_call(this, WakerState::running, arg);
        state.store(WakerState::done);
        return true;
    }

    // reset change state from WakerState::done to WakerState::sleep
    bool reset(auto&& val) {
        WakerState s = WakerState::done;
        if (!state.compare_exchange_strong(s, WakerState::reset)) {
            return false;
        }
        Reflect refl;
        refl.type = type<std::decay_t<decltype(val)>>;
        refl.is_const = std::is_const_v<std::remove_reference_t<decltype(val)>> ||
                        std::is_lvalue_reference_v<decltype(val)>;
        refl.ptr = std::addressof(val);
        auto res = impl_call(this, WakerState::reset, &refl);
        if (res != WakeResult::succeeded) {
            state.store(WakerState::done);
            return false;
        }
        this->state.store(WakerState::sleep);
        return true;
    }
};

template <class Fn>
struct WakerImpl1 : Waker2 {
   private:
    template <class Fn>
    friend std::shared_ptr<Waker2> make_waker2(Fn&& fn);
    Fn fn;

    static WakeResult waker1_impl(Waker2* w, WakerState state, void* arg) {
        WakerImpl1<Fn>* wi1 = static_cast<WakerImpl1<Fn>*>(w);
        if (state != WakerState::reset) {
            return fn(wi1->get_self(), state, arg);
        }
        auto reflect = static_cast<Waker2::Reflect*>(arg);
        if (reflect->type == type<Fn>) {
            return WakeResult::failed;
        }
        if (reflect->is_const) {
            fn = *static_cast<const Fn*>(reflect.ptr);
        }
        else {
            fn = std::move(*static_cast<Fn*>(reflect.ptr));
        }
        return WakeResult::succeeded;
    }

    void set(const std::shared_ptr<Waker2>& w2) {
        set_weak(w2);
    }

   public:
    template <class F>
    WakerImpl1(F&& f)
        : fn(std::forward<F>(f)), Waker2(waker1_impl) {}
};

template <class Fn, class... Args>
struct WakerImpl2 : Waker2 {
   private:
    template <class Fn>
    friend std::shared_ptr<Waker2> make_waker2(Fn&& fn);
    Fn fn;
    std::tuple<Args...> args;

    template <size_t... idx>
    static WakeResult do_call(auto&& fn,
                              std::shared_ptr<Waker2> w,
                              WakerState state, void* arg, std::index_sequence<idx...>) {
        return std::invoke(fn, w, state, arg, std::get<idx>(arg)...);
    }

    static WakeResult waker1_impl(Waker2* w, WakerState state, void* arg) {
        WakerImpl1<Fn>* wi1 = static_cast<WakerImpl1<Fn>*>(w);
        if (state != WakerState::reset) {
            return do_call(fn, wi1->get_self(), state, arg, std::make_index_sequence<sizeof...(Args)>);
        }
        auto reflect = static_cast<Waker2::Reflect*>(arg);
        if (reflect->type == type<Fn>) {
            return WakeResult::failed;
        }
        if (reflect->is_const) {
            fn = *static_cast<const Fn*>(reflect.ptr);
        }
        else {
            fn = std::move(*static_cast<Fn*>(reflect.ptr));
        }
        return WakeResult::succeeded;
    }

    void set(const std::shared_ptr<Waker2>& w2) {
        set_weak(w2);
    }

   public:
    template <class F>
    WakerImpl1(F&& f)
        : fn(std::forward<F>(f)), Waker2(waker1_impl) {}
};
