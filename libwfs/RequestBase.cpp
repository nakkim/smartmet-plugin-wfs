#include "RequestBase.h"
#include "PluginImpl.h"
#include "QueryBase.h"
#include "WfsConst.h"
#include "WfsException.h"
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <macgyver/Exception.h>
#include <fmt/format.h>
#include <sstream>

namespace bw = SmartMet::Plugin::WFS;
namespace bwx = SmartMet::Plugin::WFS::Xml;

namespace ba = boost::algorithm;

namespace {

  boost::regex r_wfs_version("^2\\.0\\.[0-9]$");

  void check_value_equal(const std::string& name, const std::string& value, const std::string& expected)
  {
    if (value != expected) {
      const std::string msg = fmt::format("Unsupported value '{}' of {} ('{}' expected)",
					  value, name, expected);
      throw Fmi::Exception(BCP, msg);
    }
  }

  void check_wfs_version_string(const std::string& version_string)
  {
    if (not boost::regex_match(version_string, r_wfs_version)) {
      const std::string msg = fmt::format("Unsupported WFS version '{}' (2.0.X expected)",
					  version_string);
      throw Fmi::Exception(BCP, msg);
    }
  }

} // anonymous namespace

bw::RequestBase::RequestBase(const std::string& language, const PluginImpl& plugin_impl)
  : plugin_impl(plugin_impl)
  , language(language)
  , soap_request(false)
  , status(SmartMet::Spine::HTTP::not_a_status)
{
}

bw::RequestBase::~RequestBase() {}

void bw::RequestBase::set_is_soap_request(bool value)
{
  soap_request = value;
}

