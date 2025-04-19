#pragma once

#include <string>
#include <unordered_map>

enum class PrefValueType { INT, FLOAT, BOOL, STRING };

struct PrefEntry {
    PrefValueType type;
    std::string value;
};

class Prefs {
  public:
    Prefs() = delete;
    Prefs(const char *path);
    ~Prefs();

    bool contains(const char *key);
    bool remove(const char *key);
    bool clear();

    // Save all prefs to file (for changes where save wasn't true when calling
    // set_* methods)
    void save();

    void set_int(const char *key, int value, bool save = true);
    void set_float(const char *key, float value, bool save = true);
    void set_bool(const char *key, bool value, bool save = true);
    void set_string(const char *key, const std::string &value, bool save = true);

    int get_int(const char *key);
    float get_float(const char *key);
    bool get_bool(const char *key);
    std::string get_string(const char *key);

    int get_int_or(const char *key, int fallback);
    float get_float_or(const char *key, float fallback);
    bool get_bool_or(const char *key, bool fallback);
    std::string get_string_or(const char *key, const std::string &fallback);

  private:
    std::unordered_map<const char *, PrefEntry> internal_storage;
};
