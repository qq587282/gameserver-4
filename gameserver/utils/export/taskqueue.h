/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Simon Sandström
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UTILS_TASKQUEUE_H_
#define UTILS_TASKQUEUE_H_

#include <functional>

class TaskQueue
{
 public:
  using Task = std::function<void(void)>;

  TaskQueue() = default;
  virtual ~TaskQueue() = default;

  // Delete copy constructors
  TaskQueue(const TaskQueue&) = delete;
  TaskQueue& operator=(const TaskQueue&) = delete;

  // Add a Task that should expire as soon as possible
  virtual void addTask(const Task& task, int tag) = 0;

  // Add a Task that should expire in expire_ms milliseconds (or later)
  virtual void addTask(const Task& task, int tag, unsigned expire_ms) = 0;

  // Cancel all Tasks with the given tag
  virtual void cancelAllTasks(int tag) = 0;
};

#endif  // UTILS_TASKQUEUE_H_