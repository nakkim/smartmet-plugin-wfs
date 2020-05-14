#include "XPathSnapshot.h"
#include "XmlUtils.h"
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <xercesc/dom/DOMLSParserFilter.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace xercesc;
using SmartMet::Plugin::WFS::Xml::XmlDomErrorHandler;
using SmartMet::Plugin::WFS::Xml::XPathSnapshot;

namespace
{
struct ParserFilter : public DOMLSParserFilter
{
  ParserFilter() {}
  virtual ~ParserFilter() {}
  FilterAction acceptNode(DOMNode* node)
  {
    try
    {
      if (node->getNodeType() == DOMNode::TEXT_NODE)
      {
        DOMText* text_node = dynamic_cast<DOMText*>(node);
        if (text_node)
        {
          const XMLCh* x_text = text_node->getNodeValue();
          // FIXME: perhaps that could be written more efficiently
          std::string text = SmartMet::Plugin::WFS::Xml::to_string(x_text);
          if (text.find_first_not_of(" \t\r\n") == std::string::npos)
          {
            // FILTER_REJECT does not seem to work OK on RHEL6 (Xerces-C 3.0.0).
            // It does work on Ubuntu though (Xerces-C 3.1.1).
            // So the reason is perhaps Xerces-C bug. Replace text node
            // contents with empty string as the workaround.
            text_node->setNodeValue(X(""));
            // return FILTER_REJECT;
          }
        }
      }
      return FILTER_ACCEPT;
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }
  }

  virtual FilterAction startElement(DOMElement*) { return FILTER_ACCEPT; }
  virtual xercesc::DOMNodeFilter::ShowType getWhatToShow() const
  {
    return xercesc::DOMNodeFilter::SHOW_ALL;
  }
};

ParserFilter filter;
}  // namespace

XPathSnapshot::XPathSnapshot()
    : xqillaImplementation(DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"))),
      parser(xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS, 0)),
      document(nullptr),
      resolver(nullptr),
      expression(nullptr),
      xpath_result(nullptr)
{
  try
  {
    parser->getDomConfig()->setParameter(XMLUni::fgDOMNamespaces, true);
    parser->getDomConfig()->setParameter(XMLUni::fgXercesSchema, true);
    parser->getDomConfig()->setParameter(XMLUni::fgDOMValidateIfSchema, false);
    parser->getDomConfig()->setParameter(XMLUni::fgXercesContinueAfterFatalError, false);
    parser->getDomConfig()->setParameter(XMLUni::fgXercesLoadSchema, false);
    parser->getDomConfig()->setParameter(XMLUni::fgDOMErrorHandler, this);

    parser->setFilter(&filter);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

XPathSnapshot::~XPathSnapshot()
{
  if (xpath_result)
    xpath_result->release();
  if (expression)
    expression->release();
  if (resolver)
    resolver->release();
}

void XPathSnapshot::parse_dom_document(const std::string& src, const std::string& public_id)
{
  try
  {
    MemBufInputSource input_source(
        reinterpret_cast<const XMLByte*>(src.c_str()), src.length(), public_id.c_str());
    parse_dom_document(&input_source);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void XPathSnapshot::parse_dom_document(xercesc::InputSource* input_source)
{
  try
  {
    try
    {
      AutoRelease<DOMLSInput> input(xqillaImplementation->createLSInput());
      input->setByteStream(input_source);
      document = parser->parse(input.get());
      if (document == nullptr)
      {
        throw SmartMet::Spine::Exception(BCP, "Failed to parse stored query result");
      }

      if (resolver)
        resolver->release();
      resolver = document->createNSResolver(document->getDocumentElement());
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t XPathSnapshot::xpath_query(const std::string& xpath_string)
{
  try
  {
    assert_have_document();

    try
    {
      if (xpath_result)
      {
        xpath_result->release();
        xpath_result = nullptr;
      }

      if (expression)
      {
        expression->release();
        expression = nullptr;
      }

      expression = document->createExpression(X(xpath_string.c_str()), resolver);
      xpath_result =
          expression->evaluate(document, DOMXPathResult::UNORDERED_NODE_SNAPSHOT_TYPE, 0);

      return xpath_result->getSnapshotLength();
    }
    catch (...)
    {
      std::cerr << METHOD_NAME << ": failed to process XPath '" << xpath_string << "'" << std::endl;
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMDocument* XPathSnapshot::get_document()
{
  try
  {
    assert_have_document();
    return document;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t XPathSnapshot::size() const
{
  try
  {
    assert_have_result();
    return xpath_result->getSnapshotLength();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

xercesc::DOMNode* XPathSnapshot::get_item(int ind)
{
  try
  {
    assert_have_result();

    try
    {
      xpath_result->snapshotItem(ind);
      return xpath_result->getNodeValue();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string XPathSnapshot::lookup_prefix(const std::string& uri) const
{
  try
  {
    assert_have_document();
    const XMLCh* x_prefix = resolver->lookupPrefix(X(uri.c_str()));
    if (x_prefix)
    {
      return UTF8(x_prefix);
    }
    else
    {
      throw SmartMet::Spine::Exception(BCP, ": URI '" + uri + "' not found!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void XPathSnapshot::assert_have_document() const
{
  try
  {
    if (not document)
    {
      throw SmartMet::Spine::Exception(BCP, "INTERNAL ERROR: one must parse XML doccument first!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void XPathSnapshot::assert_have_result() const
{
  try
  {
    if (not xpath_result)
    {
      throw SmartMet::Spine::Exception(BCP,
                                       "INTERNAL ERROR: one must evaluate XPath expression first!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void XPathSnapshot::handle_exceptions(const std::string& location) const
{
  try
  {
    namespace bwx = SmartMet::Plugin::WFS::Xml;

    try
    {
      throw;
    }
    catch (const std::exception&)
    {
      throw;
    }
    catch (...)
    {
      try
      {
        throw;
      }
      catch (const DOMException& err)
      {
        std::ostringstream msg;
        msg << Fmi::current_exception_type() << ": errorCode=" << err.code << " message='"
            << bwx::to_string(err.msg);
        throw SmartMet::Spine::Exception::Trace(BCP, msg.str());
      }
      catch (...)
      {
        std::ostringstream msg;
        msg << "Unexpected exception of type '" << Fmi::current_exception_type() << "'!";
        throw SmartMet::Spine::Exception::Trace(BCP, msg.str());
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
