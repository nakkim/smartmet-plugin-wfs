#pragma once

#include "PluginData.h"
#include "RequestBase.h"
#include <xercesc/dom/DOMDocument.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Request
{
/**
 *  @brief Represents ListStoredQueries request
 */
class ListStoredQueries : public RequestBase
{
  const PluginImpl& plugin_data;

 private:
  ListStoredQueries(const std::string& language, const PluginImpl& plugin_data);

 public:
  virtual ~ListStoredQueries();

  virtual RequestType get_type() const;

  virtual void execute(std::ostream& output) const;

  static boost::shared_ptr<ListStoredQueries> create_from_kvp(
      const std::string& language,
      const SmartMet::Spine::HTTP::Request& http_request,
      const PluginImpl& plugin_data);

  static boost::shared_ptr<ListStoredQueries> create_from_xml(const std::string& language,
                                                              const xercesc::DOMDocument& document,
                                                              const PluginImpl& plugin_data);

  virtual int get_response_expires_seconds() const;
};

}  // namespace Request
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
