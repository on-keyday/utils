/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <atomic>
#include <memory>
#include <optional>

namespace futils::thread {

    template <typename T>
    struct QueueNode {
        T data;
        std::atomic<QueueNode*> next;
        std::atomic<std::uint8_t> ref_count = 1;

        constexpr QueueNode() = default;
        template <class Arg>
        constexpr QueueNode(Arg&& arg)
            : data(std::forward<Arg>(arg)) {}

        void inc_ref() {
            ref_count.fetch_add(1, std::memory_order_relaxed);
        }

        template <class A>
        void dec_ref(A& a) {
            using traits = std::allocator_traits<A>;
            if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                traits::destroy(a, this);
                traits::deallocate(a, this, 1);
            }
        }
    };

    // thread safety
    // A - push thread
    // B - pop thread
    // scenario 1:
    // last == first
    // A: last = tail.exchange(node)
    // B: first = head.load()
    // A: last->inc_ref() // ref_count = 2
    // A: last->next = node
    // B: next = first->next.load() // acquired
    // B: head.store(next) // next becomes the new head
    // B: first->dec_ref() // ref_count = 1
    // A: last->notify_one() // notify B but nothing happens
    // A: last->dec_ref() // ref_count = 0, destroy the old first
    // scenario 2:
    // last == first
    // A: last = tail.exchange(node)
    // B: first = head.load()
    // B: next = first->next.load() // not acquired
    // B: first->next.wait() // wait for the next node
    // A: last->inc_ref() // ref_count = 2
    // A: last->next = node
    // A: last->notify_one() // notify B
    // B: next = first->next.load() // acquired
    // A: last->dec_ref() // ref_count = 1
    // B: head.store(next) // next becomes the new head
    // B: first->dec_ref() // ref_count = 0, destroy the old first
    template <typename T, class A>
    struct MultiProduceSingleConsumeQueue {
       private:
        std::atomic<QueueNode<T>*> head;
        std::atomic<QueueNode<T>*> tail;
        using rebound_alloc = typename std::allocator_traits<A>::template rebind_alloc<QueueNode<T>>;
        rebound_alloc allocator;
        using traits = typename std::allocator_traits<A>::template rebind_traits<QueueNode<T>>;

       public:
        MultiProduceSingleConsumeQueue() {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node);
            head.store(node);
            tail.store(node);
        }

        ~MultiProduceSingleConsumeQueue() {
            auto node = head.load();
            while (node) {
                auto next = node->next.load();
                traits::destroy(allocator, node);
                traits::deallocate(allocator, node, 1);
                node = next;
            }
        }

        // called by the producer (multi thread)
        void push(auto&& data) {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, std::forward<decltype(data)>(data));
            node->next = nullptr;
            auto last = tail.exchange(node);
            last->inc_ref();  // temporary for notify_one
            last->next = node;
            last->next.notify_one();
            last->dec_ref(allocator);  // release temporary reference
        }

        std::optional<T> pop_wait() {
            auto first = head.load();
            auto next = first->next.load();
            if (!next) {
                first->next.wait(nullptr);  // wait for the next node
                next = first->next.load();  // maybe spurious wakeup, but not loop
                if (!next) {
                    return std::nullopt;  // no data
                }
            }
            head.store(next);                   // next becomes the new head (stub node)
            auto data = std::move(next->data);  // get the data
            first->dec_ref(allocator);          // destroy the old head
            return data;
        }

        // called by the consumer (single thread)
        std::optional<T> pop() {
            auto first = head.load();
            auto next = first->next.load();
            if (!next) {
                return std::nullopt;  // no data
            }
            head.store(next);                   // next becomes the new head (stub node)
            auto data = std::move(next->data);  // get the data
            first->dec_ref(allocator);          // destroy the old head
            return data;
        }

        std::optional<T> peek() {
            auto first = head.load();
            auto next = first->next.load();
            if (!next) {
                return std::nullopt;  // no data
            }
            return next->data;
        }
    };

    template <typename T, class A>
    struct MultiProduceMultiConsumeQueue {
       private:
        std::atomic<QueueNode<T>*> head;
        std::atomic<QueueNode<T>*> tail;
        using rebound_alloc = typename std::allocator_traits<A>::template rebind_alloc<QueueNode<T>>;
        rebound_alloc allocator;
        using traits = typename std::allocator_traits<A>::template rebind_traits<QueueNode<T>>;

       public:
        MultiProduceMultiConsumeQueue() {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node);
            head.store(node);
            tail.store(node);
        }

        ~MultiProduceMultiConsumeQueue() {
            auto node = head.load();
            while (node) {
                auto next = node->next.load();
                traits::destroy(allocator, node);
                traits::deallocate(allocator, node, 1);
                node = next;
            }
        }

        // called by the producer (multi thread)
        void push(T&& data) {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, std::move(data));
            node->next = nullptr;
            auto last = tail.exchange(node);
            last->next = node;
        }

        // called by the consumer (multi thread)
        std::optional<T> pop() {
            // get the head (and clear the head)
            // from here, the queue is used by only one consumer
            auto acquire = head.exchange(nullptr);
            if (!acquire) {  // acquired by another consumer
                return std::nullopt;
            }
            auto next = acquire->next.load();
            if (!next) {
                head.store(acquire);  // no data, restore the head
                return std::nullopt;
            }
            auto data = std::move(next->data);
            // next becomes the new head (stub node)
            head.store(next);
            traits::destroy(allocator, acquire);  // destroy the old head
            traits::deallocate(allocator, acquire, 1);
            return data;
        }
    };

    template <typename T, class A>
    struct MultiProducerChannelBuffer {
       private:
        MultiProduceSingleConsumeQueue<T, A> queue;

       public:
        void send(T&& data) {
            queue.push(std::move(data));
        }

        std::optional<T> receive() {
            return queue.pop();
        }

        std::optional<T> receive_wait() {
            return queue.pop_wait();
        }
    };

}  // namespace futils::thread
