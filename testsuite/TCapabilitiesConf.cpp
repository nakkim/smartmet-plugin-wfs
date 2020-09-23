#define BOOST_TEST_MODULE TCapabilitiesTest
#define BOOST_TEST_DYN_LINK 1
#include <iostream>
#include <boost/test/unit_test.hpp>
#include <CapabilitiesConf.h>
#include <macgyver/Exception.h>

using namespace boost::unit_test;
using libconfig::Setting;

test_suite *init_unit_test_suite(int argc, char *argv[])
{
  const char *name = "SmartMet::Plugin::WFS::CapabilitiesConf tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Plugin::WFS::CapabilitiesConf;

BOOST_AUTO_TEST_CASE(test_parsing_capabilities_info_1)
{
  libconfig::Config cfg_src;
  auto& root = cfg_src.getRoot();
  auto& x1 = root.add("title", Setting::TypeGroup);
  x1.add("eng", Setting::TypeString) = "Title";
  x1.add("fin", Setting::TypeString) = "Otsikko";
  root.add("abstract", Setting::TypeString) = "Abstract";
  auto& x2 = root.add("address", Setting::TypeGroup);
  x2.add("city", Setting::TypeString) = "Helsinki";

  SmartMet::Plugin::WFS::CapabilitiesConf conf;
  try {
    conf.parse("eng", cfg_src.getRoot());
  } catch (const Fmi::Exception& e) {
    std::cout << e.getStackTrace() << std::endl;
    BOOST_REQUIRE(false);
  }

  {
    CTPP::CDT hash;
    conf.apply(hash, "eng");
    //std::cout << hash.RecursiveDump() << std::endl;
    BOOST_REQUIRE(hash.Exists("title"));
    BOOST_REQUIRE_EQUAL(hash.At("title").ToString(), std::string("Title"));
    BOOST_REQUIRE(hash.Exists("abstract"));
    BOOST_REQUIRE_EQUAL(hash.At("abstract").ToString(), std::string("Abstract"));
    BOOST_REQUIRE(hash.Exists("address"));
    auto& addr = hash.At("address");
    BOOST_REQUIRE(addr.Exists("city"));
    BOOST_REQUIRE_EQUAL(addr.At("city").ToString(), std::string("Helsinki"));
    //std::cout << hash.Dump() << std::endl;
    BOOST_REQUIRE_EQUAL(int(hash.Size()), 4);
    BOOST_REQUIRE_EQUAL(int(addr.Size()), 1);
  }

  {
    CTPP::CDT hash;
    conf.apply(hash, "fin");
    //std::cout << hash.RecursiveDump() << std::endl;
    BOOST_REQUIRE(hash.Exists("title"));
    BOOST_REQUIRE_EQUAL(hash.At("title").ToString(), std::string("Otsikko"));
    BOOST_REQUIRE(hash.Exists("abstract"));
    BOOST_REQUIRE_EQUAL(hash.At("abstract").ToString(), std::string("Abstract"));
    BOOST_REQUIRE(hash.Exists("address"));
    auto& addr = hash.At("address");
    BOOST_REQUIRE(addr.Exists("city"));
    BOOST_REQUIRE_EQUAL(addr.At("city").ToString(), std::string("Helsinki"));
    //std::cout << hash.Dump() << std::endl;
    BOOST_REQUIRE_EQUAL(int(hash.Size()), 4);
    BOOST_REQUIRE_EQUAL(int(addr.Size()), 1);
  }
}
