#pragma once

#include "StoredForecastQueryHandler.h"
#include <grid-files/common/AdditionalParameters.h>
#include <engines/grid/Engine.h>
#include <engines/gis/GeometryStorage.h>
#include <newbase/NFmiSvgTools.h>
#include <newbase/NFmiSvgPath.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

typedef StoredForecastQueryHandler::Query Query;
typedef std::map<std::string, Engine::Querydata::ModelParameter> ModelParameters;
typedef boost::shared_ptr<Spine::Table> Table_sptr;



class StoredGridForecastQueryHandler: public StoredQueryHandlerBase,
    protected virtual SupportsExtraHandlerParams,
    protected SupportsLocationParameters,
    protected SupportsTimeParameters,
    protected SupportsTimeZone
{

  public:

                StoredGridForecastQueryHandler(
                    Spine::Reactor* reactor,
                    boost::shared_ptr<StoredQueryConfig> config,
                    PluginImpl& plugin_impl,
                    boost::optional<std::string> template_file_name);

    virtual     ~StoredGridForecastQueryHandler();

    void        init_handler();
    void        query(const StoredQuery& query, const std::string& language, const boost::optional<std::string>& hostname, std::ostream& output) const;

  protected:


    void        parse_models(const RequestParameterMap& params, Query& dest) const;
    void        parse_level_heights(const RequestParameterMap& param, Query& dest) const;
    void        parse_levels(const RequestParameterMap& param, Query& dest) const;
    void        parse_times(const RequestParameterMap& param, Query& dest) const;
    void        parse_params(const RequestParameterMap& param, Query& dest) const;

    Table_sptr  extract_forecast(Query& query) const;

    uint        processGridQuery(
                    Query& wfsQuery,
                    const std::string& tag,
                    const Spine::LocationPtr loc,
                    std::string country,
                    QueryServer::Query& gridQuery,
                    Table_sptr output,
                    uint rowCount) const;

  private:

    Engine::Geonames::Engine*     geoEngine;
    Engine::Grid::Engine*         gridEngine;

    Engine::Gis::GeometryStorage  itsGeometryStorage;
    Fmi::TimeZones                itsTimezones;
    mutable string_vec            itsProducerList;
    mutable time_t                itsProducerList_updateTime;
    mutable T::ProducerInfoList   itsProducerInfoList;
    mutable time_t                itsProducerInfoList_updateTime;
    mutable T::GenerationInfoList itsGenerationInfoList;
    mutable time_t                itsGenerationInfoList_updateTime;

    std::vector<Spine::Parameter> common_params;
    double                        max_np_distance;
    bool                          separate_groups;

    std::size_t                   ind_geoid;
    std::size_t                   ind_epoch;
    std::size_t                   ind_place;
    std::size_t                   ind_lat;
    std::size_t                   ind_lon;
    std::size_t                   ind_elev;
    std::size_t                   ind_level;
    std::size_t                   ind_region;
    std::size_t                   ind_country;
    std::size_t                   ind_country_iso;
    std::size_t                   ind_localtz;

    static const char*            P_MODEL;
    static const char*            P_ORIGIN_TIME;
    static const char*            P_LEVEL_HEIGHTS;
    static const char*            P_LEVEL;
    static const char*            P_LEVEL_TYPE;
    static const char*            P_PARAM;
    static const char*            P_FIND_NEAREST_VALID;
    static const char*            P_LOCALE;
    static const char*            P_MISSING_TEXT;
    static const char*            P_CRS;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
