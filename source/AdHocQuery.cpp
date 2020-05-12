#include "AdHocQuery.h"
#include "FeatureID.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XPathSnapshot.h"
#include "XmlUtils.h"
#include <boost/lambda/lambda.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>

XERCES_CPP_NAMESPACE_USE;

#define FES_NAMESPACE_URI "http://www.opengis.net/fes/2.0"

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;
namespace bl = boost::lambda;

static bool is_query_parameter(const std::string& element_name);

static bool is_comparison_operation(const std::string& element_name);

static bool read_value_reference(const SmartMet::Spine::Value& value, std::string& value_reference);

static void get_comparison_operation(const std::string& element_name, std::string& operation);

static void check_param_max_occurs(int actual_count,
                                   int max_occurs,
                                   const std::string& location,
                                   const std::string& param_name);

bw::AdHocQuery::AdHocQuery() {}
bw::AdHocQuery::~AdHocQuery() {}
bw::QueryBase::QueryType bw::AdHocQuery::get_type() const
{
  return QUERY;
}

void bw::AdHocQuery::create_from_kvp(const std::string& language,
                                     const StandardPresentationParameters& spp,
                                     const SmartMet::Spine::HTTP::Request& http_request,
                                     const bw::StoredQueryMap& stored_query_map,
                                     const TypeNameStoredQueryMap& typename_storedquery_map,
                                     std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    boost::optional<std::string> request;
    boost::optional<std::string> bbox;
    boost::optional<std::string> resource_id;
    boost::optional<std::string> filter;
    boost::optional<std::string> filter_language;
    std::vector<std::string> property_names;
    std::vector<std::string> type_names;
    std::vector<std::string> aliases;

    const auto& params = http_request.getParameterMap();

    BOOST_FOREACH (const auto& item, params)
    {
      const std::string& param_name = Fmi::ascii_toupper_copy(item.first);

      if (param_name == "REQUEST")
      {
        request = item.second;
        continue;
      }

      if (param_name == "BBOX")
      {
        bbox = item.second;
        continue;
      }

      if (param_name == "RESOURCEID")
      {
        resource_id = item.second;
        continue;
      }

      if (param_name == "FILTER")
      {
        filter = item.second;
        continue;
      }

      if (param_name == "FILTER_LANGUAGE")
      {
        filter_language = item.second;
        continue;
      }

      if (param_name == "PROPERTYNAME")
      {
        ba::split(property_names, item.second, ba::is_any_of(","));
        continue;
      }

      if (param_name == "TYPENAMES")
      {
        ba::split(type_names, item.second, ba::is_any_of(","));
        continue;
      }

      if (param_name == "ALIASES")
      {
        ba::split(aliases, item.second, ba::is_any_of(","));
        continue;
      }
    }

    // Sanity check: not a request
    if (not request)
    {
      SmartMet::Spine::Exception exception(BCP, "Not a request!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }

    // Sanity check: not get feature or get property value
    if (not ba::iequals(*request, "getFeature") and not ba::iequals(*request, "getPropertyValue"))
    {
      SmartMet::Spine::Exception exception(BCP,
                                           "Expected 'getFeature' or 'getPropertyValue' request!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }

    // Sanity check: have typenames?
    if (type_names.empty())
    {
      SmartMet::Spine::Exception exception(BCP, "No 'TYPENAMES' parameter found!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }

    // Find stored query ids
    std::vector<std::string> stored_query_ids;
    bw::AdHocQuery::get_stored_query_ids_from_typenames(
        typename_storedquery_map, type_names, stored_query_ids);

    // Nothing to do, if no query-ids found
    if (stored_query_ids.empty())
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to extract 'storedquery_id'!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }

    if (bbox)
    {
      bw::AdHocQuery::create_bbox_query_from_kvp(
          *bbox, language, stored_query_ids, spp, stored_query_map, queries);
    }
    else if (resource_id)
    {
      SmartMet::Spine::Exception exception(BCP, "RESOURCEID not implemented!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }
    else if (filter)
    {
      bw::AdHocQuery::create_filter_query_from_kvp(
          *filter, filter_language, language, stored_query_ids, spp, stored_query_map, queries);
    }
    else
    {
      // No filter or parameters were defined, create query/ies with default values.
      BOOST_FOREACH (const auto& query_id, stored_query_ids)
      {
        std::vector<boost::shared_ptr<bw::QueryBase>> batch_of_queries;

        // Get stored query handler object by storedquery_id parameter value
        boost::shared_ptr<bw::StoredQueryHandlerBase> handler =
            stored_query_map.get_handler_by_name(query_id);

        // Create query object to the query list.
        bw::AdHocQuery::create_query(handler, batch_of_queries);

        bw::AdHocQuery::process_parms(language, query_id, spp, batch_of_queries);

        queries.insert(queries.end(), batch_of_queries.begin(), batch_of_queries.end());
      }
    }

    // Add possible projection clause to queries.
    bw::AdHocQuery::add_projection_clause(property_names, queries);

    // Replace possible aliases with actual type names.
    bw::AdHocQuery::replace_aliases(aliases, type_names, queries);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::create_filter_query_from_kvp(
    const std::string& filter,
    const boost::optional<std::string>& filter_language,
    const std::string& language,
    const std::vector<std::string>& stored_query_ids,
    const StandardPresentationParameters& spp,
    const bw::StoredQueryMap& stored_query_map,
    std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // FILTER_LANGUAGE defaults to fes:Filter. Something else -> not implemented
    if (not filter_language ||
        (filter_language && *filter_language == "urn:ogc:def:query Language:OGC-FES:Filter"))
    {
      bwx::XPathSnapshot xps;
      xps.parse_dom_document(filter, "fes:Filter:TMP");

      const auto* root = xps.get_document()->getDocumentElement();
      if (root == nullptr)
      {
        SmartMet::Spine::Exception exception(BCP, "Failed to access filter 'TMP' element!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }

      const std::string& root_name = XMLString::transcode(root->getTagName());

      if (root_name != "fes:Filter")
      {
        SmartMet::Spine::Exception exception(BCP, "Failed to parse filter xml!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }

      BOOST_FOREACH (const auto& query_id, stored_query_ids)
      {
        std::vector<boost::shared_ptr<bw::QueryBase>> batch_of_queries;

        // Get stored query handler object by storedquery_id parameter value
        boost::shared_ptr<bw::StoredQueryHandlerBase> handler =
            stored_query_map.get_handler_by_name(query_id);

        // Store element name to element tree.
        std::vector<std::string> element_tree;
        element_tree.push_back(Fmi::ascii_tolower_copy(root_name));

        // Parse xml ad hoc query.
        bw::AdHocQuery::extract_xml_parameters(*root, handler, element_tree, batch_of_queries);

        // Set parms
        bw::AdHocQuery::process_parms(language, query_id, spp, batch_of_queries);

        queries.insert(queries.end(), batch_of_queries.begin(), batch_of_queries.end());
      }
    }
    else
    {
      SmartMet::Spine::Exception exception(BCP, "Selected 'FILTER_LANGUAGE' not implemented!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::create_bbox_query_from_kvp(
    const std::string& bbox_string,
    const std::string& language,
    const std::vector<std::string>& stored_query_ids,
    const StandardPresentationParameters& spp,
    const bw::StoredQueryMap& stored_query_map,
    std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    BOOST_FOREACH (const auto& query_id, stored_query_ids)
    {
      boost::shared_ptr<bw::StoredQueryHandlerBase> handler =
          stored_query_map.get_handler_by_name(query_id);

      // Does this query support BBOX ???
      const std::string param_name = "bbox";
      boost::shared_ptr<const SmartMet::Plugin::WFS::StoredQueryConfig> config =
          handler->get_config();
      const auto& param_desc_map = config->get_param_descriptions();
      auto desc_it = param_desc_map.find(param_name);

      if (desc_it == param_desc_map.end())
      {
        // This stored query does not support BBOX, nothing to do.
        std::ostringstream msg;
        msg << param_name << "="
            << "'" << bbox_string << "'";
        SmartMet::Spine::Exception exception(BCP, "Stored query does not support BBOX!");
        exception.addDetail(msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }
      else
      {
        try
        {
          const auto& param_desc = desc_it->second;
          const bw::StoredQueryParamDef& param_def = param_desc.param_def;
          SmartMet::Spine::Value value = param_def.readValue(bbox_string);
          value.check_limits(param_desc.lower_limit, param_desc.upper_limit);

          boost::shared_ptr<bw::AdHocQuery> query(new bw::AdHocQuery);
          query->handler = handler;
          query->id = query_id;
          query->debug_format = spp.get_output_format() == "debug";
          query->language = language;
          query->set_stale_seconds(config->get_expires_seconds());

          query->params.reset(new bw::RequestParameterMap);
          query->orig_params = query->params;
          query->skipped_params.clear();

          query->params->insert_value(param_name, value);

          query->params = query->handler->process_params(query_id, query->orig_params);

          bw::FeatureID feature_id(query_id, query->params->get_map(), 0);
          feature_id.add_param("language", language);

          if (query->debug_format)
            feature_id.add_param("debugFormat", 1);

          query->cache_key = feature_id.get_id();

          queries.push_back(query);
        }
        catch (...)
        {
          std::ostringstream msg;
          msg << "While parsing value '" << bbox_string << "' of request parameter '" << param_name
              << "'";

          SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!", nullptr);
          exception.addDetail(msg.str());
          if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
          exception.addParameter(WFS_LANGUAGE, language);
          throw exception;
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::create_from_xml(const std::string& language,
                                     const StandardPresentationParameters& spp,
                                     const xercesc::DOMElement& query_root,
                                     const bw::StoredQueryMap& stored_query_map,
                                     const bw::TypeNameStoredQueryMap& typename_storedquery_map,
                                     std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    std::vector<std::string> type_names;

    // Try to get the stored query id associated with the typeNames attribute of the query.
    get_xml_typenames(query_root, type_names);

    // Find stored query ids
    std::vector<std::string> stored_query_ids;
    bw::AdHocQuery::get_stored_query_ids_from_typenames(
        typename_storedquery_map, type_names, stored_query_ids);

    // Sanity check: do we have anything to go on?
    if (stored_query_ids.empty())
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to get stored query ID!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }

    // Find the the Filter element in the query.
    std::vector<xercesc::DOMElement*> filter_root =
        bwx::get_child_elements(query_root, FES_NAMESPACE_URI, "Filter");

    BOOST_FOREACH (const auto& query_id, stored_query_ids)
    {
      std::vector<boost::shared_ptr<bw::QueryBase>> batch_of_queries;

      // Get stored query handler object by storedquery_id parameter value
      boost::shared_ptr<bw::StoredQueryHandlerBase> handler =
          stored_query_map.get_handler_by_name(query_id);

      // Is there any Filter element defined?
      if (not filter_root.empty())
      {
        // Filter is defined, we need to parse it.

        const std::string& root_name = XMLString::transcode(filter_root[0]->getTagName());

        // Store element name to element tree.
        std::vector<std::string> element_tree;
        element_tree.push_back(Fmi::ascii_tolower_copy(root_name));

        // Parse xml ad hoc query.
        bw::AdHocQuery::extract_xml_parameters(
            *filter_root[0], handler, element_tree, batch_of_queries);
      }
      else
      {
        // No filter were defined, create query with default values.
        bw::AdHocQuery::create_query(handler, batch_of_queries);
      }

      bw::AdHocQuery::process_parms(language, query_id, spp, batch_of_queries);

      queries.insert(queries.end(), batch_of_queries.begin(), batch_of_queries.end());
    }

    // Add possible projection clause to queries.
    bw::AdHocQuery::add_projection_clause_from_xml(query_root, queries);

    // Replace possible aliases with actual type names.
    bw::AdHocQuery::replace_aliases_from_xml(query_root, queries);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::get_xml_typenames(const xercesc::DOMElement& query_root,
                                       std::vector<std::string>& typenames)
{
  try
  {
    const std::string ns = bwx::to_string(query_root.getNamespaceURI());
    const std::string name = bwx::to_string(query_root.getLocalName());

    if ((ns == WFS_NAMESPACE_URI) and (name == "Query"))
    {
      const std::string type_name =
          bwx::get_mandatory_attr(query_root, WFS_NAMESPACE_URI, "typeNames");
      ba::split(typenames, type_name, ba::is_any_of(" "));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::extract_xml_parameters(
    const xercesc::DOMElement& root_element,
    boost::shared_ptr<const bw::StoredQueryHandlerBase> handler,
    std::vector<std::string>& element_tree,
    std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // List for storing first query object indexes of OR elements.
    std::vector<unsigned int> or_query_indexes;

    // Create first query object to the query list.
    bw::AdHocQuery::create_query(handler, queries);

    // Parse filter xml elements in the query.
    bw::AdHocQuery::extract_filter_elements(
        root_element, element_tree, queries, nullptr, or_query_indexes);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::create_query(boost::shared_ptr<const bw::StoredQueryHandlerBase> handler,
                                  std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    boost::shared_ptr<bw::AdHocQuery> query(new bw::AdHocQuery);

    query->params.reset(new bw::RequestParameterMap);
    query->orig_params = query->params;
    query->skipped_params.clear();
    query->handler = handler;

    queries.push_back(query);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::add_projection_clause_from_xml(
    const xercesc::DOMElement& query_root, std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // Find the PropertyName elements in the query.
    std::vector<xercesc::DOMElement*> property_elements =
        bwx::get_child_elements(query_root, WFS_NAMESPACE_URI, "PropertyName");

    std::vector<std::string> property_names;

    BOOST_FOREACH (const xercesc::DOMElement* property, property_elements)
    {
      // Read PropertyName-element value.
      const auto values = param_extractor.extract_param(*property, "xsi:string");
      property_names.push_back(values[0].get_string());
    }

    bw::AdHocQuery::add_projection_clause(property_names, queries);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::add_projection_clause(const std::vector<std::string>& property_names,
                                           std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // Was there any property names defined?
    if (not property_names.empty())
    {
      std::string projection = "//(";

      // Loop through the property names.
      for (unsigned int i = 0; i < property_names.size(); i++)
      {
        projection += property_names[i];

        // If not the last propertyname, add " | " to the projection string.
        if (i != (property_names.size() - 1))
        {
          projection += " | ";
        }
      }

      projection += ")";

      // Loop through the queries and add projection clause to each of them.
      BOOST_FOREACH (boost::shared_ptr<bw::QueryBase> base_query, queries)
      {
        AdHocQuery* query = &dynamic_cast<AdHocQuery&>(*base_query);

        // Add projection clause to the end of the filtering expression so that the syntax is
        // "//wfs:member["filtering conditions"]//(PropertyName1 | PropertyName2...)" or just
        // "//(PropertyName1 | PropertyName2...)" if no filtering conditions were defined.
        query->filter_expression += projection;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::replace_aliases_from_xml(
    const xercesc::DOMElement& query_root, std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // Read value of attribute aliases.
    auto attr_aliases = bwx::get_attr(query_root, WFS_NAMESPACE_URI, "aliases");

    // Is there attribute aliases?
    if ((attr_aliases.second) && (not attr_aliases.first.empty()))
    {
      // Get the different aliases.
      std::vector<std::string> aliases;
      ba::split(aliases, attr_aliases.first, ba::is_any_of(" "));

      // Read value of attribute typeNames.
      auto attr_type_names = bwx::get_attr(query_root, WFS_NAMESPACE_URI, "typeNames");

      // Get the different type names.
      std::vector<std::string> type_names;
      ba::split(type_names, attr_type_names.first, ba::is_any_of(" "));

      bw::AdHocQuery::replace_aliases(aliases, type_names, queries);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::replace_aliases(const std::vector<std::string>& aliases,
                                     const std::vector<std::string>& type_names,
                                     std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // Is there any aliases?
    if (not aliases.empty())
    {
      // There should be as many aliases as there are type names.
      if (aliases.size() != type_names.size())
      {
        std::ostringstream msg;
        msg << "The number of aliases does not match the number of type names.";
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }

      // Loop through the queries and replace aliases in each of the filtering expressions.
      BOOST_FOREACH (boost::shared_ptr<bw::QueryBase> base_query, queries)
      {
        AdHocQuery* query = &dynamic_cast<AdHocQuery&>(*base_query);

        // Is there a filtering expression that would need alias replacing.
        if (not query->filter_expression.empty())
        {
          // Replace each of the aliases with relevant type name.
          for (unsigned int i = 0; i < aliases.size(); i++)
          {
            if (not aliases[i].empty())
            {
              std::string old_expr = aliases[i] + "/";
              std::string new_expr = type_names[i] + "/";

              ba::replace_all(query->filter_expression, old_expr, new_expr);
            }
          }  // for (...)
        }
      }  // BOOST_FOREACH(...)
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::process_parms(const std::string& language,
                                   const std::string& query_id,
                                   const StandardPresentationParameters& spp,
                                   std::vector<boost::shared_ptr<bw::QueryBase>>& queries)
{
  try
  {
    // Loop through the created queries and process parameters etc.
    BOOST_FOREACH (boost::shared_ptr<bw::QueryBase> base_query, queries)
    {
      AdHocQuery* query = &dynamic_cast<AdHocQuery&>(*base_query);

      query->language = language;
      query->params = query->handler->process_params(query_id, query->orig_params);
      query->id = query_id;
      query->debug_format = spp.get_output_format() == "debug";

      bw::FeatureID feature_id(query_id, query->params->get_map(), 0);
      feature_id.add_param("language", language);

      if (query->debug_format)
        feature_id.add_param("debugFormat", 1);

      query->cache_key = feature_id.get_id();
      query->set_stale_seconds(query->handler->get_config()->get_expires_seconds());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::extract_filter_elements(const xercesc::DOMElement& root_element,
                                             std::vector<std::string>& element_tree,
                                             std::vector<boost::shared_ptr<QueryBase>>& queries,
                                             AdHocQuery* upper_level_and_query,
                                             std::vector<unsigned int>& or_query_indexes)
{
  try
  {
    std::multiset<std::string> param_name_set;
    std::set<std::string> forbidden;
    bool isFirstOrQuery = true;

    // Get the child filter elements for the current root element.
    std::vector<xercesc::DOMElement*> filter_elements =
        bwx::get_child_elements(root_element, "*", "*");

    // Loop through the filter elements.
    BOOST_FOREACH (const xercesc::DOMElement* element, filter_elements)
    {
      const std::string& tag_name = XMLString::transcode(element->getTagName());
      std::string element_name = Fmi::ascii_tolower_copy(tag_name);

      // fes:Not filtering element is not supported.
      if (element_name == "fes:not")
      {
        SmartMet::Spine::Exception exception(BCP,
                                             "The 'fes:not' filtering element is not supported!");
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }

      // Get pointer to the last query in the list.
      AdHocQuery* query = &dynamic_cast<AdHocQuery&>(*queries.back());

      // Check if the current element is inside OR-element and the last query in the list is
      // already used (contains parameters/filtering expressions) or it is reserved as the query for
      // AND-element.
      if ((element_tree.back() == "fes:or") &&
          (is_query_used(query) || (query == upper_level_and_query)))
      {
        // New query object needs to be created for the current element.
        bw::AdHocQuery::create_query(query->handler, queries);

        // Get pointer to the last query in the list.
        query = &dynamic_cast<AdHocQuery&>(*queries.back());

        param_name_set.clear();
        forbidden.clear();

        // Check if this element is the first element inside OR-element and it actually represents
        // some query
        // parameter (it is not AND/OR element) and also that the OR-element itself is inside
        // AND-element.
        // This is checked because we need to memorize the starting locations of query objects for
        // different OR-elements
        // inside AND-element so that we can later combine the parameters/filtering conditions of
        // for
        // example
        // the following structure:
        //
        // <fes:And>
        //    <fes:Or>
        //        <fes:BBOX> </fes:BBOX>
        //        <fes:BBOX> </fes:BBOX>
        //    </fes:Or>
        //    <fes:Or>
        //        <fes:During> </fes:During>
        //        <fes:During> </fes:During>
        //    </fes:Or>
        // </fes:And>
        //
        if (isFirstOrQuery && (element_tree[element_tree.size() - 2] == "fes:and") &&
            (element_name != "fes:and") && (element_name != "fes:or"))
        {
          isFirstOrQuery = false;

          // Store the index of the current query.
          or_query_indexes.push_back(queries.size() - 1);
        }
      }

      // Check if this element is not any element which contains query parameters / comparison
      // operations and
      // so it in turn needs to be parsed for its child filter elements.
      if (!is_query_parameter(element_name) && !is_comparison_operation(element_name))
      {
        AdHocQuery* and_query = nullptr;
        unsigned int and_query_index = 0;

        if (element_name == "fes:and")
        {
          // Create a query object which is used for storing parameters which are inside this
          // current
          // AND-element
          // (Is not used to contain parameters inside possible OR-elements). For example see
          // structure below:
          //
          // <fes:And>
          //    <fes:BBOX> </fes:BBOX>
          //    <fes:During> </fes:During>
          // </fes:And>
          //
          and_query = &dynamic_cast<AdHocQuery&>(*queries.back());
          and_query_index = queries.size() - 1;
        }
        else if (element_tree.back() != "fes:or")
        {
          // Otherwise if the current element is not inside OR-element then use the query from
          // parent
          // element for storing
          // parameters inside AND-element.
          and_query = upper_level_and_query;
        }

        // Store the current element name to list.
        element_tree.push_back(element_name);

        // Parse the child elements of the current element.
        bw::AdHocQuery::extract_filter_elements(
            *element, element_tree, queries, and_query, or_query_indexes);

        // Remove the current element name from list.
        element_tree.pop_back();

        // Did we finish parsing AND-element?
        if (element_name == "fes:and")
        {
          handle_end_of_and_element(queries, or_query_indexes, and_query_index, and_query);
        }
      }
      else  // Read parameter values / comparison operations from XML.
      {
        // Check if we need to write this parameter to the common AND-element query.
        if ((upper_level_and_query != nullptr) && (element_tree.back() != "fes:or"))
        {
          query = upper_level_and_query;
        }

        if (is_comparison_operation(element_name))
        {
          read_comparison_operation(*element, element_name, query);
        }
        else  // is_query_parameter
        {
          read_query_parameter(
              *element, element_tree, element_name, param_name_set, forbidden, query);
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::handle_end_of_and_element(std::vector<boost::shared_ptr<QueryBase>>& queries,
                                               std::vector<unsigned int>& or_query_indexes,
                                               unsigned int and_query_index,
                                               AdHocQuery* and_query)
{
  try
  {
    // Was there at least two OR-elements inside this AND-element?
    if (or_query_indexes.size() >= 2)
    {
      // Now we need to combine the parameters of the query objects created for these OR-elements.

      // Memorize the number of queries as it increases when we create new queries with combined
      // parameters.
      unsigned int queries_size_1 = queries.size();
      unsigned int queries_size_2 = 0;

      // Loop through the query object sets created for parameters for inside each OR-element.
      // For example the following structure has yielded the following query sets:
      //                                       Query:        Query set:
      // <fes:And>
      //    <fes:Or>
      //        <fes:BBOX> </fes:BBOX>      => queries[0] => or_query_indexes[0] = 0
      //        <fes:BBOX> </fes:BBOX>      => queries[1]
      //    </fes:Or>
      //    <fes:Or>
      //        <fes:During> </fes:During>  => queries[3] => or_query_indexes[1] = 3
      //        <fes:During> </fes:During>  => queries[4]
      //    </fes:Or>
      // </fes:And>
      //
      for (unsigned int or_set = 0; or_set < or_query_indexes.size();
           or_set = (or_set == 0) ? or_set + 2 : or_set + 1)
      {
        // Memorize the number of queries as it increases when we create new queries with combined
        // parameters.
        queries_size_2 = queries.size();

        // Index is the index of the first query of the next set or if this is the last set
        // then it is the index of last query (which existed before combining).
        auto last_index_1 = (or_set < (or_query_indexes.size() - 1)) ? or_query_indexes[or_set + 1]
                                                                     : queries_size_1;

        // Loop through the queries belonging to this set.
        for (auto index_1 = or_query_indexes[or_set]; index_1 < last_index_1; index_1++)
        {
          // Get pointer to query in the current set.
          const AdHocQuery* or_query_1 = &dynamic_cast<AdHocQuery&>(*queries[index_1]);

          // If the set is 0 then the queries in this set are combined with the queries in set 1,
          // otherwise they are combined
          // with the combined queries resulting from last round (locating between indexes
          // queries_size_1..queries_size_2).
          auto first_index_2 = (or_set == 0) ? or_query_indexes[1] : queries_size_1;
          auto last_index_2 = ((or_set == 0) && (or_query_indexes.size() > 2))
                                  ? or_query_indexes[or_set + 2]
                                  : queries_size_2;

          // Loop through the queries belonging to the next set.
          for (auto index_2 = first_index_2; index_2 < last_index_2; index_2++)
          {
            // Get pointer to query of the next set.
            const AdHocQuery* or_query_2 = &dynamic_cast<AdHocQuery&>(*queries[index_2]);

            // Create a new query for containing the combined parameters.
            bw::AdHocQuery::create_query(and_query->handler, queries);
            AdHocQuery* combined_query = &dynamic_cast<AdHocQuery&>(*queries.back());

            // Copy parameters.
            copy_params(or_query_1, combined_query);
            copy_params(or_query_2, combined_query);
          }
        }

        // Delete the combined queries resulting from the previous round,
        queries.erase(queries.begin() + queries_size_1, queries.begin() + queries_size_2);
      }

      // Erase the OR-element queries which existed before combining queries.
      queries.erase(queries.begin() + or_query_indexes[0], queries.begin() + queries_size_1);
    }

    // Clear the list of query set indexes.
    or_query_indexes.clear();

    // Is the query reserved for AND-element parameters empty?
    if (!is_query_used(and_query))
    {
      // Delete the unused AND-element query.
      queries.erase(queries.begin() + and_query_index);
    }
    // Is there any other queries originating from the AND-element (OR-element queries inside the
    // AND-element)?
    else if (queries.size() > (and_query_index + 1))
    {
      // If so then we need to combine this common AND-element query with the OR-element queries.

      // Loop through the OR-element queries.
      for (auto or_query_index = (and_query_index + 1); or_query_index < queries.size();
           or_query_index++)
      {
        AdHocQuery* or_query = &dynamic_cast<AdHocQuery&>(*queries[or_query_index]);

        // Copy AND-query parameters to OR-query.
        copy_params(and_query, or_query);
      }

      // Delete the now unneeded AND-query.
      queries.erase(queries.begin() + and_query_index);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::read_comparison_operation(const xercesc::DOMElement& element,
                                               const std::string& element_name,
                                               AdHocQuery* query)
{
  try
  {
    unsigned char nb_of_value_references = 0;
    std::string reference = "";
    std::string limit_value = "";
    std::string operation = "";
    std::string new_filter = "";
    bool is_value_string = false;
    bool is_reference_attribute = false;
    bool is_2nd_reference_attribute = false;
    bool match_case = true;
    enum MatchActionType
    {
      ALL,
      ANY,
      ONE
    } match_action = ANY;

    // Read value of attribute matchCase.
    auto attr_match_case = bwx::get_attr(element, FES_NAMESPACE_URI, "matchCase");
    if (attr_match_case.second)
    {
      if (attr_match_case.first == "false")
      {
        match_case = false;
      }
    }

    // Read value of attribute matchAction.
    auto attr_match_action = bwx::get_attr(element, FES_NAMESPACE_URI, "matchAction");
    if (attr_match_action.second)
    {
      if (attr_match_action.first == "All")
      {
        match_action = ALL;
      }
      else if (attr_match_action.first == "Any")
      {
        match_action = ANY;
      }
      else if (attr_match_action.first == "One")
      {
        match_action = ONE;
      }
    }

    // Get the comparison operation related to the current filter element.
    get_comparison_operation(element_name, operation);

    // Get the child elements for the current filter element.
    std::vector<xercesc::DOMElement*> child_elements =
        bwx::get_child_elements(element, FES_NAMESPACE_URI, "*");

    // Loop through the filter child elements.
    BOOST_FOREACH (const xercesc::DOMElement* child_element, child_elements)
    {
      // Read element name.
      const std::string& tag_name = XMLString::transcode(child_element->getTagName());
      std::string child_name = Fmi::ascii_tolower_copy(tag_name);

      // Read element value.
      const auto values = param_extractor.extract_param(*child_element, "xsi:string");

      if (child_name == "fes:valuereference")
      {
        nb_of_value_references++;

        // Is this the first ValueReference element in the query?
        if (nb_of_value_references == 1)
        {
          is_reference_attribute = read_value_reference(values[0], reference);
        }
        else  // The comparison is between two ValueReference elements.
        {
          // Put the value of second ValueReference element to limit_value.
          is_2nd_reference_attribute = read_value_reference(values[0], limit_value);
        }
      }
      else if (child_name == "fes:literal")
      {
        limit_value = values[0].get_string();

        // Check if limit_value is a string.
        char* pEnd;
        (void)std::strtod(limit_value.c_str(), &pEnd);
        if (pEnd != nullptr)
        {
          if ((strlen(pEnd) > 0) || (limit_value == "NaN"))
          {
            is_value_string = true;

            // Should value be changed to lower case value?
            if (match_case == false)
            {
              limit_value = Fmi::ascii_tolower_copy(limit_value);
            }

            limit_value = "'" + limit_value + "'";
          }
        }
      }
    }

    // XPath comparison between two node-sets compares string values of the nodes. As it is not
    // known
    // whether the nodes contain actually string or numerical values we'll use string comparison for
    // operations "=" and "!=" (which should work also for numerial values if there are not too many
    // trailing zeros)
    // and numerical comparison for operations "<", ">", "<=" and ">=" (which aren't very practical
    // for string values anyway).
    if ((nb_of_value_references == 2) && (operation != "=") && (operation != "!="))
    {
      reference = "number(" + reference + ")";
      limit_value = "number(" + limit_value + ")";
    }
    // Is caseless search requested?
    // (This is else branch because XPath query "lower-case(number(...))" causes exception.)
    else if ((match_case == false)
             // Avoid XPath query "lower-case(...) for elements with numerical values because it
             // causes exception.
             // But it can be used when comparing two ValueReference elements.
             && ((is_value_string == true) || (nb_of_value_references == 2)))
    {
      // Avoid XPath query "lower-case(descendant::*/@attribute)=..." because it causes exception.
      if (is_reference_attribute == false)
      {
        reference = "lower-case(" + reference + ")";
      }

      if ((nb_of_value_references == 2) && (is_2nd_reference_attribute == false))
      {
        limit_value = "lower-case(" + limit_value + ")";
      }
    }

    // Choose filtering expression syntax according to requested match action.
    if (match_action == ALL)
    {
      // Filtering expression is of type: //wfs:member[every $x in descendant::REFERENCE satisfies
      // $x
      // = VALUE].

      new_filter = "every $x in " + reference + " satisfies $x " + operation + " " + limit_value;
    }
    else if (match_action == ONE)
    {
      // Filtering expression is of type: //wfs:member[count(descendant::REFERENCE[. = VALUE]) = 1].

      new_filter = "count(" + reference + "[. " + operation + " " + limit_value + "]) = 1";
    }
    else  // match_action == ANY
    {
      // Filtering expression is of type: //wfs:member[descendant::REFERENCE = VALUE].

      new_filter = reference + " " + operation + " " + limit_value;
    }

    // Is this the first expression for this query?
    if (query->filter_expression.empty())
    {
      // Set this expression as the first filtering expression for this query.
      query->filter_expression = "//wfs:member[(" + new_filter + ")]";
    }
    else
    {
      // Delete the last character which is "]".
      query->filter_expression.erase(query->filter_expression.end() - 1);
      // Add this expression to the end of the existing query expression.
      query->filter_expression += " and (" + new_filter + ")]";
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::read_query_parameter(const xercesc::DOMElement& element,
                                          const std::vector<std::string>& element_tree,
                                          const std::string& element_name,
                                          std::multiset<std::string>& param_name_set,
                                          std::set<std::string>& forbidden,
                                          AdHocQuery* query)
{
  try
  {
    const char* method = "SmartMet::Plugin::WFS::AdHocQuery::read_query_parameter";
    const std::string& local_name = XMLString::transcode(element.getLocalName());
    std::string param_name = Fmi::ascii_tolower_copy(local_name);

    // Decide parameter names/types from XML element names.
    std::string xml_type = "";
    if (element_name == "fes:bbox")
    {
      xml_type = "xsi:Envelope";
    }
    else if (element_name == "gml:timeposition")
    {
      xml_type = "xsi:dateTime";

      auto tree_size = element_tree.size();
      if (tree_size >= 5)
      {
        if ((element_tree[tree_size - 4] == "fes:during") &&
            (element_tree[tree_size - 3] == "gml:timeperiod") &&
            (element_tree[tree_size - 1] == "gml:timeinstant"))
        {
          if (element_tree[tree_size - 2] == "gml:begin")
          {
            param_name = "starttime";
          }
          else if (element_tree[tree_size - 2] == "gml:end")
          {
            param_name = "endtime";
          }
        }
      }
    }

    boost::shared_ptr<const SmartMet::Plugin::WFS::StoredQueryConfig> config =
        query->handler->get_config();
    const auto& param_desc_map = config->get_param_descriptions();
    auto desc_it = param_desc_map.find(Fmi::ascii_tolower_copy(param_name));

    if (desc_it == param_desc_map.end())
    {
      // Skip unknown parameters (required to support GetFeatureById)
      // Save skipped parameters information however for logging purposes
      query->skipped_params.push_back(bwx::xml2string(&element));
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
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        throw exception.disableStackTrace();
      }

      for (auto it = param_desc.conflicts_with.begin(); it != param_desc.conflicts_with.end(); ++it)
      {
        forbidden.insert(Fmi::ascii_tolower_copy(*it));
      }

      check_param_max_occurs(param_name_set.count(param_name) + 1,
                             param_desc.max_occurs,
                             "SmartMet::Plugin::WFS::AdHocQuery::extract_filter_elements",
                             param_name);

      // If the xml type is yet undefined, use the type in the parameter description.
      if (xml_type == "")
      {
        xml_type = param_desc.xml_type;
      }

      // Read parameter value from XML.
      const auto values = param_extractor.extract_param(element, xml_type);

      BOOST_FOREACH (const auto& value, values)
      {
        value.check_limits(param_desc.lower_limit, param_desc.upper_limit);

        // Write parameter value to query.
        query->params->insert_value(param_name, value);
      }

      param_name_set.insert(param_name);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::copy_params(const AdHocQuery* src_query, AdHocQuery* target_query)
{
  try
  {
    const RequestParameterMap& src_query_param_map = src_query->get_param_map();
    const std::set<std::string> src_query_param_map_keys = src_query_param_map.get_keys();

    // Copy parameters from src_query to target_query.
    BOOST_FOREACH (const auto& src_query_param_key, src_query_param_map_keys)
    {
      target_query->params->insert_value(src_query_param_key,
                                         src_query->get_param(src_query_param_key));
    }

    // Is there some filtering expression in src_query to copy?
    if (!src_query->filter_expression.empty())
    {
      // No filtering expression defined for target_query?
      if (target_query->filter_expression.empty())
      {
        // Copy the whole filtering expression from src_query to target_query.
        target_query->filter_expression = src_query->filter_expression;
      }
      else
      {
        // Delete the last character which is "]".
        target_query->filter_expression.erase(target_query->filter_expression.end() - 1);
        target_query->filter_expression += " and ";

        // Find the start of src_query filter expression which is the "[".
        std::size_t start_of_src = src_query->filter_expression.find_first_of("[") + 1;
        std::size_t end_of_src = src_query->filter_expression.size();
        if (start_of_src != std::string::npos)
        {
          // Add src_query expression to the end of target_query expression.
          target_query->filter_expression +=
              src_query->filter_expression.substr(start_of_src, end_of_src);
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::filter(boost::shared_ptr<QueryBase> query, bwx::XPathSnapshot& xps)
{
  try
  {
    AdHocQuery* ad_hoc_query = &dynamic_cast<AdHocQuery&>(*query);

    // If there is no filtering expression defined then set it to default one.
    if (ad_hoc_query->filter_expression.empty())
    {
      ad_hoc_query->filter_expression = "//wfs:member";
    }

    xps.xpath_query(ad_hoc_query->filter_expression);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::AdHocQuery::is_query_used(const AdHocQuery* query)
{
  try
  {
    // Is there any query parameters or filtering expressions defined?
    return ((query->get_param_map().size() != 0) || (!query->filter_expression.empty()));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::AdHocQuery::get_stored_query_ids_from_typenames(
    const TypeNameStoredQueryMap& typename_storedquery_map,
    const std::vector<std::string>& type_names,
    std::vector<std::string>& stored_query_ids)
{
  try
  {
    BOOST_FOREACH (const auto& type_name, type_names)
    {
      const std::string& lname = Fmi::ascii_tolower_copy(type_name);
      const std::vector<std::string>& tmp =
          typename_storedquery_map.get_stored_queries_by_name(lname);
      stored_query_ids.insert(stored_query_ids.end(), tmp.begin(), tmp.end());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_query_parameter(const std::string& element_name)
{
  try
  {
    return ((element_name == "fes:bbox") || (element_name == "gml:timeposition"));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void get_comparison_operation(const std::string& element_name, std::string& operation)
{
  try
  {
    if (element_name == "fes:propertyisequalto")
    {
      operation = "=";
    }
    else if (element_name == "fes:propertyisnotequalto")
    {
      operation = "!=";
    }
    else if (element_name == "fes:propertyislessthan")
    {
      operation = "<";
    }
    else if (element_name == "fes:propertyisgreaterthan")
    {
      operation = ">";
    }
    else if (element_name == "fes:propertyislessthanorequalto")
    {
      operation = "<=";
    }
    else if (element_name == "fes:propertyisgreaterthanorequalto")
    {
      operation = ">=";
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_comparison_operation(const std::string& element_name)
{
  try
  {
    return ((element_name == "fes:propertyisequalto") ||
            (element_name == "fes:propertyisnotequalto") ||
            (element_name == "fes:propertyislessthan") ||
            (element_name == "fes:propertyisgreaterthan") ||
            (element_name == "fes:propertyislessthanorequalto") ||
            (element_name == "fes:propertyisgreaterthanorequalto"));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool read_value_reference(const SmartMet::Spine::Value& value, std::string& value_reference)
{
  try
  {
    bool is_reference_attribute = false;

    value_reference = value.get_string();

    // Is value reference an attribute without any node definitions?
    if (value_reference.substr(0, 1) == "@")
    {
      is_reference_attribute = true;

      value_reference = "*/" + value_reference;
    }

    // Add axis definition.
    value_reference = "descendant::" + value_reference;

    return is_reference_attribute;
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
      throw exception.disableStackTrace();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
