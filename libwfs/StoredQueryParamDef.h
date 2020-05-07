#pragma once

#include "WfsException.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/TypeName.h>
#include <spine/Value.h>
#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Defines type of stored query parameter
 */
class StoredQueryParamDef
{
 public:
  enum ValueType
  {
    UNDEFINED,
    STRING,
    INT,
    UINT,
    DOUBLE,
    TIME,
    BOOL,
    POINT,
    BBOX
  };

 private:
  ValueType value_type;
  bool is_array;
  unsigned min_size;
  unsigned max_size;

 public:
  StoredQueryParamDef();

  void parse_def(const std::string& desc);

  virtual ~StoredQueryParamDef();

  inline ValueType getValueType() const { return value_type; }
  inline bool isArray() const { return is_array; }
  inline unsigned getMinSize() const { return min_size; }
  inline unsigned getMaxSize() const { return max_size; }
  /**
   *   @brief Read the single value according the type specification
   *
   *   The actual type stored in return value is:
   *   - std::string for type 'string'
   *   - int64_t for type 'int'
   *   - uint64_t for type 'unsigned'
   *   - double for type 'double'
   *   - boost::posix_time::ptime for type time
   */
  SmartMet::Spine::Value readValue(const std::string& value) const;

  /**
   *   @brief Read values from preparsed string vector
   *
   *   Additionally the size of vector is checked. One can
   *   also read a scalar in which case source vector
   *   size must be 1
   */
  std::vector<SmartMet::Spine::Value> readValues(const std::vector<std::string>& values) const;

  SmartMet::Spine::Value convValue(const SmartMet::Spine::Value& value) const;

  void dump(std::ostream& stream) const;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
