#include "request/GetFeature.h"
#include "AdHocQuery.h"
#include "ErrorResponseGenerator.h"
#include "StoredQuery.h"
#include "StoredQueryMap.h"
#include "TypeNameStoredQueryMap.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XPathSnapshot.h"
#include "XmlUtils.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <smartmet/spine/Convenience.h>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMXPathNSResolver.hpp>
#include <xercesc/dom/DOMXPathResult.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/util/Janitor.hpp>
#include <xqilla/xqilla-dom3.hpp>
#include <set>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

using boost::format;
using boost::str;

bw::Request::GetFeature::GetFeature(const std::string& language,
                                    PluginData& plugin_data)

				    : RequestBase(language)
				    , plugin_data(plugin_data)
				    , query_cache(plugin_data.get_query_cache())
{
}

bw::Request::GetFeature::~GetFeature() {}
bw::RequestBase::RequestType bw::Request::GetFeature::get_type() const
{
  return GET_FEATURE;
}
void bw::Request::GetFeature::execute(std::ostream& output) const
{
  try
  {
    switch (queries.size())
    {
      case 0:
        return;

      case 1:
        execute_single_query(output);
        return;

      default:
        execute_multiple_queries(output);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::Request::GetFeature::get_cached_responses()
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

int bw::Request::GetFeature::get_response_expires_seconds() const
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
                                 : plugin_data.get_config().getDefaultExpiresSeconds();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::Request::GetFeature::execute_single_query(std::ostream& ost) const
{
  try
  {
    std::vector<std::string> query_responses;
    collect_query_responses(query_responses, false);
    std::string response = query_responses.at(0);

    bool use_default_format =
        spp.get_output_format() == StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT;

    if (spp.is_hits_only_request())
    {
      boost::shared_ptr<xercesc::DOMDocument> result = create_hits_only_response(response);
      response = bwx::xml2string(result->getDocumentElement());
    }
    else if ((spp.get_have_counts()) || (queries[0]->get_type() == QueryBase::QUERY))
    {
      // May not be XML for non default output format, so selecting members as below may not
      // work, so do not try to do that at all. Report an error instead
      if (not use_default_format)
      {
        std::ostringstream msg;
        msg << "The 'startIndex' and 'count' parameters are supported only with"
            << " the default output format '"
            << StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT << "'!";
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      // Parse response got from either response cache or by executing query
      bwx::XPathSnapshot xps;
      xps.parse_dom_document(response, "wfs:GetFeature:TMP");
      std::string prefix = xps.lookup_prefix(WFS_NAMESPACE_URI);
      if (prefix != "")
        prefix += ":";

      // We do not need the old response any more as it is going to be replaced.
      // Clear it to conserve memory.
      response.clear();

      const auto* root = xps.get_document()->getDocumentElement();
      std::string tag_name = UTF8(root->getTagName());

      // Create a new DOM document and clone old document root node without children
      xercesc::DOMImplementation* impl =
          xercesc::DOMImplementationRegistry::getDOMImplementation(X("LS"));
      AutoDelete<xercesc::DOMDocument> result_dom_doc(impl->createDocument());
      result_dom_doc->appendChild(result_dom_doc->importNode(root, false));
      // FIXME: should we copy attributes or that just happens?
      // ?????: it seems that attributes are justr copied and there is no need to
      //        do anyhing more with that

      // If query is ad hoc query then perform filtering.
      if (queries[0]->get_type() == QueryBase::QUERY)
      {
        bw::AdHocQuery::filter(queries[0], xps);
      }
      else  // Query is a stored query.
      {
        // Generate XPATH query to select response members.
        std::ostringstream expr;
        expr << "/" << prefix << "FeatureCollection/" << prefix << "member";
        xps.xpath_query(expr.str());
      }

      int num_members = xps.size();
      int start_index = spp.get_start_index();
      int count = spp.get_count();
      int end_index = start_index + count - 1;
      int num_returned = 0;

      // std::cout << "# Start index   : " << start_index << std::endl;
      // std::cout << "# Count         : " << count << std::endl;
      // std::cout << "# End index     : " << end_index << std::endl;

      for (int i = start_index; i <= end_index and i < num_members; i++)
      {
        // std::cout << "# Taking index " << i << std::endl;
        xercesc::DOMNode* orig_member = xps.get_item(i);
        // Import member together with all child nodes and insert into result document
        xercesc::DOMNode* imported_member = result_dom_doc->importNode(orig_member, true);
        result_dom_doc->getDocumentElement()->appendChild(imported_member);
        num_returned++;
      }

      // Update root element attribute numReturned
      const std::string num_returned_val = str(format("%1%") % num_returned);
      bwx::set_attr(*result_dom_doc->getDocumentElement(), "numberReturned", num_returned_val);

      // Output XML DOM document as a string
      response = bwx::xml2string(result_dom_doc->getDocumentElement());
      response.insert(0, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    }

    // Substitute fmi_apikey, fmi_apikey_prefix and hostname for the final response
    substitute_all(response, ost);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::Request::GetFeature::execute_multiple_queries(std::ostream& ost) const
{
  try
  {
    using namespace xercesc;

    assert_use_default_format();

    std::vector<std::string> query_responses;
    bool some_succeeded = collect_query_responses(query_responses, true);

    auto xml_doc_p =
        bwx::create_dom_document("http://www.opengis.net/wfs/2.0", "wfs:FeatureCollection");
    auto root = xml_doc_p->getDocumentElement();
    bwx::set_attr(*root, "xmlns:gml", "http://www.opengis.net/gml/3.2");
    bwx::set_attr(*root, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    bwx::set_attr(*root,
                  "xsi:schemaLocation",
                  "http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd "
                  "http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd");

    Janitor<XMLCh> a_timeStamp(xercesc::XMLString::transcode("timeStamp"));

    boost::optional<std::string> time_stamp;

    std::size_t member_ind = 0;
    std::size_t num_matched = 0;
    std::size_t num_returned = 0;
    const std::size_t start_index = spp.get_start_index();
    const std::size_t count = spp.get_count();
    const std::size_t end_index = start_index + count - 1;
    const bool hits_only = spp.is_hits_only_request();

    for (std::size_t i = 0; i < query_responses.size(); ++i)
    {
      const auto& query_response = query_responses.at(i);
      try
      {
        bwx::XPathSnapshot xps;
        xps.parse_dom_document(query_response, "wfs:GetFeature:TMP");
        DOMElement* query_response_root = xps.get_document()->getDocumentElement();
        const std::string root_name = UTF8(query_response_root->getLocalName());
        const auto* x_prefix = query_response_root->getPrefix();
        std::string prefix = x_prefix ? std::string(UTF8(x_prefix)) + ":" : std::string(":");
        const std::string root_ns = UTF8(query_response_root->getNamespaceURI());
        if ((root_ns == WFS_NAMESPACE_URI) and (root_name == "FeatureCollection"))
        {
          // std::cout << "### wfs::FeatureCollection" << std::endl;

          // ### wfs:FeatureCollection
          if (query_response_root->hasAttribute(a_timeStamp.get()))
          {
            const XMLCh* tmp1 = query_response_root->getAttribute(a_timeStamp.get());
            if (tmp1)
              time_stamp = bwx::to_string(tmp1);
          }

          // If query is ad hoc query then perform filtering.
          if (queries[i]->get_type() == QueryBase::QUERY)
          {
            bw::AdHocQuery::filter(queries[i], xps);
          }
          else  // Query is a stored query.
          {
            // Generate XPATH query to select response members.
            std::ostringstream expr;
            expr << "/" << prefix << "FeatureCollection/" << prefix << "member";
            xps.xpath_query(expr.str());
          }

          // Get number of members
          std::size_t num_members = xps.size();

          // std::cout << "### name='" << root_name << "' prefix='" << prefix << "' ns='" << root_ns
          // << "'"
          //          << std::endl;
          // std::cout << "### expr='" << expr.str() << "'" << std::endl;
          // std::cout << "### num_members=" << num_members << " hits_only=" << hits_only <<
          // std::endl;

          if (hits_only)
          {
            member_ind += num_members;
          }
          else
          {
            std::size_t curr_num_returned = 0;
            auto* qr_member = bwx::append_child_element(*root, WFS_NAMESPACE_URI, "wfs:member");
            // Import root element of query response without child elements
            auto* fc_node = xml_doc_p->importNode(query_response_root, false);
            auto* fc_elem = &dynamic_cast<DOMElement&>(*fc_node);
            qr_member->appendChild(fc_elem);

            // Import required query response members
            for (std::size_t i = 0; i < num_members; i++)
            {
              num_matched++;
              if ((member_ind >= start_index) and (member_ind <= end_index))
              {
                num_returned++;
                curr_num_returned++;
                auto* imported_member = xml_doc_p->importNode(xps.get_item(i), true);
                fc_elem->appendChild(imported_member);
              }
              member_ind++;
            }

            bwx::set_attr(*fc_elem, "numberReturned", str(format("%1%") % curr_num_returned));

            // std::cout << "### query#" << i+1 << ": numReturned=" << curr_num_returned <<
            // std::endl;
          }
        }
        else
        {
          // Not a wfs::FeatureCollection. Copy entire doc
          auto* qr_member = bwx::append_child_element(*root, WFS_NAMESPACE_URI, "wfs:member");
          auto* imported_doc = xml_doc_p->importNode(query_response_root, true);
          qr_member->appendChild(imported_doc);
        }
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

    if (not some_succeeded and queries.size() > 0)
    {
      // There is some queries and they all failed. Return HTTP error in that case
      set_http_status(SmartMet::Spine::HTTP::bad_request);
    }

    bwx::set_attr(*root, "numberMatched", str(format("%1%") % member_ind));
    bwx::set_attr(*root, "numberReturned", str(format("%1%") % num_matched));

    if (not time_stamp)
    {
      namespace pt = boost::posix_time;
      const std::string tm = Fmi::to_iso_extended_string(plugin_data.get_time_stamp()) + "Z";
      time_stamp = tm;
    }

    bwx::set_attr(*root, "timeStamp", *time_stamp);

    std::string result = bwx::xml2string(root);

    ost << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" << result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<xercesc::DOMDocument> bw::Request::GetFeature::create_hits_only_response(
    const std::string& src) const
{
  try
  {
    bool use_default_format =
        spp.get_output_format() == StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT;

    // May not be XML for non default output format, so selecting members as below may not
    // work, so do not try to do that at all. Report an error instead
    if (not use_default_format)
    {
      std::ostringstream msg;
      msg << "The 'startIndex' and 'count' paremeters are supported only with"
          << " the default output format '" << StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT
          << "'!";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }

    // Parse response got from either response cache or by executing query
    bwx::XPathSnapshot xps;
    xps.parse_dom_document(src, "wfs:GetFeature:TMP");
    std::string prefix = xps.lookup_prefix(WFS_NAMESPACE_URI);
    if (prefix != "")
      prefix += ":";

    const auto* root = xps.get_document()->getDocumentElement();
    std::string tag_name = UTF8(root->getTagName());

    // If query is ad hoc query then perform filtering.
    if (queries[0]->get_type() == QueryBase::QUERY)
    {
      bw::AdHocQuery::filter(queries[0], xps);
    }
    else  // Query is a stored query.
    {
      // Generate XPATH query to select response members.
      std::ostringstream expr;
      expr << "/" << prefix << "FeatureCollection/" << prefix << "member";
      xps.xpath_query(expr.str());
    }

    int num_members = xps.size();
    const std::string& num_members_str = str(format("%1%") % num_members);

    const XMLCh* time_stamp =
        xps.get_document()->getDocumentElement()->getAttribute(X("timeStamp"));

    auto result = bwx::create_dom_document(WFS_NAMESPACE_URI, "wfs:FeatureCollection");
    auto* new_root = result->getDocumentElement();
    new_root->setAttributeNS(X(WFS_NAMESPACE_URI), X("numberMatched"), X(num_members_str.c_str()));
    new_root->setAttributeNS(X(WFS_NAMESPACE_URI), X("numberReturned"), X("0"));
    if (time_stamp)
      new_root->setAttributeNS(X(WFS_NAMESPACE_URI), X("timeStamp"), time_stamp);
    new_root->setAttributeNS(
        X(XSI_NAMESPACE_URI),
        X("xsi:schemaLocation"),
        X("http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd"));

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::Request::GetFeature::collect_query_responses(std::vector<std::string>& query_responses,
                                                      bool handle_errors) const
{
  try
  {
    bool some_succeeded = false;

    BOOST_FOREACH (auto query_ptr, queries)
    {
      boost::optional<std::string> cached_response = query_ptr->get_cached_response();
      if (cached_response)
      {
        std::ostringstream tmp;
        substitute_all(*cached_response, tmp);
        query_responses.push_back(tmp.str());
        some_succeeded = true;
      }
      else
      {
        std::ostringstream result_stream;

        try
        {
          query_ptr->execute(result_stream, get_language());
          const std::string cache_key = query_ptr->get_cache_key();
          query_cache.insert(cache_key, result_stream.str());

          // FIXME: should we restrict caching on too large responses?
          std::ostringstream tmp;
          substitute_all(result_stream.str(), tmp);
          query_responses.push_back(tmp.str());
          some_succeeded = true;
        }
        catch (...)
        {
          if (handle_errors)
          {
            StoredQuery* p_sq = dynamic_cast<StoredQuery*>(query_ptr.get());

            if (p_sq)
            {
              ErrorResponseGenerator err_gen(plugin_data);
              auto error_response =
                  err_gen.create_error_response(ErrorResponseGenerator::REQ_PROCESSING, *p_sq);
              std::ostringstream msg;
              msg << SmartMet::Spine::log_time_str() << " [WFS] [ERROR] [" << METHOD_NAME
                  << "] : " << error_response.log_message << std::endl;
              std::cout << msg.str() << std::flush;
              query_responses.push_back(error_response.response);
            }
            else
            {
              throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
            }
          }
          else
          {
            throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
          }
        }
      }
    }

    return some_succeeded;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::Request::GetFeature::assert_use_default_format() const
{
  try
  {
    bool use_default_format =
        spp.get_output_format() == StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT;

    if (not use_default_format)
    {
      std::ostringstream msg;
      msg << "The 'startIndex' and 'count' parameters are supported only with"
          << " the default output format '" << StandardPresentationParameters::DEFAULT_OUTPUT_FORMAT
          << "'!";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::Request::GetFeature> bw::Request::GetFeature::create_from_kvp(
    const std::string& language,
    const SmartMet::Spine::HTTP::Request& http_request,
    PluginData& plugin_data)
{
  try
  {
    bw::Request::GetFeature::check_request_name(http_request, "GetFeature");
    check_wfs_version(http_request);

    const StoredQueryMap& stored_query_map = plugin_data.get_stored_query_map();

    boost::shared_ptr<bw::Request::GetFeature> result(
        new bw::Request::GetFeature(language, plugin_data));

    // Verify whether we have request=getFeature in HTTP request
    auto request = http_request.getParameter("request");

    if (not request or strcasecmp(request->c_str(), "getFeature") != 0)
    {
      // Should not happen
      SmartMet::Spine::Exception exception(BCP, "Operation parsing failed!");
      exception.addDetail("Not a GetFeature request.");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    result->spp.read_from_kvp(http_request);

    // StoredQuery or AdHocQuery
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
          plugin_data.get_typename_stored_query_map();
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

boost::shared_ptr<bw::Request::GetFeature> bw::Request::GetFeature::create_from_xml(
    const std::string& language,
    const xercesc::DOMDocument& document,
    PluginData& plugin_data)
{
  try
  {
    bw::Request::GetFeature::check_request_name(document, "GetFeature");
    const xercesc::DOMElement* root = get_xml_root(document);

    const StoredQueryMap& stored_query_map = plugin_data.get_stored_query_map();

    const char* method_name = "SmartMet::Plugin::WFS::Request::GetFeature::create_from_xml";
    boost::shared_ptr<bw::Request::GetFeature> result(
        new bw::Request::GetFeature(language, plugin_data));

    result->check_mandatory_attributes(document);
    result->spp.read_from_xml(*root);

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
            plugin_data.get_typename_stored_query_map();
        bw::AdHocQuery::create_from_xml(language,
                                        result->spp,
                                        *child,
                                        stored_query_map,
                                        typename_stored_query_map,
                                        result->queries);
      }
      else if (name == "StoredQuery")
      {
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
