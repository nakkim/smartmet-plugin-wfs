#pragma once

#include <boost/shared_ptr.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMLSSerializer.hpp>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/util/XMLString.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
std::pair<std::string, bool> to_opt_string(const XMLCh* src);

std::string to_string(const XMLCh* src);

std::pair<std::string, std::string> get_name_info(const xercesc::DOMNode* node);

void check_name_info(const xercesc::DOMNode* node,
                     const std::string& ns,
                     const std::string& name,
                     const char* given_location = nullptr);

std::string check_name_info(const xercesc::DOMNode* node,
                            const std::string& ns,
                            const std::set<std::string>& allowed_name,
                            const char* given_location = nullptr);

std::pair<std::string, bool> get_attr(const xercesc::DOMElement& elem,
                                      const std::string& ns,
                                      const std::string& name);

std::string get_mandatory_attr(const xercesc::DOMElement& elem,
                               const std::string& ns,
                               const std::string& name);

std::string get_optional_attr(const xercesc::DOMElement& elem,
                              const std::string& ns,
                              const std::string& name,
                              const std::string& default_value);

void verify_mandatory_attr_value(const xercesc::DOMElement& elem,
                                 const std::string& ns,
                                 const std::string& name,
                                 const std::string& exp_value);

/**
 *   @brief Extract text from XML DOM element
 *
 *   Text contained in Text and CDATA nodes is extracted.
 *   Comment and attribute nodes are silently ignored.
 *   All other node types (like DOM element) causes
 *   std::runtime_error to be thrown.
 */
std::string extract_text(const xercesc::DOMElement& element);

/**
 *   @brief Create xercesc::DOMSerializer for outputting XML
 *
 *   Note one must call xercesc::DOMSerializer member function
 *   release() to indicate that serializer is no more needed
 */
xercesc::DOMLSSerializer* create_dom_serializer();

std::string xml2string(const xercesc::DOMNode* node);

std::vector<xercesc::DOMElement*> get_child_elements(const xercesc::DOMElement& element,
                                                     const std::string& ns,
                                                     const std::string& name);

/**
 *   @brief Creates DOM document with empty root element using provided
 *          namespace and local name
 */
boost::shared_ptr<xercesc::DOMDocument> create_dom_document(const std::string& ns,
                                                            const std::string& name);

xercesc::DOMElement* create_element(xercesc::DOMDocument& doc,
                                    const std::string& ns,
                                    const std::string& name);

xercesc::DOMElement* append_child_element(xercesc::DOMElement& parent,
                                          const std::string& ns,
                                          const std::string& name);

xercesc::DOMElement* append_child_text_element(xercesc::DOMElement& parent,
                                               const std::string& ns,
                                               const std::string& name,
                                               const std::string& content);

void set_attr(xercesc::DOMElement& element, const std::string& name, const std::string& value);

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
