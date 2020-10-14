#include "XmlEnvInit.h"
#include <macgyver/TypeName.h>
#include <macgyver/Exception.h>
#include <xercesc/util/PlatformUtils.hpp>
#include <xqilla/utils/UTF8Str.hpp>
#include <xqilla/utils/XQillaPlatformUtils.hpp>
#include <sstream>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
EnvInit::EnvInit()
{
  try
  {
    try
    {
      XQillaPlatformUtils::initialize();
    }
    catch (const xercesc::XMLException& err)
    {
      std::string str = UTF8(err.getMessage());
      Fmi::Exception exception(BCP, "Error during Xerces-C initialization!", nullptr);
      exception.addDetail(str);
      throw exception;
    }

    // Disable Xerces-C net accessor for now
    xercesc::XMLPlatformUtils::fgNetAccessor = nullptr;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EnvInit::~EnvInit()
{
  XQillaPlatformUtils::terminate();
}

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
