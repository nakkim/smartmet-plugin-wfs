#include "XmlErrorHandler.h"
#include "XmlError.h"
#include "XmlUtils.h"
#include <boost/format.hpp>
#include <spine/Exception.h>
#include <sstream>

namespace bwx = SmartMet::Plugin::WFS::Xml;

bwx::XmlErrorHandler::XmlErrorHandler(bool throw_on_error) : throw_on_error(throw_on_error)
{
  try
  {
    resetErrors();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bwx::XmlErrorHandler::~XmlErrorHandler() {}

void bwx::XmlErrorHandler::warning(const xercesc::SAXParseException& exc)
{
  try
  {
    add_message("WARNING", exc);
    num_warnings++;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bwx::XmlErrorHandler::error(const xercesc::SAXParseException& exc)
{
  try
  {
    add_message("ERROR", exc);
    num_errors++;
    check_terminate("Error", bwx::XmlError::ERROR);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bwx::XmlErrorHandler::fatalError(const xercesc::SAXParseException& exc)
{
  try
  {
    add_message("FATAL ERROR", exc);
    num_fatal_errors++;
    check_terminate("Fatal error", bwx::XmlError::FATAL_ERROR);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bwx::XmlErrorHandler::resetErrors()
{
  try
  {
    num_warnings = num_errors = num_fatal_errors = 0;
    messages.clear();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bwx::XmlErrorHandler::check_errors(const std::string& text)
{
  using boost::format;
  using boost::str;
  if (not isOk())
  {
    XmlError error(text, haveFatalErrors() ? bwx::XmlError::FATAL_ERROR : bwx::XmlError::ERROR);
    error.add_messages(get_messages());
    throw error;
  }
}

void bwx::XmlErrorHandler::add_message(const std::string& prefix,
                                       const xercesc::SAXParseException& exc)
{
  try
  {
    std::ostringstream msg;
    msg << prefix << ": " << to_opt_string(exc.getMessage()).first << " at " << exc.getLineNumber()
        << ':' << exc.getColumnNumber() << ", publicId='" << to_opt_string(exc.getPublicId()).first
        << "', systemId='" << to_opt_string(exc.getSystemId()).first << "'";
    messages.push_back(msg.str());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bwx::XmlErrorHandler::check_terminate(const std::string& prefix,
                                           XmlError::error_level_t error_level)
{
  try
  {
    if (throw_on_error)
    {
      throw bwx::XmlError(prefix + " while parsing XML input. Parsing terminated", error_level);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
