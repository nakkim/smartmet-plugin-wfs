#include "stored_queries/StoredCoverageQueryHandler.h"
#include <gis/Box.h>
#include <newbase/NFmiEnumConverter.h>

#include <boost/algorithm/string/replace.hpp>
#include <iomanip>

namespace bw = SmartMet::Plugin::WFS;

bw::StoredCoverageQueryHandler::StoredCoverageQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<bw::StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredContourQueryHandler(reactor, config, plugin_data, template_file_name)
{
  try
  {
    if (config->find_setting(config->get_root(), "contour_param.limits", false))
      itsLimits = config->get_mandatory_config_array<double>("contour_param.limits");
    itsUnit = config->get_optional_config_param<std::string>("contour_param.unit", "");

    // check number of limits
    if ((itsLimits.size() & 1) != 0)
    {
      SmartMet::Spine::Exception exception(BCP, "Invalid parameter value!");
      exception.addDetail(
          "Parameter 'contour_params.limits' must contain even amount of decimal numbers.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredCoverageQueryHandler::clipGeometry(OGRGeometryPtr& pGeom, Fmi::Box& bbox) const
{
  try
  {
    if (pGeom && !pGeom->IsEmpty())
      pGeom.reset(Fmi::OGR::polyclip(*pGeom, bbox));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<bw::ContourQueryResultPtr> bw::StoredCoverageQueryHandler::processQuery(
    ContourQueryParameter& queryParameter) const
{
  try
  {
    std::vector<ContourQueryResultPtr> query_results;

    std::vector<double> limits;

    std::vector<SmartMet::Engine::Contour::Range>& query_limits =
        (reinterpret_cast<CoverageQueryParameter&>(queryParameter)).limits;
    if (query_limits.size() > 0)
    {
      for (auto l : query_limits)
      {
        double lolimit = DBL_MIN;
        double hilimit = DBL_MAX;
        if (l.lolimit)
          lolimit = *l.lolimit;
        if (l.hilimit)
          hilimit = *l.hilimit;
        limits.push_back(lolimit);
        limits.push_back(hilimit);
      }
    }
    else
    {
      limits = itsLimits;
      unsigned int numLimits(limits.size() / 2);
      for (std::size_t i = 0; i < numLimits; i++)
      {
        std::size_t limitsIndex(i * 2);
        double lolimit = limits[limitsIndex];
        double hilimit = limits[limitsIndex + 1];
        query_limits.push_back(SmartMet::Engine::Contour::Range(lolimit, hilimit));
      }
    }

    // contains result for all coverages
    ContourQueryResultSet result_set = getContours(queryParameter);

    // order result set primarily by isoline value and secondarily by timestep
    unsigned int max_geoms_per_timestep = 0;
    for (auto result_item : result_set)
      if (max_geoms_per_timestep < result_item->area_geoms.size())
        max_geoms_per_timestep = result_item->area_geoms.size();

    for (std::size_t i = 0; i < max_geoms_per_timestep; i++)
    {
      for (auto result_item : result_set)
        if (i < result_item->area_geoms.size())
        {
          CoverageQueryResultPtr result(new CoverageQueryResult);
          WeatherAreaGeometry wag = result_item->area_geoms[i];
          std::size_t limitsIndex(i * 2);
          result->lolimit = limits[limitsIndex];
          result->hilimit = limits[limitsIndex + 1];
          result->name = queryParameter.parameter.name();
          result->unit = itsUnit;
          result->area_geoms.push_back(wag);
          query_results.push_back(result);
        }
    }

    return query_results;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Engine::Contour::Options bw::StoredCoverageQueryHandler::getContourEngineOptions(
    const boost::posix_time::ptime& time, const ContourQueryParameter& queryParameter) const
{
  return SmartMet::Engine::Contour::Options(
      queryParameter.parameter,
      time,
      (reinterpret_cast<const CoverageQueryParameter&>(queryParameter)).limits);
}

boost::shared_ptr<bw::ContourQueryParameter> bw::StoredCoverageQueryHandler::getQueryParameter(
    const SmartMet::Spine::Parameter& parameter,
    const SmartMet::Engine::Querydata::Q& q,
    OGRSpatialReference& sr) const
{
  try
  {
    boost::shared_ptr<bw::ContourQueryParameter> ret(new CoverageQueryParameter(parameter, q, sr));

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredCoverageQueryHandler::setResultHashValue(CTPP::CDT& resultHash,
                                                        const ContourQueryResult& resultItem) const
{
  try
  {
    const CoverageQueryResult& coverageResultItem =
        reinterpret_cast<const CoverageQueryResult&>(resultItem);

    resultHash["name"] = coverageResultItem.name;
    if (!coverageResultItem.unit.empty())
      resultHash["unit"] = coverageResultItem.unit;
    resultHash["lovalue"] = coverageResultItem.lolimit;
    resultHash["hivalue"] = coverageResultItem.hilimit;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_coverage_query_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginData& plugin_data,
    boost::optional<std::string> template_file_name)
{
  StoredCoverageQueryHandler* qh =
      new StoredCoverageQueryHandler(reactor, config, plugin_data, template_file_name);
  boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> result(qh);
  result->init_handler();
  return result;
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_coverage_query_handler_factory(
    &wfs_coverage_query_handler_create);
