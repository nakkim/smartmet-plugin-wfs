#pragma once

// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#include "CapabilitiesConf.h"
#include "Hosts.h"
#include "WfsFeatureDef.h"
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <engines/gis/CRSRegistry.h>
#include <spine/ConfigBase.h>
#include <libconfig.h++>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief The configuration of WFS plugin
 *
 *   An example is given below:
 *   @verbatim
 *
 *   url = "/wfs";
 *   storedQueryConfigDirs = ["/etc/smartmet/plugins/wfs/stored_queries",
 * "/etc/smartmet/plugins/wfs/other_stored_queries"];
 *   storedQueryTemplateDir = "/etc/smartmet/plugins/wfs/templates";
 *   listStoredQueriesTemplate = "list_stored_queries.c2t";
 *
 *   @endverbatim
 */
class Config : private boost::noncopyable, public SmartMet::Spine::ConfigBase
{
 public:
  Config(const std::string& configfile);

  const std::string& defaultUrl() const { return itsDefaultUrl; }
  const std::vector<std::string>& getStoredQueriesConfigDirs() const { return sq_config_dirs; }
  const std::string getGetFeatureById() const { return getFeatureById; }
  const std::string getXMLGrammarPoolDumpFn() const { return xml_grammar_pool_dump; }
  bool getValidateXmlOutput() const { return validate_output; }
  bool getFailOnValidateErrors() const { return fail_on_validate_errors; }
  std::string getProxy() const { return httpProxy; }
  std::string getNoProxy() const { return noProxy; }
  bool getEnableDemoQueries() const { return enable_demo_queries; }
  bool getEnableTestQueries() const { return enable_test_queries; }
  bool getEnableConfigurationPolling() const { return enable_configuration_polling; }
  bool getSQRestrictions() const { return sq_restrictions; }
  int getDefaultExpiresSeconds() const { return default_expires_seconds; }
  const boost::filesystem::path& get_template_directory() const { return template_directory; }
  const std::string& get_geoserver_conn_string() const { return geoserver_conn_str; }
  const std::vector<std::string>& get_languages() const { return languages; }
  inline int getCacheSize() const { return cache_size; }
  inline int getCacheTimeConstant() const { return cache_time_constant; }
  inline const std::string& get_default_locale() const { return default_locale; }
  std::vector<boost::shared_ptr<WfsFeatureDef> > read_features_config(
      SmartMet::Engine::Gis::CRSRegistry& theCRSRegistry);
  const CapabilitiesConf& get_capabilities_config() const { return capabilities_conf; }
  boost::optional<std::pair<std::string, std::string> > get_admin_credentials() const
  {
    return adminCred;
  }
  const Hosts& get_hosts() const { return hosts; }

  void read_typename_config(std::map<std::string, std::string>& typename_storedqry);

 private:
  void read_capabilities_config();
  void read_admin_cred();
  void read_hosts_info();

 private:
  std::string itsDefaultUrl;
  std::string getFeatureById;
  std::vector<std::string> sq_config_dirs;
  std::string geoserver_conn_str;
  std::string default_locale;
  std::string httpProxy;
  std::string noProxy;
  int cache_size;
  int cache_time_constant;
  int default_expires_seconds;
  std::vector<std::string> languages;
  boost::filesystem::path template_directory;
  std::string xml_grammar_pool_dump;
  bool validate_output;
  bool fail_on_validate_errors;
  bool enable_demo_queries;
  bool enable_test_queries;
  bool enable_configuration_polling;
  bool sq_restrictions;
  CapabilitiesConf capabilities_conf;

  /**
   *   @brief Admin user name and password if provided
   *
   *   Currently only used for reload request
   */
  boost::optional<std::pair<std::string, std::string> > adminCred;

  /**
   *   @brief Information about hosts to which WFS responses points
   */
  Hosts hosts;

};  // class Config

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
