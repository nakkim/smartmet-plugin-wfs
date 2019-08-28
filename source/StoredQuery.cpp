#include "StoredQuery.h"
#include "FeatureID.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <set>
#include <sstream>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

static void check_param_max_occurs(int actual_count,
                                   int max_occurs,
                                   const std::string& location,
                                   const std::string& param_name);

static void check_param_min_occurs(int actual_count,
                                   int min_occurs,
                                   const std::string& location,
                                   const std::string& param_name);

bwx::ParameterExtractor bw::StoredQuery::param_extractor;

bw::StoredQuery::StoredQuery() : handler(), debug_format(false) {}

bw::StoredQuery::~StoredQuery() {}

bw::QueryBase::QueryType bw::StoredQuery::get_type() const
{
  return STORED_QUERY;
}

std::string bw::StoredQuery::get_cache_key() const
{
  return cache_key;
}

boost::shared_ptr<bw::StoredQuery> bw::StoredQuery::create_from_kvp(
    const std::string& language,
    const StandardPresentationParameters& spp,
    const SmartMet::Spine::HTTP::Request& http_request,
    const bw::StoredQueryMap& stored_query_map)
{
  try
  {
    bool redirect = false;
    std::string query_id;
    std::string redirected_query_id;
    std::set<std::string> redirect_id_set;

    if (not get_kvp_stored_query_id(http_request, &query_id))
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to extract 'storedquery_id'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    redirect_id_set.insert(query_id);

    boost::shared_ptr<bw::StoredQuery> query(new bw::StoredQuery);

    do
    {
      query->language = language;

      // Get stored query handler object by storedquery_id parameter value
      query->handler = stored_query_map.get_handler_by_name(query_id);

      // Extract KVP query parameters convert them to appropriate types
      auto config = query->handler->get_config();
      bw::StoredQuery::extract_kvp_parameters(http_request, *config, *query);
      query->params = query->handler->process_params(query_id, query->orig_params);

      redirected_query_id = "";
      redirect = query->handler->redirect(*query, redirected_query_id);
      if (redirect)
      {
        if (redirect_id_set.insert(redirected_query_id).second and (redirected_query_id != ""))
        {
          query_id = redirected_query_id;
        }
        else
        {
          throw SmartMet::Spine::Exception(
              BCP,
              "SmartMet::Plugin::WFS::StoredQuery::create_from_xml: invalid"
              " stored query redirect");
        }
      }
    } while (redirect);

    query->id = query_id;
    query->debug_format = spp.get_output_format() == "debug";
    // We do not need query sequence number here

    bw::FeatureID feature_id(query_id, query->params->get_map(), 0);
    feature_id.add_param("source", query->handler->get_data_source());
    feature_id.add_param("language", language);
    if (query->debug_format)
      feature_id.add_param("debugFormat", 1);
    query->cache_key = feature_id.get_id();
    query->set_stale_seconds(query->handler->get_config()->get_expires_seconds());

    return query;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::StoredQuery> bw::StoredQuery::create_from_xml(
    const std::string& language,
    const StandardPresentationParameters& spp,
    const xercesc::DOMElement& element,
    const bw::StoredQueryMap& stored_query_map)
{
  try
  {
    bool redirect = false;
    std::string query_id;
    std::string redirected_query_id;
    std::set<std::string> redirect_id_set;

    if (not get_xml_stored_query_id(element, &query_id))
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to extract stored query ID!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    redirect_id_set.insert(query_id);

    boost::shared_ptr<bw::StoredQuery> query(new bw::StoredQuery);

    do
    {
      query->language = language;

      // Get stored query handler object by storedquery_id parameter value
      query->handler = stored_query_map.get_handler_by_name(query_id);

      // Extract KVP query parameters convert them to appropriate types
      auto config = query->handler->get_config();
      bw::StoredQuery::extract_xml_parameters(element, *config, *query);
      query->params = query->handler->process_params(query_id, query->orig_params);

      redirected_query_id = "";
      redirect = query->handler->redirect(*query, redirected_query_id);
      if (redirect)
      {
        if (redirect_id_set.insert(redirected_query_id).second and (redirected_query_id != ""))
        {
          query_id = redirected_query_id;
        }
        else
        {
          throw SmartMet::Spine::Exception(
              BCP,
              "SmartMet::Plugin::WFS::StoredQuery::create_from_xml: invalid"
              " stored query redirect");
        }
      }
    } while (redirect);

    query->id = query_id;
    query->debug_format = spp.get_output_format() == "debug";

    bw::FeatureID feature_id(query_id, query->params->get_map(), 0);
    feature_id.add_param("source", query->handler->get_data_source());
    feature_id.add_param("language", language);
    if (query->debug_format)
      feature_id.add_param("debugFormat", 1);
    query->cache_key = feature_id.get_id();
    query->set_stale_seconds(query->handler->get_config()->get_expires_seconds());

    return query;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::StoredQuery> bw::StoredQuery::create_from_feature_id(
    const std::string& feature_id_str,
    const StoredQueryMap& sq_map,
    const bw::StoredQuery& orig_query)
{
  try
  {
    boost::shared_ptr<bw::FeatureID> feature_id = bw::FeatureID::create_from_id(feature_id_str);
    const std::string& query_id = feature_id->get_stored_query_id();

    boost::shared_ptr<bw::StoredQuery> query(new bw::StoredQuery);
    query->id = query_id;
    boost::shared_ptr<bw::RequestParameterMap> param_map(
        new RequestParameterMap(feature_id->get_params()));
    query->params = param_map;
    query->orig_params = param_map;
    query->handler = sq_map.get_handler_by_name(query_id);

    query->language = orig_query.language;
    query->debug_format = orig_query.debug_format;

    bw::FeatureID cache_feature_id(query_id, query->params->get_map(), 0);
    cache_feature_id.add_param("source", query->handler->get_data_source());
    cache_feature_id.add_param("language", query->language);
    if (query->debug_format)
      cache_feature_id.add_param("debugFormat", 1);
    query->cache_key = cache_feature_id.get_id();
    query->set_stale_seconds(query->handler->get_config()->get_expires_seconds());

    return query;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQuery::execute(std::ostream& output, const std::string& language) const
{
  try
  {
    assert(handler != nullptr);

    handler->query(*this, language, output);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const SmartMet::Spine::Value& bw::StoredQuery::get_param(const std::string& name) const
{
  try
  {
    auto range = params->get_map().equal_range(name);
    if (range.first == range.second)
    {
      SmartMet::Spine::Exception exception(BCP, "Mandatory parameter '" + name + "' missing!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    const SmartMet::Spine::Value& v = (range.first++)->second;

    if (range.first != range.second)
    {
      SmartMet::Spine::Exception exception(BCP, "'" + name + "' specified more than once!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    return v;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> bw::StoredQuery::get_param_values(const std::string& name) const
{
  try
  {
    std::vector<SmartMet::Spine::Value> result = params->get_values(name);
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::StoredQuery::get_kvp_stored_query_id(const SmartMet::Spine::HTTP::Request& http_request,
                                              std::string* stored_query_id)
{
  try
  {
    // Verify whether we have request=getFeature in HTTP request
    auto request = http_request.getParameter("request");

    if (not request)
      return false;

    if (not ba::iequals(*request, "getFeature") and not ba::iequals(*request, "getPropertyValue"))
      return false;

    // Verify whether we have storedquery_id in HTTP request parameters and return it
    // if found.
    auto sqi = http_request.getParameter("storedquery_id");
    if (sqi)
    {
      *stored_query_id = *sqi;
      return true;
    }
    else
    {
      return false;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::StoredQuery::get_xml_stored_query_id(const xercesc::DOMElement& query_root,
                                              std::string* stored_query_id)
{
  try
  {
    try
    {
      const std::string ns = bwx::to_string(query_root.getNamespaceURI());
      const std::string name = bwx::to_string(query_root.getLocalName());
      if ((ns == WFS_NAMESPACE_URI) and (name == "StoredQuery"))
      {
        const std::string id = bwx::get_mandatory_attr(query_root, WFS_NAMESPACE_URI, "id");
        *stored_query_id = id;
        return true;
      }
      return false;
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation failed!", nullptr);
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQuery::extract_kvp_parameters(const SmartMet::Spine::HTTP::Request& request,
                                             const StoredQueryConfig& config,
                                             bw::StoredQuery& query)
{
  try
  {
    const char* method = "SmartMet::Plugin::WFS::StoredQuery::extract_kvp_parameters";

    query.params.reset(new bw::RequestParameterMap);
    query.orig_params = query.params;
    query.skipped_params.clear();

    auto param_desc_map = config.get_param_descriptions();

    const auto& params = request.getParameterMap();

    std::multiset<std::string> param_name_set;
    std::set<std::string> forbidden;

    BOOST_FOREACH (const auto& item, params)
    {
      const std::string& param_name = Fmi::ascii_tolower_copy(item.first);
      const std::string& value_str = item.second;

      auto desc_it = param_desc_map.find(param_name);
      const auto& param_desc = desc_it->second;

      if (forbidden.count(param_name))
      {
        std::string sep = " ";
        std::ostringstream msg;
        msg << method << ": stored query parameter '" << item.first << "' conflicts with";
        for (auto it = param_desc.conflicts_with.begin(); it != param_desc.conflicts_with.end();
             ++it)
        {
          msg << sep << "'" << *it << "'";
          sep = ", ";
        }
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception;
      }

      const bw::StoredQueryParamDef& param_def = param_desc.param_def;
      if (desc_it == param_desc_map.end())
      {
        /* Skip unknown parameters (required to support GetFeatureById) */
        /* Save skipped parameters information however for logging purposes */
        std::ostringstream msg;
        msg << param_name << "="
            << "'" << value_str << "'";
        query.skipped_params.push_back(msg.str());
      }
      else
      {
        // Parameter is not yet processed, therefore compare with count + 1
        check_param_max_occurs(param_name_set.count(param_name) + 1,
                               param_desc.max_occurs,
                               "SmartMet::Plugin::WFS::StoredQuery::extract_kvp_parameters",
                               param_name);

        try
        {
          if (param_def.isArray())
          {
            std::vector<std::string> parts;
            ba::split(parts, value_str, ba::is_any_of(","));
            // FIXME: unescape parts if needed (for example escaped ','
            std::vector<SmartMet::Spine::Value> new_values = param_def.readValues(parts);
            BOOST_FOREACH (const auto& value, new_values)
            {
              value.check_limits(param_desc.lower_limit, param_desc.upper_limit);
              query.params->insert_value(param_name, value);
            }
          }
          else
          {
            std::vector<SmartMet::Spine::Value> vect;
            SmartMet::Spine::Value value = param_def.readValue(value_str);
            value.check_limits(param_desc.lower_limit, param_desc.upper_limit);
            query.params->insert_value(param_name, value);
          }

          for (auto it = param_desc.conflicts_with.begin(); it != param_desc.conflicts_with.end();
               ++it)
          {
            forbidden.insert(Fmi::ascii_tolower_copy(*it));
          }

          param_name_set.insert(param_name);
        }
        catch (...)
        {
          std::ostringstream msg;
          msg << "While parsing value '" << value_str << "' of request parameter '" << param_name
              << "'";
          SmartMet::Spine::Exception exception(BCP, "Operation failed!", nullptr);
          exception.addDetail(msg.str());
          throw exception;
        }
      }
    }

    // Now verify that all mandatory parameters are specified enough times
    BOOST_FOREACH (const auto& param_desc, param_desc_map)
    {
      check_param_min_occurs(param_name_set.count(param_desc.second.name),
                             param_desc.second.min_occurs,
                             "SmartMet::Plugin::WFS::StoredQuery::extract_xml_parameters",
                             param_desc.second.name);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQuery::extract_xml_parameters(const xercesc::DOMElement& query_root,
                                             const StoredQueryConfig& config,
                                             bw::StoredQuery& query)
{
  try
  {
    const char* method = "SmartMet::Plugin::WFS::StoredQuery::extract_xml_parameters";

    query.params.reset(new bw::RequestParameterMap);
    query.orig_params = query.params;
    query.skipped_params.clear();

    std::multiset<std::string> param_name_set;
    std::set<std::string> forbidden;

    std::vector<xercesc::DOMElement*> param_elements =
        bwx::get_child_elements(query_root, WFS_NAMESPACE_URI, "Parameter");

    const auto& param_desc_map = config.get_param_descriptions();

    BOOST_FOREACH (const xercesc::DOMElement* element, param_elements)
    {
      const std::string& param_name = bwx::get_mandatory_attr(*element, WFS_NAMESPACE_URI, "name");

      auto desc_it = param_desc_map.find(Fmi::ascii_tolower_copy(param_name));
      if (desc_it == param_desc_map.end())
      {
        /* Skip unknown parameters (required to support GetFeatureById) */
        /* Save skipped parameters information however for logging purposes */
        query.skipped_params.push_back(bwx::xml2string(element));
      }
      else
      {
        const auto& param_desc = desc_it->second;

        if (forbidden.count(param_name))
        {
          std::string sep = " ";
          std::ostringstream msg;
          msg << method << ": stored query parameter '" << param_name << "' conflicts with";
          for (auto it = param_desc.conflicts_with.begin(); it != param_desc.conflicts_with.end();
               ++it)
          {
            msg << sep << "'" << *it << "'";
            sep = ", ";
          }
          SmartMet::Spine::Exception exception(BCP, msg.str());
          throw exception;
        }

        for (auto it = param_desc.conflicts_with.begin(); it != param_desc.conflicts_with.end();
             ++it)
        {
          forbidden.insert(Fmi::ascii_tolower_copy(*it));
        }

        check_param_max_occurs(param_name_set.count(param_name) + 1,
                               param_desc.max_occurs,
                               "SmartMet::Plugin::WFS::StoredQuery::extract_xml_parameters",
                               param_name);

        const auto values = param_extractor.extract_param(*element, param_desc.xml_type);

        BOOST_FOREACH (const auto& value, values)
        {
          value.check_limits(param_desc.lower_limit, param_desc.upper_limit);
          query.params->insert_value(param_name, value);
        }

        param_name_set.insert(param_name);
      }
    }

    // Now verify that all mandatory parameters are specified enough times
    BOOST_FOREACH (const auto& param_desc, param_desc_map)
    {
      check_param_min_occurs(param_name_set.count(param_desc.second.name),
                             param_desc.second.min_occurs,
                             "SmartMet::Plugin::WFS::StoredQuery::extract_xml_parameters",
                             param_desc.second.name);
    }
  }
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!", nullptr);
    if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
    throw exception;
  }
}

void bw::StoredQuery::dump_query_info(std::ostream& output) const
{
  try
  {
    std::ostringstream msg;
    std::string sep = "";
    msg << "STORED QUERY: name='" << handler->get_query_name() << "', ";
    msg << "params={";

    // Collect parameter names for message
    const auto names = params->get_keys();

    BOOST_FOREACH (const auto& name, names)
    {
      msg << sep << name << "=(";
      auto range = params->get_map().equal_range(name);
      for (auto it = range.first; it != range.second; ++it)
      {
        msg << '(' << it->second << ')';
      }
      msg << ')';
      sep = "; ";
    }

    msg << "}, ";
    if (not skipped_params.empty())
    {
      sep = "";
      msg << "skipped_params=(";
      BOOST_FOREACH (const auto& param, skipped_params)
      {
        msg << sep << '"' << param << '"';
        sep = ", ";
      }
      msg << ')';
    }
    msg << "\n";

    output << msg.str() << std::flush;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void check_param_max_occurs(int actual_count,
                            int max_occurs,
                            const std::string& location,
                            const std::string& param_name)
{
  try
  {
    if (actual_count > max_occurs)
    {
      std::ostringstream msg;
      msg << location << ": parameter '" << param_name << "' specified more than ";
      if (max_occurs == 1)
      {
        msg << "once";
      }
      else
      {
        msg << max_occurs << " times";
      }
      msg << " (found " << actual_count << " occurances)";

      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void check_param_min_occurs(int actual_count,
                            int min_occurs,
                            const std::string& location,
                            const std::string& param_name)
{
  try
  {
    if (actual_count < min_occurs)
    {
      std::ostringstream msg;
      msg << location << ": parameter '" << param_name << "' must be specified at least "
          << min_occurs << " times (found " << actual_count << " occurrences).";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& bw::operator<<(
    std::ostream& ost, const std::map<std::string, std::vector<SmartMet::Spine::Value> >& params)
{
  try
  {
    ost << '{';
    BOOST_FOREACH (const auto& item, params)
    {
      ost << "('" << item.first << "' ==> (";
      std::copy(item.second.begin(),
                item.second.end(),
                std::ostream_iterator<SmartMet::Spine::Value>(ost));
      ost << ')';
    }
    ost << '}';
    return ost;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
