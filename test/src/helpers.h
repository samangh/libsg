#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <iostream>

class scoped_deadline {
public:
    explicit scoped_deadline(std::string msg,
                             std::chrono::seconds limit = std::chrono::seconds(30))
    {
      m_thread = std::thread([this, msg, limit]() {
          const auto deadline = std::chrono::steady_clock::now() + limit;
          while (!m_done.load(std::memory_order::acquire)) {
              if (std::chrono::steady_clock::now() >= deadline) {
                  std::cerr << msg << std::endl;
                  std::abort();
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
      });
    }

    ~scoped_deadline() {
        m_done.store(true, std::memory_order::release);
        m_thread.join();
    }

    scoped_deadline(const scoped_deadline&)            = delete;
    scoped_deadline& operator=(const scoped_deadline&) = delete;

private:
    std::atomic<bool> m_done{false};
    std::thread m_thread;
};
