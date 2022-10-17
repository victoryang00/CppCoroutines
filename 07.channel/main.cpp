//
// Created by benny on 2022/3/17.
//
#include "Channel.h"
#include "Executor.h"
#include "Scheduler.h"
#include "Task.h"
#include "io_utils.h"

using namespace std::chrono_literals;


Task<void, LooperExecutor> Producer(Channel<int> &channel) {
  int i = 0;
  int j;
  for(j = 0; j < SIZE; j += 1) {
    co_wait((channel + PADDINGWIDTH * j) << 0);
  }
  while(i < ITERATION) {
    for(j = 0; j < SIZE; j += 1) {
      int incr;
      if(((i + j) % 2) == 0)
        incr = 0;
      else
        incr = 1;
      debug("send: ", PADDINGWIDTH * j, i);
      co_await ((channel + PADDINGWIDTH * j) << incr);
      co_await 50ms;
    }
  }

  co_await 5s;
  channel.close();
  debug("close channel, exit.");
}

Task<void, LooperExecutor> Consumer(Channel<int> &channel) {
  while(channel.is_active()) {
    try {
      int       i, j;
      int       current = 0, prev = 0;
      int       index;
      long long diff_count = 0;
      co_await (channel >> index);
      debug("receive: ", index);
      co_await 500ms;
      for(i = 0; i < ITERATION; i++) {
        for(j = index; j < SIZE; j += RTHREADS) {
          if(j != index)
            prev = current;
          co_await ((channel + j * PADDINGWIDTH) >> index);
          if(current != prev)
            diff_count += 1;
        }
      }
      debug("diff_count: ", diff_count);
    } catch(std::exception &e) { debug("exception: ", e.what()); }
  }

  debug("exit.");
}

void test_channel() {
  auto channel  = Channel<int>(PADDINGWIDTH * SIZE);
  auto producer = Producer(channel);
  auto consumer = Consumer(channel);

  std::this_thread::sleep_for(100s);
}

int main() {
  test_channel();
  return 0;
}
