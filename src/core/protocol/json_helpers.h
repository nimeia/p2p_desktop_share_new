#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace lan::protocol {

// Safe JSON helpers. These are intentionally tiny to keep message handling readable.

inline std::optional<std::string> GetString(const boost::json::object& obj, std::string_view key) {
  auto it = obj.find(key);
  if (it == obj.end() || !it->value().is_string()) return std::nullopt;
  return std::string(it->value().as_string().c_str());
}

inline std::string GetStringOr(const boost::json::object& obj, std::string_view key, std::string fallback = {}) {
  if (auto v = GetString(obj, key)) return *v;
  return fallback;
}

inline bool StringEquals(const boost::json::value& v, std::string_view s) {
  return v.is_string() && v.as_string() == s;
}

inline void SetString(boost::json::object& obj, std::string_view key, std::string_view value) {
  obj[std::string(key)] = boost::json::string(value);
}

} // namespace lan::protocol
