#include "stored_queries/StoredGridForecastQueryHandler.h"
#include "FeatureID.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConvenience.h"
#include <boost/algorithm/string.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/format.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TypeName.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiQueryData.h>
#include <smartmet/engines/querydata/MetaQueryOptions.h>
#include <smartmet/spine/Convenience.h>
#include <smartmet/spine/Exception.h>
#include <smartmet/spine/ParameterFactory.h>
#include <smartmet/spine/Table.h>
#include <smartmet/spine/TimeSeriesGenerator.h>
#include <smartmet/spine/TimeSeriesOutput.h>
#include <smartmet/spine/Value.h>
#include <smartmet/spine/HTTP.h>
#include <grid-files/common/AdditionalParameters.h>
#include <grid-files/common/Typedefs.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/GraphFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/identification/GridDef.h>
#include <engines/grid/Engine.h>
#include <limits>
#include <locale>
#include <map>
#include <grid-content/queryServer/definition/QueryConfigurator.h>


namespace ba = boost::algorithm;
namespace pt = boost::posix_time;
namespace lt = boost::local_time;

using boost::format;
using boost::str;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

const char* StoredGridForecastQueryHandler::P_MODEL = "model";
const char* StoredGridForecastQueryHandler::P_ORIGIN_TIME = "originTime";
const char* StoredGridForecastQueryHandler::P_LEVEL_HEIGHTS = "levelHeights";
const char* StoredGridForecastQueryHandler::P_LEVEL = "level";
const char* StoredGridForecastQueryHandler::P_LEVEL_TYPE = "levelType";
const char* StoredGridForecastQueryHandler::P_PARAM = "param";
const char* StoredGridForecastQueryHandler::P_FIND_NEAREST_VALID = "findNearestValid";
const char* StoredGridForecastQueryHandler::P_LOCALE = "locale";
const char* StoredGridForecastQueryHandler::P_MISSING_TEXT = "missingText";
const char* StoredGridForecastQueryHandler::P_CRS = "crs";

namespace
{
struct StationRec
{
    std::string geoid;
    std::string name;
    std::string lat;
    std::string lon;
    std::string elev;
};
}  // namespace



