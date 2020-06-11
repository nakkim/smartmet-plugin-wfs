
#include "XmlUtils.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <spine/Exception.h>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/util/Janitor.hpp>
#include <xercesc/util/XMLUni.hpp>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
std::pair<std::string, bool> to_opt_string(const XMLCh* src)
{
  try
  {
    if (src)
    {
      xercesc::Janitor<char> tmp(xercesc::XMLString::transcode(src));
      return std::make_pair<std::string, bool>(tmp.get(), true);
      ;
    }
    else
    {
      return std::make_pair<std::string, bool>("", false);
      ;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string to_string(const XMLCh* src)
{
  try
  {
    if (src)
    {
      xercesc::Janitor<char> tmp(xercesc::XMLString::transcode(src));
      return tmp.get();
    }
    else
    {
      throw SmartMet::Spine::Exception(
          BCP, "SmartMet::Plugin::WFS::Xml::to_string: XML string expected but got nullptr");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::pair<std::string, std::string> get_name_info(const xercesc::DOMNode* node)
{
  try
  {
    if (node)
    {
      const XMLCh* x_name = node->getLocalName();
      if (x_name == nullptr)
        x_name = node->getNodeName();
      const std::string name = to_string(x_name);
      const std::string ns_info = to_opt_string(node->getNamespaceURI()).first;
      return std::make_pair(name, ns_info);
    }
    else
    {
      throw SmartMet::Spine::Exception(
          BCP,
          "SmartMet::Plugin::WFS::Xml::get_name_info(): nullptr is not permitted as the argument");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void check_name_info(const xercesc::DOMNode* node,
                     const std::string& ns,
                     const std::string& name,
                     const char* given_location)
{
  try
  {
    if (node == nullptr)
    {
      std::ostringstream msg;
      msg << "No XML DOM node aavailble (nullptr specified)";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    const auto name_info = get_name_info(node);
    const char* location = given_location ? given_location : __PRETTY_FUNCTION__;

    if ((name_info.first != name) or (name_info.second != ns))
    {
      std::ostringstream msg;
      msg << location << ": invalid element name {" << name_info.second << "}" << name_info.first
          << " ({" << ns << "}" << name << " expected)";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string check_name_info(const xercesc::DOMNode* node,
                            const std::string& ns,
                            const std::set<std::string>& allowed_names,
                            const char* given_location)
{
  try
  {
    const auto name_info = get_name_info(node);
    const char* location = given_location ? given_location : __PRETTY_FUNCTION__;

    if ((allowed_names.count(name_info.first) == 0) or (name_info.second != ns))
    {
      std::string d = "";
      std::ostringstream msg;
      msg << location << ": invalid element name {" << name_info.second << "}" << name_info.first
          << " ({" << ns << "}(";
      BOOST_FOREACH (const std::string nm, allowed_names)
      {
        msg << d << nm;
        d = "|";
      }
      msg << ") expected";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
    else
    {
      return name_info.first;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::pair<std::string, bool> get_attr(const xercesc::DOMElement& elem,
                                      const std::string& ns,
                                      const std::string& name)
{
  try
  {
    (void)ns;
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    if (elem.hasAttribute(x_name.get()))
    {
      const XMLCh* x_val = elem.getAttribute(x_name.get());
      return std::make_pair(to_string(x_val), true);
    }
    else
    {
      return std::make_pair<std::string, bool>("", false);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_mandatory_attr(const xercesc::DOMElement& elem,
                               const std::string& ns,
                               const std::string& name)
{
  try
  {
    (void)ns;
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    if (elem.hasAttribute(x_name.get()))
    {
      const XMLCh* x_val = elem.getAttribute(x_name.get());
      return to_string(x_val);
    }
    else
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::Xml::get_mandatory_attr():"
          << " mandatory attribute " << name << " missing";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_optional_attr(const xercesc::DOMElement& elem,
                              const std::string& ns,
                              const std::string& name,
                              const std::string& default_value)
{
  try
  {
    auto tmp = get_attr(elem, ns, name);
    return tmp.second ? tmp.first : default_value;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void verify_mandatory_attr_value(const xercesc::DOMElement& elem,
                                 const std::string& ns,
                                 const std::string& name,
                                 const std::string& exp_value)
{
  const std::string value = get_mandatory_attr(elem, ns, name);
  if (value != exp_value)
  {
    std::ostringstream msg;
    msg << "SmartMet::Plugin::WFS::Xml::verify_mandatory_attr_value(): incorrect value '" << value
        << "' of mandatory fixed attribute {" << ns << "}" << name << " ('" << exp_value
        << "' expected)";
    throw SmartMet::Spine::Exception(BCP, msg.str());
  }
}

std::string extract_text(const xercesc::DOMElement& element)
{
  try
  {
    std::ostringstream text;
    for (xercesc::DOMNode* curr = element.getFirstChild(); curr; curr = curr->getNextSibling())
    {
      xercesc::DOMNode::NodeType type = curr->getNodeType();
      if (type == xercesc::DOMNode::ATTRIBUTE_NODE or type == xercesc::DOMNode::COMMENT_NODE)
      {
        // We are not interested about these
      }
      else if (type == xercesc::DOMNode::TEXT_NODE or type == xercesc::DOMNode::CDATA_SECTION_NODE)
      {
        const XMLCh* data = curr->getNodeValue();
        // Should not be nullptr, but let us play safe
        const std::string tmp_str = to_opt_string(data).first;
        text << tmp_str;
      }
      else
      {
        throw SmartMet::Spine::Exception(BCP, "Unsupported XML node!");
      }
    }

    return text.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMLSSerializer* create_dom_serializer()
{
  try
  {
    xercesc::Janitor<XMLCh> features(xercesc::XMLString::transcode("LS"));

    xercesc::DOMImplementationLS* impl = nullptr;
    xercesc::DOMImplementation* impl_base =
        xercesc::DOMImplementationRegistry::getDOMImplementation(features.get());

    if (impl_base != nullptr)
    {
      impl = dynamic_cast<xercesc::DOMImplementationLS*>(impl_base);
    }

    if (impl == nullptr)
    {
      throw SmartMet::Spine::Exception(BCP,
                                       "Failed to get instance of xercesc::DOMImplementationLS");
    }

    return impl->createLSSerializer();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string xml2string(const xercesc::DOMNode* node)
{
  try
  {
    xercesc::DOMLSSerializer* serializer = create_dom_serializer();

    // Make the output more human readable by inserting line feeds.
    if (serializer->getDomConfig()->canSetParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,
                                                    true))
      serializer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);

    xercesc::Janitor<XMLCh> x_result(serializer->writeToString(node));
    std::string result = to_string(x_result.get());
    serializer->release();
    ba::trim_if(result, ba::is_any_of(" \t\r\n"));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<xercesc::DOMElement*> get_child_elements(const xercesc::DOMElement& element,
                                                     const std::string& ns,
                                                     const std::string& name)
{
  try
  {
    std::vector<xercesc::DOMElement*> result;
    xercesc::Janitor<XMLCh> x_ns(xercesc::XMLString::transcode(ns.c_str()));
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    for (xercesc::DOMNode* child = element.getFirstChild(); child; child = child->getNextSibling())
    {
      if (child->getNodeType() == xercesc::DOMNode::ELEMENT_NODE)
      {
        xercesc::DOMElement& child_element = dynamic_cast<xercesc::DOMElement&>(*child);
        if ((ns == "*") or xercesc::XMLString::equals(x_ns.get(), child_element.getNamespaceURI()))
        {
          if ((name == "*") or
              xercesc::XMLString::equals(x_name.get(), child_element.getLocalName()))
          {
            result.push_back(&child_element);
          }
        }
      }
    }
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<xercesc::DOMDocument> create_dom_document(const std::string& ns,
                                                            const std::string& name)
{
  try
  {
    xercesc::Janitor<XMLCh> features(xercesc::XMLString::transcode("LS"));

    xercesc::DOMImplementation* impl =
        xercesc::DOMImplementationRegistry::getDOMImplementation(features.get());

    if (impl == nullptr)
    {
      throw SmartMet::Spine::Exception(BCP, "Failed to get xercesc::DOMImplementation instance");
    }
    else
    {
      boost::shared_ptr<xercesc::DOMDocument> doc(impl->createDocument());
      doc->appendChild(create_element(*doc, ns, name));
      return doc;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMElement* create_element(xercesc::DOMDocument& doc,
                                    const std::string& ns,
                                    const std::string& name)
{
  try
  {
    xercesc::Janitor<XMLCh> x_ns(xercesc::XMLString::transcode(ns.c_str()));
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    xercesc::DOMElement* element = doc.createElementNS(x_ns.get(), x_name.get());
    return element;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMElement* append_child_element(xercesc::DOMElement& parent,
                                          const std::string& ns,
                                          const std::string& name)
{
  try
  {
    xercesc::Janitor<XMLCh> x_ns(xercesc::XMLString::transcode(ns.c_str()));
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    xercesc::DOMElement* child =
        parent.getOwnerDocument()->createElementNS(x_ns.get(), x_name.get());
    parent.appendChild(child);
    return child;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMElement* append_child_text_element(xercesc::DOMElement& parent,
                                               const std::string& ns,
                                               const std::string& name,
                                               const std::string& value)
{
  try
  {
    xercesc::Janitor<XMLCh> x_value(xercesc::XMLString::transcode(value.c_str()));
    xercesc::DOMElement* child = append_child_element(parent, ns, name);
    xercesc::DOMText* text = parent.getOwnerDocument()->createTextNode(x_value.get());
    child->appendChild(text);
    return child;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_attr(xercesc::DOMElement& element, const std::string& name, const std::string& value)
{
  try
  {
    xercesc::Janitor<XMLCh> x_name(xercesc::XMLString::transcode(name.c_str()));
    xercesc::Janitor<XMLCh> x_value(xercesc::XMLString::transcode(value.c_str()));
    element.setAttribute(x_name.get(), x_value.get());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
