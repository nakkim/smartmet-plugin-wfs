#pragma once

#include <set>
#include <spine/ConfigBase.h>
#include <spine/MultiLanguageString.h>
#include <spine/MultiLanguageStringArray.h>
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
      SmartMet::Spine::MultiLanguageStringP voice;
      SmartMet::Spine::MultiLanguageStringP fax;
    };

    struct Address
    {
      SmartMet::Spine::MultiLanguageStringP deliveryPoint;
      SmartMet::Spine::MultiLanguageStringP city;
      SmartMet::Spine::MultiLanguageStringP area;
      SmartMet::Spine::MultiLanguageStringP postalCode;
      SmartMet::Spine::MultiLanguageStringP country;
      SmartMet::Spine::MultiLanguageStringP email;
    };

  public:
    CapabilitiesConf();

    virtual ~CapabilitiesConf();

    void parse(const std::string& default_language, libconfig::Setting& setting);

    void apply(CTPP::CDT& hash, const std::string& language) const;

    const std::set<std::string>& get_supported_formats() const { return supportedFormats; }

    void add_output_format(const std::string& text);

    static std::string conv_output_format_str(const std::string& src);

 private:
  SmartMet::Spine::MultiLanguageStringP title;
  SmartMet::Spine::MultiLanguageStringP abstract;
  SmartMet::Spine::MultiLanguageStringArray::Ptr keywords;
  SmartMet::Spine::MultiLanguageStringP fees;
  SmartMet::Spine::MultiLanguageStringP providerName;
  SmartMet::Spine::MultiLanguageStringP providerSite;
  std::shared_ptr<Address> address;
  std::shared_ptr<Phone> phone;
  SmartMet::Spine::MultiLanguageStringP accessConstraints;
  SmartMet::Spine::MultiLanguageStringP onlineResource;
  SmartMet::Spine::MultiLanguageStringP contactInstructions;
  std::set<std::string> supportedFormats;
};

} // namespace WFS
} // namespace Plugin
} // namespace SmartMet

