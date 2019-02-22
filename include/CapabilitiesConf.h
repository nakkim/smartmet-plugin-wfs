#pragma once

#include "MultiLanguageString.h"
#include "MultiLanguageStringArray.h"
#include <spine/ConfigBase.h>
#include <libconfig.h++>
#include <ctpp2/CDT.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

  class CapabilitiesConf
  {
    struct Phone
    {
      SmartMet::Plugin::WFS::MultiLanguageStringP voice;
      SmartMet::Plugin::WFS::MultiLanguageStringP fax;
    };

    struct Address
    {
      SmartMet::Plugin::WFS::MultiLanguageStringP deliveryPoint;
      SmartMet::Plugin::WFS::MultiLanguageStringP city;
      SmartMet::Plugin::WFS::MultiLanguageStringP area;
      SmartMet::Plugin::WFS::MultiLanguageStringP postalCode;
      SmartMet::Plugin::WFS::MultiLanguageStringP country;
      SmartMet::Plugin::WFS::MultiLanguageStringP email;
    };

  public:
    CapabilitiesConf();

    virtual ~CapabilitiesConf();

    void parse(const std::string& default_language, libconfig::Setting& setting);

    void apply(CTPP::CDT& hash, const std::string& language) const;

 private:
  SmartMet::Plugin::WFS::MultiLanguageStringP title;
  SmartMet::Plugin::WFS::MultiLanguageStringP abstract;
  SmartMet::Plugin::WFS::MultiLanguageStringArray::Ptr keywords;
  SmartMet::Plugin::WFS::MultiLanguageStringP fees;
  SmartMet::Plugin::WFS::MultiLanguageStringP providerName;
  SmartMet::Plugin::WFS::MultiLanguageStringP providerSite;
  std::shared_ptr<Address> address;
  std::shared_ptr<Phone> phone;
  SmartMet::Plugin::WFS::MultiLanguageStringP accessConstraints;
  SmartMet::Plugin::WFS::MultiLanguageStringP onlineResource;
  SmartMet::Plugin::WFS::MultiLanguageStringP contactInstructions;
};

} // namespace WFS
} // namespace Plugin
} // namespace SmartMet

