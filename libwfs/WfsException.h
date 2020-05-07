#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>
#include <ctpp2/CDT.hpp>
#include <exception>
#include <ostream>
#include <string>
#include <vector>

#define WFS_EXCEPTION_CODE "WfsExceptionCode"
#define WFS_LANGUAGE "WfsLanguage"
#define WFS_LOCATION "WfsLocation"

#define WFS_MISSING_PARAMETER_VALUE "MissingParameterValue"
#define WFS_OPERATION_NOT_SUPPORTED "OperationNotSupported"
#define WFS_OPERATION_PARSING_FAILED "OperationParsingFailed"
#define WFS_OPERATION_PROCESSING_FAILED "OperationProcessingFailed"
#define WFS_OPTION_NOT_SUPPORTED "OptionNotSupported"
#define WFS_INVALID_PARAMETER_VALUE "InvalidParameterValue"
#define WFS_VERSION_NEGOTIATION_FAILED "VersionNegotiationFailed"

#if 0  // ### REPLACED BY SmartMet::Spine::Exception

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class WfsException : public std::exception
{
 public:
  enum ExceptionCode
  {
    MISSING_PARAMETER_VALUE,
    OPERATION_NOT_SUPPORTED,
    OPERATION_PARSING_FAILED,
    OPERATION_PROCESSING_FAILED,
    OPTION_NOT_SUPPORTED,
    INVALID_PARAMETER_VALUE,
    VERSION_NEGOTIATION_FAILED
  };

 public:
  WfsException(ExceptionCode exceptionCode, const std::string& text);
  virtual ~WfsException() throw();

  std::string exceptionCodeString() const;

  virtual const char* what() const throw();

  void addText(const std::string& text);

  operator CTPP::CDT() const;

  inline ExceptionCode getExceptionCode() const { return exceptionCode; }
  inline const std::vector<std::string>& getTextStrings() const { return textStrings; }
  inline void set_language(const std::string& language) { this->language = language; }
  inline void set_location(const std::string& location) { this->location = location; }
  template <typename Iterator>
  void addText(Iterator begin, Iterator end)
  {
    while (begin != end)
      addText(*begin++);
  }

  void print_on(std::ostream& output) const;

  void set_timestamp(const boost::posix_time::ptime& timestamp);

 private:
  ExceptionCode exceptionCode;
  std::string whatString;
  std::vector<std::string> textStrings;
  boost::optional<std::string> language;
  boost::optional<std::string> location;
  boost::optional<boost::posix_time::ptime> timestamp;
};

inline std::ostream& operator<<(std::ostream& output, const WfsException& err)
{
  err.print_on(output);
  return output;
}

} // namespace WFS
} // namespace Plugin
} // namespace SmartMet

#endif
