#include "XmlError.h"
#include <boost/format.hpp>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
XmlError::XmlError(const std::string& text, error_level_t error_level = XmlError::ERROR)
    : std::runtime_error(text), error_level(error_level)
{
}

XmlError::~XmlError() throw() {}

void XmlError::add_messages(const std::list<std::string>& messages)
{
  try
  {
    this->messages = messages;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const char* XmlError::error_level_name(enum error_level_t error_level)
{
  try
  {
    using boost::format;
    using boost::str;
    switch (error_level)
    {
      case ERROR:
        return "ERROR";
      case FATAL_ERROR:
        return "FATAL_ERROR";
      default:
        throw SmartMet::Spine::Exception(
            BCP, str(format("XmlError: unknown error level %2%") % (int)error_level));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
