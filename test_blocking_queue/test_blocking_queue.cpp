#include <string>
#include <chrono>
#include <iostream>
#include <thread>

#include "time_utils.h"
#include "blocking_queue.h"

using namespace std::chrono_literals;

typedef std::chrono::time_point<std::chrono::system_clock> time_point;

struct Data {
  friend std::ostream& operator<<(std::ostream&, const Data&);

  std::string field;
  int value;
  time_point timestamp;
};

inline std::ostream& operator<<(std::ostream& ostream, const Data& data)
{
  return ostream << "[" << data.field << ": " << data.value
                 << " created at " << to_string(data.timestamp) << "]";
}

class Producer {
  std::thread thread;
  bool done{false};
  bool started{false};
  int count{0};
  BlockingTimeoutQueue<Data>& queue;

public:
  explicit Producer(BlockingTimeoutQueue<Data>& queue) : queue(queue) {}

  void run() {
    while (!done) {
      std::this_thread::sleep_for(1000ms);
      const auto now = std::chrono::system_clock::now();
      queue.push(Data{"field", ++count, now});
    }
  }

  void start() {
    std::cout << "Producer started..." << std::endl;
    started = true;
    thread = std::thread(&Producer::run, this);
  }

  void cancel() {
    if (!started)
      return;
    done = true;
    started = false;
    if (thread.joinable())
      thread.join();
    std::cout << "Producer stopped" << std::endl;
  }
};

class Consumer {
  std::thread thread;
  bool done{false};
  bool started{false};
  BlockingTimeoutQueue<Data>& queue;

public:
  explicit Consumer(BlockingTimeoutQueue<Data>& queue) : queue(queue) {}

  void run() const {
    while (!done) {
      Data data;
      const auto start = std::chrono::system_clock::now();
      if (const auto success = queue.pop(data, 5000ms)) {
        const auto now = std::chrono::system_clock::now();
        const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - data.timestamp).count();
        const auto waiting_time = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        std::cout << "Consumer popped " << data
                  << " @ " << to_string(now)
                  << " with latency " << latency << "us"
                  << " with waiting time " << (waiting_time / 1000.0) << "ms" << std::endl;
      } else {
        std::cout << "***** Consumer timeout!" << std::endl;
      }
    }
  }

  void start() {
    std::cout << "Consumer started..." << std::endl;
    started = true;
    thread = std::thread(&Consumer::run, this);
  }

  void cancel() {
    if (!started)
      return;
    done = true;
    started = false;
    if (thread.joinable())
      thread.join();
    std::cout << "Consumer stopped" << std::endl;
  }
};

int main([[maybe_unused]]int argc, [[maybe_unused]]char **argv) {

  BlockingTimeoutQueue<Data> queue(false);
  Producer producer(queue);
  Consumer consumer(queue);
  std::cout << "Waiting in main thread..." << std::endl;

  consumer.start();
  producer.start();

  std::this_thread::sleep_for(15s);
  std::cout << "stopping producer and consumer..." << std::endl;
  producer.cancel();
  consumer.cancel();
  std::cout << "done" << std::endl;
}
