#include "XmlDomErrorHandler.h"
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <xqilla/xqilla-dom3.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using SmartMet::Plugin::WFS::Xml::XmlDomErrorHandler;

XmlDomErrorHandler::XmlDomErrorHandler() {}

XmlDomErrorHandler::~XmlDomErrorHandler() {}

bool XmlDomErrorHandler::handleError(const xercesc::DOMError &dom_error)
{
  try
  {
    std::ostringstream msg;
    msg << METHOD_NAME << ": " << UTF8(dom_error.getMessage()) << " at "
        << dom_error.getLocation()->getLineNumber() << ':'
        << dom_error.getLocation()->getColumnNumber();

    switch (dom_error.getSeverity())
    {
      case xercesc::DOMError::DOM_SEVERITY_WARNING:
        std::cerr << (std::string("WARNING: ") + msg.str() + "\n") << std::flush;
        break;

      case xercesc::DOMError::DOM_SEVERITY_ERROR:
        throw SmartMet::Spine::Exception::Trace(BCP, "DOM ERROR: " + msg.str());

      case xercesc::DOMError::DOM_SEVERITY_FATAL_ERROR:
        throw SmartMet::Spine::Exception::Trace(BCP, "FATAL DOM ERROR: " + msg.str());
    }

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
