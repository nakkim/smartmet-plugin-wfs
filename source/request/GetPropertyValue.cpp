#include "request/GetPropertyValue.h"
#include "AdHocQuery.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XPathSnapshot.h"
#include "XmlDomErrorHandler.h"
#include "XmlUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMXPathNSResolver.hpp>
#include <xercesc/dom/DOMXPathResult.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/util/Janitor.hpp>
#include <xqilla/xqilla-dom3.hpp>

#define SCHEMA_LOCATION "schemaLocation"
#define TIMESTAMP "timeStamp"

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

using boost::format;
using boost::str;

using Fmi::current_exception_type;
using SmartMet::Plugin::WFS::Request::GetPropertyValue;

static void decrease(boost::optional<int>& value)
{
  if (value and *value > 0)
  {
    value = *value - 1;
  }
}

GetPropertyValue::GetPropertyValue(const std::string& language,
                                   PluginImpl& plugin_impl)
  : bw::RequestBase(language, plugin_impl)
  , query_cache(plugin_impl.get_query_cache())
{
}

GetPropertyValue::~GetPropertyValue() {}
bw::RequestBase::RequestType GetPropertyValue::get_type() const
{
  return bw::RequestBase::GET_PROPERTY_VALUE;
}

void GetPropertyValue::execute(std::ostream& output) const
{
  try
  {
    if (spp.get_output_format() == "debug")
    {
      SmartMet::Spine::Exception exception(
          BCP, "The 'debug' output format is not supported for the'GetPropertyValue' request!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    boost::shared_ptr<xercesc::DOMDocument> result =
        bwx::create_dom_document(WFS_NAMESPACE_URI, "wfs:ValueCollection");

    int num_returned = 0;
    int num_matched = 0;
    bool is_timestamp_set = false;
    bool is_schemalocation_set = false;
    ;

    if (queries.size() > 0)
    {
      std::vector<std::string> query_responses;
      bool some_succeeded = collect_query_responses(query_responses);

      if (some_succeeded)
      {
        add_responses(result,
                      query_responses,
                      num_returned,
                      num_matched,
                      is_timestamp_set,
                      is_schemalocation_set);
      }
      else
      {
        set_http_status(SmartMet::Spine::HTTP::bad_request);
      }
    }

    finalize(result, num_returned, num_matched, is_timestamp_set, is_schemalocation_set);

    output << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
           << bwx::xml2string(result->getDocumentElement());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::initialize(boost::optional<int>& max_members,
                                  boost::optional<int>& start_index) const
{
  try
  {
    if (spp.is_hits_only_request())
    {
      max_members = 0;
    }
    else
    {
      if (spp.get_have_counts())
      {
        start_index = spp.get_start_index();
        max_members = spp.get_count();
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::finalize(boost::shared_ptr<xercesc::DOMDocument> result,
                                const int cumulative_num_returned,
                                const int cumulative_num_matched,
                                const bool is_timestamp_set,
                                const bool is_schemalocation_set) const
{
  try
  {
    xercesc::DOMElement* result_root = result->getDocumentElement();

    bwx::set_attr(*result_root, "numberMatched", Fmi::to_string(cumulative_num_matched));
    bwx::set_attr(*result_root, "numberReturned", str(format("%1%") % cumulative_num_returned));

    if (not is_timestamp_set)
    {
      namespace pt = boost::posix_time;
      const std::string tm = Fmi::to_iso_extended_string(plugin_impl.get_time_stamp()) + "Z";
      bwx::set_attr(*result_root, TIMESTAMP, tm);
    }

    if (not is_schemalocation_set)
    {
      bwx::set_attr(*result_root, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
      bwx::set_attr(*result_root,
                    "xsi:schemaLocation",
                    "http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd "
                    "http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::add_responses(boost::shared_ptr<xercesc::DOMDocument> result,
                                     const std::vector<std::string>& query_responses,
                                     int& cumulative_num_returned,
                                     int& cumulative_num_matched,
                                     bool& is_timestamp_set,
                                     bool& is_schemalocation_set) const
{
  try
  {
    using namespace xercesc;

    boost::optional<int> max_members;
    boost::optional<int> start_index;
    initialize(max_members, start_index);

    int number_of_responses = query_responses.size();

    for (int i = 0; i < number_of_responses; i++)
    {
      const auto& query_response = query_responses.at(i);

      try
      {
        filter(result,
               query_response,
               cumulative_num_returned,
               cumulative_num_matched,
               max_members,
               start_index,
               queries[i],
               is_timestamp_set,
               is_schemalocation_set);
      }
      catch (const xercesc::DOMException& err)
      {
        std::cerr << METHOD_NAME << ": DOM exception: code=" << (int)err.code
                  << " message=" << err.getMessage() << std::endl;
        throw SmartMet::Spine::Exception::Trace(BCP, "DOM exception received!");
      }
      catch (const bwx::XmlError& err)
      {
        std::cerr << METHOD_NAME << ": XML parsing (non validating) failed:\n" << err.what();

        BOOST_FOREACH (const auto& msg, err.get_messages())
        {
          std::cerr << "XML_PARSE_MSG: " << msg << std::endl;
        }

        std::cerr << "XML source is:\n" << query_response << std::endl;
        throw SmartMet::Spine::Exception::Trace(BCP, "XML parsing failed!");
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::filter(boost::shared_ptr<xercesc::DOMDocument> result,
                              const std::string& response,
                              int& cumulative_num_returned,
                              int& cumulative_num_matched,
                              boost::optional<int>& max_members,
                              boost::optional<int>& start_index,
                              const boost::shared_ptr<QueryBase> query,
                              bool& is_timestamp_set,
                              bool& is_schemalocation_set) const
{
  try
  {
    using namespace xercesc;

    if (query->get_type() == QueryBase::QUERY)
    {
      AdHocQuery* ad_hoc_query = &dynamic_cast<AdHocQuery&>(*query);

      if (ad_hoc_query and ad_hoc_query->has_filter())
      {
        bw::Xml::XPathSnapshot xps;
        xps.parse_dom_document(response, "wfs:GetPropertyValue:TMP:FILTER");
        bw::AdHocQuery::filter(query, xps);

        int num_members = xps.size();

        for (int i = 0; i < num_members; i++)
        {
          DOMNode* result_node = xps.get_item(i);

          extract_property(result,
                           bwx::xml2string(result_node),
                           cumulative_num_returned,
                           cumulative_num_matched,
                           max_members,
                           start_index,
                           is_timestamp_set,
                           is_schemalocation_set);
        }

        return;
      }
    }

    extract_property(result,
                     response,
                     cumulative_num_returned,
                     cumulative_num_matched,
                     max_members,
                     start_index,
                     is_timestamp_set,
                     is_schemalocation_set);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::extract_property(boost::shared_ptr<xercesc::DOMDocument> result,
                                        const std::string& response,
                                        int& cumulative_num_returned,
                                        int& cumulative_num_matched,
                                        boost::optional<int>& max_members,
                                        boost::optional<int>& start_index,
                                        bool& is_timestamp_set,
                                        bool& is_schemalocation_set) const
{
  try
  {
    using namespace xercesc;

    bw::Xml::XPathSnapshot xpath_snapshot;
    xpath_snapshot.parse_dom_document(response, "wfs:GetPropertyValue:TMP:EXTRACT");
    xpath_snapshot.xpath_query(xpath_string);

    int num_members = xpath_snapshot.size();

    if (num_members > 0)
    {
      cumulative_num_matched += num_members;

      if (not max_members or (max_members and *max_members > 0))
      {
        append_members(result,
                       xpath_snapshot,
                       cumulative_num_returned,
                       max_members,
                       start_index,
                       is_timestamp_set,
                       is_schemalocation_set);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::append_members(boost::shared_ptr<xercesc::DOMDocument> result,
                                      bw::Xml::XPathSnapshot& xpath_snapshot,
                                      int& cumulative_num_returned,
                                      boost::optional<int>& max_members,
                                      boost::optional<int>& start_index,
                                      bool& is_timestamp_set,
                                      bool& is_schemalocation_set) const
{
  try
  {
    using namespace xercesc;

    DOMElement* result_root = result->getDocumentElement();

    if (not is_timestamp_set)
      is_timestamp_set =
          copy_timestamp(xpath_snapshot.get_document()->getDocumentElement(), result_root);

    if (not is_schemalocation_set)
      is_schemalocation_set =
          copy_schema_location(xpath_snapshot.get_document()->getDocumentElement(), result_root);

    int first, last;
    calculate_loop_limits(max_members, start_index, xpath_snapshot.size(), first, last);

    for (int i = first; i <= last; i++)
    {
      DOMNode* result_node = xpath_snapshot.get_item(i);
      DOMNode::NodeType result_node_type = result_node->getNodeType();
      const XMLCh* result_node_name = result_node->getNodeName();

      // If the item is wfs:member node element append directly to result_root.
      if ((DOMNode::NodeType::ELEMENT_NODE == result_node_type) &&
          (XMLString::compareIString(result_node_name, X("wfs:member")) == 0))
      {
        result_root->appendChild(result->importNode(result_node, true));
      }
      else
      {
        auto* member = bwx::append_child_element(*result_root, WFS_NAMESPACE_URI, "wfs:member");
        switch (result_node_type)
        {
          case DOMNode::ATTRIBUTE_NODE:
            member->appendChild(
                result->createTextNode(dynamic_cast<DOMAttr&>(*result_node).getValue()));
            break;
          default:
            member->appendChild(result->importNode(result_node, true));
        }
      }

      cumulative_num_returned++;
      decrease(start_index);
      decrease(max_members);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::calculate_loop_limits(const boost::optional<int>& max_members,
                                             const boost::optional<int>& start_index,
                                             const int num_members,
                                             int& first,
                                             int& last) const
{
  try
  {
    first = start_index ? *start_index : 0;

    int count = max_members ? *max_members : 0x7FFFFF;

    if (count > num_members)
      count = num_members;

    last = first + count - 1;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool GetPropertyValue::copy_schema_location(const xercesc::DOMElement* source,
                                            xercesc::DOMElement* destination) const
{
  try
  {
    const XMLCh* x_schemaLoc = source->getAttributeNS(X(XSI_NAMESPACE_URI), X(SCHEMA_LOCATION));

    if (x_schemaLoc)
    {
      destination->setAttributeNS(X(XSI_NAMESPACE_URI), X("xsi:" SCHEMA_LOCATION), x_schemaLoc);
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool GetPropertyValue::copy_timestamp(const xercesc::DOMElement* source,
                                      xercesc::DOMElement* destination) const
{
  try
  {
    if (source->hasAttribute(X(TIMESTAMP)))
    {
      const auto timeStamp = source->getAttribute(X(TIMESTAMP));
      destination->setAttribute(X(TIMESTAMP), timeStamp);
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<GetPropertyValue> GetPropertyValue::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    PluginImpl& plugin_impl)
{
  try
  {
    bw::Request::GetPropertyValue::check_request_name(http_request, "GetPropertyValue");
    check_wfs_version(http_request);

    // Verify whether we have request=getFeature in HTTP request
    auto request = http_request.getParameter("request");

    if (not request or strcasecmp(request->c_str(), "GetPropertyValue") != 0)
    {
      // Should not happen
      SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!");
      exception.addDetail("Not a GetPropertyValue request.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    // Verify whether we have request=getFeature in HTTP request
    auto value_reference = http_request.getParameter("valuereference");
    if (not value_reference)
    {
      SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!");
      exception.addDetail("Mandatory request parameter VALUEREFERENCE missing.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    const StoredQueryMap& stored_query_map = plugin_impl.get_stored_query_map();
    boost::shared_ptr<GetPropertyValue> result(
       new GetPropertyValue(language, plugin_impl));
    result->spp.read_from_kvp(http_request);
    result->xpath_string = *value_reference;

    // if HTTP request parameters have no storedquery id assume ad hoc - query
    auto stored_query_id = http_request.getParameter("storedquery_id");

    if (stored_query_id)
    {
      boost::shared_ptr<bw::StoredQuery> query =
          bw::StoredQuery::create_from_kvp(language, result->spp, http_request, stored_query_map);
      result->queries.push_back(query);
    }
    else
    {
      const TypeNameStoredQueryMap& typename_stored_query_map =
          plugin_impl.get_typename_stored_query_map();
      bw::AdHocQuery::create_from_kvp(language,
                                      result->spp,
                                      http_request,
                                      stored_query_map,
                                      typename_stored_query_map,
                                      result->queries);
    }

    result->fast = result->get_cached_responses();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<GetPropertyValue> GetPropertyValue::create_from_xml(
    const std::string& language,
    const xercesc::DOMDocument& document,
    PluginImpl& plugin_impl)
{
  try
  {
    const StoredQueryMap& stored_query_map = plugin_impl.get_stored_query_map();
    bw::Request::GetPropertyValue::check_request_name(document, "GetPropertyValue");
    const xercesc::DOMElement* root = get_xml_root(document);

    const char* method_name = "SmartMet::Plugin::WFS::Request::GetPropertyValue::create_from_xml";
    boost::shared_ptr<GetPropertyValue> result(
        new GetPropertyValue(language, plugin_impl));

    result->check_mandatory_attributes(document);
    result->spp.read_from_xml(*root);

    const auto value_reference =
        bwx::get_mandatory_attr(*root, WFS_NAMESPACE_URI, "valueReference");
    result->xpath_string = value_reference;

    std::set<std::string> allowed_children;
    allowed_children.insert("Query");
    allowed_children.insert("StoredQuery");

    auto children = bwx::get_child_elements(*root, WFS_NAMESPACE_URI, "*");
    BOOST_FOREACH (const xercesc::DOMElement* child, children)
    {
      const std::string name =
          bwx::check_name_info(child, WFS_NAMESPACE_URI, allowed_children, method_name);

      if (name == "Query")
      {
        const TypeNameStoredQueryMap& typename_stored_query_map =
            plugin_impl.get_typename_stored_query_map();
        bw::AdHocQuery::create_from_xml(language,
                                        result->spp,
                                        *child,
                                        stored_query_map,
                                        typename_stored_query_map,
                                        result->queries);
      }
      else if (name == "StoredQuery")
      {
        // There should be no more than one according to the XML schema.
        boost::shared_ptr<bw::StoredQuery> query =
            bw::StoredQuery::create_from_xml(language, result->spp, *child, stored_query_map);
        result->queries.push_back(query);
      }
      else
      {
        // Not supposed to be here
        assert(0);
      }
    }

    result->fast = result->get_cached_responses();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool GetPropertyValue::collect_query_responses(std::vector<std::string>& query_responses) const
{
  try
  {
    bool some_succeeded = false;

    BOOST_FOREACH (auto query_ptr, queries)
    {
      boost::optional<std::string> cached_response = query_ptr->get_cached_response();

      if (cached_response)
      {
        add_query_responses(query_responses, *cached_response);
        some_succeeded = true;
      }
      else
      {
        std::ostringstream result_stream;
        query_ptr->execute(result_stream, get_language(), get_hostname());
        const std::string cache_key = query_ptr->get_cache_key();
        query_cache.insert(cache_key, result_stream.str());

        // FIXME: should we restrict caching on too large responses?

        add_query_responses(query_responses, result_stream.str());
        some_succeeded = true;
      }
    }

    return some_succeeded;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void GetPropertyValue::add_query_responses(std::vector<std::string>& query_responses,
                                           const std::string& response) const
{
  try
  {
    std::ostringstream tmp;
    substitute_all(response, tmp);
    query_responses.push_back(tmp.str());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool GetPropertyValue::get_cached_responses()
{
  try
  {
    bool found = true;
    for (auto it = queries.begin(); it != queries.end(); ++it)
    {
      auto query = *it;
      boost::optional<std::string> cached_response = query_cache.find(query->get_cache_key());
      if (cached_response)
      {
        query->set_cached_response(*cached_response);
      }
      else
      {
        found = false;
      }
    }

    return found;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

int GetPropertyValue::get_response_expires_seconds() const
{
  try
  {
    int smallest_value = -1;

    // When multiple queries present, select the smallest stale value.
    for (auto it = queries.begin(); it != queries.end(); ++it)
    {
      auto query = *it;
      int seconds = query->get_stale_seconds();
      if (smallest_value < 0)
        smallest_value = seconds;
      else if (smallest_value > seconds)
        smallest_value = seconds;
    }

    // return default value if no any query present.
    return (smallest_value >= 0) ? smallest_value
                                 : plugin_impl.get_config().getDefaultExpiresSeconds();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
