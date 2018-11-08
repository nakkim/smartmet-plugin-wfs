#include "PluginData.h"
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <spine/Exception.h>

namespace bw = SmartMet::Plugin::WFS;
namespace pt = boost::posix_time;

bw::PluginData::PluginData(SmartMet::Spine::Reactor *theReactor, const char *theConfig)
    : itsConfig(theConfig), wfs_capabilities(new bw::WfsCapabilities)
{
  try
  {
    if (theConfig == nullptr)
    {
      std::ostringstream msg;
      msg << "ERROR: No configuration provided for WFS plugin";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    void *engine = theReactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");
    itsGeonames = reinterpret_cast<SmartMet::Engine::Geonames::Engine *>(engine);

    engine = theReactor->getSingleton("Querydata", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Querydata engine available");
    itsQEngine = reinterpret_cast<SmartMet::Engine::Querydata::Engine *>(engine);

    engine = theReactor->getSingleton("Gis", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Gis engine available");
    itsGisEngine = reinterpret_cast<SmartMet::Engine::Gis::Engine *>(engine);

    debug_level = itsConfig.get_optional_config_param<int>("debugLevel", 1);
    fallback_hostname =
        itsConfig.get_optional_config_param<std::string>("fallback_hostname", "localhost");
    fallback_protocol =
        itsConfig.get_optional_config_param<std::string>("fallback_protocol", "http");

    create_template_formatters();
    create_xml_parser();
    init_geo_server_access();

    const auto &feature_vect = itsConfig.read_features_config(itsGisEngine->getCRSRegistry());
    BOOST_FOREACH (auto feature, feature_vect)
    {
      wfs_capabilities->register_feature(feature);
    }

    create_typename_stored_query_map();

    create_stored_query_map(theReactor);

    std::string s_locked_timestamp;
    if (itsConfig.get_config().lookupValue("lockedTimeStamp", s_locked_timestamp))
    {
      locked_time_stamp = Fmi::TimeParser::parse(s_locked_timestamp);
      std::cout << "*****************************************************************\n";
      std::cout << "Using fixed timestamp "
                << (Fmi::to_iso_extended_string(*locked_time_stamp) + "Z") << std::endl;
      std::cout << "*****************************************************************\n";
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::PluginData::~PluginData() {}

void bw::PluginData::updateStoredQueryMap(Spine::Reactor *theReactor)
{
  try
  {
    stored_query_map->update_handlers(theReactor, *this);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Stored query handlers update failed!");
  }
}

boost::posix_time::ptime bw::PluginData::get_time_stamp() const
{
  try
  {
    if (locked_time_stamp)
    {
      return *locked_time_stamp;
    }
    else
    {
      return pt::second_clock::universal_time();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime bw::PluginData::get_local_time_stamp() const
{
  try
  {
    if (locked_time_stamp)
    {
      return *locked_time_stamp;
    }
    else
    {
      return pt::second_clock::universal_time();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::PluginData::create_template_formatters()
{
  try
  {
    const std::string gctn =
        itsConfig.get_mandatory_config_param<std::string>("getCapabilitiesTemplate");

    const std::string lsqtn =
        itsConfig.get_mandatory_config_param<std::string>("listStoredQueriesTemplate");

    const std::string dsqtn =
        itsConfig.get_mandatory_config_param<std::string>("describeStoredQueriesTemplate");

    const std::string fttn =
        itsConfig.get_mandatory_config_param<std::string>("featureTypeTemplate");

    const std::string etn = itsConfig.get_mandatory_config_param<std::string>("exceptionTemplate");

    const std::string dfn = itsConfig.get_mandatory_config_param<std::string>("ctppDumpTemplate");

    const auto template_dir = itsConfig.get_template_directory();

    // We do not actually create the formatters since using them is not
    // thread safe. Instead, we store the name and ask the formatter
    // factory for a thread local copy based on the name

    getCapabilitiesFormatterPath = template_dir / gctn;
    listStoredQueriesFormatterPath = template_dir / lsqtn;
    describeStoredQueriesFormatterPath = template_dir / dsqtn;
    featureTypeFormatterPath = template_dir / fttn;
    exceptionFormatterPath = template_dir / etn;
    ctppDumpFormatterPath = template_dir / dfn;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::PluginData::create_xml_parser()
{
  try
  {
    const std::string xml_grammar_pool_fn = itsConfig.getXMLGrammarPoolDumpFn();
    std::cout << "\t\t+ [Using XML grammar pool dump file '" << xml_grammar_pool_fn << "']"
              << std::endl;
    xml_parser.reset(new Xml::ParserMT(xml_grammar_pool_fn, false));

    std::string serialized_xml_schemas = itsConfig.get_optional_path("serializedXmlSchemas", "");
    if (serialized_xml_schemas != "")
    {
      xml_parser->load_schema_cache(serialized_xml_schemas);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::PluginData::init_geo_server_access()
{
  try
  {
    try
    {
      geo_server_db.reset(new GeoServerDB(itsConfig.get_geoserver_conn_string(), 5));
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to connect to GeoServer!", nullptr);
      exception.addParameter("Connection string", itsConfig.get_geoserver_conn_string());
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::PluginData::create_typename_stored_query_map()
{
  try
  {
    type_name_stored_query_map.reset(new bw::TypeNameStoredQueryMap);

    std::map<std::string, std::string> typename_storedqry;
    itsConfig.read_typename_config(typename_storedqry);

    type_name_stored_query_map->init(typename_storedqry);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::PluginData::create_stored_query_map(SmartMet::Spine::Reactor *theReactor)
{
  try
  {
    const std::vector<std::string> &sq_config_dirs = itsConfig.getStoredQueriesConfigDirs();

    stored_query_map.reset(new bw::StoredQueryMap);

    int parallel = itsConfig.get_optional_config_param<int>("parallelInit", 0);

    if (parallel)
    {
      BOOST_FOREACH (const std::string &sq_config_dir, sq_config_dirs)
      {
        stored_query_map->set_background_init(true);
        boost::thread thread(boost::bind(&bw::StoredQueryMap::read_config_dir,
                                         boost::ref(stored_query_map),
                                         theReactor,
                                         sq_config_dir,
                                         boost::ref(itsConfig.get_template_directory()),
                                         boost::ref(*this)));

        thread.detach();
      }
    }
    else
    {
      BOOST_FOREACH (const std::string &sq_config_dir, sq_config_dirs)
      {
        stored_query_map->read_config_dir(
            theReactor, sq_config_dir, itsConfig.get_template_directory(), *this);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
