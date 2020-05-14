#pragma once

#include "XmlError.h"
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <list>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
class XmlErrorHandler : public xercesc::ErrorHandler
{
 public:
  XmlErrorHandler(bool throw_on_error);
  virtual ~XmlErrorHandler();
  virtual void warning(const xercesc::SAXParseException& exc);
  virtual void error(const xercesc::SAXParseException& exc);
  virtual void fatalError(const xercesc::SAXParseException& exc);
  virtual void resetErrors();

  inline bool isOk() const { return (num_errors == 0) and (num_fatal_errors == 0); }
  inline bool haveFatalErrors() const { return num_fatal_errors > 0; }
  inline const std::list<std::string>& get_messages() const { return messages; }
  void check_errors(const std::string& location);

 private:
  void add_message(const std::string& prefix, const xercesc::SAXParseException& exc);
  void check_terminate(const std::string& prefix, XmlError::error_level_t error_level);

 private:
  bool throw_on_error;
  std::list<std::string> messages;
  int num_warnings;
  int num_errors;
  int num_fatal_errors;
};

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
