#pragma once

#include "Config.h"
#include "GeoServerDB.h"
#include "StoredQueryMap.h"
#include "TemplateFactory.h"
#include "TypeNameStoredQueryMap.h"
#include "WfsCapabilities.h"
#include "XmlEnvInit.h"
#include "XmlParser.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/shared_ptr.hpp>
#include <engines/geonames/Engine.h>
#include <engines/gis/CRSRegistry.h>
#include <engines/gis/Engine.h>
#include <engines/querydata/Engine.h>
#include <macgyver/TimedCache.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredQueryMap;

typedef Fmi::TimedCache::Cache<std::string, std::string> QueryResponseCache;

class PluginData : public boost::noncopyable
{
 public:
  PluginData(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  virtual ~PluginData();

  inline SmartMet::Engine::Gis::CRSRegistry& get_crs_registry() const
  {
    return itsGisEngine->getCRSRegistry();
  }
  inline Config& get_config() { return itsConfig; }
  inline const Config& get_config() const { return itsConfig; }
  inline boost::shared_ptr<Fmi::TemplateFormatter> get_get_capabilities_formater() const
  {
    return itsTemplateFactory.get(getCapabilitiesFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_list_stored_queries_formatter() const
  {
    return itsTemplateFactory.get(listStoredQueriesFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_describe_stored_queries_formatter() const
  {
    return itsTemplateFactory.get(describeStoredQueriesFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_feature_type_formatter() const
  {
    return itsTemplateFactory.get(featureTypeFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_exception_formatter() const
  {
    return itsTemplateFactory.get(exceptionFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_ctpp_dump_formatter() const
  {
    return itsTemplateFactory.get(ctppDumpFormatterPath);
  }

  inline boost::shared_ptr<Fmi::TemplateFormatter> get_stored_query_formatter(
      const boost::filesystem::path& filename) const
  {
    return itsTemplateFactory.get(filename);
  }

  inline boost::shared_ptr<Xml::ParserMT> get_xml_parser() const { return xml_parser; }
  inline boost::shared_ptr<GeoServerDB> get_geo_server_database() const { return geo_server_db; }
  inline const StoredQueryMap& get_stored_query_map() const { return *stored_query_map; }
  inline const TypeNameStoredQueryMap& get_typename_stored_query_map() const
  {
    return *type_name_stored_query_map;
  }

  inline const std::vector<std::string>& get_languages() const { return itsConfig.get_languages(); }
  inline int get_debug_level() const { return debug_level; }
  inline std::string get_fallback_hostname() const { return fallback_hostname; }
  inline std::string get_fallback_protocol() const { return fallback_protocol; }
  boost::posix_time::ptime get_time_stamp() const;

  boost::posix_time::ptime get_local_time_stamp() const;

  inline WfsCapabilities& get_capabilities() { return *wfs_capabilities; }
  inline const WfsCapabilities& get_capabilities() const { return *wfs_capabilities; }

  void updateStoredQueryMap(Spine::Reactor* theReactor);

 private:
  void create_template_formatters();
  void create_xml_parser();
  void init_geo_server_access();
  void create_stored_query_map(SmartMet::Spine::Reactor* theReactor);
  void create_typename_stored_query_map();

 private:
  Config itsConfig;

  SmartMet::Engine::Geonames::Engine* itsGeonames;
  SmartMet::Engine::Querydata::Engine* itsQEngine;
  SmartMet::Engine::Gis::Engine* itsGisEngine;

  TemplateFactory itsTemplateFactory;

  boost::filesystem::path getCapabilitiesFormatterPath;
  boost::filesystem::path listStoredQueriesFormatterPath;
  boost::filesystem::path describeStoredQueriesFormatterPath;
  boost::filesystem::path featureTypeFormatterPath;
  boost::filesystem::path exceptionFormatterPath;
  boost::filesystem::path ctppDumpFormatterPath;

  boost::shared_ptr<Xml::ParserMT> xml_parser;
  boost::shared_ptr<GeoServerDB> geo_server_db;
  std::unique_ptr<StoredQueryMap> stored_query_map;
  std::unique_ptr<TypeNameStoredQueryMap> type_name_stored_query_map;
  std::unique_ptr<WfsCapabilities> wfs_capabilities;
  int debug_level;
  std::string fallback_hostname;
  std::string fallback_protocol;

  /**
   *   @brief Locked timestamp for testing only
   */
  boost::optional<boost::posix_time::ptime> locked_time_stamp;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
