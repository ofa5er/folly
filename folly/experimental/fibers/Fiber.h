/*
 * Copyright 2015 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <functional>

#include <boost/context/all.hpp>
#include <boost/version.hpp>
#include <folly/AtomicLinkedList.h>
#include <folly/IntrusiveList.h>
#include <folly/experimental/fibers/BoostContextCompatibility.h>

namespace folly { namespace fibers {

class Baton;
class FiberManager;

/**
 * @class Fiber
 * @brief Fiber object used by FiberManager to execute tasks.
 *
 * Each Fiber object can be executing at most one task at a time. In active
 * phase it is running the task function and keeps its context.
 * Fiber is also used to pass data to blocked task and thus unblock it.
 * Each Fiber may be associated with a single FiberManager.
 */
class Fiber {
 public:
  /**
   * Sets data for the blocked task
   *
   * @param data this data will be returned by await() when task is resumed.
   */
  void setData(intptr_t data);

  Fiber(const Fiber&) = delete;
  Fiber& operator=(const Fiber&) = delete;

  ~Fiber();
 private:
  enum State {
    INVALID,                    /**< Does't have task function */
    NOT_STARTED,                /**< Has task function, not started */
    READY_TO_RUN,               /**< Was started, blocked, then unblocked */
    RUNNING,                    /**< Is running right now */
    AWAITING,                   /**< Is currently blocked */
    AWAITING_IMMEDIATE,         /**< Was preempted to run an immediate function,
                                     and will be resumed right away */
  };

  State state_{INVALID};        /**< current Fiber state */

  friend class Baton;
  friend class FiberManager;

  explicit Fiber(FiberManager& fiberManager);

  template <typename F>
  void setFunction(F&& func);

  template <typename F, typename G>
  void setFunctionFinally(F&& func, G&& finally);

  template <typename G>
  void setReadyFunction(G&& func);

  static void fiberFuncHelper(intptr_t fiber);
  void fiberFunc();

  /**
   * Switch out of fiber context into the main context,
   * performing necessary housekeeping for the new state.
   *
   * @param state New state, must not be RUNNING.
   *
   * @return The value passed back from the main context.
   */
  intptr_t preempt(State state);

  /**
   * Examines how much of the stack we used at this moment and
   * registers with the FiberManager (for monitoring).
   */
  void recordStackPosition();

  FiberManager& fiberManager_;  /**< Associated FiberManager */
  FContext fcontext_;           /**< current task execution context */
  intptr_t data_;               /**< Used to keep some data with the Fiber */
  std::function<void()> func_;  /**< task function */
  std::function<void()> readyFunc_; /**< function to be executed before jumping
                                         to this fiber */

  /**
   * Points to next fiber in remote ready list
   */
  folly::AtomicLinkedListHook<Fiber> nextRemoteReady_;

  static constexpr size_t kUserBufferSize = 256;
  std::aligned_storage<kUserBufferSize>::type userBuffer_;

  void* getUserBuffer();

  std::function<void()> resultFunc_;
  std::function<void()> finallyFunc_;

  folly::IntrusiveListHook listHook_; /**< list hook for different FiberManager
                                           queues */
  pid_t threadId_{0};
};

}}

#include <folly/experimental/fibers/Fiber-inl.h>
