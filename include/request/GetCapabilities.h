#pragma once

#include "PluginData.h"
#include "RequestBase.h"
#include <boost/shared_ptr.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Request
{
/**
 *   @brief Class for representing GetCapabilities WFS request
 */
class GetCapabilities : public RequestBase
{
 public:
  GetCapabilities(const std::string& language, const PluginData& plugin_data);

  virtual ~GetCapabilities();

  virtual RequestType get_type() const;

  virtual void execute(std::ostream& ost) const;

  static boost::shared_ptr<GetCapabilities> create_from_kvp(
      const std::string& language,
      const SmartMet::Spine::HTTP::Request& http_request,
      const PluginData& plugin_data);

  static boost::shared_ptr<GetCapabilities> create_from_xml(const std::string& language,
                                                            const xercesc::DOMDocument& document,
                                                            const PluginData& plugin_data);

  virtual int get_response_expires_seconds() const;

 private:
  const PluginData& plugin_data;
  std::set<std::string> languages;
  boost::optional<std::string> requested_language;
};

}  // namespace Request
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
