#pragma once

struct Message {
  enum Type {
    kInterruptXHCI,
    kInterruptLAPICTimer,
    kTimerTimeout,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;
  } arg;
};