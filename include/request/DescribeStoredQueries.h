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
 *  @brief Represents DescribeStoredQueries request
 */
class DescribeStoredQueries : public RequestBase
{
  const PluginData& plugin_data;
  std::vector<std::string> ids;
  bool show_hidden;

 private:
  DescribeStoredQueries(const std::string& language,
                        const std::vector<std::string>& ids,
                        const PluginData& plugin_data);

 public:
  virtual ~DescribeStoredQueries();

  virtual RequestType get_type() const;

  virtual void execute(std::ostream& ost) const;

  static boost::shared_ptr<DescribeStoredQueries> create_from_kvp(
      const std::string& language,
      const SmartMet::Spine::HTTP::Request& http_request,
      const PluginData& plugin_data);

  static boost::shared_ptr<DescribeStoredQueries> create_from_xml(
      const std::string& language,
      const xercesc::DOMDocument& document,
      const PluginData& plugin_data);

  virtual int get_response_expires_seconds() const;
};

}  // namespace Request
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
