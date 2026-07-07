#pragma once

#include <Arduino.h>

class CounterStore {
public:
  static constexpr size_t kMaxCounters = 16;

  CounterStore();

  bool exists(const String& name) const;
  int32_t get(const String& name) const;
  bool set(const String& name, int32_t value);
  bool increment(const String& name, int32_t amount = 1);
  bool decrement(const String& name, int32_t amount = 1);
  bool reset(const String& name);
  void clear();

  String toJson() const;

private:
  struct Entry {
    String name;
    int32_t value;
    bool used;
  };

  Entry entries_[kMaxCounters];

  int findIndex(const String& name) const;
  int findOrCreate(const String& name);
};
