/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include "async.h"

namespace futils::thread {

    template <typename T>
    struct QueueNode {
        std::atomic<QueueNode*> next;
        T data;
    };

    template <typename T, class A>
    struct MultiProduceSingleConsumeQueue {
       private:
        std::atomic<QueueNode<T>*> head;
        std::atomic<QueueNode<T>*> tail;
        A allocator;
        using traits = std::allocator_traits<A>;

       public:
        MultiProduceSingleConsumeQueue() {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, QueueNode<T>{nullptr, T{}});
            head.store(node);
            tail.store(node);
        }

        // called by the producer (multi thread)
        void push(T&& data) {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, QueueNode<T>{nullptr, std::move(data)});
            node->next = nullptr;
            auto last = tail.exchange(node);
            last->next = node;
        }

        // called by the consumer (single thread)
        std::optional<T> pop() {
            auto first = head.load();
            auto next = first->next.load();
            if (!next) {
                return std::nullopt;
            }
            head.store(next);                   // next becomes the new head (stub node)
            auto data = std::move(next->data);  // get the data
            traits::destroy(allocator, first);  // destroy the old head
            traits::deallocate(allocator, first, 1);
            return data;
        }
    };

    template <typename T, class A>
    struct SingleProduceMultiConsumeQueue {
       private:
        std::atomic<QueueNode<T>*> head;
        std::atomic<QueueNode<T>*> tail;
        A allocator;
        using traits = std::allocator_traits<A>;

       public:
        SingleProduceMultiConsumeQueue() {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, QueueNode<T>{nullptr, T{}});
            head.store(node);
            tail.store(node);
        }

        // called by the producer (single thread)
        void push(T&& data) {
            auto node = traits::allocate(allocator, 1);
            traits::construct(allocator, node, QueueNode<T>{nullptr, std::move(data)});
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
    };

}  // namespace futils::thread
