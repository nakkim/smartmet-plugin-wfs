#ifndef WITHOUT_OBSERVATION

#include "stored_queries/StoredEnvMonitoringNetworkQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "SupportsLocationParameters.h"
#include <boost/icl/type_traits/to_string.hpp>
#include <smartmet/engines/observation/DBRegistry.h>
#include <smartmet/spine/Exception.h>
#include <memory>

namespace bw = SmartMet::Plugin::WFS;
namespace bo = SmartMet::Engine::Observation;

namespace
{
const char* P_NETWORK_ID = "networkId";
const char* P_CLASS_ID = "classId";
const char* P_CLASS_NAME = "className";
const char* P_GROUP_ID = "groupId";
const char* P_STATION_ID = "stationId";
const char* P_STATION_NAME = "stationName";
const char* P_MISSING_TEXT = "missingText";
const char* P_INSPIRE_NAMESPACE = "inspireNamespace";
const char* P_AUTHORITY_DOMAIN = "authorityDomain";
}  // namespace

bw::StoredEnvMonitoringNetworkQueryHandler::StoredEnvMonitoringNetworkQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
    : bw::SupportsExtraHandlerParams(config),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      m_geoEngine(nullptr),
      m_obsEngine(nullptr)
{
  try
  {
    register_array_param<int64_t>(P_NETWORK_ID, *config);
    register_array_param<int64_t>(P_CLASS_ID, *config);
    register_array_param<std::string>(P_CLASS_NAME, *config, false);
    register_array_param<int64_t>(P_GROUP_ID, *config);
    register_array_param<int64_t>(P_STATION_ID, *config);
    register_array_param<std::string>(P_STATION_NAME, *config, false);
    register_scalar_param<std::string>(P_INSPIRE_NAMESPACE, *config);
    register_scalar_param<std::string>(P_AUTHORITY_DOMAIN, *config);
    m_missingText = config->get_optional_config_param<std::string>(P_MISSING_TEXT, "NaN");
    m_debugLevel = config->get_debug_level();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredEnvMonitoringNetworkQueryHandler::~StoredEnvMonitoringNetworkQueryHandler() {}

void bw::StoredEnvMonitoringNetworkQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();

    void* engine;

    // Get Geonames
    engine = reactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Geonames engine available");

    m_geoEngine = reinterpret_cast<SmartMet::Engine::Geonames::Engine*>(engine);

    // Get Observation
    engine = reactor->getSingleton("Observation", nullptr);
    if (engine == nullptr)
      throw SmartMet::Spine::Exception(BCP, "No Observation engine available");

    m_obsEngine = reinterpret_cast<SmartMet::Engine::Observation::Engine*>(engine);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredEnvMonitoringNetworkQueryHandler::query(const StoredQuery& query,
                                                       const std::string& language,
						       const boost::optional<std::string> &hostname,
                                                       std::ostream& output) const
{
  try
  {
    const auto& params = query.get_param_map();
    auto inspireNamespace = params.get_single<std::string>(P_INSPIRE_NAMESPACE);
    auto authorityDomain = params.get_single<std::string>(P_AUTHORITY_DOMAIN);

    // Get the sequence number of query in the request
    auto sqId = query.get_query_id();
    FeatureID featureId(get_config()->get_query_id(), params.get_map(), sqId);

    // Removing some feature id parameters
    const char* removeParams[] = {
        P_NETWORK_ID, P_CLASS_ID, P_CLASS_NAME, P_GROUP_ID, P_STATION_ID, P_STATION_NAME};
    for (unsigned i = 0; i < sizeof(removeParams) / sizeof(*removeParams); i++)
    {
      featureId.erase_param(removeParams[i]);
    }

    if (m_debugLevel > 0)
      query.dump_query_info(std::cout);

    std::string lang = language;
    bw::SupportsLocationParameters::engOrFinToEnOrFi(lang);

    // Get request parameters.
    std::vector<int64_t> networkIdVector;
    params.get<int64_t>(P_NETWORK_ID, std::back_inserter(networkIdVector));

    std::vector<int64_t> classIdVector;
    params.get<int64_t>(P_CLASS_ID, std::back_inserter(classIdVector));

    std::vector<std::string> classNameVector;
    params.get<std::string>(P_CLASS_NAME, std::back_inserter(classNameVector));

    std::vector<int64_t> groupIdVector;
    params.get<int64_t>(P_GROUP_ID, std::back_inserter(groupIdVector));

    std::vector<int64_t> stationIdVector;
    params.get<int64_t>(P_STATION_ID, std::back_inserter(stationIdVector));

    std::vector<std::string> stationNameVector;
    params.get<std::string>(P_STATION_NAME, std::back_inserter(stationNameVector));

    // Using GROUP_MEMBERS_V1 as a base configuration
    bo::MastQueryParams emnQueryParams(dbRegistryConfig("GROUP_MEMBERS_V1"));
    emnQueryParams.addField("GROUP_ID");

    emnQueryParams.addJoinOnConfig(dbRegistryConfig("NETWORK_MEMBERS_V1"), "STATION_ID");

    // Join on NETWORKS_V1 view
    emnQueryParams.addJoinOnConfig(dbRegistryConfig("STATION_GROUPS_V2"), "GROUP_ID");
    emnQueryParams.addField("GROUP_CODE");
    emnQueryParams.addField("GROUP_NAME");
    emnQueryParams.addField("GROUP_DESC");

    if (not stationNameVector.empty())
    {
      // Join on STATION_NAMES_V1 view. Needed for search of netwprk by STATION_NAME
      emnQueryParams.addJoinOnConfig(dbRegistryConfig("STATION_NAMES_V1"), "STATION_ID");
    }

    emnQueryParams.addOrderBy("GROUP_ID", "ASC");
    emnQueryParams.useDistinct();

    for (std::vector<int64_t>::const_iterator it = networkIdVector.begin();
         it != networkIdVector.end();
         ++it)
      emnQueryParams.addOperation("OR_GROUP_network_id", "NETWORK_ID", "PropertyIsEqualTo", *it);

    for (auto& classId : classIdVector)
      emnQueryParams.addOperation("OR_GROUP_class_id", "CLASS_ID", "PropertyIsEqualTo", classId);

    for (std::vector<std::string>::const_iterator it = classNameVector.begin();
         it != classNameVector.end();
         ++it)
      emnQueryParams.addOperation("OR_GROUP_class_name", "CLASS_NAME", "PropertyIsLike", *it);

    for (auto& groupId : groupIdVector)
      emnQueryParams.addOperation("OR_GROUP_group_id", "GROUP_ID", "PropertyIsEqualTo", groupId);

    for (std::vector<int64_t>::const_iterator it = stationIdVector.begin();
         it != stationIdVector.end();
         ++it)
      emnQueryParams.addOperation("OR_GROUP_station_id", "STATION_ID", "PropertyIsEqualTo", *it);

    if (not stationNameVector.empty())
    {
      for (std::vector<std::string>::const_iterator it = stationNameVector.begin();
           it != stationNameVector.end();
           ++it)
        emnQueryParams.addOperation("OR_GROUP_station_name", "STATION_NAME", "PropertyIsLike", *it);
    }

    /*
    emnQueryParams.addOperation(
        "OR_GROUP_language_code", "LANGUAGE_CODE", "PropertyIsEqualTo", lang);
    */

    // bo::EnvironmentalMonitoringFacilityQuery emnQuery;
    bo::MastQuery emnQuery;
    emnQuery.setQueryParams(&emnQueryParams);
    m_obsEngine->makeQuery(&emnQuery);

    CTPP::CDT hash;
    params.dump_params(hash["query_parameters"]);
    dump_named_params(params, hash["named_parameters"]);
    hash["authorityDomain"] = authorityDomain;
    hash["responseTimestamp"] =
        boost::posix_time::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
    hash["queryId"] = query.get_query_id();

    // Container with the data fetched from obsengine.
    std::shared_ptr<bo::QueryResult> resultContainer = emnQuery.getQueryResultContainer();

    int groupCount = 0;

    if (resultContainer)
    {
      bo::QueryResult::ValueVectorType::const_iterator groupIdIt =
          resultContainer->begin("GROUP_ID");
      bo::QueryResult::ValueVectorType::const_iterator groupIdItEnd =
          resultContainer->end("GROUP_ID");
      bo::QueryResult::ValueVectorType::const_iterator groupCodeIt =
          resultContainer->begin("GROUP_CODE");
      bo::QueryResult::ValueVectorType::const_iterator groupNameIt =
          resultContainer->begin("GROUP_NAME");
      bo::QueryResult::ValueVectorType::const_iterator groupDescIt =
          resultContainer->begin("GROUP_DESC");

      size_t groupNameVectorSize = resultContainer->size("GROUP_NAME");
      size_t groupIdVectorSize = groupIdItEnd - groupIdIt;
      std::string groupIdOld;

      if (groupIdVectorSize == groupNameVectorSize)
      {
        // Filling the group and station data
        int groupCounter = 0;
        for (; groupIdIt != groupIdItEnd; ++groupIdIt, ++groupCodeIt, ++groupNameIt, ++groupDescIt)
        {
          const std::string groupId = bo::QueryResult::toString(groupIdIt);
          if (groupIdOld != groupId)
          {
            groupCount++;
            groupCounter++;
            hash["networks"][groupCounter - 1]["id"] = groupId;
            const std::string groupCode = bo::QueryResult::toString(groupCodeIt);
            if (not groupCode.empty())
              hash["networks"][groupCounter - 1]["code"] = groupCode;
            hash["networks"][groupCounter - 1]["name"] = bo::QueryResult::toString(groupNameIt);
            hash["networks"][groupCounter - 1]["description"] =
                bo::QueryResult::toString(groupDescIt);
            hash["networks"][groupCounter - 1]["inspireNamespace"] = inspireNamespace;

            featureId.add_param(P_GROUP_ID, groupId);
            hash["networks"][groupCounter - 1]["featureId"] = featureId.get_id();
            featureId.erase_param(P_GROUP_ID);
          }
          groupIdOld = groupId;
        }
      }
      else
      {
        std::ostringstream msg;
        msg << "warning: bw::StoredEnvMonitoringNetworkQueryHandler::query - varying size data "
               "vectors!\n";
        std::cerr << msg.str();
      }
    }

    const std::string networksMatched = boost::to_string(groupCount);
    hash["networksMatched"] = networksMatched;
    hash["networksReturned"] = networksMatched;

    hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
    hash["fmi_apikey_prefix"] = bw::QueryBase::FMI_APIKEY_PREFIX_SUBST;
    hash["hostname"] = QueryBase::HOSTNAME_SUBST;
    hash["protocol"] = QueryBase::PROTOCOL_SUBST;
    hash["language"] = language;

    // Format output
    format_output(hash, output, query.get_use_debug_format());

  }  // In case of some other failures
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Operation processing failed!", nullptr);
    if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
    exception.addParameter(WFS_LANGUAGE, language);
    throw exception;
  }
}

const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig>
bw::StoredEnvMonitoringNetworkQueryHandler::dbRegistryConfig(const std::string& configName) const
{
  try
  {
    // Get database registry from Observation

    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistry> dbRegistry =
        m_obsEngine->dbRegistry();
    if (not dbRegistry)
    {
      SmartMet::Spine::Exception exception(BCP, "Database registry is not available!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbrConfig =
        dbRegistry->dbRegistryConfig(configName);
    if (not dbrConfig)
    {
      SmartMet::Spine::Exception exception(BCP,
                                           "Database registry configuration is not available!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      exception.addParameter("Config name", configName);
      throw exception;
    }

    return dbrConfig;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase>
wfs_stored_env_monitoring_network_handler_create(SmartMet::Spine::Reactor* reactor,
                                                 boost::shared_ptr<StoredQueryConfig> config,
                                                 PluginImpl& plugin_data,
                                                 boost::optional<std::string> template_file_name)
{
  try
  {
    bw::StoredEnvMonitoringNetworkQueryHandler* qh = new bw::StoredEnvMonitoringNetworkQueryHandler(
        reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> instance(qh);
    instance->init_handler();
    return instance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef
    wfs_stored_env_monitoring_network_handler_factory(
        &wfs_stored_env_monitoring_network_handler_create);

/**

@page WFS_SQ_ENV_MONITORING_NETWORK_QUERY_HANDLER Stored Query handler for querying Environmental
Monitoring Facilities

@section WFS_SQ_ENV_MONITORING_NETWORK_QUERY_HANDLER_INTRO Introduction

This handler adds support for stored queries for ....

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredEnvMonitoringNetworkQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_env_monitoring_network_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_ENV_MONITORING_NETWORK_QUERY_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameters are in use

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>networkId</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>int</td>
  <td>The networkid is a unique identity of network. Every station has a unique identity in a
network if it is part of it (e.g. ICAO code for airports).</td>
</tr>

<tr>
  <td>classId</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td>Station groups are classified into classes. ClassId is an unique identity for a class.</td>
</tr>

<tr>
  <td>className</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td>Class name is just a name for a class with classId identity.</td>
</tr>

<tr>
  <td>groupId</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td>A station is placed in groups depending on which kind of measurents it does. GroupId is a
unique identity for a group.</td>
</tr>

<tr>
  <td>stationId</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td>StationId is a unique local identity for a station.</td>
</tr>

<tr>
  <td>stationName</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>StationName is just a name for a station.</td>
</tr>

<tr>
  <td>inspireNamespace</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td></td>
</tr>

<tr>
  <td>authorityDomain</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Authority domain used for example in codeSpace names of XML response.</td>
</tr>

</table>

*/

#endif  // WITHOUT_OBSERVATION
