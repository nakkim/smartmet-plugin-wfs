#include "stored_queries/StoredIsolineQueryHandler.h"
#include <gis/Box.h>
#include <newbase/NFmiEnumConverter.h>

#include <boost/algorithm/string/replace.hpp>
#include <iomanip>

namespace bw = SmartMet::Plugin::WFS;

bw::StoredIsolineQueryHandler::StoredIsolineQueryHandler(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<bw::StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config, false),
      StoredContourQueryHandler(reactor, config, plugin_data, template_file_name)
{
  try
  {
    itsName = config->get_mandatory_config_param<std::string>("contour_param.name");
    itsUnit = config->get_optional_config_param<std::string>("contour_param.unit", "");
    itsIsoValues = config->get_mandatory_config_array<double>("contour_param.isovalues");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredIsolineQueryHandler::clipGeometry(OGRGeometryPtr& pGeom, Fmi::Box& bbox) const
{
  try
  {
    if (pGeom && !pGeom->IsEmpty())
      pGeom.reset(Fmi::OGR::lineclip(*pGeom, bbox));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<bw::ContourQueryResultPtr> bw::StoredIsolineQueryHandler::processQuery(
    ContourQueryParameter& queryParameter) const
{
  try
  {
    std::vector<ContourQueryResultPtr> query_results;

    for (std::size_t i = 0; i < itsIsoValues.size(); i++)
      (static_cast<IsolineQueryParameter&>(queryParameter)).isovalues.push_back(itsIsoValues[i]);

    // contains result for all isolines
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
          IsolineQueryResultPtr result(new IsolineQueryResult);
          WeatherAreaGeometry wag = result_item->area_geoms[i];
          result->name = itsName;
          result->unit = itsUnit;
          result->isovalue = itsIsoValues[i];
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

SmartMet::Engine::Contour::Options bw::StoredIsolineQueryHandler::getContourEngineOptions(
    const boost::posix_time::ptime& time, const ContourQueryParameter& queryParameter) const
{
  try
  {
    return SmartMet::Engine::Contour::Options(
        queryParameter.parameter,
        time,
        (reinterpret_cast<const IsolineQueryParameter&>(queryParameter)).isovalues);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::ContourQueryParameter> bw::StoredIsolineQueryHandler::getQueryParameter(
    const SmartMet::Spine::Parameter& parameter,
    const SmartMet::Engine::Querydata::Q& q,
    OGRSpatialReference& sr) const
{
  try
  {
    boost::shared_ptr<ContourQueryParameter> ret(new bw::IsolineQueryParameter(parameter, q, sr));

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredIsolineQueryHandler::setResultHashValue(CTPP::CDT& resultHash,
                                                       const ContourQueryResult& resultItem) const
{
  try
  {
    const IsolineQueryResult& isolineResultItem =
        reinterpret_cast<const IsolineQueryResult&>(resultItem);

    resultHash["name"] = isolineResultItem.name;
    if (!isolineResultItem.unit.empty())
      resultHash["unit"] = isolineResultItem.unit;
    resultHash["isovalue"] = isolineResultItem.isovalue;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
using namespace SmartMet::Plugin::WFS;

boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> wfs_isoline_query_handler_create(
    SmartMet::Spine::Reactor* reactor,
    boost::shared_ptr<StoredQueryConfig> config,
    PluginImpl& plugin_data,
    boost::optional<std::string> template_file_name)
{
  try
  {
    StoredIsolineQueryHandler* qh =
        new StoredIsolineQueryHandler(reactor, config, plugin_data, template_file_name);
    boost::shared_ptr<SmartMet::Plugin::WFS::StoredQueryHandlerBase> result(qh);
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

SmartMet::Plugin::WFS::StoredQueryHandlerFactoryDef wfs_isoline_query_handler_factory(
    &wfs_isoline_query_handler_create);
