// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace aster {

class SignalConnection {
public:
  SignalConnection() = default;
  SignalConnection(const SignalConnection &) = delete;
  SignalConnection &operator=(const SignalConnection &) = delete;

  SignalConnection(SignalConnection &&other) noexcept {
    *this = std::move(other);
  }

  SignalConnection &operator=(SignalConnection &&other) noexcept {
    if (this != &other) {
      disconnect();
      disconnect_ = std::move(other.disconnect_);
      connected_ = std::move(other.connected_);
    }
    return *this;
  }

  ~SignalConnection() = default;

  void disconnect() {
    if (disconnect_) {
      disconnect_();
      disconnect_ = {};
    }
    if (connected_) {
      connected_->store(false);
    }
  }

  [[nodiscard]] bool connected() const {
    return connected_ && connected_->load();
  }

private:
  template <typename... Args> friend class Signal;

  SignalConnection(std::function<void()> disconnect, std::shared_ptr<std::atomic_bool> connected)
      : disconnect_(std::move(disconnect)), connected_(std::move(connected)) {}

  std::function<void()> disconnect_;
  std::shared_ptr<std::atomic_bool> connected_;
};

template <typename... Args> class Signal {
public:
  using Callback = std::function<void(Args...)>;

  Signal() : state_(std::make_shared<State>()) {}

  [[nodiscard]] SignalConnection connect(Callback callback) {
    if (!callback) {
      return {};
    }

    const std::uint64_t id = state_->next_id++;
    auto connected = std::make_shared<std::atomic_bool>(true);
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->slots.push_back({id, std::move(callback), connected});
    }

    std::weak_ptr<State> weak_state = state_;
    return SignalConnection(
        [weak_state, id, connected]() {
          connected->store(false);
          const std::shared_ptr<State> state = weak_state.lock();
          if (!state) {
            return;
          }
          std::lock_guard<std::mutex> lock(state->mutex);
          for (Slot &slot : state->slots) {
            if (slot.id == id) {
              slot.active->store(false);
              break;
            }
          }
        },
        connected);
  }

  void emit(Args... args) const {
    std::vector<Callback> callbacks;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      callbacks.reserve(state_->slots.size());
      for (const Slot &slot : state_->slots) {
        if (slot.active->load()) {
          callbacks.push_back(slot.callback);
        }
      }
    }

    for (const Callback &callback : callbacks) {
      callback(args...);
    }
    compact();
  }

  void operator()(Args... args) const {
    emit(args...);
  }

  [[nodiscard]] std::size_t listenerCount() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return static_cast<std::size_t>(std::count_if(state_->slots.begin(), state_->slots.end(),
                                                  [](const Slot &slot) {
                                                    return slot.active->load();
                                                  }));
  }

  void clear() {
    std::lock_guard<std::mutex> lock(state_->mutex);
    for (Slot &slot : state_->slots) {
      slot.active->store(false);
    }
    state_->slots.clear();
  }

private:
  struct Slot {
    std::uint64_t id = 0u;
    Callback callback;
    std::shared_ptr<std::atomic_bool> active;
  };

  struct State {
    mutable std::mutex mutex;
    std::vector<Slot> slots;
    std::uint64_t next_id = 1u;
  };

  void compact() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->slots.erase(std::remove_if(state_->slots.begin(), state_->slots.end(),
                                       [](const Slot &slot) { return !slot.active->load(); }),
                        state_->slots.end());
  }

  std::shared_ptr<State> state_;
};

} // namespace aster
