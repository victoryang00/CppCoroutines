//
// Created by benny on 2022/3/21.
//

#ifndef CPPCOROUTINES_TASKS_07_CHANNEL_CHANNEL_H_
#define CPPCOROUTINES_TASKS_07_CHANNEL_CHANNEL_H_

#include "ChannelAwaiter.h"
#include "coroutine_common.h"
#include <exception>
#include <unordered_map>

template <typename ValueType> struct Channel {

  struct ChannelClosedException : std::exception {
    const char *what() const noexcept override { return "Channel is closed."; }
  };

  void check_closed() {
    if (!_is_active.load(std::memory_order_relaxed)) {
      throw ChannelClosedException();
    }
  }

  void try_push_reader(ReaderAwaiter<ValueType> *reader_awaiter) {
    std::unique_lock lock(channel_lock);
    auto index = reader_awaiter->index;
    check_closed();

    if (!buffer.empty()) {
      auto value = buffer[index];
      buffer.erase(std::next(buffer.begin(), index));

      if (!writer_list.empty()) {
        auto writer =
            std::find(writer_list.begin(), writer_list.end(),
                      [&](auto &writer) { return writer->_index == index; });
        writer_list.erase(writer);
        buffer[index] = (*writer)->_value;
        lock.unlock();

        (*writer)->resume();
      } else {
        lock.unlock();
      }

      reader_awaiter->resume(value, index);
      return;
    }

    if (!writer_list.empty()) {
      auto writer =
          std::find(writer_list.begin(), writer_list.end(),
                    [&](auto &writer) { return writer->_index == index; });
      writer_list.erase(writer);
      lock.unlock();

      reader_awaiter->resume((*writer)->_value,index);
      (*writer)->resume();
      return;
    }

    reader_list.push_back(reader_awaiter);
  }

  void try_push_writer(WriterAwaiter<ValueType> *writer_awaiter) {
    std::unique_lock lock(channel_lock);
    auto index = writer_awaiter->index;
    check_closed();
    // suspended readers
    if (!reader_list.empty()) {
      auto reader =
          std::find(reader_list.begin(), reader_list.end(),
                    [&index](ReaderAwaiter<ValueType> *reader_awaiter) {
                      return reader_awaiter->index == index;
                    });
      reader_list.erase(reader);
      lock.unlock();

      (*reader)->resume(writer_awaiter->_value, index);
      writer_awaiter->resume();
      return;
    }

    // write to buffer
    if (buffer.size() < buffer_capacity) {
      buffer[index] = writer_awaiter->_value;
      lock.unlock();
      writer_awaiter->resume();
      return;
    }

    // suspend writer
    writer_list[index] = writer_awaiter;
  }

  void remove_writer(WriterAwaiter<ValueType> *writer_awaiter) {
    std::lock_guard lock(channel_lock);
    auto it = std::find(writer_list.begin(), writer_list.end(),
                               writer_awaiter);
    auto size = it - writer_list.begin();
    writer_list.erase(it);
    debug("remove writer ", size);
  }

  void remove_reader(ReaderAwaiter<ValueType> *reader_awaiter) {
    std::lock_guard lock(channel_lock);
    auto it = std::find(reader_list.begin(), reader_list.end(),
                               reader_awaiter);
    auto size = it - reader_list.begin();
    reader_list.erase(it);
    debug("remove reader ", size);
  }

  auto write(ValueType value, int index) {
    check_closed();
    return WriterAwaiter<ValueType>(this, value, index);
  }

  auto operator<<(ValueType value) { return write(value, 0); }

  auto operator<<(std::tuple<ValueType, int> t) {
    auto value = std::get<0>(t);
    auto index = std::get<1>(t);
    return write(value, index);
  }

  auto read(int index) {
    check_closed();
    return ReaderAwaiter<ValueType>(this, index);
  }

  auto operator>>(ValueType &value_ref) {
    auto awaiter = read(0);
    awaiter.p_value = &value_ref;
    return awaiter;
  }

  auto operator>>(std::tuple<ValueType, int> t) {
    auto value_ref = std::get<0>(t);
    auto index = std::get<1>(t);
    auto awaiter = read(index);
    awaiter.p_value = &value_ref;
    return awaiter;
  }

  void close() {
    bool expect = true;
    if (_is_active.compare_exchange_strong(expect, false,
                                           std::memory_order_relaxed)) {
      clean_up();
    }
  }

  explicit Channel(int capacity = 0) : buffer_capacity(capacity) {
    _is_active.store(true, std::memory_order_relaxed);
  }

  bool is_active() { return _is_active.load(std::memory_order_relaxed); }

  Channel(Channel &&channel) = delete;

  Channel(Channel &) = delete;

  Channel &operator=(Channel &) = delete;

  ~Channel() { close(); }

private:
  int buffer_capacity;
  std::vector<ValueType> buffer;
  std::vector<WriterAwaiter<ValueType> *> writer_list;
  std::vector<ReaderAwaiter<ValueType> *> reader_list;

  std::atomic<bool> _is_active;

  std::mutex channel_lock;
  std::condition_variable channel_condition;

  void clean_up() {
    std::lock_guard lock(channel_lock);

    for (auto writer : writer_list) {
      writer->resume();
    }
    writer_list.clear();

    for (auto reader : reader_list) {
      reader->resume();
    }
    reader_list.clear();

    decltype(buffer) empty_buffer;
    std::swap(buffer, empty_buffer);
  }
};

#endif // CPPCOROUTINES_TASKS_07_CHANNEL_CHANNEL_H_
