#pragma once

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <spine/Value.h>
#include <xercesc/dom/DOMElement.hpp>
#include <cassert>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
/**
 *   @brief Extracts stored query parameter from xercesc::DOMElement according to provided type
 *
 *   Currently namespace prefix is being ignored and silently skipped for simplicity
 */
class ParameterExtractor
{
 public:
  typedef boost::function1<std::vector<SmartMet::Spine::Value>, const xercesc::DOMElement&>
      xml_param_extract_t;

 public:
  ParameterExtractor();
  virtual ~ParameterExtractor();

  std::vector<SmartMet::Spine::Value> extract_param(const xercesc::DOMElement& element,
                                                    const std::string& xml_type);

 private:
  void add_type(const std::string& name, xml_param_extract_t extractor);

  template <typename IntType>
  void add_int_type(const std::string& name);

  std::map<std::string, xml_param_extract_t> param_extractor_map;

  static std::vector<SmartMet::Spine::Value> extract_string(const xercesc::DOMElement& elem);

  static std::vector<SmartMet::Spine::Value> extract_boolean(const xercesc::DOMElement& elem);

  static std::vector<SmartMet::Spine::Value> extract_integer(const xercesc::DOMElement& elem,
                                                             int64_t lower_limit,
                                                             int64_t upper_limit);

  static std::vector<SmartMet::Spine::Value> extract_unsigned_integer(
      const xercesc::DOMElement& elem, uint64_t lower_limit, uint64_t upper_limit);

  static std::vector<SmartMet::Spine::Value> extract_double(const xercesc::DOMElement& elem);

  static std::vector<SmartMet::Spine::Value> extract_date_time(const xercesc::DOMElement& elem);

  // gml:NameList
  static std::vector<SmartMet::Spine::Value> extract_name_list(const xercesc::DOMElement& elem);

  // gml:doubleList
  static std::vector<SmartMet::Spine::Value> extract_double_list(const xercesc::DOMElement& elem);

  // gml:doubleList
  static std::vector<SmartMet::Spine::Value> extract_integer_list(const xercesc::DOMElement& elem);

  // gml::pos
  static std::vector<SmartMet::Spine::Value> extract_gml_pos(const xercesc::DOMElement& elem);

  // gml::Envelope
  static std::vector<SmartMet::Spine::Value> extract_gml_envelope(const xercesc::DOMElement& elem);
};

template <typename IntType>
void ParameterExtractor::add_int_type(const std::string& name)
{
  assert(std::numeric_limits<IntType>::is_integer);
  if (std::numeric_limits<IntType>::is_signed)
  {
    add_type(name,
             boost::bind(&extract_integer,
                         ::_1,
                         std::numeric_limits<IntType>::min(),
                         std::numeric_limits<IntType>::max()));
  }
  else
  {
    add_type(name,
             boost::bind(&extract_unsigned_integer,
                         ::_1,
                         std::numeric_limits<IntType>::min(),
                         std::numeric_limits<IntType>::max()));
  }
}

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
