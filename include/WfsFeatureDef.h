#pragma once

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <engines/gis/CRSRegistry.h>
#include <spine/ConfigBase.h>
#include <spine/MultiLanguageString.h>
#include <array>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class WfsFeatureDef
{
 public:
  WfsFeatureDef(SmartMet::Engine::Gis::CRSRegistry& crs_registry,
                const std::string& default_language,
                boost::shared_ptr<SmartMet::Spine::ConfigBase> config,
                libconfig::Setting& setting);

  virtual ~WfsFeatureDef();

  inline const std::string& get_name() const { return name; }
  inline const std::string& get_xml_type() const { return xml_type; }
  inline const std::string& get_xml_namespace() const { return xml_namespace; }
  inline boost::optional<std::string> get_xml_namespace_loc() const { return xml_namespace_loc; }
  inline std::string get_title(const std::string& language) const { return title->get(language); }
  inline std::string get_abstract(const std::string& language) const
  {
    return abstract->get(language);
  }

  inline std::string get_default_crs() const { return default_crs_url; }
  inline const std::vector<std::string>& get_other_crs() const { return other_crs_urls; }
  inline const std::array<double, 4> get_bbox() const { return bbox; }

 private:
  static std::string resolve_crs_url(const std::string& name,
                                     SmartMet::Engine::Gis::CRSRegistry& crs_registry);

 private:
  std::string name;
  std::string xml_type;
  std::string xml_namespace;
  boost::optional<std::string> xml_namespace_loc;
  SmartMet::Spine::MultiLanguageStringP title;
  SmartMet::Spine::MultiLanguageStringP abstract;
  std::string default_crs_url;
  std::vector<std::string> other_crs_urls;
  std::array<double, 4> bbox;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
