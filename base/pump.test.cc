/* <base/pump.test.cc>

   Unit test for <base/pump.h>.

   Copyright 2010-2014 OrlyAtomics, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <base/pump.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include <util/io.h>

#include <test/kit.h>

using namespace Base;
using namespace std;
using namespace std::chrono;
using namespace Util;

static const char *Msg = "Mofo the Psychic Gorilla!";

static constexpr size_t MsgSize = 25;

// Disable because the test harness sees the uint8_t* and tries to print it as a c-string
// which means calling strlen() which reads past the end of the buffer...
#if 0
FIXTURE(GrowingPool) {
  TGrowingPool<1024> pool;
  auto b1 = pool.Allocate();
  auto b2 = pool.Allocate();
  auto b3 = pool.Allocate();
  EXPECT_NE(b1, b2);
  EXPECT_NE(b2, b3);
  EXPECT_NE(b1, b3);

  pool.Recycle(b2);
  auto b4 = pool.Allocate();
  EXPECT_EQ(b2, b4);

  pool.Recycle(b3);
  pool.Recycle(b1);
  EXPECT_EQ(pool.Allocate(), b3);
  EXPECT_EQ(pool.Allocate(), b1);
}
#endif

FIXTURE(OnePipeManyCycles) {
  assert(strlen(Msg) == MsgSize);
  const size_t cycle_repeat_count = 300;
  size_t i;
  for (i = 0; i < cycle_repeat_count; ++i) {
    const size_t
        msg_repeat_count = 300,
        expected_size = msg_repeat_count * MsgSize;
    size_t actual_size = 0;
    TPump pump;
    TFd read, write;
    pump.NewPipe(read, write);
    assert(read);
    assert(write);
    thread reader(
        [&actual_size, &read] {
          char buf[MsgSize / 3];
          for (;;) {
            ssize_t size;
            IfLt0(size = ReadAtMost(read, buf, sizeof(buf)));
            if (!size) {
              break;
            }
            actual_size += size;
          }
          read.Reset();
        }
    );
    thread writer(
        [msg_repeat_count, &write] {
          for (size_t i = 0; i < msg_repeat_count; ++i) {
            WriteExactly(write, Msg, MsgSize);
          }
          write.Reset();
        }
    );
    reader.join();
    writer.join();
    if (actual_size != expected_size) {
      break;
    }
  }
  EXPECT_EQ(i, cycle_repeat_count);
}

FIXTURE(OneCycleManyPipes) {
  TPump pump;
  atomic_size_t success_count(0);
  vector<thread> pipes;
  for (size_t i = 0; i < 300; ++i) {
    pipes.push_back(thread(
        [&pump, &success_count, i] {
          const size_t
              msg_repeat_count = 300,
              expected_size = msg_repeat_count * MsgSize;
          size_t actual_size = 0;
          TFd read, write;
          pump.NewPipe(read, write);
          thread reader(
              [&actual_size, &read] {
                char buf[MsgSize / 3];
                for (;;) {
                  ssize_t size;
                  IfLt0(size = ReadAtMost(read, buf, sizeof(buf)));
                  if (!size) {
                    break;
                  }
                  actual_size += size;
                }
                read.Reset();
              }
          );
          thread writer(
              [msg_repeat_count, &write] {
                for (size_t i = 0; i < msg_repeat_count; ++i) {
                  WriteExactly(write, Msg, MsgSize);
                }
                write.Reset();
              }
          );
          reader.join();
          writer.join();
          if (actual_size == expected_size) {
            ++success_count;
          }
        }
    ));
  }
  for (auto &pipe: pipes) {
    pipe.join();
  }
  EXPECT_EQ(success_count, pipes.size());
}

FIXTURE(WaitForIdle) {
  TPump pump;
  TFd read, write;
  pump.NewPipe(read, write);
  thread waiter(
      [&pump] {
        pump.WaitForIdle();
      }
  );
  read.Reset();
  write.Reset();
  waiter.join();
}

FIXTURE(WaitForIdleForSuccess) {
  TPump pump;
  TFd read, write;
  pump.NewPipe(read, write);
  atomic_bool
      is_ready(false),
      is_idle(false);
  thread waiter(
      [&pump, &is_ready, &is_idle] {
        while (!is_ready);
        is_idle = pump.WaitForIdleFor(milliseconds(100));
      }
  );
  read.Reset();
  write.Reset();
  is_ready = true;
  waiter.join();
  EXPECT_TRUE(is_idle);
}

FIXTURE(WaitForIdleForTimeout) {
  TPump pump;
  TFd read, write;
  pump.NewPipe(read, write);
  atomic_bool
      is_ready(false),
      is_idle(true);
  thread waiter(
      [&pump, &is_ready, &is_idle] {
        while (!is_ready);
        is_idle = pump.WaitForIdleFor(milliseconds(100));
      }
  );
  is_ready = true;
  waiter.join();
  EXPECT_FALSE(is_idle);
}

FIXTURE(WaitForIdleUntilSuccess) {
  TPump pump;
  TFd read, write;
  pump.NewPipe(read, write);
  atomic_bool
      is_ready(false),
      is_idle(false);
  thread waiter(
      [&pump, &is_ready, &is_idle] {
        while (!is_ready);
        is_idle = pump.WaitForIdleUntil(system_clock::now() + milliseconds(100));
      }
  );
  read.Reset();
  write.Reset();
  is_ready = true;
  waiter.join();
  EXPECT_TRUE(is_idle);
}

FIXTURE(WaitUntilIdleForTimeout) {
  TPump pump;
  TFd read, write;
  pump.NewPipe(read, write);
  atomic_bool
      is_ready(false),
      is_idle(true);
  thread waiter(
      [&pump, &is_ready, &is_idle] {
        while (!is_ready);
        is_idle = pump.WaitForIdleUntil(system_clock::now() + milliseconds(100));
      }
  );
  is_ready = true;
  waiter.join();
  EXPECT_FALSE(is_idle);
}
