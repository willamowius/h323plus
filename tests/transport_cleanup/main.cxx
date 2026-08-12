/*
 * Regression test for transport thread cleanup.
 *
 * Copyright (c) 2026 XMeeting contributors
 * Licensed under the Mozilla Public License Version 1.0.
 */

#include <ptlib.h>

#include <h323ep.h>
#include <transports.h>

#include <atomic>
#include <iostream>

namespace {

struct ThreadState {
  std::atomic<bool> started;
  std::atomic<bool> destroyed;
  std::atomic<bool> destroyedWhileRunning;

  ThreadState()
    : started(false),
      destroyed(false),
      destroyedWhileRunning(false)
  {
  }
};

class DelayedTransportThread : public PThread
{
  PCLASSINFO(DelayedTransportThread, PThread);

 public:
  explicit DelayedTransportThread(ThreadState & state)
    : PThread(65536, NoAutoDeleteThread, NormalPriority, "Cleanup regression"),
      state(state)
  {
  }

  ~DelayedTransportThread()
  {
    if (!IsTerminated())
      state.destroyedWhileRunning.store(true);
    state.destroyed.store(true);
  }

  void Main()
  {
    // Keep only the external state pointer in local storage. The old cleanup
    // code deletes this PThread after ten seconds while Main() is still
    // executing on POSIX. Avoiding further member access lets the destructor
    // report that ordering error instead of relying only on a crash.
    ThreadState * const result = &state;
    result->started.store(true);

    const PTimeInterval runFor(12500);
    const PTimeInterval startedAt = PTimer::Tick();
    while (PTimer::Tick() - startedAt < runFor) {
      // Intentionally contain no cancellation point. CleanUpOnTermination()
      // must not delete the PThread object until this Main() has returned.
    }
  }

 private:
  ThreadState & state;
};

class TransportCleanupTestProcess : public PProcess
{
  PCLASSINFO(TransportCleanupTestProcess, PProcess);

 public:
  TransportCleanupTestProcess()
    : PProcess("H323Plus", "transport-cleanup-test", 1, 0, ReleaseCode, 0)
  {
  }

  void Main()
  {
    H323EndPoint endpoint;
    H323TransportTCP transport(endpoint);
    ThreadState state;

    DelayedTransportThread * const worker = new DelayedTransportThread(state);
    transport.AttachThread(worker);
    worker->Resume();

    const PTimeInterval startupDeadline = PTimer::Tick() + PTimeInterval(5000);
    while (!state.started.load() && PTimer::Tick() < startupDeadline)
      PThread::Sleep(10);

    if (!state.started.load()) {
      std::cerr << "FAIL: worker thread did not start" << std::endl;
      SetTerminationValue(1);
      return;
    }

    transport.CleanUpOnTermination();

    if (state.destroyedWhileRunning.load()) {
      std::cerr << "FAIL: transport deleted a thread whose Main() was running"
                << std::endl;
      SetTerminationValue(1);
      return;
    }
    if (!state.destroyed.load()) {
      std::cerr << "FAIL: transport did not delete its terminated thread"
                << std::endl;
      SetTerminationValue(1);
      return;
    }

    std::cout << "PASS: transport waited for thread termination before deletion"
              << std::endl;
  }
};

} // namespace

PCREATE_PROCESS(TransportCleanupTestProcess);
