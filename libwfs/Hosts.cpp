#include "Hosts.h"
#include <macgyver/Exception.h>

namespace
{
std::string default_wms_host = "wms.fmi.fi";
}

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
void Hosts::addHost(const std::string& server, const std::string& wms_host, bool keep_apikey)
{
  HostInfo info{server, wms_host, keep_apikey};
  host_info.emplace_back(info);
}

const std::string& Hosts::getWMSHost(const std::string& server) const
{
  for (const auto& info : host_info)
    if (info.server == server)
      return info.wms_host;

  return default_wms_host;
}

bool Hosts::getKeepApikeyFlag(const std::string& server) const
{
  for (const auto& info : host_info)
    if (info.server == server)
      return info.keep_apikey;

  return true;
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
