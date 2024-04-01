#ifndef BLOCKING_CONCURRENTQUEUE
#define BLOCKING_CONCURRENTQUEUE

#include "pch.h"

#include <mutex>
#include <thread>
#include <condition_variable>

#include "time_utils.h"

namespace common {

    template <typename T>
    class SpScQueue
    {
    public:

        SpScQueue() = default;

        SpScQueue(const SpScQueue&) = delete;

        SpScQueue& operator=(const SpScQueue&) = delete;

        void push(const T& item)
        {
            std::unique_lock<std::mutex> ul(mutex);
            queue.push(item);
        }

        bool pop(T& item)
        {
            std::unique_lock<std::mutex> ul(mutex);
            if (queue.empty()) {
                return false;
            }
            item = std::move(queue.front());
            queue.pop();
            return true;
        }

        bool pop_until_last(T& item)
        {
            std::unique_lock<std::mutex> ul(mutex);
            if (queue.empty()) {
                return false;
            }
            item = std::move(queue.back());
            queue.clear();
            return true;
        }

    private:
        std::queue<T> queue;
        std::mutex mutex;
    };


    /*
        A blocking queue from

            https://juanchopanzacpp.wordpress.com/2013/02/26/concurrent-queue-c11/

        Copyright (c) 2013 Juan Palacios juan.palacios.puyana@gmail.com
        Subject to the BSD 2-Clause License
           - see < http://opensource.org/licenses/BSD-2-Clause>
    */
    template <typename T>
    class BlockingQueue
    {
    public:

        BlockingQueue() = default;

        BlockingQueue(const BlockingQueue&) = delete;

        BlockingQueue& operator=(const BlockingQueue&) = delete;

        void push(const T& item)
        {
            std::unique_lock<std::mutex> mlock(mutex);
            queue.push(std::move(item));
            mlock.unlock();
            cond.notify_one();
        }

        T pop()
        {
            std::unique_lock<std::mutex> mlock(mutex);
            while (queue.empty()) {
                cond.wait(mlock);
            }
            auto val = std::move(queue.front());
            queue.pop();
            return val;
        }

        void pop(T& item)
        {
            std::unique_lock<std::mutex> mlock(mutex);
            while (queue.empty()) {
                cond.wait(mlock);
            }
            item = std::move(queue.front());
            queue.pop();
        }

    private:
        std::queue<T> queue;
        std::mutex mutex;
        std::condition_variable cond;
    };

    /*
        Inspired from this

            https://codereview.stackexchange.com/questions/39199/multi-producer-consumer-queue-without-boost-in-c11

    */
    template<typename T>
    class BlockingTimeoutQueue {
    public:
        BlockingTimeoutQueue() = default;

        BlockingTimeoutQueue(const BlockingTimeoutQueue&) = delete;

        BlockingTimeoutQueue& operator=(const BlockingTimeoutQueue&) = delete;

        ~BlockingTimeoutQueue() = default;

        void push(const T& item)
        {
            std::unique_lock<std::mutex> ul(mutex);
            queue.push(item);
            pushed_cond.notify_all();
        }

        template<class R, class P>
        bool pop(T& item, const std::chrono::duration<R, P>& timeout)
        {
            std::unique_lock<std::mutex> ul(mutex);
            if (queue.empty()) {
                if (!pushed_cond.wait_for(ul, timeout, [this]() {return !this->queue.empty(); })) {
                    return false;
                }
            }
            item = std::move(queue.front());
            queue.pop();
            return true;
        }

        bool pop(T& item)
        {
           std::unique_lock<std::mutex> ul(mutex);
           if (queue.empty()) {
              return false;
           }
           item = std::move(queue.front());
           queue.pop();
           return true;
        }

        template<class Op>
        int pop_all(Op op) {
           std::unique_lock<std::mutex> ul(mutex);
           auto n = 0;
           while (!queue.empty()) {
              op(queue.front());
              queue.pop();
              ++n;
           }
           return n;
        }

    private:
        std::queue<T> queue;
        std::mutex mutex;
        std::condition_variable pushed_cond;
    };


    template<typename T>
    class BlockingTimeoutBoundedQueue {
    public:
        explicit BlockingTimeoutBoundedQueue(std::size_t max_size)
            : max_size(max_size)
            , queue()
        {}

        BlockingTimeoutBoundedQueue(const BlockingTimeoutBoundedQueue&) = delete;

        BlockingTimeoutBoundedQueue& operator=(const BlockingTimeoutBoundedQueue&) = delete;

        ~BlockingTimeoutBoundedQueue() = default;

        template <class R, class P>
        bool push(const T& item, const std::chrono::duration<R, P>& timeout)
        {
            std::unique_lock<std::mutex> ul(mutex);
            if (queue.size() >= max_size) {
                if (!popped_cond.wait_for(ul, timeout, [this]() {return this->queue.size() < this->max_size; }))
                    return false;
            }
            queue.push(std::move(item));
            pushed_cond.notify_all();
            return true;
        }

        template <class R, class P>
        bool pop(T& item, const std::chrono::duration<R, P>& timeout)
        {
            std::unique_lock<std::mutex> ul(mutex);
            if (queue.empty()) {
                if (!pushed_cond.wait_for(ul, timeout, [this]() {return !this->queue.empty(); }))
                    return false;
            }
            item = std::move(queue.front());
            queue.pop();
            if (queue.size() >= max_size - 1)
                popped_cond.notify_all();
            return true;
        }

    private:
        std::size_t max_size;
        std::queue<T> queue;
        std::mutex mutex;
        std::condition_variable pushed_cond;
        std::condition_variable popped_cond;
    };
}

#endif