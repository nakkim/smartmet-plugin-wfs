#include "SupportsLocationParameters.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <cassert>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;
using SmartMet::Spine::Value;

namespace ba = boost::algorithm;

const char *bw::SupportsLocationParameters::P_PLACES = "places";
const char *bw::SupportsLocationParameters::P_LATLONS = "latlons";
const char *bw::SupportsLocationParameters::P_GEOIDS = "geoids";
const char *bw::SupportsLocationParameters::P_MAX_DISTANCE = "maxDistance";
const char *bw::SupportsLocationParameters::P_KEYWORD = "keyword";
const char *bw::SupportsLocationParameters::P_FMISIDS = "fmisids";
const char *bw::SupportsLocationParameters::P_WMOS = "wmos";
const char *bw::SupportsLocationParameters::P_KEYWORD_OVERWRITABLE = "keyword_overwritable";

void bw::SupportsLocationParameters::engOrFinToEnOrFi(std::string &language)
{
  try
  {
    std::string lang = Fmi::ascii_tolower_copy(language);
    if (lang == "fin")
      language = "fi";
    if (lang == "eng")
      language = "en";
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsLocationParameters::SupportsLocationParameters(
    boost::shared_ptr<bw::StoredQueryConfig> config, unsigned options)
    : SupportsExtraHandlerParams(config, false),
      support_keywords((options & SUPPORT_KEYWORDS) != 0),
      include_fmisids((options & INCLUDE_FMISIDS) != 0),
      include_geoids((options & INCLUDE_GEOIDS) != 0),
      include_wmos((options & INCLUDE_WMOS) != 0)
{
  try
  {
    register_array_param<std::string>(P_PLACES, *config);
    register_array_param<double>(P_LATLONS, *config, 0, 999, 2);
    if (include_fmisids)
      register_array_param<int64_t>(P_FMISIDS, *config);
    register_array_param<int64_t>(P_GEOIDS, *config);
    if (include_wmos)
      register_array_param<int64_t>(P_WMOS, *config);
    register_scalar_param<double>(P_MAX_DISTANCE, *config);
    if (support_keywords)
    {
      register_scalar_param<std::string>(P_KEYWORD, *config);
      register_scalar_param<bool>(P_KEYWORD_OVERWRITABLE, *config);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsLocationParameters::~SupportsLocationParameters() {}

void bw::SupportsLocationParameters::get_location_options(
    SmartMet::Engine::Geonames::Engine *geo_engine,
    const RequestParameterMap &param,
    const std::string &language_requested,
    std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const
{
  try
  {
    assert(geo_engine != nullptr);
    assert(locations != nullptr);

    locations->clear();

    const bool keyword_overwritable = param.get_optional<bool>(P_KEYWORD_OVERWRITABLE, false);
    std::string language = Fmi::ascii_tolower_copy(language_requested);
    engOrFinToEnOrFi(language);

    double max_distance = param.get_single<double>(P_MAX_DISTANCE);

    // Handle latitude-logitude pairs
    std::vector<double> values;
    param.get<double>(P_LATLONS, std::back_inserter(values), 0, 998, 2);
    if (values.size() & 1)
    {
      throw SmartMet::Spine::Exception(BCP, "Invalid location list in parameter 'latlon'!")
          .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
          .disableStackTrace();
    }
    for (std::size_t i = 0; i < values.size(); i += 2)
    {
      const double &lat = values[i];
      const double &lon = values[i + 1];
      const std::string name = str(boost::format("%.4f,%.4f") % lon % lat);
      // Note: geoengine and locus use kilometers instead of meters
      SmartMet::Spine::LocationPtr loc =
          geo_engine->lonlatSearch(lon, lat, language, max_distance / 1000.0);
      locations->push_back(std::make_pair(name, loc));
    }

    get_geoids(geo_engine, param, language, locations);
    get_fmisids(geo_engine, param, language, locations);
    get_wmos(geo_engine, param, language, locations);

    // Handle site names
    SmartMet::Spine::LocationList ptrs;
    Locus::QueryOptions opts;
    opts.SetCountries("all");
    opts.SetSearchVariants(true);
    opts.SetLanguage(language);
    opts.SetResultLimit(1);

    std::vector<std::string> names;
    param.get<std::string>(P_PLACES, std::back_inserter(names));
    BOOST_FOREACH (const std::string name, names)
    {
      SmartMet::Spine::LocationList ptrs = geo_engine->nameSearch(opts, name);
      if (ptrs.empty())
      {
        throw SmartMet::Spine::Exception(
            BCP, "No locations found for the place with the requested language!")
            .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
            .addParameter("Place", name)
            .addParameter("Language", language_requested)
            .disableStackTrace();
      }
      else
      {
        locations->push_back(std::make_pair(name, ptrs.front()));
      }
    }

    const bool locations_given = (not locations->empty() or param.count(P_FMISIDS) or
                                  param.count(P_WMOS) or param.count("boundingBox"));
    const bool overwrite_keyword_by_locations = (keyword_overwritable and locations_given);
    if (support_keywords and not overwrite_keyword_by_locations)
    {
      std::string keyword = param.get_optional<std::string>(P_KEYWORD, "");
      if (ba::trim_copy(keyword) != "")
      {
        Locus::QueryOptions opts;
        opts.SetLanguage(language);
        SmartMet::Spine::LocationList places =
            geo_engine->keywordSearch(opts, ba::trim_copy(keyword));
        if (places.empty())
        {
          throw SmartMet::Spine::Exception(BCP, "No locations found for the keyword!")
              .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
              .addParameter("Keyword", keyword)
              .disableStackTrace();
        }
        else
        {
          BOOST_FOREACH (SmartMet::Spine::LocationPtr loc, places)
          {
            locations->push_back(std::make_pair(loc->name, loc));
          }
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsLocationParameters::get_geoids(
    SmartMet::Engine::Geonames::Engine *geo_engine,
    const RequestParameterMap &param,
    const std::string &language,
    std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const
{
  try
  {
    if (include_geoids)
    {
      std::vector<int64_t> ids;
      param.get<int64_t>(P_GEOIDS, std::back_inserter(ids));
      BOOST_FOREACH (int64_t id, ids)
      {
        if (id < std::numeric_limits<long>::min() || id > std::numeric_limits<long>::max())
        {
          throw SmartMet::Spine::Exception(BCP, "The 'geoid' value is out of the range!")
              .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
              .addParameter("SmartMet::Spine::GeoId", std::to_string(id))
              .disableStackTrace();
        }
        else
        {
          SmartMet::Spine::LocationPtr loc = geo_engine->idSearch(id, language);
          locations->push_back(std::make_pair(Fmi::to_string(id), loc));
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsLocationParameters::get_fmisids(
    SmartMet::Engine::Geonames::Engine *geo_engine,
    const RequestParameterMap &param,
    const std::string &language,
    std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const
{
  try
  {
    Locus::QueryOptions opts;
    opts.SetCountries("all");
    opts.SetSearchVariants(true);
    opts.SetLanguage("fmisid");
    opts.SetFeatures("SYNOP,STUK");
    opts.SetResultLimit(1);

    std::vector<int64_t> ids;
    param.get<int64_t>(P_FMISIDS, std::back_inserter(ids));
    BOOST_FOREACH (int64_t id, ids)
    {
      if (id < std::numeric_limits<long>::min() || id > std::numeric_limits<long>::max())
      {
        throw SmartMet::Spine::Exception(BCP, "The 'fmisid' value is out of the range!")
            .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
            .addParameter("Fmsid", std::to_string(id))
            .disableStackTrace();
      }
      else
      {
        std::string id_s = Fmi::to_string(id);
        SmartMet::Spine::LocationList locList = geo_engine->nameSearch(opts, id_s);
        if (include_fmisids and locList.empty())
        {
          throw SmartMet::Spine::Exception(BCP, "Unknown 'fmisid' value!")
              .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
              .addParameter("Fmsid", std::to_string(id))
              .disableStackTrace();
        }
        // Including location of the current fmisid if enabled.
        if (include_fmisids)
          locations->push_back(std::make_pair(id_s, locList.front()));
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsLocationParameters::get_wmos(
    SmartMet::Engine::Geonames::Engine *geo_engine,
    const RequestParameterMap &param,
    const std::string &language,
    std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const
{
  try
  {
    Locus::QueryOptions opts;
    opts.SetCountries("all");
    opts.SetSearchVariants(true);
    opts.SetLanguage("wmo");
    opts.SetFeatures("SYNOP,STUK");
    opts.SetResultLimit(1);

    std::vector<int64_t> ids;
    param.get<int64_t>(P_WMOS, std::back_inserter(ids));
    BOOST_FOREACH (int64_t id, ids)
    {
      if (id < std::numeric_limits<long>::min() || id > std::numeric_limits<long>::max())
      {
        throw SmartMet::Spine::Exception(BCP, "The 'wmoid' value is out of the range!")
            .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
            .addParameter("WmoId", std::to_string(id))
            .disableStackTrace();
      }
      else
      {
        std::string id_s = Fmi::to_string(id);
        SmartMet::Spine::LocationList locList = geo_engine->nameSearch(opts, id_s);
        if (include_wmos and locList.empty())
        {
          throw SmartMet::Spine::Exception(BCP, "Unknown 'wmoid' value!")
              .addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED)
              .addParameter("WmoId", id_s)
              .disableStackTrace();
        }
        // Including location of the current wmo id if enabled.
        if (include_wmos)
          locations->push_back(std::make_pair(id_s, locList.front()));
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
/**

@page WFS_SQ_LOCATION_PARAMS Location related parameters

@section WFS_SQ_LOCATION_PARAMS_INTRO Introduction

Stored query location parameters contains parameters which characterizes the place
location for querying observations or forecast data. Note that various station IDs
(like FMISID) are not included into this group. Bounding box support is implemented separately
and as result it is also not included into this group.

@section WFS_SQ_LOCATION_PARAMS_PARAMS The stored query handler built-in parameters of this group

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Data type</th>
<th>Description</th>
</tr>

<tr>
  <td>place</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>string</td>
  <td>Maps to the place names like 'Helsinki' or 'Kumpula,Helsinki'</td>
</tr>

<tr>
  <td>latlons</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>double</td>
  <td>Maps to stored query parameter containing longitude and latitude (in degrees) pairs</td>
</tr>

<tr>
  <td>geoids</td>
  <td>@ref WFS_CFG_ARRAY_PARAM_TMPL</td>
  <td>integer</td>
  <td>Maps to stored query array parameter containing GEOID values for stations. Note that
      this parameter may be left out by constructor parameter</td>
</tr>

<tr>
  <td>maxDistance</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>double</td>
  <td>Maps to scalar parameter which specifies maximal permitted distance from provided
      point for looking up stations</td>
</tr>

<tr>
  <td>keyword</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>string</td>
  <td>Maps to keyword for stations look-up. Note that this parameter can be left out
     by constructor options</td>
</tr>

<tr>
  <td>keyword_overwritable</td>
  <td>@ref WFS_CFG_SCALAR_PARAM_TMPL</td>
  <td>bool</td>
  <td>The default values defined by @b keyword can be overwritten by using
      location related input parameters if value is true.</td>
</tr>


</table>

*/
