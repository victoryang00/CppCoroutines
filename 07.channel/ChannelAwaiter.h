//
// Created by benny on 2022/3/21.
//

#ifndef CPPCOROUTINES_TASKS_07_CHANNEL_CHANNELAWAITER_H_
#define CPPCOROUTINES_TASKS_07_CHANNEL_CHANNELAWAITER_H_

#include "Executor.h"
#include "coroutine_common.h"
#include <utility>

template <typename ValueType> struct Channel;

template <typename ValueType> struct WriterAwaiter {
  Channel<ValueType> *channel;
  AbstractExecutor *executor = nullptr;
  ValueType _value;
  std::coroutine_handle<> handle;
  int index;

  WriterAwaiter(Channel<ValueType> *channel, ValueType value, int index)
      : channel(channel), _value(value), index(index) {}

  WriterAwaiter(WriterAwaiter &&other) noexcept
      : channel(std::exchange(other.channel, nullptr)),
        executor(std::exchange(other.executor, nullptr)), _value(other._value),
        handle(other.handle) {}

  bool await_ready() { return false; }

  auto await_suspend(std::coroutine_handle<> coroutine_handle) {
    this->handle = coroutine_handle;
    channel->try_push_writer(this);
  }

  void await_resume() {
    channel->check_closed();
    channel = nullptr;
  }

  void resume() {
    if (executor) {
      executor->execute([this]() { handle.resume(); });
    } else {
      handle.resume();
    }
  }

  ~WriterAwaiter() {
    if (channel)
      channel->remove_writer(this);
  }
};

template <typename ValueType> struct ReaderAwaiter {
  Channel<ValueType> *channel;
  AbstractExecutor *executor = nullptr;
  ValueType _value;
  ValueType *p_value = nullptr;
  std::coroutine_handle<> handle;
  int index;

  explicit ReaderAwaiter(Channel<ValueType> *channel, int index)
      : channel(channel), index(index) {}

  ReaderAwaiter(ReaderAwaiter &&other) noexcept
      : channel(std::exchange(other.channel, nullptr)),
        executor(std::exchange(other.executor, nullptr)), _value(other._value),
        p_value(std::exchange(other.p_value, nullptr)), handle(other.handle) {}

  bool await_ready() { return false; }

  auto await_suspend(std::coroutine_handle<> coroutine_handle) {
    this->handle = coroutine_handle;
    channel->try_push_reader(this);
  }

  int await_resume() {
    auto channel = this->channel;
    this->channel = nullptr;
    channel->check_closed();
    return _value;
  }

  void resume(ValueType value, int index) {
    this->_value = value;
    this->index = index;
    if (p_value) {
      *p_value = value;
    }
    resume();
  }

  void resume() {
    if (executor) {
      executor->execute([this]() { handle.resume(); });
    } else {
      handle.resume();
    }
  }

  ~ReaderAwaiter() {
    if (channel)
      channel->remove_reader(this);
  }
};

#endif // CPPCOROUTINES_TASKS_07_CHANNEL_CHANNELAWAITER_H_
