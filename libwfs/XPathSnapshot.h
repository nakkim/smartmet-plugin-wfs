#pragma once

#include "XmlDomErrorHandler.h"
#include <boost/scoped_ptr.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xqilla/xqilla-dom3.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Xml
{
class XPathSnapshot : private XmlDomErrorHandler
{
 public:
  XPathSnapshot();

  virtual ~XPathSnapshot();

  void parse_dom_document(const std::string& source, const std::string& public_id);

  void parse_dom_document(xercesc::InputSource* input_source);

  std::size_t xpath_query(const std::string& xpath_expr);

  xercesc::DOMDocument* get_document();

  std::size_t size() const;

  xercesc::DOMNode* get_item(int ind);

  std::string lookup_prefix(const std::string& uri) const;

 private:
  void handle_exceptions(const std::string& location) const __attribute__((noreturn));

  void assert_have_document() const;

  void assert_have_result() const;

 private:
  xercesc::DOMImplementation* xqillaImplementation;
  AutoRelease<xercesc::DOMLSParser> parser;
  xercesc::DOMDocument* document;
  xercesc::DOMXPathNSResolver* resolver;
  xercesc::DOMXPathExpression* expression;
  xercesc::DOMXPathResult* xpath_result;
};

}  // namespace Xml
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
