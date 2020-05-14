#if 0  // ### REPLACED BY SmartMet::Spine::Exception

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <WfsConvenience.h>
#include <WfsException.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
WfsException::WfsException(WfsException::ExceptionCode exceptionCode, const std::string& text)
    : exceptionCode(exceptionCode), whatString(text), textStrings()
{
}

WfsException::~WfsException() throw() {}
std::string WfsException::exceptionCodeString() const
{
  switch (exceptionCode)
  {
    case MISSING_PARAMETER_VALUE:
      return "MissingParameterValue";

    case OPERATION_NOT_SUPPORTED:
      return "OperationNotSupported";

    case OPERATION_PARSING_FAILED:
      return "OperationParsingFailed";

    case OPERATION_PROCESSING_FAILED:
      return "OperationProcessingFailed";

    case OPTION_NOT_SUPPORTED:
      return "OptionNotSupported";

    case INVALID_PARAMETER_VALUE:
      return "InvalidParameterValue";

    case VERSION_NEGOTIATION_FAILED:
      return "VersionNegotiationFailed";

    default:
      return "InvalidExceptionCode";
  }
}

const char* WfsException::what() const throw() { return whatString.c_str(); }
void WfsException::addText(const std::string& text) { textStrings.push_back(text); }
WfsException::operator CTPP::CDT() const
{
  CTPP::CDT hash;

  if (language) hash["language"] = *language;

  hash["exceptionList"][0]["exceptionCode"] = exceptionCodeString();

  if (location) hash["exceptionList"][0]["location"] = *location;

  hash["exceptionList"][0]["textList"][0] = whatString;
  for (std::size_t i = 0; i < textStrings.size(); i++)
  {
    hash["exceptionList"][0]["textList"][i + 1] = textStrings[i];
  }

  return hash;
}

void WfsException::print_on(std::ostream& stream) const
{
  namespace ba = boost::algorithm;
  const auto now = timestamp ? timestamp : boost::posix_time::second_clock::local_time();
  std::ostringstream msg;
  msg << now << ": " << exceptionCodeString() << ": "
      << ba::trim_right_copy_if(whatString, ba::is_any_of(" \t\r\n")) << "\n";
  for (std::size_t i = 0; i < textStrings.size(); i++)
  {
    const std::string& text = textStrings[i];
    if (text.length() > 0)
    {
      msg << now << ":>\t" << ba::trim_right_copy_if(textStrings[i], ba::is_any_of(" \t\r\n"))
          << "\n";
    }
  }
  stream << msg.str() << std::flush;
}

void WfsException::set_timestamp(const boost::posix_time::ptime& timestamp)
{
  this->timestamp = timestamp;
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#endif
