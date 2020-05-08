#include "WfsFeatureDef.h"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <spine/Exception.h>
#include <algorithm>

namespace bw = SmartMet::Plugin::WFS;

bw::WfsFeatureDef::WfsFeatureDef(SmartMet::Spine::CRSRegistry& crs_registry,
                                 const std::string& default_language,
                                 boost::shared_ptr<SmartMet::Spine::ConfigBase> config,
                                 libconfig::Setting& setting)
{
  try
  {
    config->assert_is_group(setting);

    // Valitettavasti gcc-4.4.X ei hyväksi tätä luokan jäsenten alustamisen aikana
    std::array<double, 4> tmp_bbox = {{-180.00, -90.0, 180.0, 90.0}};
    bbox = tmp_bbox;

    name = config->get_mandatory_config_param<std::string>(setting, "name");

    xml_type = config->get_mandatory_config_param<std::string>(setting, "xmlType");

    xml_namespace = config->get_mandatory_config_param<std::string>(setting, "xmlNamespace");

    if (setting.exists("xmlNamespaceLoc"))
    {
      xml_namespace_loc =
          config->get_mandatory_config_param<std::string>(setting, "xmlNamespaceLoc");
    }

    auto& s1 = config->get_mandatory_config_param<libconfig::Setting&>(setting, "title");
    title = SmartMet::Spine::MultiLanguageString::create(default_language, s1);

    auto& s2 = config->get_mandatory_config_param<libconfig::Setting&>(setting, "abstract");
    abstract = SmartMet::Spine::MultiLanguageString::create(default_language, s2);

    auto crs = config->get_mandatory_config_param<std::string>(setting, "defaultCRS");
    default_crs_url = resolve_crs_url(crs, crs_registry);

    std::vector<std::string> tmp1;
    config->get_config_array(setting, "otherCRS", tmp1);
    std::transform(
        tmp1.begin(),
        tmp1.end(),
        std::back_inserter(other_crs_urls),
        boost::bind(&bw::WfsFeatureDef::resolve_crs_url, ::_1, boost::ref(crs_registry)));

    std::vector<double> tmp2;
    if (config->get_config_array(setting, "boundingBox", tmp2, 4, 4))
    {
      std::copy(tmp2.begin(), tmp2.end(), bbox.data());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::WfsFeatureDef::~WfsFeatureDef() {}

std::string bw::WfsFeatureDef::resolve_crs_url(const std::string& name,
                                               SmartMet::Spine::CRSRegistry& crs_registry)
{
  try
  {
    std::string value;
    bool ok = crs_registry.get_attribute(name, "projUri", &value);
    if (ok)
    {
      return value;
    }
    else
    {
      return name;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_PLUGIN_CONFIG_FEATURE_DEF WFS plugin feature description file

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
<td>The feature name for internal use. The value provided in this entry
    must be used when providing return value types for stored queries</td>
</tr>

<tr>
<td>xmlType</td>
<td>string</td>
<td>mandatory</td>
<td>qualified XML name for the feature</td>
</tr>

<tr>
<td>xmlNamespace</td>
<td>string</td>
<td>mandatory</td>
<td>XML namespace URI for the feature</td>
</tr>

<tr>
<td>xmlNamespaceLoc</td>
<td>string</td>
<td>optional</td>
<td>the URI to provide in xsi:schemaLocation attribute for this namespace
    (only if specified)</td>
</tr>

<tr>
<td>title</td>
<td>@ref WFS_CFG_MULTI_LANGUAGE_STRING</td>
<td>mandatory</td>
<td>The name of feature for use in GetCapabilities response</td>
</tr>

<tr>
<td>abstract</td>
<td>@ref WFS_CFG_MULTI_LANGUAGE_STRING</td>
<td>mandatory</td>
<td>The description of feature for use in GetCapabilities response</td>
</tr>

<tr>
<td>defaultCRS</td>
<td>string</td>
<td>mandatory</td>
<td>The default CRS for feature for use in GetCapabilities response, for example @b EPSG\::4326
</td>
</tr>

<tr>
<td>otherCRS</td>
<td>string or array of strings</td>
<td>optional</td>
<td>Other supported CRSs for the feature</td>
</tr>

<tr>
<td>boundingBox</td>
<td>double array of size 4</td>
<td>optional</td>
<td>The bounding box of CRS</td>
</tr>

</table>

<hr>

An example:
@verbatim
name : "omso:PointTimeSeriesObservation";

xmlType : "PointTimeSeriesObservation";
xmlNamespace : "http://inspire.ec.europa.eu/schemas/omso/3.0";
xmlNamespaceLoc : "http://inspire.ec.europa.eu/schemas/omso/3.0/SpecialisedObservations.xsd";

title:
{
        eng: "Observation";
        fin: "Havainnot";
};

abstract:
{
        eng: "Timeseries data of surface weather observations from meteorological and
        road weather observation stations, as well as air quality measurement stations,
        as well as point forecasts of the basic surface weather parameters for the major
        towns and cities in Finland. Each of these features contains measured or
        predicted atmospheric property values from one station over the requested time
        period and resolution.";
        fin: "Aikasarjadataa pintasää- ja tiesäähavaintoasemilta pistedatana.";
};

defaultCRS: "EPSG::4258";
otherCRS: ["EPSG:4326","EPSG:3035","EPSG:3067","EPSG:3047","EPSG:2393","EPSG:3857"];
@endverbatim

*/
