#ifndef WITHOUT_OBSERVATION

#include "stored_queries/StoredAviationObservationQueryHandler.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <engines/observation/DBRegistry.h>
#include <engines/observation/VerifiableMessageQuery.h>
#include <engines/observation/VerifiableMessageQueryParams.h>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <algorithm>
#include <unordered_map>
#include <utility>

namespace bw = SmartMet::Plugin::WFS;
namespace bo = SmartMet::Engine::Observation;
namespace pt = boost::posix_time;
namespace ba = boost::algorithm;

namespace
{
const char* P_ICAO_CODE = "icaoCode";
const char* P_BEGIN_TIME = "beginTime";
const char* P_END_TIME = "endTime";
const char* P_STATION_TYPE = "stationType";
const char* P_RETURN_ONLY_LATEST = "returnOnlyLatest";
}  // namespace

bw::StoredAviationObservationQueryHandler::StoredAviationObservationQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)

    : bw::SupportsExtraHandlerParams(config),
      bw::RequiresGeoEngine(reactor),
      bw::RequiresObsEngine(reactor),
      bw::StoredQueryHandlerBase(reactor, config, plugin_data, template_file_name),
      bw::SupportsLocationParameters(reactor, config, SUPPORT_KEYWORDS | INCLUDE_GEOIDS),
      bw::SupportsBoundingBox(config, plugin_data.get_crs_registry())
{
  try
  {
    register_array_param<std::string>(P_ICAO_CODE, *config);
    register_scalar_param<pt::ptime>(P_BEGIN_TIME, *config);
    register_scalar_param<pt::ptime>(P_END_TIME, *config);
    register_scalar_param<std::string>(P_STATION_TYPE, *config);
    register_scalar_param<std::string>(P_RETURN_ONLY_LATEST, *config, "false");
    m_sqRestrictions = plugin_data.get_config().getSQRestrictions();
    m_maxHours = config->get_optional_config_param<double>("maxHours", 7.0 * 24.0);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::StoredAviationObservationQueryHandler::~StoredAviationObservationQueryHandler() {}

void bw::StoredAviationObservationQueryHandler::query(const StoredQuery& query,
                                                      const std::string& language,
						      const boost::optional<std::string>& hostname,
                                                      std::ostream& output) const
{
  try
  {
    const auto& params = query.get_param_map();

    try
    {
      std::map<std::string, SmartMet::Spine::LocationPtr> validIcaoCodes;

      const bool returnOnlyLatest =
          (params.get_single<std::string>(P_RETURN_ONLY_LATEST) == "true");

      // Search the name
      Locus::QueryOptions nameSearchOptions;
      nameSearchOptions.SetLanguage("icao");
      nameSearchOptions.SetCountries("all");
      nameSearchOptions.SetSearchVariants(true);
      nameSearchOptions.SetFeatures("AIRP");  // Airports

      // Search locations.
      std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > validLocations;
      get_location_options(params, language, &validLocations);

      // Get ICAO codes requested
      std::vector<std::string> icaoCodeVector;
      params.get<std::string>(P_ICAO_CODE, std::back_inserter(icaoCodeVector));

      const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";
      SmartMet::Engine::Gis::CRSRegistry& crs_registry = plugin_impl.get_crs_registry();

      // Default output CRS.
      const std::string crs = DATA_CRS_NAME;
      bool show_height = false;
      std::string proj_uri = "UNKNOWN";
      std::string proj_epoch_uri = "UNKNOWN";
      std::string axis_labels = "";
      crs_registry.get_attribute(crs, "showHeight", &show_height);
      crs_registry.get_attribute(crs, "projUri", &proj_uri);
      crs_registry.get_attribute(crs, "projEpochUri", &proj_epoch_uri);
      crs_registry.get_attribute(crs, "axisLabels", &axis_labels);

      // Get bounding box.
      SmartMet::Spine::BoundingBox requestedBBox;
      bool haveBBox = get_bounding_box(params, crs, &requestedBBox);

      std::string stationType =
          Fmi::ascii_tolower_copy(params.get_single<std::string>(P_STATION_TYPE));
      if (stationType.empty())
        stationType = "finavia";

      // This is the station set allowed in this handler.
      SmartMet::Spine::LocationList locationsByStationType =
          geo_engine->keywordSearch(nameSearchOptions, stationType);

      if (haveBBox)
      {
        std::unique_ptr<SmartMet::Spine::BoundingBox> queryBBox;
        queryBBox.reset(new SmartMet::Spine::BoundingBox);
        *queryBBox = transform_bounding_box(requestedBBox, DATA_CRS_NAME);

        // Select the locations inside the bounding box.
        for (SmartMet::Spine::LocationList::iterator it = locationsByStationType.begin();
             it != locationsByStationType.end();
             ++it)
        {
          if (queryBBox->xMin <= (*it)->longitude and queryBBox->xMax >= (*it)->longitude and
              queryBBox->yMin <= (*it)->latitude and queryBBox->yMax >= (*it)->latitude)
          {
            // Do not add duplicates.
            if (validIcaoCodes.find((*it)->name) == validIcaoCodes.end())
              validIcaoCodes.insert(std::make_pair((*it)->name, (*it)));
          }
        }
      }

      // Validity of ICAO code(s)
      for (std::vector<std::string>::const_iterator it = icaoCodeVector.begin();
           it != icaoCodeVector.end();
           ++it)
      {
        // Check that the icao code length is 4 characters.
        if (((*it).end() - (*it).begin()) != 4)
          continue;

        SmartMet::Spine::LocationList locationList =
            geo_engine->nameSearch(nameSearchOptions, *it);

        // ICAO code is uniq so the first one must be the one.
        if (locationList.size() == 0 or locationList.front()->name != *it)
        {
          std::ostringstream msg;
          msg << "ICAO code '" << *it << "' not found.";
          SmartMet::Spine::Exception exception(BCP, msg.str());
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
          throw exception;
        }

        // Do not add dublicates.
        if (validIcaoCodes.find(locationList.front()->name) == validIcaoCodes.end())
          validIcaoCodes.insert(std::make_pair(locationList.front()->name, locationList.front()));
      }

      // Check that the valid location is an airport.
      for (std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> >::iterator it =
               validLocations.begin();
           it != validLocations.end();
           ++it)
      {
        SmartMet::Spine::LocationList::const_iterator locIt = locationsByStationType.begin();
        for (; locIt != locationsByStationType.end(); ++locIt)
        {
          if ((*locIt)->geoid == (*it).second->geoid)
            break;
        }

        // Add only allowed stations, ones.
        if (locIt != locationsByStationType.end() and
            validIcaoCodes.find((*locIt)->name) == validIcaoCodes.end())
        {
          validIcaoCodes.insert(std::make_pair((*locIt)->name, *locIt));
        }
      }

      SmartMet::Spine::BoundingBox bbox;
      bbox.crs = crs;

      // Create CTPP hash
      CTPP::CDT hash;

      // Positions of iwxxmContentVector which have an iwxxm message.
      std::vector<unsigned int> messagePositions;
      std::vector<std::string> iwxxmContentVector;
      int numMatched = 0;

      // Make database query only if there is something to look for.
      if (not validIcaoCodes.empty())
      {
        const std::shared_ptr<bo::DBRegistryConfig> registryConfig =
            obs_engine->dbRegistry()->dbRegistryConfig("QC_RUT.AVIOBS_V2");
        if (registryConfig == nullptr)
        {
          std::cerr
              << "warning: SmartMet::Plugin::WFS::StoredAviationObservationQueryHandler::query is "
                 "using empty database registry configuration.\n";
        }

        bo::VerifiableMessageQueryParams queryParams(registryConfig);
        bo::VerifiableMessageQuery verifiableMessageQuery;

        // Special case where only the latest message will be returned inside a time range.
        if (returnOnlyLatest)
          queryParams.setReturnOnlyLatest();

        // The column names in the db to fetch.
        const std::list<std::string> selectNameList = {
            "STATION_ID", "MESSAGE_TIME", "IWXXM_CONTENT"};
        try
        {
          for (std::map<std::string, SmartMet::Spine::LocationPtr>::iterator it =
                   validIcaoCodes.begin();
               it != validIcaoCodes.end();
               ++it)
            queryParams.addStationId((*it).first);

          for (std::list<std::string>::const_iterator it = selectNameList.begin();
               it != selectNameList.end();
               ++it)
          {
            if (!queryParams.addSelectName(*it))
              std::cerr << "SmartMet::Plugin::WFS::query : Add of the select name '" << *it
                        << "' failed\n";
          }

          const boost::posix_time::ptime startTime = params.get_single<pt::ptime>(P_BEGIN_TIME);
          const boost::posix_time::ptime endTime = params.get_single<pt::ptime>(P_END_TIME);

          if (m_sqRestrictions)
            check_time_interval(startTime, endTime, m_maxHours);

          queryParams.setTimeRange(startTime, endTime);

          verifiableMessageQuery.setQueryParams(&queryParams);

          obs_engine->makeQuery(&verifiableMessageQuery);
        }
        catch (...)
        {
          SmartMet::Spine::Exception exception(BCP, "Operation processing failed!", nullptr);
          if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
          throw exception;
        }

        std::shared_ptr<bo::QueryResult> resultContainer =
            verifiableMessageQuery.getQueryResultContainer();

        // IWXXM message exist if not "0"
        resultContainer->getValueVectorData("IWXXM_CONTENT", iwxxmContentVector);

        // Fill the IWXXM data
        CTPP::CDT& group = hash["groups"][0];
        if (iwxxmContentVector.size() > 0)
        {
          int i = 0;
          for (std::vector<std::string>::iterator it = iwxxmContentVector.begin();
               it != iwxxmContentVector.end();
               ++it)
          {
            if ((*it).size() > 1)
            {
              messagePositions.push_back(it - iwxxmContentVector.begin());
              group["contentList"][i++]["iwxxm"] = *it;
            }
          }

          numMatched = messagePositions.size();

          // Define airport bounding box. If some IWXXM is found
          // the station position is included.
          std::vector<std::string> stationIdVector;
          resultContainer->getValueVectorData("STATION_ID", stationIdVector);
          bool initBBox = true;
          for (std::vector<unsigned int>::iterator it = messagePositions.begin();
               it != messagePositions.end();
               ++it)
          {
            std::map<std::string, SmartMet::Spine::LocationPtr>::const_iterator icaoLocation =
                validIcaoCodes.find(stationIdVector.at(*it));

            if (icaoLocation == validIcaoCodes.end())
              continue;

            if (initBBox)
            {
              initBBox = false;
              bbox.xMin = (*icaoLocation).second->longitude;
              bbox.xMax = (*icaoLocation).second->longitude;
              bbox.yMin = (*icaoLocation).second->latitude;
              bbox.yMax = (*icaoLocation).second->latitude;
            }
            else
            {
              bbox.xMin = std::min(bbox.xMin, (*icaoLocation).second->longitude);
              bbox.xMax = std::max(bbox.xMax, (*icaoLocation).second->longitude);
              bbox.yMin = std::min(bbox.yMin, (*icaoLocation).second->latitude);
              bbox.yMax = std::max(bbox.yMax, (*icaoLocation).second->latitude);
            }
          }

          bool swap_coord = false;
          crs_registry.get_attribute(bbox.crs, "swapCoord", &swap_coord);
          if (swap_coord)
          {
            double tmp_xMin = bbox.xMin;
            double tmp_xMax = bbox.xMax;
            bbox.xMin = bbox.yMin;
            bbox.yMin = tmp_xMin;
            bbox.xMax = bbox.yMax;
            bbox.yMax = tmp_xMax;
          }
        }
      }

      const std::string s_numMatched = boost::to_string(numMatched);
      hash["numReturned"] = s_numMatched;
      hash["numMatched"] = s_numMatched;

      hash["responseTimestamp"] =
          Fmi::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
      hash["queryId"] = query.get_query_id();

      hash["projSrsDim"] = (show_height ? 3 : 2);
      hash["projSrsName"] = proj_uri;
      if (!axis_labels.empty())
        hash["projSrsAxisLabels"] = axis_labels;

      hash["lowerCorner"] = std::to_string(static_cast<long double>(bbox.xMin))
                                .append(" ")
                                .append(std::to_string(static_cast<long double>(bbox.yMin)));
      hash["upperCorner"] = std::to_string(static_cast<long double>(bbox.xMax))
                                .append(" ")
                                .append(std::to_string(static_cast<long double>(bbox.yMax)));

      // Format output
      format_output(hash, output, query.get_use_debug_format());
    }  // In case of some other failures
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation failed!", nullptr);
      exception.addParameter(WFS_LANGUAGE, language);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredAviationObservationQueryHandler::update_parameters(
    const RequestParameterMap& params,
    int seq_id,
    std::vector<boost::shared_ptr<RequestParameterMap> >& result) const
{
}

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase>
wfs_stored_aviation_observation_handler_create(SmartMet::Spine::Reactor* reactor,
                                               boost::shared_ptr<StoredQueryConfig> config,
                                               PluginImpl& plugin_data,
                                               boost::optional<std::string> template_file_name)
{
  try
  {
    bw::StoredAviationObservationQueryHandler* qh = new bw::StoredAviationObservationQueryHandler(
        reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> instance(qh);
    return instance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_stored_aviation_observation_handler_factory(
    &wfs_stored_aviation_observation_handler_create);

/**

@page WFS_SQ_AVIATION_OBSERVATION_QUERY_HANDLER Stored Query handler for querying Metar

@section WFS_SQ_AVIATION_OBSERVATION_HANDLER_INTRO Introduction

The stored query handler provide access to the weather reports used by pilots
 in fulfillment of a part of a pre-flight weather briefing, and by meteorologists,
 who use aggregated METAR information to assist in weather forecasting. A METAR
 weather report is returned in IWXXM format which contain also the original METAR.

<table border="1">
  <tr>
     <td>Implementation</td>
     <td>SmartMet::Plugin::WFS::StoredAviationObservationQueryHandler</td>
  </tr>
  <tr>
    <td>Constructor name (for stored query configuration)</td>
    <td>@b wfs_stored_aviation_observation_handler_factory</td>
  </tr>
</table>

@section WFS_SQ_AVIATION_OBSERVATION_QUERY_HANDLER_PARAMS Query handler built-in parameters

The following stored query handler parameters are being used by this stored query handler:

@ref WFS_SQ_PARAM_BBOX_PARAM "Bounding box"

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>beginTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the time of the begin of time interval</td>
</tr>

<tr>
  <td>endTime</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>time</td>
  <td>Specifies the time of the end of time interval</td>
</tr>

<tr>
  <td>icaoCode</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>International Civil Aviation Organization (ICAO) airport code is a four-character alphanumeric
code designating each airport around the world.</td>
</tr>

<tr>
  <td>geoids</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>int</td>
  <td></td>
</tr>


</table>

@section WFS_SQ_AVIATION_OBSERVATION_QUERY_HANDLER_EXTRA_CFG_PARAM Additional parameters

This section describes this stored query handler configuration parameters which does not map to
stored query parameters
and must be specified on top level of stored query configuration file when present

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>maxHours</td>
  <td>double</td>
  <td>optional</td>
  <td>Specifies maximal permitted time interval in hours. The default value
      is 168 hours (7 days). This parameter is not valid, if the *optional*
      storedQueryRestrictions parameter is set to false in WFS Plugin
      configurations.</td>
</tr>

<tr>
  <td>returnOnlyLatest</td>
  <td>bool</td>
  <td>optional</td>
  <td>If the paramer value is true only the latest message will be returned
      from the station in a given time interval (starttime,endtime).
      The default value is false.</td>
</tr>

</table>

*/

#endif  // WITHOUT_OBSERVATION
