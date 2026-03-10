#pragma once
#include <string>
#include <boost/json.hpp>

namespace lan::util {

inline std::string Serialize(const boost::json::object& obj) {
  return boost::json::serialize(obj);
}

} // namespace lan::util
