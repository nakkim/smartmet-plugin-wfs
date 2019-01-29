#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <newbase/NFmiParameterName.h>
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <spine/Parameter.h>
#include <spine/Reactor.h>
#include <spine/SmartMet.h>
#include <spine/Value.h>
#include <iostream>
#include <libconfig.h++>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// FIXME: update compiler versions for which we need __attribute__((__may_alias__)) as needed.
#if defined(__GNUC__)
#if (__GNUC__ == 4 && __GNUC_MINOR__ == 4)
#define MAY_ALIAS __attribute__((__may_alias__))
#endif
#endif /* defined(__GNUC__) */

#ifndef MAY_ALIAS
#define MAY_ALIAS
#endif

namespace SmartMet
{
inline bool special(const SmartMet::Spine::Parameter& theParam)
{
  try
  {
    switch (theParam.type())
    {
      case SmartMet::Spine::Parameter::Type::Data:
      case SmartMet::Spine::Parameter::Type::Landscaped:
        return false;
      case SmartMet::Spine::Parameter::Type::DataDerived:
      case SmartMet::Spine::Parameter::Type::DataIndependent:
        return true;
    }
    // ** NOT REACHED **
    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**
 *   @brief Add SmartMet::Spine::Parameter to the vector and return index of freshly appended vector
 * item
 */
inline std::size_t add_param(std::vector<SmartMet::Spine::Parameter>& dest,
                             const std::string& name,
                             SmartMet::Spine::Parameter::Type type,
                             FmiParameterName number = kFmiBadParameter)
{
  SmartMet::Spine::Parameter param(name, type, number);
  dest.push_back(param);
  return dest.size() - 1;
}

namespace Plugin
{
namespace WFS
{
/**
 *   @brief Escape XML string
 */
std::string xml_escape(const std::string& src);

/**
 *   @brief Provides range check when assigning to shorter integer type
 *
 *   Throws std::runtime error when out of range
 *
 *   Usually only first template parameter (destination type)
 *   must be specified.
 */
template <typename DestIntType, typename SourceIntType>
DestIntType cast_int_type(const SourceIntType value)
{
  try
  {
    if ((value < std::numeric_limits<DestIntType>::min() ||
         (value > std::numeric_limits<DestIntType>::max())))
    {
      std::ostringstream msg;
      msg << "Value " << value << " is out of range " << std::numeric_limits<DestIntType>::min()
          << "..." << std::numeric_limits<DestIntType>::max();
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
    else
    {
      return value;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

using SmartMet::Spine::string2bool;
using SmartMet::Spine::string2ptime;

std::string get_mandatory_header(const SmartMet::Spine::HTTP::Request& request,
                                 const std::string& name);

template <typename ParamType>
ParamType get_param(const SmartMet::Spine::HTTP::Request& request,
                    const std::string& name,
                    ParamType default_value)
{
  try
  {
    auto value = request.getParameter(name);
    if (value)
      return boost::lexical_cast<ParamType>(*value);
    else
      return default_value;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void check_time_interval(const boost::posix_time::ptime& start,
                         const boost::posix_time::ptime& end,
                         double max_hours);

void assert_unreachable(const char* file, int line) __attribute__((noreturn));

std::string as_string(const std::vector<SmartMet::Spine::Value>& src);

std::string as_string(
    const boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& src);

std::string remove_trailing_0(const std::string& src);

std::string guess_default_locale();

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#define ASSERT_UNREACHABLE assert_unreachable(__FILE__, __LINE__)
