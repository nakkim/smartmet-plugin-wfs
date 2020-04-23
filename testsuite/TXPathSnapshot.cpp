#define BOOST_TEST_MODULE TXPathSnapshot
#define BOOST_TEST_DYN_LINK 1
#include <iostream>
#include <boost/test/unit_test.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include "XmlEnvInit.h"
#include "XPathSnapshot.h"

using namespace boost::unit_test;

test_suite *init_unit_test_suite(int argc, char *argv[])
{
  const char *name = "XPathSnapshot tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using namespace SmartMet::Plugin::WFS::Xml;

static const char *src =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<wfs:GetFeature\n"
    "         service=\"WFS\"\n"
    "         version=\"2.0.0\"\n"
    "         xmlns:wfs=\"http://www.opengis.net/wfs/2.0\"\n"
    "         xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    "         xsi:schemaLocation=\"http://www.opengis.net/wfs/2.0 "
    "http://schemas.opengis.net/wfs/2.0/wfs.xsd\">\n"
    "   <wfs:StoredQuery id=\"ObsTest\">\n\n"
    "     <wfs:Parameter name=\"begin\">20120729T100000Z</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"end\">20120729T190000Z</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"place\">Kumpula,Helsinki</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"place\">Kaisaniemi,Helsinki</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"place\">Sepänkylä,Espoo</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"timestep\">0</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"locale\">fi_FI</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"parameter\">Temperature</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"parameter\">pressure</wfs:Parameter>\n"
    "     <wfs:Parameter name=\"parameter\">Precipitation1h</wfs:Parameter>\n"
    "   </wfs:StoredQuery>\n"
    "</wfs:GetFeature>\n";

BOOST_AUTO_TEST_CASE(test_xpath_implementation)
{
  using namespace xercesc;

  EnvInit init;

  BOOST_TEST_MESSAGE("+ [Test XPath implementation]");

  XPathSnapshot snapshot;

  const char *public_id = "wfs_temp_doc";
  MemBufInputSource input_source(reinterpret_cast<const XMLByte *>(src), strlen(src), public_id);

  BOOST_REQUIRE_NO_THROW(snapshot.parse_dom_document(&input_source));

  std::size_t size = 0;
  DOMNode *node = 0;
  BOOST_REQUIRE_NO_THROW(size = snapshot.xpath_query("/wfs:GetFeature/wfs:StoredQuery"));
  BOOST_CHECK_EQUAL(1, (int)size);
  BOOST_CHECK_EQUAL(1, (int)snapshot.size());
  BOOST_REQUIRE_NO_THROW(node = snapshot.get_item(0));
  BOOST_REQUIRE(node != NULL);
  BOOST_CHECK_EQUAL(DOMNode::ELEMENT_NODE, node->getNodeType());

  const char *test_str_2 = "/wfs:GetFeature/wfs:StoredQuery/wfs:Parameter/@name";
  BOOST_REQUIRE_NO_THROW(size = snapshot.xpath_query(test_str_2));
  BOOST_CHECK_EQUAL(10, (int)size);
  BOOST_REQUIRE_NO_THROW(node = snapshot.get_item(3));
  BOOST_REQUIRE(node != NULL);
  BOOST_CHECK_EQUAL(DOMNode::ATTRIBUTE_NODE, node->getNodeType());
  DOMAttr *attr = NULL;
  BOOST_REQUIRE_NO_THROW(attr = &dynamic_cast<DOMAttr &>(*node));
  std::string attr_name = UTF8(attr->getValue());
  BOOST_CHECK_EQUAL(std::string("place"), attr_name);

  std::string prefix;
  BOOST_REQUIRE_NO_THROW(prefix = snapshot.lookup_prefix("http://www.opengis.net/wfs/2.0"));
  BOOST_CHECK_EQUAL(std::string("wfs"), prefix);
  BOOST_REQUIRE_NO_THROW(prefix =
                             snapshot.lookup_prefix("http://www.w3.org/2001/XMLSchema-instance"));
  BOOST_CHECK_EQUAL(std::string("xsi"), prefix);
}
