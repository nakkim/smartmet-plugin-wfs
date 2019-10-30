// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include "WfsConvenience.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <spine/ConfigTools.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <locale>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace fs = boost::filesystem;
namespace ba = boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
const char* default_url = "/wfs";
const char* c_get_feature_by_id = "urn:ogc:def:query:OGC-WFS::GetFeatureById";

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config(const string& configfile)
    : SmartMet::Spine::ConfigBase(configfile, "WFS plugin"), itsDefaultUrl(default_url)
{
  std::cout << (SmartMet::Spine::log_time_str() + " [WFS] Using plugin configuration file " +
                configfile)
            << std::endl;

  try
  {
    itsDefaultUrl = get_optional_config_param<std::string>("url", itsDefaultUrl);
    sq_config_dirs = get_mandatory_path_array("storedQueryConfigDirs", 1);
    const std::string sq_template_dir = get_mandatory_path("storedQueryTemplateDir");
    getFeatureById = get_optional_config_param<std::string>("getFeatureById", c_get_feature_by_id);
    geoserver_conn_str = get_optional_config_param<std::string>("geoserverConnStr", "");
    default_locale = get_optional_config_param<std::string>("locale", guess_default_locale());
    cache_size = get_optional_config_param<int>("cacheSize", 100);
    cache_time_constant = get_optional_config_param<int>("cacheTimeConstant", 60);
    default_expires_seconds = get_optional_config_param<int>("defaultExpiresSeconds", 60);
    validate_output = get_optional_config_param<bool>("validateXmlOutput", false);
    fail_on_validate_errors = get_optional_config_param<bool>("failOnValidateErrors", false);
    enable_demo_queries = get_optional_config_param<bool>("enableDemoQueries", false);
    enable_test_queries = get_optional_config_param<bool>("enableTestQueries", false);
    enable_configuration_polling =
        get_optional_config_param<bool>("enableConfigurationPolling", false);

    sq_restrictions = get_optional_config_param<bool>("storedQueryRestrictions", true);
    httpProxy = get_optional_config_param<std::string>("httpProxy", "");
    noProxy = get_optional_config_param<std::string>("noProxy", "");

    std::cout << (SmartMet::Spine::log_time_str() + " [WFS] Using locale " + default_locale)
              << std::endl;

    std::vector<std::string> xml_grammar_pool_fns;
    // Ensure that setting exists (or report an error otherwise)

    auto grammarpaths = get_mandatory_path_array("xmlGrammarPoolDump", 1);

    // Not get the dump
    bool xgp_found = false;
    BOOST_FOREACH (const auto& fn, grammarpaths)
    {
      if (fs::exists(fn))
      {
        xgp_found = true;
        xml_grammar_pool_dump = fn;
        break;
      }
    }

    if (not xgp_found)
    {
      throw SmartMet::Spine::Exception(BCP, "No usable XML grammar pool file found!");
    }

    if (xml_grammar_pool_dump == "" or not fs::exists(xml_grammar_pool_dump))
    {
      throw SmartMet::Spine::Exception(
          BCP, "XML grammar pool file '" + xml_grammar_pool_dump + "' not found!");
    }

    languages = get_mandatory_config_array<std::string>("languages", 1);
    BOOST_FOREACH (std::string& language, languages)
    {
      Fmi::ascii_tolower(language);
    }

    BOOST_FOREACH (std::string& sq_config_dir, sq_config_dirs)
    {
      fs::path sqcd(sq_config_dir);
      if (!fs::exists(sqcd) || !fs::is_directory(sqcd))
        throw SmartMet::Spine::Exception(BCP,
                                         sq_config_dir +
                                             " provided as stored queries"
                                             " configuration directory is not a directory");
    }

    read_capabilities_config();

    // Verify that stored queries template directory exists
    template_directory = sq_template_dir;

    read_admin_cred();

    read_hosts_info();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<boost::shared_ptr<WfsFeatureDef> > Config::read_features_config(
    SmartMet::Engine::Gis::CRSRegistry& theCRSRegistry)
{
  try
  {
    fs::path features_dir(get_mandatory_path("featuresDir"));

    if (not fs::exists(features_dir))
    {
      throw SmartMet::Spine::Exception(BCP, "Directory '" + features_dir.string() + "' not found!");
    }

    if (not fs::is_directory(features_dir))
    {
      throw SmartMet::Spine::Exception(BCP, "'" + features_dir.string() + "' is not a directory");
    }

    std::vector<boost::shared_ptr<WfsFeatureDef> > result;

    for (auto it = fs::directory_iterator(features_dir); it != fs::directory_iterator(); ++it)
    {
      const fs::path entry = *it;
      const auto fn = entry.filename().string();
      if (fs::is_regular_file(entry) and not ba::starts_with(fn, ".") and
          not ba::starts_with(fn, "#") and ba::ends_with(fn, ".conf"))
      {
        boost::shared_ptr<SmartMet::Spine::ConfigBase> desc(
            new SmartMet::Spine::ConfigBase(entry.string(), "Feature description"));

        try
        {
          boost::shared_ptr<WfsFeatureDef> feature_def(
              new WfsFeatureDef(theCRSRegistry, languages.at(0), desc, desc->get_root()));
          result.push_back(feature_def);
        }
        catch (const std::exception& err)
        {
          std::cerr << SmartMet::Spine::log_time_str() << ": error reading feature description"
                    << " file '" << entry.string() << "'" << std::endl;

          SmartMet::Spine::Exception exception(
              BCP, "Error while reading feature description!", nullptr);
          exception.addParameter("File", entry.string());
          throw exception;
        }
      }
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Config::read_typename_config(std::map<std::string, std::string>& typename_storedqry)
{
  try
  {
    try
    {
      libconfig::Setting& root = get_root();

      if (root.exists("typename-storedquery-mapping"))
      {
        libconfig::Setting& map = assert_is_list(root["typename-storedquery-mapping"]);
        int count = map.getLength();

        for (int i = 0; i < count; ++i)
        {
          const libconfig::Setting& item = map[i];
          std::string type_name, stored_queries;

          if (!(item.lookupValue("type_name", type_name) &&
                item.lookupValue("stored_queries", stored_queries)))
          {
            continue;
          }

          typename_storedqry[type_name] = stored_queries;
        }
      }
    }
    catch (const libconfig::ConfigException& err)
    {
      std::cerr << "Failed to parse stored query configuration file '" << get_file_name() << "'"
                << std::endl;
      handle_libconfig_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Config::read_capabilities_config()
{
  try
  {
    const char* setting_name = "capabilitiesConfig";
    if (get_config().exists(setting_name))
    {
      libconfig::Setting& s1 = get_config().lookup(setting_name);
      const std::string default_language =
          languages.empty() ? std::string("eng") : *languages.begin();
      if (s1.getType() == libconfig::Setting::TypeString)
      {
        const std::string fn = get_mandatory_path(setting_name);
        libconfig::Config raw_capabilities_conf;
        std::cout << SmartMet::Spine::log_time_str()
                  << " [WFS][INFO] Reading capabilities information from '" << fn << '\''
                  << std::endl;
        raw_capabilities_conf.readFile(fn.c_str());
        capabilities_conf.parse(default_language, raw_capabilities_conf.getRoot());
      }
      else if (s1.getType() == libconfig::Setting::TypeGroup)
      {
        capabilities_conf.parse(default_language, s1);
      }
      else
      {
        throw SmartMet::Spine::Exception::Trace(
            BCP, "Incorrect value of configuration entry capabilities_config");
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Config::read_admin_cred()
{
  try
  {
    const char* setting_name = "admin";
    if (get_config().exists(setting_name))
    {
      libconfig::Setting& s1 = assert_is_group(get_config().lookup(setting_name));
      const std::string user = get_optional_config_param<std::string>(s1, "admin", "admin");
      const std::string password = get_mandatory_config_param<std::string>(s1, "password");
      adminCred = std::make_pair(user, password);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Config::read_hosts_info()
{
  const char* default_wms_host = "wms.fmi.fi";
  try
  {
    const char* setting_name = "hosts";
    if (get_config().exists(setting_name))
    {
      libconfig::Setting& s1 = assert_is_list(get_config().lookup(setting_name));

      for (int i = 0; i < s1.getLength(); i++)
      {
        auto& info = s1[i];
        auto server = get_mandatory_config_param<std::string>(info, "server");
        auto wms = get_optional_config_param<std::string>(info, "wms", default_wms_host);
        auto keep_apikey = get_optional_config_param<bool>(info, "keep_apikey", true);
        hosts.addHost(server, wms, keep_apikey);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================

/**

@page WFS_PLUGIN_CONFIG WFS Plugin configuration

This document describes entries of SmartMet WFS plugin configuration. Names of mandatory
parameters
are shown in bold in the table below

<table border="1">

<tr>
<th>Parameter</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
<td>url</td>
<td>string</td>
<td>optional (default @b /wfs )</td>
<td>The directory part of URL for WFS service. @b Note that supported languages will be added to the
provided
    string.</td>
</tr>

<tr>
<td>@b storedQueryConfigDirs</td>
<td>string or array of strings</td>
<td>mandatory</td>
<td>Specifies directories where @ref WFS_CFG_STORED_QUERY "stored queries configuration files"
    are located. Using relative path like <i>../plugins/wfs/cnf/opendata_stored_queries</i> is
accepted.
    Stored queries configuration file names in this directory must both
    - end with @b .conf
    - not begin with '.' or '#'</td>
</tr>

<tr>
<td>@b storedQueryTemplateDir</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies directory where to look for CTPP2 format template files if their absolute
    path is not explicitly specified. Using relative path like <i>../plugins/wfs/cnf/templates</i>
    is accepted. </td>
</tr>

<tr>
<td>getFeatureById</td>
<td>string</td>
<td>optional</td>
<td>The stored query ID for GetFeatureByID request (default value
    @b urn:ogc:def:query:OGC-WFS:GetFeatureById )</td>
</tr>

<tr>
<td>@b geoServerConnString</td>
<td>string</td>
<td>mandatory</td>
<td>Describes how to connect to GeoServer (PostGIS). The format of this string is
    @verbatim
    dbname=... user=... password=... host=...
    @endverbatim</td>
    One must of course substitute actual parameter values in this string
</tr>

<tr>
<td>cacheSize</td>
<td>integer</td>
<td>optional (default 100)</td>
<td>Specifies stored queries response cache size</td>
</tr>

<tr>
<td>cacheTimeConstant</td>
<td>integer</td>
<td>optional (default 60)</td>
<td>Specifies stored queries response cache time constant</td>
</tr>

<tr>
<td>defaultExpiresSeconds</td>
<td>integer</td>
<td>optional (default 60)</td>
<td>Specifies default expires seconds for the Expires header field of a requests
    response. The value can also be used in the Cache-Control header field. For
    most of the code the value has no effect, because on the first hand expires
    seconds is supposed to set in the request level. Use this value as a backup
    in a case of failure. Note that some request use this as a default value
    so if the default value is overrided by a definition, smart choice is a plus.</td>
</tr>

<tr>
<td>validateXmlOutput</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to perform WFS response validation. Allows to ensure that
    the response XML validates OK against XML schemas. Validate failures are reported
    but does not however prevent response to be sent to user. It is recommended
    not to enable this parameter for production use to avoid unnecessary
    use of resources. Note that incoming XML requests are validated always.
    Schemas required for validation incoming XML requests must be present in
    XML grammar dump (see @b xmlGrammarPoolDump)</td>
</tr>

<tr>
<td>failOnValidateErrors</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to perform WFS response validation errors cause request to fail.
    Ignored if response XML validation is not enabled.
</tr>

<tr>
<td>httpProxy</td>
<td>string (URL)</td>
<td>optional (default empty)</td>
<td>Specifies HTTP/HTTPS proxy to be used for XML schema download. Absence of setting
    or empty string means that proxy is not used. Ignored unless XML output validation
    is enabled (@b validateXmlOutput). One can also use environment variables for
    specifying proxy server (see <b>man curl</b> for more info).</td>
</tr>

<tr>
<td>noProxy</td>
<td>string (URL)</td>
<td>optional (default empty)</td>
<td>Specifies for which hosts HTTP/HTTPS proxy is not used. Ignored unless XML output validation
    is enabled (@b validateXmlOutput). One can also use environment variables for
    specifying proxy server (see <b>man curl</b> for more info).</td>
</tr>

<tr>
<td>httpProxy</td>
<td>string</td>
<td>optional (default empty)</td>
<td>Specifies HTTP proxy to use. Only used or XML schema download when XML output validation
   is enabled. Schemas required for validating XML format HTTP requests must be provided in
   XML grammar pool (request validation is always enabled)</td>
</tr>

<tr>
<td>enableDemoQueries</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to enable DEMO stored queries</td>
</tr>

<tr>
<td>enableTestQueries</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to enable test stored queries</td>
</tr>

<tr>
<td>@b xmlGrammarPoolDump</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies location of file which contains <a
href="http://xerces.apache.org/xerces-c/">Xerces-C</a>
    XML <a href="http://xerces.apache.org/xerces-c/apiDocs-3/classXMLGrammarPool.html">grammar pool
dump</a></td>
</tr>

<tr>
<td>@b languages</td>
<td>string or array of strings</td>
<td>mandatory</td>
<td>Specifies supported languages. The first (or only) language specification is treated as the
    default one</td>
</tr>

<tr>
<td>@b locale</td>
<td>string</td>
<td>optional</td>
<td>Locale to use.</td>
</tr>

<tr>
<td>@b featuresDir</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies directory where WFS @ref WFS_PLUGIN_CONFIG_FEATURE_DEF "feature description files" are
    located. Feature description files in this dierctory must both
    - end with @b .conf
    - not begin with '.' or '#'</td>
</tr>

<tr>
<td>@b getCapabilitiesTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of GetCapabilities response. The file is searched
in directory specified in parameter @b storedQueryTemplateDir unless absolute path provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>@b listStoredQueriesTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of ListStoredQueries response. The file is
searched in directory specified in parameter @b storedQueryTemplateDir unless absolute path
provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>@b describeStoredQueriesTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of DescribeStoredQueries response. The file is
searched in directory specified in parameter @b storedQueryTemplateDir unless absolute path
provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>@b featureTypeTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of DescribeFeatureType response. The file is
searched in directory specified in parameter @b storedQueryTemplateDir unless absolute path
provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>@b exceptionTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of error report. The file is searched in
directory specified in parameter @b storedQueryTemplateDir unless absolute path provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>@b ctppDumpTemplate</td>
<td>string</td>
<td>mandatory</td>
<td>Specifies CTPP2 template to use for generation of debug format response. The file is searched in
directory specified in parameter @b storedQueryTemplateDir unless absolute path provided. Read in
SmartMet::Plugin::WFS::PluginImpl::create_template_formatters</td>
</tr>

<tr>
<td>serializedXmlSchemas</td>
<td>string</td>
<td>optional</td>
<td>Specifies file containing serialized XML schemas when provided. This file is used as fallback
when schema
    resolution using locked XML grammar pool fails. Providing this parameter can be useful if XML
output
    validation is enabled. It should not be needed for validating incoming requests. Read in
    SmartMet::Plugin::WFS::PluginImpl::create_xml_parser</td>
</tr>

<tr>
<td>parallelInit</td>
<td>integer</td>
<td>optional (default @b 0)</td>
<td>Specifying non zero value tells WFS plugin to perform parallel (in separate threads)
initialization
    of stored query handlers. Plugin is immediately ready to handle incoming requests if this
feature
    is enabled, but stored query handlers become available when they are initialized. Read in
    SmartMet::Plugin::WFS::PluginImpl::create_stored_query_map</td>
</tr>

<tr>
<td>storedQueryRestrictions</td>
<td>boolean</td>
<td>optional (default true)</td>
<td>Specifies whether to use restrictions of the stored queries or not. By setting value to false,
its effect
    to a stored query depends on an implementation of the stored query handler class. Detailed
effect may
    explained in the handler class documentation.</td>
</tr>

<tr>
<td>admin</td>
<td>config group</td>
<td>optional</td>
<td>Specifies authentication settings (and possibly other parameters in the future) for performing
administrative operations. Currently only such operation is WFS plugin configuration reloading.
This group consists of parameters:
- admin - admin user name (optional, default is 'admin')
- password - password (mandatory)
</tr>

<tr>
<td>hosts</td>
<td>config group</td>
<td>optional</td>
<td>Specifies host names to use in output documents. Currently only host for WMS downloads is
supported This group consist of following parameters:
- wms - name of host for WMS downloads (default wms.fmi.fi)
</tr>

</table>

 */

/**

@page WFS_PLUGIN_CONFIG_CRS_DEF Coordinate projection descriptions for WFS plugin

Coordinate projection description must be provided in libconfig format. The following configuration
entries
are supported

<table border="1">

<tr>
<th>Parameter</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
<td>name</td>
<td>string</td>
<td>mandatory</td>
<td>The name of coordinate projection (for internal use)</td>
</tr>

<tr>
<td>swapCoord</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to swap first 2 coordinates. @b NOTE: value @b true must be used for
    for example @b EPSG:4326 to get coordinates in order: <i>longitude latitude</i></td>
</tr>

<tr>
<td>showHeight</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to include height</td>
</tr>

<tr>
<td>projEpochUri</td>
<td>string</td>
<td>mandatory</td>
<td>Provides URI for coordinates in this projection together with epoch</td>
</tr>

<tr>
<td>epsg</td>
<td>integer</td>
<td>optional</td>
<td>Provides projection EPSG number together with default values for parameters
    @b regex and @b projUri. The projection is registred in this case to GDAL using
    EPSG code</td>
</tr>

<tr>
<td>regex</td>
<td>string</td>
<td>mandatory unless @b epsg is specified</td>
<td>Provides REGEX for recognizing projection in WFS query parameter. Must be specified
    unless entry @b epsg is specified. In latest case the default value is
    @verbatim
    (?:urn:ogc:def:crs:|)EPSG:{1,2}NNNN
    @endverbatim
    where NNNN is 4 digit EPSG code</td>
</tr>

<tr>
<td>projUri</td>
<td>string</td>
<td>mandatory unless @b epsg is specified</td>
<td>Provides URI for description of projection. The default value if @b epsg is
    specified is
    @verbatim
    http://www.opengis.net/def/crs/EPSG/0/NNNN
    @endverbatim
    where NNNN is 4 digit EPSG code</td>
</tr>

<tr>
<td>proj4</td>
<td>string</td>
<td>mandatory unless @b epsg is specified, ignored otherwise</td>
<td>Specifies Proj.4 description of projection. Projection is registered using
    the value of this configuration  entry</td>
</tr>

<tr>
<td>epsgCode</td>
<td>integer</td>
<td>optional, ignored if @b epsg is provided</td>
<td>Specifies @b epsg code when projection is registered using Proj.4 description</td>
</tr>

</table>

<hr>

Some examples:
- WGS84
  @verbatim
  name  = "WGS84";
  epsg = 4326;
  swapCoord = true;
  projEpochUri = "http://xml.fmi.fi/gml/crs/compoundCRS.php?crs=4326&amp;time=unixtime";
  projUri = "http://www.opengis.net/def/crs/EPSG/0/4326";
  @endverbatim

- description of CRS using Proj.4 string:
  @verbatim
  name = "ETRS89 + EVRF2007 height";
  regex = "(?:urn:ogc:def:crs:|)EPSG:{1,2}7243";
  proj4 = "+proj=longlat +ellps=GRS80 +no_defs ";
  swapCoord = true;
  projUri = "http://www.opengis.net/def/crs/EPSG/0/7243";
  epsgCode = 7423;
  projEpochUri = "http://xml.fmi.fi/gml/crs/compoundCRS.php?crs=7423&amp;time=unixtime";
  showHeight = true;
  @endverbatim

*/