std::string bw::RequestBase::extract_request_name(const xercesc::DOMDocument& document)
{
  try
  {
    const char* FN = "SmartMet::Plugin::WFS::Xml::RequestBase::extractRequestName()";

    const xercesc::DOMElement* root = get_xml_root(document);
    auto name_info = bwx::get_name_info(root);

    if (name_info.second == SOAP_NAMESPACE_URI)
    {
      std::ostringstream msg;
      msg << FN << ": HTTP/SOAP requests are not yet supported";
      throw Fmi::Exception(BCP, msg.str());
    }
    else if (name_info.second == WFS_NAMESPACE_URI)
    {
      return name_info.first;
    }
    else
    {
      if (name_info.second == "")
      {
        std::ostringstream msg;
        msg << "SmartMet::Plugin::WFS::Xml::RequestBase::extractRequestName():"
            << " no namespace provided but " << WFS_NAMESPACE_URI << " expected";
        throw Fmi::Exception(BCP, msg.str());
      }
      else
      {
        std::ostringstream msg;
        msg << "SmartMet::Plugin::WFS::Xml::RequestBase::extractRequestName():"
            << " unexpected XML namespace " << name_info.second << " when " << WFS_NAMESPACE_URI
            << " expected";
        throw Fmi::Exception(BCP, msg.str());
      }
    }

    return name_info.first;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::RequestBase::extract_request_name(
    const SmartMet::Spine::HTTP::Request& http_request)
{
  try
  {
    auto req_name = http_request.getParameter("request");
    if (not req_name)
    {
      Fmi::Exception exception(BCP, "The 'request' parameter is missing!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    return *req_name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool bw::RequestBase::may_validate_xml() const
{
  return true;
}

void bw::RequestBase::set_fmi_apikey(const std::string& fmi_apikey)
{
  this->fmi_apikey = fmi_apikey;
}

void bw::RequestBase::set_fmi_apikey_prefix(const std::string& fmi_apikey_prefix)
{
  this->fmi_apikey_prefix = fmi_apikey_prefix;
}

void bw::RequestBase::set_hostname(const std::string& hostname)
{
  this->hostname = hostname;
}

void bw::RequestBase::set_protocol(const std::string& protocol)
{
  this->protocol = protocol;
}

void bw::RequestBase::substitute_all(const std::string& src, std::ostream& output) const
{
  try
  {
    const char *in = src.c_str();

    struct { const char* subst; std::string value; std::size_t l1; }
    subst_list[] =
      {
	{ bw::QueryBase::FMI_APIKEY_PREFIX_SUBST, fmi_apikey_prefix ? *fmi_apikey_prefix : std::string(""), 0 },
	{ bw::QueryBase::FMI_APIKEY_SUBST, fmi_apikey ? *fmi_apikey : std::string(""), 0 },
	{ bw::QueryBase::HOSTNAME_SUBST, hostname ? *hostname : std::string("localhost"), 0 },
	{ bw::QueryBase::PROTOCOL_SUBST, (protocol ? *protocol : "http") + "://", 0 }
      };

    const std::size_t num_subst = sizeof(subst_list) / sizeof(*subst_list);
    char chm[256];

    std::memset(chm, 0, 256);
    for (std::size_t i = 0; i < num_subst; i++) {
      chm[subst_list[i].subst[0] & 255] = 1;
      subst_list[i].l1 = strlen(subst_list[i].subst);
    }

    while (*in) {
      if (chm[(unsigned)(unsigned char)*in]) {
	bool subst_done = false;
	for (std::size_t i = 0; not subst_done and (i < num_subst); i++) {
	  if (strncmp(in, subst_list[i].subst, subst_list[i].l1) == 0) {
	    output << subst_list[i].value;
	    in += subst_list[i].l1;
	    subst_done = true;
	  }
	}

	if (not subst_done) {
	  output << *in++;
	}
      } else {
	output << *in++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::set_http_status(SmartMet::Spine::HTTP::Status status) const
{
  this->status = status;
}

void bw::RequestBase::check_request_name(const SmartMet::Spine::HTTP::Request& http_request,
                                         const std::string& name)
{
  try
  {
    const std::string actual_name = extract_request_name(http_request);
    if (Fmi::ascii_tolower_copy(actual_name) != Fmi::ascii_tolower_copy(name))
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::RequestBase::check_request_name(): [INTERNAL ERROR]:"
          << " conflicting reqest name '" << actual_name << "' in HTTP request ('" << name
          << "' expected)";
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::check_request_name(const xercesc::DOMDocument& document,
                                         const std::string& name)
{
  try
  {
    const std::string actual_name = extract_request_name(document);
    if (Fmi::ascii_tolower_copy(actual_name) != Fmi::ascii_tolower_copy(name))
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::RequestBase::check_request_name(): [INTERNAL ERROR]:"
          << " conflicting reqest name '" << actual_name << "' in XML document ('" << name
          << "' expected)";
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::check_mandatory_attributes(const xercesc::DOMDocument& document)
{
  try
  {
    struct
    {
      const char* name;
      std::function<void(const std::string&)> value_checker;
    } mandatory_attr_def[] =
	{
	 {"version", check_wfs_version_string}
	 ,{"service", [](const std::string& value) { check_value_equal("service", value, "WFS"); }}
	};

    int num_mandatory_attr_def = sizeof(mandatory_attr_def) / sizeof(*mandatory_attr_def);
    const xercesc::DOMElement* root = get_xml_root(document);
    for (int i = 0; i < num_mandatory_attr_def; i++)
    {
      const auto& item = mandatory_attr_def[i];
      bwx::verify_mandatory_attr_value(*root, WFS_NAMESPACE_URI, item.name, item.value_checker);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::check_output_format_attribute(const std::string& value, const PluginImpl& plugin_impl)
{
  try
  {
    namespace ba = boost::algorithm;

    const auto& fmts = plugin_impl.get_config().get_capabilities_config().get_supported_formats();
    std::string fmt = CapabilitiesConf::conv_output_format_str(value);
    if (fmts.count(fmt) == 0) {
      // Something wrong with format
      report_incorrect_output_format(fmt, plugin_impl);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::check_output_format_attribute(const xercesc::DOMElement* root,
						    const PluginImpl& plugin_impl)
{
  try
  {
    namespace ba = boost::algorithm;
    const auto& fmts = plugin_impl.get_config().get_capabilities_config().get_supported_formats();
    const auto attrInfo = bwx::get_attr(*root, WFS_NAMESPACE_URI, "outputFormat");
    if (attrInfo.second) {
      const std::string fmt = CapabilitiesConf::conv_output_format_str(attrInfo.first);
      if (fmts.count(fmt) == 0) {
	// Something wrong with format
	report_incorrect_output_format(fmt, plugin_impl);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::report_incorrect_output_format(const std::string& value,
						     const PluginImpl& plugin_impl)
{
  const auto& fmts = plugin_impl.get_config().get_capabilities_config().get_supported_formats();
  std::ostringstream msg;
  std::string dlm = " ";
  msg << "Unsupported output format '" << value
      << "' (One of";
  for (const auto& item : fmts) {
    msg << dlm << '\'' << item << '\'';
    dlm = ", ";
  }
  msg << " expected)";
  std::cout << msg.str();
  Fmi::Exception exception(BCP, msg.str());
  exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
  throw exception;
}

const xercesc::DOMElement* bw::RequestBase::get_xml_root(const xercesc::DOMDocument& document)
{
  try
  {
    xercesc::DOMElement* root = document.getDocumentElement();
    if (root == nullptr)
    {
      Fmi::Exception exception(BCP, "The XML root element is missing!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
    return root;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestBase::check_wfs_version(const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    auto version = request.getParameter("version");
    if (version) {
      check_wfs_version_string(*version);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