StoredGridForecastQueryHandler::StoredGridForecastQueryHandler(
    Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_impl,
    boost::optional<std::string> template_file_name) :
        SupportsExtraHandlerParams(config, false),
        StoredQueryHandlerBase(reactor, config, plugin_impl, template_file_name),
        SupportsLocationParameters(config, SUPPORT_KEYWORDS | INCLUDE_GEOIDS),
        SupportsTimeParameters(config),
        SupportsTimeZone(config),
        geoEngine(NULL),
        gridEngine(NULL),
        common_params(),
        ind_geoid(add_param(common_params, "geoid", Spine::Parameter::Type::DataIndependent)),
        ind_epoch(add_param(common_params, "time", Spine::Parameter::Type::DataIndependent)),
        ind_place(add_param(common_params, "name", Spine::Parameter::Type::DataIndependent)),
        ind_lat(add_param(common_params, "latitude", Spine::Parameter::Type::DataIndependent)),
        ind_lon(add_param(common_params, "longitude", Spine::Parameter::Type::DataIndependent)),
        ind_elev(add_param(common_params, "elevation", Spine::Parameter::Type::DataIndependent)),
        ind_level(add_param(common_params, "level", Spine::Parameter::Type::DataIndependent)),
        ind_region(add_param(common_params, "region", Spine::Parameter::Type::DataIndependent)),
        ind_country(add_param(common_params, "countryname", Spine::Parameter::Type::DataIndependent)),
        ind_country_iso(add_param(common_params, "iso2", Spine::Parameter::Type::DataIndependent)),
        ind_localtz(add_param(common_params, "localtz", Spine::Parameter::Type::DataIndependent))
{
  try
  {
    register_array_param<std::string>(P_MODEL, *config);
    register_scalar_param<pt::ptime>(P_ORIGIN_TIME, *config, false);
    register_array_param<double>(P_LEVEL_HEIGHTS, *config, 0, 99);
    register_array_param<int64_t>(P_LEVEL, *config, 0, 99);
    register_scalar_param<std::string>(P_LEVEL_TYPE, *config);
    register_array_param<std::string>(P_PARAM, *config, 1, 99);
    register_scalar_param<int64_t>(P_FIND_NEAREST_VALID, *config);
    register_scalar_param<std::string>(P_LOCALE, *config);
    register_scalar_param<std::string>(P_MISSING_TEXT, *config);
    register_scalar_param<std::string>(P_CRS, *config);

    max_np_distance = config->get_optional_config_param<double>("maxNpDistance", -1.0);
    separate_groups = config->get_optional_config_param<bool>("separateGroups", false);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





StoredGridForecastQueryHandler::~StoredGridForecastQueryHandler()
{
}





void StoredGridForecastQueryHandler::init_handler()
{
  try
  {
    auto* reactor = get_reactor();
    void* engine;
    engine = reactor->getSingleton("Geonames", NULL);
    if (engine == NULL)
      throw Spine::Exception(BCP, "No Geonames engine available");

    geoEngine = reinterpret_cast<Engine::Geonames::Engine*>(engine);

    engine = reactor->getSingleton("grid", NULL);
    if (engine == NULL)
      throw Spine::Exception(BCP, "No Grid engine available");

    gridEngine = reinterpret_cast<Engine::Grid::Engine*>(engine);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::query(const StoredQuery& stored_query, const std::string& language, const boost::optional<std::string>& hostname, std::ostream& output) const
{
  try
  {
    namespace pt = boost::posix_time;

    static const long ref_jd = boost::gregorian::date(1970, 1, 1).julian_day();

    int debug_level = get_config()->get_debug_level();
    if (debug_level > 0)
      stored_query.dump_query_info(std::cout);

    StoredForecastQueryHandler::Query query(get_config());

    query.language = language;
    const auto& params = stored_query.get_param_map();

    try
    {
      // Parsing request parameters
      try
      {
        Spine::ValueFormatterParam vf_param;

        query.missing_text = params.get_single<std::string>(P_MISSING_TEXT);
        vf_param.missingText = query.missing_text;
        query.set_locale(params.get_single<std::string>(P_LOCALE));
        query.max_distance = params.get_single<double>(P_MAX_DISTANCE);
        query.max_np_distance = max_np_distance > 0.0 ? max_np_distance : query.max_distance;
        query.keyword = params.get_optional<std::string>(P_KEYWORD, "");
        query.find_nearest_valid_point = params.get_single<int64_t>(P_FIND_NEAREST_VALID) != 0;
        query.tz_name = get_tz_name(params);

        parse_models(params, query);
        get_location_options(geoEngine, params, query.language, &query.locations);

        parse_level_heights(params, query);
        parse_levels(params, query);
        parse_params(params, query);
        query.value_formatter.reset(new Spine::ValueFormatter(vf_param));
        query.time_formatter.reset(Fmi::TimeFormatter::create("iso"));
        parse_times(params, query);

        const std::string crs = params.get_single<std::string>(P_CRS);
        auto transformation = plugin_impl.get_crs_registry().create_transformation("urn:ogc:def:crs:EPSG::4326", crs);
        bool show_height = false;
        std::string proj_uri = "UNKNOWN";
        std::string proj_epoch_uri = "UNKNOWN";
        plugin_impl.get_crs_registry().get_attribute(crs, "showHeight", &show_height);
        plugin_impl.get_crs_registry().get_attribute(crs, "projUri", &proj_uri);
        plugin_impl.get_crs_registry().get_attribute(crs, "projEpochUri", &proj_epoch_uri);

        query.result = extract_forecast(query);
        const std::size_t num_rows = query.result->rows().size();

        std::set<std::string> geo_id_set;
        std::multimap<int, std::string> group_map;
        std::multimap<std::string, std::size_t> result_map;
        for (std::size_t i = 0; i < num_rows; i++)
        {
          const std::string geo_id = query.result->get(ind_geoid, i);
          result_map.insert(std::make_pair(geo_id, i));
          geo_id_set.insert(geo_id);
        }

        std::size_t num_groups = 0;
        for (auto geo_id = geo_id_set.begin(); geo_id != geo_id_set.end(); ++geo_id)
        {
          group_map.insert(std::make_pair(num_groups, *geo_id));
          if (separate_groups)
            num_groups++;
        }

        if (not separate_groups and (geo_id_set.size() > 0))
          num_groups++;

        CTPP::CDT hash;

        hash["language"] = language;

        hash["responseTimestamp"] = Fmi::to_iso_extended_string(get_plugin_impl().get_time_stamp()) + "Z";
        hash["numMatched"] = num_groups;
        hash["numReturned"] = num_groups;
        hash["numParam"] = query.last_data_ind - query.first_data_ind + 1;

        hash["fmi_apikey"] = QueryBase::FMI_APIKEY_SUBST;
        hash["fmi_apikey_prefix"] = QueryBase::FMI_APIKEY_PREFIX_SUBST;
        hash["hostname"] = QueryBase::HOSTNAME_SUBST;
        hash["protocol"] = QueryBase::PROTOCOL_SUBST;
        hash["srsName"] = proj_uri;
        hash["srsDim"] = show_height ? 3 : 2;
        hash["srsEpochName"] = proj_epoch_uri;
        hash["srsEpochDim"] = show_height ? 4 : 3;

        int sq_id = stored_query.get_query_id();
        const char* location_params[] =
        { P_PLACES, P_LATLONS, P_GEOIDS, P_KEYWORD };

        for (std::size_t group_id = 0; group_id < num_groups; group_id++)
        {
          std::size_t station_ind = 0;
          auto site_range = group_map.equal_range(group_id);
          CTPP::CDT& group = hash["groups"][group_id];

          FeatureID feature_id(get_config()->get_query_id(), params.get_map(), sq_id);
          for (std::size_t i = 0; i < sizeof(location_params) / sizeof(*location_params); i++)
          {
            feature_id.erase_param(location_params[i]);
          }
          auto geoid_range = group_map.equal_range(group_id);
          for (auto it = geoid_range.first; it != geoid_range.second; ++it)
          {
            feature_id.add_param(P_GEOIDS, it->second);
          }
          group["featureId"] = feature_id.get_id();

          group["groupId"] = str(format("%1%-%2%") % sq_id % (group_id + 1));
          if (query.origin_time)
          {
            const std::string origin_time_str = Fmi::to_iso_extended_string(*query.origin_time) + "Z";
            group["dataOriginTime"] = origin_time_str;
            group["resultTime"] = origin_time_str;
          }

          for (std::size_t i = query.first_data_ind; i <= query.last_data_ind; i++)
          {
            const std::string& name = query.data_params.at(i).name();
            feature_id.erase_param(P_PARAM);
            feature_id.add_param(P_PARAM, name);
            group["paramList"][i - query.first_data_ind]["name"] = name;
            group["paramList"][i - query.first_data_ind]["featureId"] = feature_id.get_id();
          }

          group["metadata"]["boundingBox"]["lowerLeft"]["lat"] = query.bottom_left.Y();
          group["metadata"]["boundingBox"]["lowerLeft"]["lon"] = query.bottom_left.X();

          group["metadata"]["boundingBox"]["lowerRight"]["lat"] = query.bottom_right.Y();
          group["metadata"]["boundingBox"]["lowerRight"]["lon"] = query.bottom_right.X();

          group["metadata"]["boundingBox"]["upperLeft"]["lat"] = query.top_left.Y();
          group["metadata"]["boundingBox"]["upperLeft"]["lon"] = query.top_left.X();

          group["metadata"]["boundingBox"]["upperRight"]["lat"] = query.top_right.Y();
          group["metadata"]["boundingBox"]["upperRight"]["lon"] = query.top_right.X();

          group["producerName"] = query.producer_name;
#ifdef ENABLE_MODEL_PATH
          group["modelPath"] = query.model_path;
#endif

          std::size_t row_counter = 0;

          pt::ptime interval_begin = boost::date_time::pos_infin;
          pt::ptime interval_end = boost::date_time::neg_infin;

          for (auto site_iter = site_range.first; site_iter != site_range.second; ++site_iter)
          {
            const auto& geo_id = site_iter->second;
            auto row_range = result_map.equal_range(geo_id);
            lt::time_zone_ptr tzp;
            for (auto row_iter = row_range.first; row_iter != row_range.second; ++row_iter)
            {
              std::size_t i = row_iter->second;
              if (row_iter == row_range.first)
              {
                const std::string name = query.result->get(ind_place, i);
                const std::string country_code = query.result->get(ind_country_iso, i);
                const std::string country = query.result->get(ind_country, i);
                const std::string region = query.result->get(ind_region, i);
                const std::string localtz = query.result->get(ind_localtz, i);
                const double latitude = Fmi::stod(query.result->get(ind_lat, i));
                const double longitude = Fmi::stod(query.result->get(ind_lon, i));
                tzp = get_tz_for_site(longitude, latitude, query.tz_name);
                group["stationList"][station_ind]["geoid"] = geo_id;
                group["stationList"][station_ind]["name"] = name;
                set_2D_coord(transformation, latitude, longitude, group["stationList"][station_ind]);
                if (show_height)
                {
                  group["stationList"][station_ind]["elev"] = query.result->get(ind_elev, i);
                  ;
                }

                group["stationList"][station_ind]["iso2"] = country_code;
                group["stationList"][station_ind]["country"] = country;
                group["stationList"][station_ind]["localtz"] = localtz;
                if ((country_code == "FI") or (name != region))
                {
                  group["stationList"][station_ind]["region"] = region;
                }

                station_ind++;
              }

              CTPP::CDT& row_data = group["returnArray"][row_counter++];

              set_2D_coord(transformation, query.result->get(ind_lat, i), query.result->get(ind_lon, i), row_data);

              if (show_height)
              {
                row_data["elev"] = query.result->get(ind_level, i);
              }

              pt::ptime epoch = pt::from_iso_string(query.result->get(ind_epoch, i));
              long long jd = epoch.date().julian_day();
              long seconds = epoch.time_of_day().total_seconds();
              INT_64 s_epoch = 86400LL * (jd - ref_jd) + seconds;
              row_data["epochTime"] = s_epoch;
              row_data["epochTimeStr"] = format_local_time(epoch, tzp);

              for (std::size_t k = query.first_data_ind; k <= query.last_data_ind; k++)
              {
                row_data["data"][k - query.first_data_ind] = remove_trailing_0(query.result->get(k, i));
              }

              interval_begin = i == 0 ? epoch : std::min(interval_begin, epoch);
              interval_end = i == 0 ? epoch : std::max(interval_end, epoch);
            }
          }

          group["phenomenonStartTime"] = Fmi::to_iso_extended_string(interval_begin) + "Z";
          group["phenomenonEndTime"] = Fmi::to_iso_extended_string(interval_end) + "Z";
        }

        //printf("BBOX %f,%f  %f,%f  %f,%f  %f,%f\n", query.top_left.X(), query.top_left.Y(), query.top_right.X(), query.top_right.Y(), query.bottom_left.X(), query.bottom_left.Y(),
        //    query.bottom_right.X(), query.bottom_right.Y());

        format_output(hash, output, stored_query.get_use_debug_format());
      }
      catch (...)
      {
        Spine::Exception exception(BCP, "Operation parsing failed!", NULL);
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
    catch (...)
    {
      Spine::Exception exception(BCP, "Operation parsing failed!", NULL);
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == NULL)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      exception.addParameter(WFS_LANGUAGE, query.language);
      throw exception;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





uint StoredGridForecastQueryHandler::processGridQuery(
    Query& wfsQuery,
    const std::string& tag,
    const Spine::LocationPtr loc,
    std::string country,
    QueryServer::Query& gridQuery,
    Table_sptr output,
    uint rowCount) const
{
  try
  {
    std::shared_ptr < ContentServer::ServiceInterface > contentServer = gridEngine->getContentServer_sptr();
    Spine::TimeSeries::Value missing_value = Spine::TimeSeries::None();
    std::string timezoneName = loc->timezone;
    boost::local_time::time_zone_ptr localtz = itsTimezones.time_zone_from_string(loc->timezone);
    boost::local_time::time_zone_ptr tz = localtz;
    uint lastRow = rowCount;
    uint generationId = 0;
    uint geometryId = 0;

    if (wfsQuery.tz_name != "localtime")
    {
      timezoneName = wfsQuery.tz_name;
      tz = itsTimezones.time_zone_from_string(timezoneName);
    }

    if (itsProducerInfoList.getLength() == 0)
    {
      contentServer->getProducerInfoList(0, itsProducerInfoList);
      itsProducerInfoList_updateTime = time(nullptr);
    }

    if (itsGenerationInfoList.getLength() == 0)
    {
      contentServer->getGenerationInfoList(0, itsGenerationInfoList);
      itsGenerationInfoList_updateTime = time(nullptr);
    }

    AdditionalParameters additionalParameters(itsTimezones, *wfsQuery.output_locale, *wfsQuery.time_formatter, *wfsQuery.value_formatter);

    int result = gridEngine->executeQuery(gridQuery);
    if (result != 0)
    {
      Spine::Exception exception(BCP, "The query server returns an error message!");
      exception.addParameter("Result", std::to_string(result));
      exception.addParameter("Message", QueryServer::getResultString(result));

      switch (result)
      {
        case QueryServer::Result::NO_PRODUCERS_FOUND:
          exception.addDetail("The reason for this situation is usually that the given producer is unknown");
          exception.addDetail("or there are no producer list available in the grid engine's configuration file.");
          break;
      }
      throw exception;
    }

       // Going through all parameters

    int pLen = (int) gridQuery.mQueryParameterList.size();
    for (int p = 0; p < pLen; p++)
    {
       // Counting the number of values that the parameter can have in single timestep.

      uint vLen = 0;
      if ((int) gridQuery.mQueryParameterList.size() > p && gridQuery.mQueryParameterList[p].mValueList.size() > 0)
      {
        // ### Going through all timesteps.

        uint timestepCount = gridQuery.mQueryParameterList[p].mValueList.size();
        for (uint x = 0; x < timestepCount; x++)
        {
          uint vv = (uint) gridQuery.mQueryParameterList[p].mValueList[x].mValueList.getLength();
          if (vv > vLen)
            vLen = vv;
        }
      }

      uint rLen = (uint) gridQuery.mQueryParameterList[p].mValueList.size();
      uint tLen = (uint) gridQuery.mForecastTimeList.size();

      uint col = p;
      uint rows = rowCount;

      if (vLen > 0 && rLen <= tLen)
      {
        // The query has returned at least some values for the parameter.

        for (uint v = 0; v < vLen; v++)
        {
          for (uint t = 0; t < tLen; t++)
          {
            if (generationId == 0)
              generationId = gridQuery.mQueryParameterList[p].mValueList[t].mGenerationId;

            uint row = rows + t;
            if (row > lastRow)
              lastRow = row;

            if (!wfsQuery.have_model_area)
            {
              if (geometryId == 0  &&  gridQuery.mQueryParameterList[p].mValueList[t].mGeometryId != 0)
                geometryId = gridQuery.mQueryParameterList[p].mValueList[t].mGeometryId;
            }

            T::GridValue val;
            if (gridQuery.mQueryParameterList[p].mValueList[t].mValueList.getGridValueByIndex(v,val) && (val.mValue != ParamValueMissing || val.mValueString.length() > 0))
            {
              if (val.mValueString.length() > 0)
              {
                // The parameter value is a string
                if (vLen == 1)
                {
                  // The parameter has only single value in the timestep
                  output->set(col, row, val.mValueString);
                }
                else
                {
                  // The parameter has multiple values in the same timestep
                }
              }
              else
              {
                // The parameter value is numeric
                if (vLen == 1)
                {
                  // The parameter has only single value in the timestep

                  int precision = 6;
                  if (gridQuery.mQueryParameterList[p].mPrecision >= 0)
                    precision = gridQuery.mQueryParameterList[p].mPrecision;

                  std::string ss = wfsQuery.value_formatter->format(val.mValue,precision);
                  output->set(col, row, ss);
                  //output->set(col, row, std::to_string(val->mValue));
                }
                else
                {
                  // The parameter has multiple values in the same timestep
                }
              }
            }
            else
            {
              // The parameter value is missing
              if (vLen == 1)
              {
                // The parameter has only single value in the timestep
                output->set(col, row, wfsQuery.missing_text);
              }
              else
              {
                // The parameter has multiple values in the same timestep
              }
            }
          }
        }
      }
      else
      {
        // The query has returned no values for the parameter. This usually means that the parameter
        // is not a data parameter. It is most likely "a special parameter" that is based on the given
        // query location, time, level, etc.

        std::string paramValue;
        uint tLen = (uint) gridQuery.mForecastTimeList.size();
        uint t = 0;
        for (auto ft = gridQuery.mForecastTimeList.begin(); ft != gridQuery.mForecastTimeList.end(); ++ft)
        {
          uint row = rows + t;
          if (row > lastRow)
            lastRow = row;

          boost::local_time::local_date_time queryTime(boost::posix_time::from_iso_string(*ft), tz);
          if (additionalParameters.getParameterValueByLocation(gridQuery.mQueryParameterList[p].mParam, tag, loc, country, gridQuery.mQueryParameterList[p].mPrecision,paramValue))
          {
            output->set(col, row, paramValue);
          }
          else
          if (additionalParameters.getParameterValueByLocationAndTime(gridQuery.mQueryParameterList[p].mParam, tag, loc, queryTime, tz,gridQuery.mQueryParameterList[p].mPrecision, paramValue))
          {
            output->set(col, row, paramValue);
          }
          else
          if (gridQuery.mQueryParameterList[p].mParam == "level")
          {
            int idx = 0;

            int levelValue = 0;
            while (idx < pLen && levelValue == 0)
            {
              if (gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel > 0)
              {
                levelValue = gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevel;
                if (gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelIdType == T::ParamLevelIdTypeValue::FMI
                    && gridQuery.mQueryParameterList[idx].mValueList[t].mParameterLevelId == 2)
                  levelValue = levelValue / 100;
              }

              idx++;
            }

            output->set(col, row, std::to_string(levelValue));
          }
          else if (gridQuery.mQueryParameterList[p].mParam == "model" || gridQuery.mQueryParameterList[p].mParam == "producer")
          {
            int idx = 0;
            while (idx < pLen)
            {
              if (gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId > 0)
              {
                T::ProducerInfo *info = itsProducerInfoList.getProducerInfoById(gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                if (info == NULL)
                {
                  contentServer->getProducerInfoList(0, itsProducerInfoList);
                  itsProducerInfoList_updateTime = time(nullptr);
                  info = itsProducerInfoList.getProducerInfoById(gridQuery.mQueryParameterList[idx].mValueList[t].mProducerId);
                }

                if (info != NULL)
                {
                  output->set(col, row, info->mName);
                  idx = pLen + 10;
                }
              }
              idx++;
            }

            if (idx == pLen)
            {
              std::string producer = "Unknown";
              if (gridQuery.mProducerNameList.size() == 1)
              {
                std::vector<std::string> pnameList;
                gridEngine->getProducerNameList(gridQuery.mProducerNameList[0], pnameList);
                if (pnameList.size() > 0)
                  producer = pnameList[0];
              }

              output->set(col, row, producer);
            }
          }
          else if (gridQuery.mQueryParameterList[p].mParam == "origintime")
          {
            int idx = 0;
            while (idx < pLen)
            {
              if (gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId > 0)
              {
                T::GenerationInfo *info = itsGenerationInfoList.getGenerationInfoById(gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                if (info == NULL && (itsGenerationInfoList_updateTime + 60) > time(nullptr))
                {
                  contentServer->getGenerationInfoList(0, itsGenerationInfoList);
                  itsGenerationInfoList_updateTime = time(nullptr);
                  info = itsGenerationInfoList.getGenerationInfoById(gridQuery.mQueryParameterList[idx].mValueList[t].mGenerationId);
                }

                if (info != NULL)
                {
                  boost::local_time::local_date_time origTime(boost::posix_time::from_iso_string(info->mAnalysisTime), tz);
                  output->set(col, row, wfsQuery.time_formatter->format(origTime));
                  idx = pLen + 10;
                }
              }
              idx++;
            }
            if (idx == pLen)
            {
              output->set(col, row, std::string("Unknown"));
            }
          }
          else if (rLen > tLen)
          {
            // It seems that there are more values available for each time step than expected.
            // This usually happens if the requested parameter definition does not contain enough
            // information in order to identify an unique parameter. For example, if the parameter
            // definition does not contain level information then the result is that we get
            // values from several levels.

            char tmp[10000];
            std::set<std::string> pList;

            for (uint r = 0; r < rLen; r++)
            {
              if (gridQuery.mQueryParameterList[p].mValueList[r].mForecastTime == *ft)
              {
                std::string producerName;
                T::ProducerInfo *producer = itsProducerInfoList.getProducerInfoById(gridQuery.mQueryParameterList[p].mValueList[r].mProducerId);
                if (producer != NULL)
                  producerName = producer->mName;

                sprintf(tmp, "%s:%d:%d:%d:%d:%s", gridQuery.mQueryParameterList[p].mValueList[r].mParameterKey.c_str(),
                    (int) gridQuery.mQueryParameterList[p].mValueList[r].mParameterLevelId, (int) gridQuery.mQueryParameterList[p].mValueList[r].mParameterLevel,
                    (int) gridQuery.mQueryParameterList[p].mValueList[r].mForecastType, (int) gridQuery.mQueryParameterList[p].mValueList[r].mForecastNumber,
                    producerName.c_str());

                if (pList.find(std::string(tmp)) == pList.end())
                  pList.insert(std::string(tmp));
              }
            }
            char *pp = tmp;
            pp += sprintf(pp, "### MULTI-MATCH ### ");
            for (auto p = pList.begin(); p != pList.end(); ++p)
              pp += sprintf(pp, "%s ", p->c_str());

            output->set(col, row, std::string(tmp));
          }
          else
          if (!additionalParameters.isAdditionalParameter(gridQuery.mQueryParameterList[p].mParam.c_str()))
          {
            // This is a normal data parameter, but the query has not return any values for it.
            output->set(col, row, wfsQuery.missing_text);
          }

          t++;
        }
      }
    }


    if (generationId > 0)
    {
      T::GenerationInfo *info = itsGenerationInfoList.getGenerationInfoById(generationId);
      if (info != NULL)
      {
        //boost::local_time::local_date_time origTime(boost::posix_time::from_iso_string(info->mAnalysisTime), tz);
        wfsQuery.origin_time.reset(new pt::ptime(boost::posix_time::from_iso_string(info->mAnalysisTime)));
      }
    }


    if (geometryId > 0)
    {
      T::Coordinate topLeft;
      T::Coordinate topRight;
      T::Coordinate bottomLeft;
      T::Coordinate bottomRight;

      if (Identification::gridDef.getGridLatLonAreaByGeometryId(geometryId, topLeft, topRight, bottomLeft, bottomRight))
      {
        wfsQuery.have_model_area = true;
        wfsQuery.top_left.Set(topLeft.x(), topLeft.y());
        wfsQuery.top_right.Set(topRight.x(), topRight.y());
        wfsQuery.bottom_left.Set(bottomLeft.x(), bottomLeft.y());
        wfsQuery.bottom_right.Set(bottomRight.x(), bottomRight.y());
      }
    }

    return lastRow + 1;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





Table_sptr StoredGridForecastQueryHandler::extract_forecast(Query& wfsQuery) const
{
  try
  {
    QueryServer::QueryConfigurator queryConfigurator;

    int debug_level = get_config()->get_debug_level();

    wfsQuery.have_model_area = false;
    Table_sptr output(new Spine::Table);

    uint rowCount = 0;
    for (auto tloc = wfsQuery.locations.begin(); tloc != wfsQuery.locations.end(); ++tloc)
    {
      T::AttributeList attributeList;

      Spine::LocationPtr loc = tloc->second;
      const std::string place = tloc->second->name;
      const std::string country = geoEngine->countryName(loc->iso2, wfsQuery.language);

      if (debug_level > 0)
        std::cout << "Location: " << loc->name << " in " << country << std::endl;


      attributeList.addAttribute("type",std::to_string(QueryServer::QueryParameter::Type::PointValues));

      attributeList.addAttribute("locationType",std::to_string(QueryServer::QueryParameter::LocationType::Point));

      std::vector<std::vector<T::Coordinate>> areaCoordinates;
      std::vector<T::Coordinate> coordinates;
      coordinates.push_back(T::Coordinate(loc->longitude, loc->latitude));
      areaCoordinates.push_back(coordinates);
      std::string coordinateStr = toString(areaCoordinates,',',';');
      attributeList.addAttribute("coordinates",coordinateStr);

      if (!wfsQuery.language.empty())
        attributeList.addAttribute("language",wfsQuery.language);

      if (wfsQuery.models.size() > 0)
      {
        std::string producers = toString(wfsQuery.models,',');
        attributeList.addAttribute("producers",producers);
      }

      if (wfsQuery.levels.size() > 0)
      {
        std::string levels = toString(wfsQuery.levels,',');
        attributeList.addAttribute("levels",levels);
      }

      if (strcasecmp(wfsQuery.level_type.c_str(), "pressure") == 0)
      {
        attributeList.addAttribute("levelId","2");
      }

      if (strcasecmp(wfsQuery.level_type.c_str(), "hybrid") == 0)
      {
        attributeList.addAttribute("levelId","3");
      }

      if (wfsQuery.level_heights.size() > 0)
      {
        std::string heights = toString(wfsQuery.level_heights,',');
        attributeList.addAttribute("heights",heights);
      }

      if (!wfsQuery.level_type.empty())
        attributeList.addAttribute("levelId",wfsQuery.level_type);

      std::string timezoneName = loc->timezone;
      if (wfsQuery.tz_name != "localtime")
        timezoneName = wfsQuery.tz_name;

      attributeList.addAttribute("timezone",timezoneName);

      attributeList.addAttribute("starttime",to_iso_string(wfsQuery.toptions->startTime));

      attributeList.addAttribute("endtime",to_iso_string(wfsQuery.toptions->endTime));

      if (wfsQuery.toptions->timeSteps)
        attributeList.addAttribute("timesteps",std::to_string(*wfsQuery.toptions->timeSteps));

      if (wfsQuery.toptions->timeStep)
        attributeList.addAttribute("timestep",std::to_string(*wfsQuery.toptions->timeStep));

      char tmp[10000];
      tmp[0] = '\0';
      char *p = tmp;

      for (auto param = wfsQuery.data_params.begin(); param != wfsQuery.data_params.end(); ++param)
      {
        if (param != wfsQuery.data_params.begin())
          p += sprintf(p,",");

        std::string paramName = param->name();
        std::string name = paramName;

        for (auto it = wfsQuery.models.begin(); it != wfsQuery.models.end(); ++it)
        {
          std::string producerName = *it;

          Engine::Grid::ParameterDetails_vec parameters;

          std::string key = producerName + ";" + paramName;

          gridEngine->getParameterDetails(producerName,paramName,parameters);

          size_t len = parameters.size();
          if (len > 0  &&  strcasecmp(parameters[0].mProducerName.c_str(),key.c_str()) != 0)
            name = paramName + ":" + parameters[0].mProducerName + ":" + parameters[0].mGeometryId + ":" + parameters[0].mLevelId[0] + ":" + parameters[0].mLevel[0] + ":" + parameters[0].mForecastType[0] + ":" + parameters[0].mForecastNumber[0];
          else
            name = paramName;
        }

        p += sprintf(p,"%s",name.c_str());
      }

      attributeList.addAttribute("param",tmp);

      QueryServer::Query query;
      //attributeList.print(std::cout,0,0);
      queryConfigurator.configure(query,attributeList);

      //query.print(std::cout,0,0);

      rowCount = processGridQuery(wfsQuery,tloc->first,loc,country,query,output,rowCount);
    }
/*

    printf("*******************\n");
    auto cols = output->columns();
    auto rows = output->rows();

    for (auto r = rows.begin(); r != rows.end(); ++r)
    {
      for (auto c = cols.begin(); c != cols.end(); ++c)
      {
        std::cout << output->get(*c, *r) << ";";
      }
      std::cout << "\n";
    }
*/
    return output;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::parse_models(const RequestParameterMap& params, StoredForecastQueryHandler::Query& dest) const
{
  try
  {
    dest.models.clear();
    params.get<std::string>(P_MODEL, std::back_inserter(dest.models));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::parse_level_heights(const RequestParameterMap& params, Query& dest) const
{
  try
  {
    dest.level_heights.clear();
    std::vector<double> heights;
    params.get<double>(P_LEVEL_HEIGHTS, std::back_inserter(heights));
    for (const auto& item : heights)
    {
      float tmp = static_cast<float>(item);
      if (!dest.level_heights.insert(tmp).second)
      {
        Spine::Exception exception(BCP, "Duplicate geometric height!");
        exception.addParameter("height",std::to_string(tmp));
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Geometric height parsing failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::parse_levels(const RequestParameterMap& params, Query& dest) const
{
  try
  {
    dest.levels.clear();

    dest.level_type = params.get_single<std::string>(P_LEVEL_TYPE);

    std::vector<int64_t> levels;
    params.get<int64_t>(P_LEVEL, std::back_inserter(levels));
    for (auto level = levels.begin(); level != levels.end(); ++level)
    {
      int tmp = cast_int_type<int>(*level);
      if (!dest.levels.insert(tmp).second)
      {
        Spine::Exception exception(BCP, "Duplicate level!");
        exception.addParameter("level",std::to_string(tmp));
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::parse_times(const RequestParameterMap& param, Query& dest) const
{
  try
  {
    dest.toptions = get_time_generator_options(param);

    if (param.count(P_ORIGIN_TIME) > 0)
      dest.origin_time.reset(new pt::ptime(param.get_single<pt::ptime>(P_ORIGIN_TIME)));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}





void StoredGridForecastQueryHandler::parse_params(const RequestParameterMap& param, Query& dest) const
{
  try
  {
    using Spine::Parameter;
    using Spine::ParameterFactory;

    dest.data_params = common_params;

    std::vector<std::string> names;
    param.get<std::string>(P_PARAM, std::back_inserter(names));
    for (auto name = names.begin(); name != names.end(); ++name)
    {
      std::size_t ind = dest.data_params.size();
      FmiParameterName number = kFmiBadParameter;
      dest.data_params.push_back(Parameter(*name, Parameter::Type::Data, number));

      if (dest.first_data_ind == 0)
        dest.first_data_ind = ind;

      dest.last_data_ind = ind;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}




} // WFS
} // Plugin
} // Smartmet



namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_grid_forecast_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_impl,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredGridForecastQueryHandler* qh = new StoredGridForecastQueryHandler(reactor, config, plugin_impl, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> result(qh);
    result->init_handler();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_grid_forecast_handler_factory(&wfs_grid_forecast_handler_create);

/**

 @page WFS_SQ_GRID_FORECAST_QUERY_HANDLER Stored Query handler for querying weather forecast data

 @section WFS_SQ_GRID_FORECAST_QUERY_HANDLER_INTRO Introduction

 This stored query handler provides support of querying weather forecast data from
 <a href="../../../engines/querydata/index.html">Querydata</a>

 <table border="1">
 <tr>
 <td>Implementation</td>
 <td>SmartMet::Plugin::WFS::StoredGridForecastQueryHandler</td>
 </tr>
 <tr>
 <td>constructor name (for stored query configuration)</td>
 <td>@b wfs_forecast_handler_factory</td>
 </table>

 @section WFS_SQ_GRID_FORECAST_QUERY_HANDLER_PARAMS Query handler built-in parameters

 The following stored query handler parameter groups are being used by this stored query handler:
 - @ref WFS_SQ_LOCATION_PARAMS
 - @ref WFS_SQ_TIME_PARAMS
 - @ref WFS_SQ_TIME_ZONE

 Additionally to parameters from these groups the following parameters are also in use

 <table border="1">

 <tr>
 <th>Entry name</th>
 <th>Type</th>
 <th>Data type</th>
 <th>Description</th>
 </tr>

 <tr>
 <td>model</td>
 <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
 <td>string</td>
 <td>Specifies weather models to use. Specifying an empty array allows to use all models
 available to Querydata</td>
 </tr>

 <tr>
 <td>originTime</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>time</td>
 <td>Specifies origin time of weather models to use when specified. May be omitted in query.
 All available origin time values are acceptable when omitted</td>
 </tr>

 <tr>
 <td>level</td>
 <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
 <td>integer</td>
 <td>Model levels to query.</td>
 </tr>

 <tr>
 <td>levelHeights</td>
 <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
 <td>doulbeList</td>
 <td>Arbitrary level heighs to query.</td>
 </tr>

 <tr>
 <td>levelType</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>string</td>
 <td>Specifies model level typo to use/td>
 </tr>

 <tr>
 <td>findNearestValid</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>integer</td>
 <td>Specifying non zero value causes look-up of nearest point from model</td>
 </tr>

 <tr>
 <td>locale</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>string</td>
 <td>Specifies locale to use (for example @b fi_FI.utf8 )</td>
 </tr>

 <tr>
 <td>missingText</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>string</td>
 <td>Specifies text to use for missing values</td>
 </tr>

 <tr>
 <td>crs</td>
 <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
 <td>string</td>
 <td>Specifies coordinate projection to use</td>
 </tr>

 </table>

 @section WFS_SQ_GENERIC_OBS_EXTRA_CFG_PARAM Additional parameters

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
 <td>max_np_distance</td>
 <td>double</td>
 <td>optional</td>
 <td>Specifies maximal distance (meters) for looking up nearest valid point. Only matters if
 query handler parameter @b findNearestValid is non zero.</td>
 </tr>

 <tr>
 <td>separateGroups</td>
 <td>boolean</td>
 <td>optional (default false)</td>
 <td>Specifying @b true causes separate response group to be generated for each site/td>
 </tr>

 </table>


 */
