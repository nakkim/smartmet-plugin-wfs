#include "XmlParameterExtractor.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlGmlTypes.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <macgyver/TimeParser.h>
#include <macgyver/TypeName.h>
#include <macgyver/Exception.h>
#include <sstream>
#include <stdexcept>
#include <stdint.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
ParameterExtractor::ParameterExtractor()
{
  try
  {
    add_type("string", &extract_string);

    add_int_type<int8_t>("byte");
    add_int_type<int16_t>("short");
    add_int_type<int32_t>("int");
    add_int_type<int64_t>("long");
    add_int_type<int64_t>("integer");

    add_int_type<uint8_t>("unsignedByte");
    add_int_type<uint16_t>("unsignedShort");
    add_int_type<uint32_t>("unsignedInt");
    add_int_type<uint64_t>("unsignedLong");
    add_int_type<uint64_t>("unsignedInteger");

    add_type("negativeInteger",
             boost::bind(&extract_integer, ::_1, std::numeric_limits<int64_t>::min(), -1));

    add_type("nonNegativeInteger",
             boost::bind(&extract_integer, ::_1, 0, std::numeric_limits<int64_t>::max()));

    add_type("nonPositiveInteger",
             boost::bind(&extract_integer, ::_1, std::numeric_limits<int64_t>::min(), 0));

    add_type("positiveInteger",
             boost::bind(&extract_integer, ::_1, 1, std::numeric_limits<int64_t>::max()));

    add_type("boolean", &extract_boolean);
    add_type("float", &extract_double);
    add_type("double", &extract_double);
    add_type("dateTime", &extract_date_time);
    add_type("NameList", &extract_name_list);        // gml:NameList
    add_type("doubleList", &extract_double_list);    // gml:doubleList
    add_type("integerList", &extract_integer_list);  // gml:integerList
    add_type("pos", &extract_gml_pos);               // gml:pos (gml:DirectPositionType)
    add_type("Envelope", &extract_gml_envelope);     // gml:Envelope (gml:EnvelopeType)

    add_type("language", &extract_string);  // FIXME: check correctness
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ParameterExtractor::~ParameterExtractor() {}

void ParameterExtractor::add_type(const std::string& name,
                                  ParameterExtractor::xml_param_extract_t extractor)
{
  try
  {
    auto result = param_extractor_map.insert(std::make_pair(name, extractor));
    if (not result.second)
    {
      throw Fmi::Exception(BCP, "Duplicate parameter handler for type '" + name + "'!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_param(
    const xercesc::DOMElement& element, const std::string& xml_type)
{
  try
  {
    std::string type_name = xml_type;

    // Currently namespace specification is ignored and only the name is matched
    // Otherwise we should use full namespace URI in the configuration file
    // and detect needed namespace prefix.
    std::size_t pos = xml_type.find_first_of(":");
    if (pos != std::string::npos)
      type_name = type_name.substr(pos + 1);

    auto handler_it = param_extractor_map.find(type_name);
    if (handler_it == param_extractor_map.end())
    {
      std::ostringstream msg;
      msg << "No handler found to extract stored query parameter with type '" << xml_type << "'!";
      throw Fmi::Exception(BCP, msg.str());
    }

    try
    {
      return (handler_it->second)(element);
    }
    catch (...)
    {
      std::ostringstream msg;
      const std::string& element_str = xml2string(&element);
      msg << "Error while reading " << xml_type << " from '" << element_str << "'";

      Fmi::Exception exception(BCP, "Operation failed!", nullptr);
      exception.addDetail(msg.str());
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_string(
    const xercesc::DOMElement& elem)
{
  try
  {
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    SmartMet::Spine::Value value(content);
    result.push_back(value);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_boolean(
    const xercesc::DOMElement& elem)
{
  try {
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    bool tmp = SmartMet::Spine::string2bool(content);
    result.push_back(SmartMet::Spine::Value(tmp));
    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_integer(
    const xercesc::DOMElement& elem, int64_t lower_limit, int64_t upper_limit)
{
  try
  {
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    int64_t int_val = std::stol(content);
    if ((int_val >= lower_limit) and (int_val <= upper_limit))
    {
      SmartMet::Spine::Value value(int_val);
      result.push_back(value);
      return result;
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::ParameterExtractor::extract_integer: value " << int_val
          << " is out of range " << lower_limit << ".." << upper_limit;
      throw Fmi::Exception(BCP, msg.str());
    }
    SmartMet::Spine::Value value(int_val);
    result.push_back(value);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_unsigned_integer(
    const xercesc::DOMElement& elem, uint64_t lower_limit, uint64_t upper_limit)
{
  try
  {
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    uint64_t uint_val = std::stoul(content);
    if ((uint_val >= lower_limit) and (uint_val <= upper_limit))
    {
      SmartMet::Spine::Value value(uint_val);
      result.push_back(value);
      return result;
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::ParameterExtractor::extract_integer: value " << uint_val
          << " is out of range " << lower_limit << ".." << upper_limit;
      throw Fmi::Exception(BCP, msg.str());
    }
    SmartMet::Spine::Value value(uint_val);
    result.push_back(value);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_double(
    const xercesc::DOMElement& elem)
{
  try
  {
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    const double double_val = std::stod(content);
    SmartMet::Spine::Value value(double_val);
    result.push_back(value);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_date_time(
    const xercesc::DOMElement& elem)
{
  try
  {
    namespace ba = boost::algorithm;
    namespace pt = boost::posix_time;
    std::vector<SmartMet::Spine::Value> result;
    const std::string content = extract_text(elem);
    pt::ptime t = Fmi::TimeParser::parse(ba::trim_copy(content));
    SmartMet::Spine::Value value(t);
    result.push_back(value);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_name_list(
    const xercesc::DOMElement& elem)
{
  try
  {
    namespace ba = boost::algorithm;
    std::vector<SmartMet::Spine::Value> result;
    std::vector<std::string> parts;
    const std::string content = ba::trim_copy(extract_text(elem));
    ba::split(parts, content, ba::is_any_of(" \t\r\n"), ba::token_compress_on);
    BOOST_FOREACH (const std::string& name, parts)
    {
      SmartMet::Spine::Value value(name);
      result.push_back(value);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_double_list(
    const xercesc::DOMElement& elem)
{
  try
  {
    namespace ba = boost::algorithm;
    std::vector<SmartMet::Spine::Value> result;
    std::vector<std::string> parts;
    const std::string content = ba::trim_copy(extract_text(elem));
    ba::split(parts, content, ba::is_any_of(" "), ba::token_compress_on);
    BOOST_FOREACH (const std::string& name, parts)
    {
      SmartMet::Spine::Value value(std::stod(name));
      result.push_back(value);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_integer_list(
    const xercesc::DOMElement& elem)
{
  try
  {
    namespace ba = boost::algorithm;
    std::vector<SmartMet::Spine::Value> result;
    std::vector<std::string> parts;
    const std::string content = ba::trim_copy(extract_text(elem));
    ba::split(parts, content, ba::is_any_of(" "), ba::token_compress_on);
    BOOST_FOREACH (const std::string& name, parts)
    {
      int64_t int_value = std::stol(name);
      SmartMet::Spine::Value value(int_value);
      result.push_back(value);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_gml_pos(
    const xercesc::DOMElement& elem)
{
  try
  {
    using SmartMet::Spine::Value;

    std::vector<xercesc::DOMElement*> v1 = get_child_elements(elem, GML_NAMESPACE_URI, "pos");
    if (v1.size() == 1)
    {
      auto pos = read_gml_direct_position_type(*v1.at(0));
      if (pos.data.size() == 2)
      {
        std::vector<SmartMet::Spine::Value> result;
        result.push_back(SmartMet::Spine::Value(pos.data[0]));
        result.push_back(SmartMet::Spine::Value(pos.data[1]));
        return result;
      }
      else
      {
        std::ostringstream msg;
        msg << "Unexpected size " << pos.data.size() << " of gml:DirectPositionType"
            << " (only size 2 is supported)";
        throw Fmi::Exception(BCP, msg.str());
      }
    }
    else
    {
      std::ostringstream msg;
      msg << "Exactly one instance of element {" << GML_NAMESPACE_URI << "}pos expected. Got '"
          << xml2string(&elem) << "'";
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> ParameterExtractor::extract_gml_envelope(
    const xercesc::DOMElement& elem)
{
  try
  {
    using SmartMet::Spine::Value;
    std::vector<xercesc::DOMElement*> v1 = get_child_elements(elem, GML_NAMESPACE_URI, "Envelope");
    if (v1.size() == 1)
    {
      auto envelope = read_gml_envelope_type(*v1.at(0));
      if (*envelope.srs_reference.srs_dimension == 2)
      {
        SmartMet::Spine::BoundingBox bbox;
        bbox.xMin = envelope.lower_corner.data[0];
        bbox.yMin = envelope.lower_corner.data[1];
        bbox.xMax = envelope.upper_corner.data[0];
        bbox.yMax = envelope.upper_corner.data[1];

        if (envelope.srs_reference.srs_name)
        {
          bbox.crs = *envelope.srs_reference.srs_name;
        }
        else
        {
          bbox.crs = "EPSG::4326";
        }

        std::vector<SmartMet::Spine::Value> result;
        result.push_back(SmartMet::Spine::Value(bbox));
        return result;
      }
      else
      {
        std::ostringstream msg;
        msg << "Unexpected value " << *envelope.srs_reference.srs_dimension << " of srsDimension"
            << " (2 expected)";
        throw Fmi::Exception(BCP, msg.str());
      }
    }
    else
    {
      std::ostringstream msg;
      msg << "Exactly one instance of element {" << GML_NAMESPACE_URI << "}Envelope expected. Got '"
          << xml2string(&elem) << "'";
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
