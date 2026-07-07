#include "counter_store.h"

CounterStore::CounterStore() {
  clear();
}

bool CounterStore::exists(const String& name) const {
  return findIndex(name) >= 0;
}

int32_t CounterStore::get(const String& name) const {
  const int index = findIndex(name);
  if (index < 0) {
    return 0;
  }
  return entries_[index].value;
}

bool CounterStore::set(const String& name, int32_t value) {
  const int index = findOrCreate(name);
  if (index < 0) {
    return false;
  }

  entries_[index].value = value;
  return true;
}

bool CounterStore::increment(const String& name, int32_t amount) {
  const int index = findOrCreate(name);
  if (index < 0) {
    return false;
  }

  entries_[index].value += amount;
  return true;
}

bool CounterStore::decrement(const String& name, int32_t amount) {
  return increment(name, -amount);
}

bool CounterStore::reset(const String& name) {
  return set(name, 0);
}

void CounterStore::clear() {
  for (size_t i = 0; i < kMaxCounters; ++i) {
    entries_[i].name = "";
    entries_[i].value = 0;
    entries_[i].used = false;
  }
}

String CounterStore::toJson() const {
  String json = "[";
  bool first = true;

  for (size_t i = 0; i < kMaxCounters; ++i) {
    if (!entries_[i].used) {
      continue;
    }

    if (!first) {
      json += ",";
    }
    first = false;

    String safeName = entries_[i].name;
    safeName.replace("\\", "\\\\");
    safeName.replace("\"", "\\\"");

    json += "{\"name\":\"" + safeName + "\",\"value\":" + String(entries_[i].value) + "}";
  }

  json += "]";
  return json;
}

int CounterStore::findIndex(const String& name) const {
  for (size_t i = 0; i < kMaxCounters; ++i) {
    if (entries_[i].used && entries_[i].name.equalsIgnoreCase(name)) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

int CounterStore::findOrCreate(const String& name) {
  const int found = findIndex(name);
  if (found >= 0) {
    return found;
  }

  for (size_t i = 0; i < kMaxCounters; ++i) {
    if (!entries_[i].used) {
      entries_[i].used = true;
      entries_[i].name = name;
      entries_[i].value = 0;
      return static_cast<int>(i);
    }
  }

  return -1;
}
