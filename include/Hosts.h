#pragma once

#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Host information
 */

class Hosts
{
 public:
  const std::string& getWMSHost(const std::string& server) const;
  bool getKeepApikeyFlag(const std::string& server) const;

  void addHost(const std::string& server, const std::string& wms_host, bool keep_apikey);

 private:
  struct HostInfo
  {
    std::string server;    // apikey handling server
    std::string wms_host;  // respective WMS server
    bool keep_apikey;      // keep or remove apikey
  };

  std::vector<HostInfo> host_info;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
