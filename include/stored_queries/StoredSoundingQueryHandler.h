#pragma once

#ifndef WITHOUT_OBSERVATION

#include "StoredQueryHandlerBase.h"
#include "SupportsBoundingBox.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsLocationParameters.h"
#include "SupportsQualityParameters.h"
#include <engines/geonames/Engine.h>
#include <engines/observation/DBRegistry.h>
#include <engines/observation/Engine.h>
#include <engines/observation/QueryResult.h>
#include <map>
#include <tuple>

// Stored query configuration parameter names.
const std::string P_BEGIN_TIME = "beginTime";
const std::string P_END_TIME = "endTime";
const std::string P_METEO_PARAMETERS = "meteoParameters";
const std::string P_STATION_TYPE = "stationType";
const std::string P_WMOS = "wmos";
const std::string P_FMISIDS = "fmisids";
const std::string P_NUM_OF_STATIONS = "numOfStations";
const std::string P_MISSING_TEXT = "missingText";
const std::string P_CRS = "crs";
const std::string P_LATEST = "latest";
const std::string P_SOUNDING_TYPE = "soundingType";
const std::string P_PUBLICITY = "publicity";
const std::string P_ALTITUDE_RANGES = "altitudeRanges";
const std::string P_PRESSURE_RANGES = "pressureRages";

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredSoundingQueryHandler : protected virtual SupportsExtraHandlerParams,
                                   public StoredQueryHandlerBase,
                                   protected SupportsLocationParameters,
                                   protected SupportsBoundingBox,
                                   protected SupportsQualityParameters
{
  const char* DATA_CRS_NAME = "urn:ogc:def:crs:EPSG::4326";

  using QueryResultShared = std::shared_ptr<SmartMet::Engine::Observation::QueryResult>;
  using ValueVectorConstIt =
      SmartMet::Engine::Observation::QueryResult::ValueVectorType::const_iterator;
  using ParamIdStr = std::string;
  using ParamId = uint64_t;
  using ParamName = std::string;
  using ParamQCName = ParamName;
  using ShowQualityCode = bool;
  using ShowValue = bool;
  using ParamTuple = std::tuple<ParamId, ParamName, ParamQCName, ShowValue, ShowQualityCode>;
  using MeteoParameterMap = std::map<ParamIdStr, ParamTuple>;
  class RadioSounding
  {
   public:
    using Id = int;
    ValueVectorConstIt mStationId;
    ValueVectorConstIt mMessageTime;
    ValueVectorConstIt mLaunchTime;
    ValueVectorConstIt mSoundingEnd;
  };
  using RadioSoundingMap = std::map<RadioSounding::Id, RadioSounding>;

 public:
  StoredSoundingQueryHandler(SmartMet::Spine::Reactor* reactor,
                             boost::shared_ptr<StoredQueryConfig> config,
                             PluginImpl& pluginData,
                             boost::optional<std::string> templateFileName);

  virtual ~StoredSoundingQueryHandler();

  virtual void init_handler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
                     const boost::optional<std::string>& hostname,
                     std::ostream& output) const;

 private:
  virtual void update_parameters(
      const RequestParameterMap& request_params,
      int seq_id,
      std::vector<boost::shared_ptr<RequestParameterMap> >& result) const;

  const std::shared_ptr<SmartMet::Engine::Observation::DBRegistryConfig> dbRegistryConfig(
      const std::string& configName) const;

  std::string solveCrs(const RequestParameterMap& params) const;

  void parseSoundingQuery(const RequestParameterMap& params,
                          const QueryResultShared& soundingQueryResult,
                          RadioSoundingMap& radioSoundingMap) const;

  void makeSoundingQuery(const RequestParameterMap& params,
                         const SmartMet::Spine::Stations& stations,
                         QueryResultShared& soundingQueryResult) const;

  void validateAndPopulateMeteoParametersToMap(const RequestParameterMap& params,
                                               MeteoParameterMap& meteoParameterMap,
                                               std::string& pressureParameterName) const;

  void makeSoundingDataQuery(const RequestParameterMap& params,
                             const RadioSoundingMap& radioSoundingMap,
                             const MeteoParameterMap& meteoParameterMap,
                             QueryResultShared& dataContainer) const;

  void getStationSearchSettings(SmartMet::Engine::Observation::Settings& settings,
                                const RequestParameterMap& params,
                                const std::string& language) const;

  void checkMaxSoundings(const pt::ptime startTime,
                         const pt::ptime& endTime,
                         const RadioSoundingMap& radioSoundingMap) const;

  double mMaxHours;
  uint64_t mMaxSoundings;
  bool mSqRestrictions;
  bool mSupportQCParameters;

  SmartMet::Engine::Observation::Engine* mObservation;
  SmartMet::Engine::Geonames::Engine* mGeonames;
};
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
