#include "StoredQueryConfig.h"
#include "Config.h"
#include "ParameterTemplateBase.h"
#include "StoredQueryHandlerBase.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <macgyver/TypeName.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace bw = SmartMet::Plugin::WFS;

SmartMet::Plugin::WFS::StoredQueryConfig::StoredQueryConfig(const std::string& config_file,
							    const Config& plugin_config)
    : SmartMet::Spine::ConfigBase(config_file, "WFS stored query configuration")
{
  try
  {
    set_locale(plugin_config.get_default_locale());
    config_last_write_time = config_write_time();
    parse_config();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Plugin::WFS::StoredQueryConfig::StoredQueryConfig(
    boost::shared_ptr<libconfig::Config> config,
    const Config& plugin_config)

    : SmartMet::Spine::ConfigBase(config, "WFS stored query configuration")
{
  try
  {
    locale_name = plugin_config.get_default_locale();
    config_last_write_time = config_write_time();
    parse_config();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMet::Plugin::WFS::StoredQueryConfig::~StoredQueryConfig() {}

void SmartMet::Plugin::WFS::StoredQueryConfig::warn_about_unused_params(
    const StoredQueryHandlerBase* handler)
{
  try
  {
    BOOST_FOREACH (const auto& map_item, param_map)
    {
      const ParamDesc& desc = map_item.second;
      if (not desc.get_used())
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str() << ": [WFS] [WARNING] Unused parameter '"
            << desc.name << "' for stored query '" << get_query_id() << "'\n";
        std::cout << msg.str() << std::flush;
      }
    }

    if (handler)
    {
      std::set<std::string> registred_params = handler->get_handler_param_names();
      if (registred_params.empty())
        return;

      auto& cfg_param_group = get_mandatory_config_param<libconfig::Setting&>(
          ParameterTemplateBase::HANDLER_PARAM_NAME);
      assert_is_group(cfg_param_group);

      const int num_items = cfg_param_group.getLength();
      for (int i = 0; i < num_items; i++)
      {
        libconfig::Setting& item = cfg_param_group[i];
        const char* c_name = item.getName();
        if (c_name and registred_params.count(c_name) == 0)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str()
              << ": [WFS] [WARNING] Unrecognized configuration entry at path " << item.getPath()
              << " in configuration of stored query '" << get_query_id() << "':\n";
          dump_setting(msg, item);
          msg << "\n";
          std::cout << msg.str() << std::flush;
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMet::Plugin::WFS::StoredQueryConfig::parse_config()
{
  try
  {
    using SmartMet::Spine::Value;
    namespace ba = boost::algorithm;

    try
    {
      disabled = get_optional_config_param<bool>("disabled", false);
      demo = get_optional_config_param<bool>("demo", false);
      test = get_optional_config_param<bool>("test", false);
      debug_level = get_optional_config_param<int>("debugLevel", 0);
      query_id = get_mandatory_config_param<std::string>("id");
      expires_seconds = get_optional_config_param<int>("expiresSeconds", 60);

      this->template_fn.reset();
      if (get_config().exists("template"))
      {
        this->template_fn = get_mandatory_config_param<std::string>("template");
      }

      constructor_name = get_mandatory_config_param<std::string>("constructor_name");
      default_language = get_optional_config_param<std::string>("defaultLanguage", "eng");
      auto& s_title = get_mandatory_config_param<libconfig::Setting&>("title");
      title = bw::MultiLanguageString::create(default_language, s_title);

      auto& s_abstract = get_mandatory_config_param<libconfig::Setting&>("abstract");
      abstract = bw::MultiLanguageString::create(default_language, s_abstract);

      return_type_names = get_mandatory_config_array<std::string>("returnTypeNames", 0);

      std::vector<std::string> formats;
      if (get_config_array("formats", formats))
      {
        BOOST_FOREACH (const std::string format, formats)
        {
          if (format.empty())
          {
            throw SmartMet::Spine::Exception(
                BCP, "Empty format in the configuration file '" + get_file_name() + "'!");
          }
        }
      }

      libconfig::Setting& c_param_desc =
          assert_is_list(get_mandatory_config_param<libconfig::Setting&>("parameters"));

      param_map.clear();
      int NP = c_param_desc.getLength();

      for (int i = 0; i < NP; i++)
      {
        ParamDesc item;
        libconfig::Setting& c_item = c_param_desc[i];
        try
        {
          assert_is_group(c_param_desc[i]);
          item.name = get_mandatory_config_param<std::string>(c_item, "name");
          item.xml_type = get_mandatory_config_param<std::string>(c_item, "xmlType");
          auto& s1 = get_mandatory_config_param<libconfig::Setting&>(c_item, "title");
          item.title = bw::MultiLanguageString::create(default_language, s1);
          auto& s2 = get_mandatory_config_param<libconfig::Setting&>(c_item, "abstract");
          item.abstract = bw::MultiLanguageString::create(default_language, s2);
          item.min_occurs = get_optional_config_param<int>(c_item, "minOccurs", 0);
          item.max_occurs = get_optional_config_param<int>(c_item, "maxOccurs", 1);

          std::string type_def = get_mandatory_config_param<std::string>(c_item, "type");
          item.param_def.parse_def(type_def);

          if (c_item.exists("lowerLimit"))
          {
            const SmartMet::Spine::Value tmp =
                SmartMet::Spine::Value::from_config(c_item["lowerLimit"]);
            item.lower_limit = item.param_def.convValue(tmp);
          }

          if (c_item.exists("upperLimit"))
          {
            const SmartMet::Spine::Value tmp =
                SmartMet::Spine::Value::from_config(c_item["upperLimit"]);
            item.upper_limit = item.param_def.convValue(tmp);
          }

          std::vector<std::string> conflict_vect;
          get_config_array(c_item, "conflictsWith", conflict_vect);
          std::copy(conflict_vect.begin(),
                    conflict_vect.end(),
                    std::inserter(item.conflicts_with, item.conflicts_with.begin()));

          const auto vtype = item.param_def.getValueType();
          if ((item.upper_limit or item.lower_limit) and
              ((vtype == StoredQueryParamDef::UNDEFINED) or
               (vtype == StoredQueryParamDef::STRING) or (vtype == StoredQueryParamDef::BOOL)))
          {
            std::ostringstream msg;
            msg << "Specifying parameter limits not supported"
                << " for parameter '" << item.name << "' in:\n";
            dump_setting(msg, c_item, 16);
            throw SmartMet::Spine::Exception(BCP, msg.str());
          }

          Fmi::ascii_tolower(item.name);

          if (!param_map.insert(std::make_pair(item.name, item)).second)
          {
            std::ostringstream msg;
            msg << "Duplicate description of stored query parameter '" << item.name
                << " in configuration file '" << get_file_name() << "'";
            throw SmartMet::Spine::Exception(BCP, msg.str());
          }
        }
        catch (const std::exception& err)
        {
          std::ostringstream msg;
          msg << "Error while parsing stored query parameter description:\n";
          dump_setting(msg, c_item, 16);
          msg << std::endl;
          std::cerr << msg.str();
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        param_names.push_back(item.name);
      }

      // Check for internal conflicts in parameter specification
      bool have_error = false;
      for (auto map_iter = param_map.begin(); map_iter != param_map.end(); ++map_iter)
      {
        const std::string& name = map_iter->first;
        const ParamDesc& desc = map_iter->second;

        std::vector<std::string> conflict_names;
        for (auto it = desc.conflicts_with.begin(); it != desc.conflicts_with.end(); ++it)
        {
          const std::string n2 = Fmi::ascii_tolower_copy(*it);
          if (n2 == name)
          {
            std::ostringstream msg;
            msg << METHOD_NAME << ": stored query parameter '" << desc.name
                << "' cannot conflict with itself" << std::endl;
            std::cerr << msg.str() << std::flush;
            have_error = true;
          }

          if (param_map.count(n2) == 0)
          {
            std::ostringstream msg;
            msg << METHOD_NAME << ": stored query parameter '" << desc.name
                << "' conflicts with non-existing parameter '" << *it << "'" << std::endl;
            std::cerr << msg.str() << std::flush;
            have_error = true;
          }

          ParamDesc& desc2 = param_map.at(n2);
          if ((desc.min_occurs > 0) and (desc2.min_occurs > 0))
          {
            std::ostringstream msg;
            msg << METHOD_NAME << ": mandatory configuration parameters '" << desc.name << "' and '"
                << desc2.name << "' marked as conflicting" << std::endl;
            std::cerr << msg.str() << std::flush;
            have_error = true;
          }

          desc2.conflicts_with.insert(desc.name);
        }
      }

      if (have_error)
      {
        throw SmartMet::Spine::Exception(
            BCP, "Conflicting parameter specifications found for stored query '" + query_id + "'!");
      }
    }
    catch (const libconfig::ConfigException& err)
    {
      std::cerr << "Failed to parse stored query configuration file '" << get_file_name() << "'"
                << std::endl;
      handle_libconfig_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string SmartMet::Plugin::WFS::StoredQueryConfig::get_template_fn() const
{
  try
  {
    if (template_fn)
    {
      return *template_fn;
    }
    else
    {
      std::ostringstream msg;
      msg << "Template file name is not available"
          << " for stored request '" << query_id << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string SmartMet::Plugin::WFS::StoredQueryConfig::get_title(const std::string& language) const
{
  try
  {
    return title->get(language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string SmartMet::Plugin::WFS::StoredQueryConfig::get_abstract(
    const std::string& language) const
{
  try
  {
    return abstract->get(language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string SmartMet::Plugin::WFS::StoredQueryConfig::get_param_title(
    int ind, const std::string& language) const
{
  try
  {
    return get_param_desc(ind).title->get(language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string SmartMet::Plugin::WFS::StoredQueryConfig::get_param_abstract(
    int ind, const std::string& language) const
{
  try
  {
    return get_param_desc(ind).abstract->get(language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMet::Plugin::WFS::StoredQueryConfig::ParamDesc::dump(std::ostream& stream) const
{
  try
  {
    stream << '(';

    stream << "(name '" << name << "')";

    stream << "(minOccurs " << min_occurs << ")";

    stream << "(maxOccurs " << min_occurs << ")";

    stream << "(xmlType '" << xml_type << "')";

    if (lower_limit)
    {
      stream << "(lowerLimit " << *lower_limit << ")";
    }

    if (upper_limit)
    {
      stream << "(upperLimit " << *upper_limit << ")";
    }

    stream << "(paramDef '";
    param_def.dump(stream);
    stream << "')";

    stream << "(title ";
    BOOST_FOREACH (const auto& item, title->get_content())
    {
      stream << "('" << item.first << "' '" << item.second << "')";
    }
    stream << ')';

    stream << "(abstract ";
    BOOST_FOREACH (const auto& item, abstract->get_content())
    {
      stream << "('" << item.first << "' '" << item.second << "')";
    }
    stream << ')';
    stream << ')';
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMet::Plugin::WFS::StoredQueryConfig::dump_params(std::ostream& stream) const
{
  try
  {
    BOOST_FOREACH (const auto& item, param_map)
    {
      stream << "(PARAMETER '" << item.first << "'";
      item.second.dump(stream);
      stream << std::endl;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::time_t SmartMet::Plugin::WFS::StoredQueryConfig::config_write_time() const
{
  try
  {
    namespace ba = boost::algorithm;
    namespace fs = boost::filesystem;

    auto filepath = get_file_name();

    fs::path p(filepath);
    auto filename = p.filename().string();

    bool validFile =
        (fs::is_regular_file(filepath) and not ba::starts_with(filename, ".") and
         not ba::starts_with(filename, "#") and filename.substr(filename.length() - 5) == ".conf");

    if (validFile)
      return fs::last_write_time(p);

    return config_last_write_time;
  }
  catch (...)
  {
    std::ostringstream msg;
    msg << "Failed to get last write time of '" << get_file_name() << "' file.";
    throw SmartMet::Spine::Exception::Trace(BCP, msg.str());
  }
}

bool SmartMet::Plugin::WFS::StoredQueryConfig::last_write_time_changed() const
{
  try
  {
    if (config_write_time() == config_last_write_time)
      return false;

    return true;
  }
  catch (const SmartMet::Spine::Exception& e)
  {
    throw;
  }
  catch (...)
  {
    std::ostringstream msg;
    msg << "Failed to test write time of '" << get_file_name() << "' file.";
    throw SmartMet::Spine::Exception::Trace(BCP, msg.str());
  }
}

void SmartMet::Plugin::WFS::StoredQueryConfig::set_locale(const std::string& name)
{
  locale_name = name;
  locale.reset(new std::locale(name.c_str()));
}

std::shared_ptr<const std::locale> SmartMet::Plugin::WFS::StoredQueryConfig::get_locale() const
{
  return locale;
}

/**

@page WFS_CFG_STORED_QUERY Stored query configuration

@section WFS_CFG_STORED_QUERY_INTRO Introduction

This libconfig style configuration object describes stored query.
The parameters listed in the table below are common for all stored query handlers.
Specific stored query handlers may have additional configuration parameters
not listed in the table below.

@section WFS_CFG_STORED_QUERY_ENTRIES Configuration entries of stored query configuration

<table border="1">
<tr><th>Entry name</th><th>Type</th><th>Use</th><th>Description</th></tr>

<tr>
  <td>disabled</td>
  <td>boolean</td>
  <td>optional (default @b false )</td>
  <td>Describes stored query as disabled when present and the value is @b true</td>
</tr>

<tr>
  <td>hidden</td>
  <td>boolean</td>
  <td>optional (default @b false)</td>
  <td>Describes stored query as hidden. It does not appear in response of request @b
ListStoredQueries
      and also in response of request @b DescribeStoredQueries (for latest it is however shown when
      information is required by stored query ID value. SmartMet::Spine::Value is read in
      SmartMet::Plugin::WFS::StoredQueryHandlerBase::StoredQueryHandlerBase</td>
</tr>

<tr>
  <td>demo</td>
  <td>boolean</td>
  <td>optional (default @b false)</td>
  <td>Describes stored query as demo query. Demo queries are disabled by default unless explicitly
globaly
      enabled by @ref WFS_PLUGIN_CONFIG entry @b enableDemoQueries. A message about demo
      query being disabled is generated when demo queries are globally disabled </td>
</tr>

<tr>
  <td>test</td>
  <td>boolean</td>
  <td>optional (default @b false)</td>
  <td>Describes stored query as intended for test purposes only. Test stored queries are silently
      ignored unless globally enabled by @ref WFS_PLUGIN_CONFIG entry @b enableTestQueries </td>
</tr>

<tr>
  <td>debugLevel</td>
  <td>integer</td>
  <td>optional (default 0)</td>
  <td>Specifies debug level for specific query. SmartMet::Spine::Value 0 means very few messages
generated. There are
      generally more messages generated for larger debug level values</td>
</tr>

<tr>
  <td>expiresSeconds</td>
  <td>integer</td>
  <td>optional (default 60)</td>
  <td>Specifies expires seconds for the Expires entity-header of a query response.
      If there is multiple stored queries in a request, the smallest value
      represents the request.</td>
</tr>

<tr>
  <td>@b id</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Specifies stored query ID</td>
</tr>

<tr>
  <td>template</td>
  <td>string</td>
  <td>optional</td>
  <td>Specifies <a href="http://ctpp.havoc.ru/en/">CTPP</a> template file to use when specified.
Some
      of stored query handlers require this parameter to be specified</td>
</tr>

<tr>
  <td>@b constructor_name</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Name of factory object for constructing stored query handler</td>
</tr>

<tr>
  <td>defaultLanguage</td>
  <td>string</td>
  <td>mandatory</td>
  <td>The default language code (like @b eng or @b fin )</td>
</tr>

<tr>
  <td>@b title</td>
  <td>@ref WFS_CFG_MULTI_LANGUAGE_STRING</td>
  <td>mandatory</td>
  <td>The short description of stored query</td>
</tr>

<tr>
  <td>@b abstract</td>
  <td>@ref WFS_CFG_MULTI_LANGUAGE_STRING</td>
  <td>mandatory</td>
  <td>Detailed description of stored query</td>
</tr>

<tr>
  <td>@b returnTypeNames</td>
  <td>string or array of strings</td>
  <td>mandatory</td>
  <td>The return types for the stored query</td>
</tr>

<tr>
  <td>@b parameters</td>
  <td>list of @ref  WFS_CFG_STORED_QUERY_PARAM</td>
  <td>mandatory</td>
  <td>The parameters of stored query</td>
</tr>

<tr>
  <td>named_params</td>
  <td>list of @ref WFS_SQ_NAMED_PARAM_DEF "cfgNamedParamDef"</td>
  <td>usually optional</td>
  <td>Additional stored query handler parameters. These parameters are added from confiuration file.
      Not all stored query handler classes can use these additional named  parameters directly.
      Named parameters can however be used as default values for handler built-in parameters.
      Additionally named parameters are exported to CTPP2 hash used to for template
      preprocessor used to generate output. As the result the values of these parameters
      are available to template preprocessor</td>
</tr>

<tr>
  <td>@b query_params</td>
  <td>group which member values are @ref WFS_CFG_SCALAR_PARAM_TMPL "cfgScalarParameterTemplate"
    or @ref WFS_CFG_ARRAY_PARAM_TMPL "cfgArrayParameterTemplate"</td>
  <td></td>
  <td>Stored query handler class object registers its parameters (as either scalar or array
      parameters) with specific names. Parameter entries with corresponding names and definition
      of mapping of these parameters must be present in this libconfig group for all built-in
      handler parameters. Additionally any additional unrecognized (not corresponding to
      registered built-in handler parameter) configuration entry in this group
      causes warning to be generated. The type of parameter (scalar or array) must
      correspond the type of built-in parameter</td>
</tr>

</table>

@section WFS_CFG_STORED_QUERY_EXAMPLE Example stored query configuration

@verbatim
disabled = false;
demo = false;

id = "fmi::observations::weather::timevaluepair";

constructor_name = "wfs_obs_handler_factory";

title:
{
        eng = "Instantaneous Weather Observations";
        fin = "Hetkelliset säähavaintoarvot";
};

abstract:
{
        eng = "Real time weather observations from weather stations. Default set contains wind
speed, direction, gust, temperature, relative humidity, dew point, pressure reduced to sea level,
one hour precipitation amount and visibility. By default, the data is returned from last 12 hour. By
default, the data covers southern Finland. The data is returned as a multi point coverage format.";

        fin = "Reaaliaikaiset säähavainnot suomen havaintoasemilta. Vakiona kysely palautta
tuulennopeuden ja -suunnan, puuskanopeuden, ilman lämpötilan, suhteellisen kosteuden, kastepisteen,
paineen (merenpinnan tasolla), tunnin sademäärän, näkyvyyden ja pilvisyyden. Oletuksena dataa
palautetaan 12 tuntia pyyntöhetkestä taaksepäin. Oletusalueena on Etelä-Suomi. Data palautetaan
'multipointcoverage'-muodossa.";
};

template = "weather_observations_timevaluepair.c2t";

parameters:
(
{
        name = "starttime";
        title = {eng = "Begin of the time interval"; fin = "Alkuaika"; };
        abstract = { eng = "Parameter begin specifies the begin of time interval in ISO-format (for
example 2012-02-27T00:00:00Z)."; fin = "Aikajakson alkuaika ISO-muodossa (esim.
2012-02-27T00:00:00Z)."; };
        xmlType = "dateTime";
        type = "time";
        lowerLimit = "2013-01-01T00:00:00";
},
{
        name = "endtime";
        title = { eng = "End of time interval"; fin = "Loppuaika"; };
        abstract = { eng = "End of time interval in ISO-format (for example 2012-02-27T00:00:00Z).";
fin = "Aikajakson loppuaika ISO-muodossa (esim. 2012-02-27T00:00:00Z)."; };
        xmlType = "dateTime";
        type = "time";
},
{
        name = "timestep";
        title = { eng = "The time step of data in minutes"; fin= "Aika-askel minuutteina";};
        abstract = { eng = "The time step of data in minutes. Notice that timestep is calculated
from start of the ongoing hour or day. "; fin= "Aika-askel minuutteina. Huomaa, että aika-askel
lasketaan tasaminuuteille edellisestä tasatunnista tai vuorokaudesta.";};
        xmlType = "int";
        type = "uint";
},
{
        name = "parameters";
        title = { eng = "Parameters to return"; fin = "Meteorologiset parametrit"; };
        abstract = { eng = "Comma separated list of meteorological parameters to return."; fin =
"Meteorologiset paraemtrit pilkulla erotettuna.";};
        xmlType = "NameList";
        type = "string[1..99]";
        minOccurs = 0;
        maxOccurs = 999;
},
{
        name = "crs";
        title = { eng = "Coordinate projection to use in results"; fin = "Projektio"; };
        abstract = { eng = "Coordinate projection to use in results. For example EPSG::3067"; fin =
"Projektiotieto. Esimerkiksi EPSG::3067";};
        xmlType = "xsi:string";
        type = "string";
}
,{
        name = "bbox";
        title = { eng = "Bounding box of area for which to return data."; fin = "Aluerajaus";  };
        abstract = { eng = "Bounding box of area for which to return data (lon,lat,lon,lat). For
example 21,61,22,62"; fin = "Aluerajaus (lon,lat,lon,lat). Esimerkiksi 21,61,22,62"; };
        xmlType = "xsi:string";
        type = "bbox";
        minOccurs = 0;
        maxOccurs = 1;
},
{
        name = "place";
        title = { eng = "The location for which to provide data"; fin = "Paikannimi"; };
        abstract = { eng = "The location for which to provide forecast. Region can be given after
location name separated by comma (for example Kumpula,Kolari)."; fin = "Paikannimi. Alueen voi antaa
paikannimen jälkeen pilkulla erotettuna (esim. Kumpula,Kolari)."; };
        xmlType = "xsi:string";
        type = "string";
        minOccurs = 0;
        maxOccurs = 99;
},
{
        name = "fmisid";
        title = { eng = "FMI observation station identifier."; fin = "Säähavaintoaseman id.";  };
        abstract = { eng = "Identifier of the observation station."; fin = "Säähavaintoaseman id.";
};
        xmlType = "int";
        type = "uint";
        minOccurs = 0;
        maxOccurs = 99;
},
{
        name = "maxlocations";
        title = { eng = "Amount of locations"; fin = "Haettujen paikkojen määrä"; };
        abstract = { eng = "How many observation stations are fetched around queried locations. Note
that stations are only searched with 50 kilometers radius around the location."; fin = "Etsittävien
havaintoasemien määrä kunkin paikan läheisyydestä. Havaintoasemia etsitään maksimissaan 50
kilometrin säteellä paikoista."; };
        xmlType = "int";
        type = "uint";
},
{
        name = "geoid";
        title = { eng = "Geoid of the location for which to return data."; fin = "Haettavan paikan
geoid."; };
        abstract = { eng = "Geoid of the location for which to return data. (ID from geonames.org)";
fin = "Haettavan paikan geoid (geonames.org:sta)"; };
        xmlType = "int";
        type = "uint";
        minOccurs = 0;
        maxOccurs = 99;
},
{
        name = "wmo";
        title = { eng = "WMO code of the location for which to return data."; fin = "Haettavan
paikan wmo."; };
        abstract = { eng = "WMO code of the location for which to return data."; fin = "Haettavan
paikan wmo-koodi."; };
        xmlType = "int";
        type = "uint";
        minOccurs = 0;
        maxOccurs = 99;
}
);

returnTypeNames = [ "omso:PointTimeSeriesObservation" ];

handler_params:
{
        beginTime = "${starttime: 12 hours ago}";
        endTime = "${endtime: now}";
        places = ["${place}"];
        latlons = [];
        meteoParameters = ["${parameters > defaultMeteoParam}"];
        stationType = "opendata";
        timeStep = "${timestep:0}";
        maxDistance = 50000.0;
        wmos = ["${wmo}"];
        lpnns = [];
        fmisids = ["${fmisid}"];
        geoids = ["${geoid}"];
        observationType = "default";
        numOfStations = "${maxlocations:1}";
        hours = [];
        weekDays = [];
        locale = "fi_FI.utf8";
        missingText = "NaN";
        boundingBox = "${bbox}";
        maxEpochs = 20000;
        separateGroups = 1;
        crs = "${crs:EPSG::4258}";
        timeZone = "UTC";
        keyword = "";
};

named_params = (
    {
        name = "defaultMeteoParam";
        def = ["t2m","ws_10min","wg_10min",
"wd_10min","rh","td","r_1h","ri_10min","snow_aws","p_sea","vis"];
    },
    {
        name = "intent";
        def = ["atmoshphere",
"http://inspire.ec.europa.eu/codeList/ProcessParameterValue/value/groundObservation/observationIntent"];
   }

);
@endverbatim

 */

/**

@page WFS_CFG_STORED_QUERY_PARAM cfgStoredQueryParameterDef

@section WFS_CFG_STORED_QUERY_PARAM_INTRO Introduction

This libconfig style configuration object describes stored query parameter. A warning message
is generated if unused (not used in any mapping of stored query handler parameters) stored query
parameter is present

@section WFS_CFG_STORED_QUERY_PARAM_ENTRIES Configuration entries

<table border="1">
<tr><th>Entry name</th><th>Type</th><th>Use</th><th>Description</th></tr>

<tr>
  <td>name</td>
  <td>string</td>
  <td>mandatory</td>
  <td>the stored query parameter name (for example @b bbox)</td>
</tr>

<tr>
  <td>xmlType</td>
  <td>string</td>
  <td>mandatory</td>
  <td>the XML data type for the parameter (for example @b xsi:string). This type is expected to be
qualified XML type name</td>
</tr>

<tr>
  <td>title</td>
  <td>@ref WFS_CFG_MULTI_LANGUAGE_STRING "multilanguage string"</td>
  <td>mandatory</td>
  <td>The title of the parameter for DescribeStoredQueries request</td>
</tr>

<tr>
  <td>abstract</td>
  <td>@ref WFS_CFG_MULTI_LANGUAGE_STRING "multilanguage string"</td>
  <td>mandatory</td>
  <td>The longer description (abstract) of the parameter for DescribeStoredQueries request</td>
</tr>

<tr>
  <td>minOccurs</td>
  <td>unsigned integer</td>
  <td>optional (default 0)</td>
  <td>minimal permitted number of occurrences of the parameter</td>
</tr>

<tr>
  <td>maxOccurs</td>
  <td>unsigned integer</td>
  <td>optional (default 1)</td>
  <td>maximal permitted number of occurrences of the parameter</td>
</tr>

<tr>
  <td>type</td>
  <td>string</td>
  <td>mandatory</td>
  <td>describes type of stored query parameter</td>
</tr>

<tr>
  <td>lowerLimit</td>
  <td>number or time</td>
  <td>optional</td>
  <td>defines lower permitted value of the parameter. Absence of this configuration parameter means
that there is no lower limit</td>
</tr>

<tr>
  <td>upperLimit</td>
  <td>number or time</td>
  <td>optional</td>
  <td>defines upper permitted value of the parameter. Absence of this configuration parameter means
that there is no upper limit</td>
</tr>

<tr>
  <td>conflictsWith</td>
  <td>string or array of strings</td>
  <td>optional</td>
  <td>lists other parameters which conflicts with this one and cannot be specified at the same time.
Conflict in
       opposite direction is assumed and there is no need to specify it explicitly</td>
</tr>

@section WFS_CFG_STORED_QUERY_PARAM_DATA_TYPES Parameter data types

Parameter data type must be specified in configuration entry @b type (see table above). Following
formats are accepted:
- <b>type</b> - scalar value of type @b type.
- <b>type[size]</b> - array of type $b type with size @b size
- <b>type[size1..size2]</b> - array of type @b type and size from @b size1 to @b size2

Mentioned array sizes are for each occurrence of the stored query parameter. The number of
occurrences is limitted by
the values of entries @b minOccurs and @b maxOccurs.

Following data types are supported:
- @b string
- @b int - integer (really 64-bit signed integer)
- @b uint - unsigned integer (really 64 bit unsigned integer)
- @b double
- @b time - time
- @b bool
- @b point
- @b bbox

Limitted conversions between types are supported:
- conversion from strings to other types (required to handle default values of parameters)
- conversions between integer types
- conversions from integer types to double

Parameter is considered as an array if at least one of the following is true:
- array size specified
- the value of @b maxOccurs is 2 or larger

@subsection WFS_CFG_STORED_QUERY_PARAM_DATA_TYPES_TIME Supported time formats

The supported time formats are (checked in this order)
- Special relative time specification
- XML <a href="http://www.schemacentral.com/sc/xsd/t-xsd_dateTime.html">dateTime</a> format.
Ommitting seconds
  is supported even if seconds are required by standard
- ones supported by Fmi::TimeParser like YYYY-MM-SSTHH:MM:SSZ

The format (REGEX) of relative time specification is one of the following
@verbatim
now (rounded (down|up|) [0-9]+ (minutes|minute|min)|)
([0-9]+|) (second|minute|hour|day)(s|) ago(? (rounded (down|up|) [0-9]+ (minutes|minute|min)|)
after ([0-9]+|) (second|minute|hour|day)(s|)(? (rounded (down|up|) [0-9]+ (minutes|minute|min)|)
@endverbatim

Some examples are given in the table below. For example it is assumed that the current time is
<i>2013-06-11 10:28:17</i>

<table border="1">

<tr>
<th>String</th>
<th>Time value</th>
<th>Comments</th>
</tr>

<tr>
<td>now</td>
<td>2013-06-11 10:28:17</td>
<td>Current time</td>
</tr>

<tr>
<td>now rounded down 10 min</td>
<td>2013-06-11 10:20:00</td>
<td>current time rounded down to nearest full 10 minutes</td>
</tr>

<tr>
<td>6 hours ago rounded 60 min</td>
<td>2013-01-11 04:00:00</td>
<td>6 hours ago rounded down to nearest full hour (word @b down may be omitted)</td>
</tr>

<tr>
<td>after day rounded up 60 min</td>
<td>2013-06-12 11:00:00</td>
<td>after 1 day rounded up to next full hour</td>
</tr>

</table>

@subsection WFS_CFG_STORED_QUERY_PARAM_DATA_TYPES_POINT Point format

The point format is
@verbatim
first_coord,second_coord[,CRS]
@endverbatim

@subsection WFS_CFG_STORED_QUERY_PARAM_DATA_TYPES_BBOX Bounding box format

The format of bounding box parameter is
@verbatim
x1,y1,x2,y2[,CRS]
@endverbatim

@section WFS_CFG_STORED_QUERY_PARAM_XML_DATA_TYPES XML data types

Parameter XML data type must be specified in mandatory configuration entry @b xmlType. The value of
this
entry is used:
- to describe the type of parameter in response to @b DescribeStoredQueries request
- to extract parameter value or values when reading XML format query

Fully qualified name must be specified (namespace prefix must be provided according to ones used
DescribeStoredQueries CTTP2 tempalte

Following types are supported:
- xsi:byte
- xsi:short
- xsi:int
- xsi:long
- xsi:integer (really restricted to 64 bits)
- xsi:unsignedByte
- xsi:unsignedShort
- xsi:unsignedInt
- xsi:unsignedLong
- xsi:unsignedInteger (really restricted to 64 bits)
- xsi:negativeInteger
- xsi:nonNegativeInteger
- xsi:nonPositiveInteger
- xsi:positiveInteger
- xsi:float
- xsi:double
- xsi:dateTime
- gml:NameList
- gml:doubleList
- gml:integerList
- gml:pos
- gml:Envelope
- xsi:language

Following XML namespaces correspond to namespace prefixes
<table border="1">
<tr><td>xsi</td><td>http://www.w3.org/2001/XMLSchema-instance</td></tr>
<tr><td>gml</td><td>http://www.opengis.net/gml/3.2</td></tr>
</table>

*/
