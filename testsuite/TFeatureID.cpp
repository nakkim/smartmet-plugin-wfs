#include <iostream>
#include <random>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>
#include <newbase/NFmiPoint.h>
#include "FeatureID.h"

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "FeatureID tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using namespace SmartMet;
using namespace SmartMet::Plugin::WFS;
using SmartMet::Spine::Value;

namespace
{
void dump(const std::multimap<std::string, Value>& data)
{
  std::cout << '(';
  BOOST_FOREACH (const auto& item, data)
  {
    std::cout << "('" << item.first << "' " << item.second << ')';
  }
  std::cout << ')';
}
}

BOOST_AUTO_TEST_CASE(test_generating_feature_id_and_parsing_it)
{
  const char* stored_query_id = "fmi::test::query";
  std::multimap<std::string, Value> params;
  params.insert(std::make_pair("A", Value(1.0)));
  params.insert(std::make_pair("A", Value(2.0)));
  params.insert(std::make_pair("B", Value("foo")));
  params.insert(std::make_pair("A", Value(1)));

  FeatureID id1(stored_query_id, params);

  boost::shared_ptr<FeatureID> id2;
  try
  {
    id2 = FeatureID::create_from_id(id1.get_id());
    if (id2->get_params() != params)
    {
      std::cout << "GOT: ";
      dump(id2->get_params());
      std::cout << "\n";
      std::cout << "EXPECTED: ";
      dump(id2->get_params());
      std::cout << "\n";
      BOOST_REQUIRE(false);
    }
  }
  catch (const std::exception& err)
  {
    BOOST_REQUIRE(false);
  }
}

BOOST_AUTO_TEST_CASE(test_edit_params)
{
  const char* stored_query_id = "fmi::test::query";
  const double c_bbox[4] = {21.0, 60.0, 23.0, 62.0};

  std::multimap<std::string, Value> params;
  params.insert(std::make_pair("A", Value(1.0)));
  params.insert(std::make_pair("A", Value(2.0)));
  params.insert(std::make_pair("B", Value("foo")));
  params.insert(std::make_pair("A", Value(1)));

  FeatureID id1(stored_query_id, params);
  id1.add_param("bbox", c_bbox, c_bbox + 4);

  BOOST_CHECK_EQUAL(4, (int)id1.get_params().count("bbox"));

  boost::shared_ptr<FeatureID> id2;
  try
  {
    id2 = FeatureID::create_from_id(id1.get_id());
    if (id2->get_params() != id1.get_params())
    {
      std::cout << "GOT: ";
      dump(id2->get_params());
      std::cout << "\n";
      std::cout << "EXPECTED: ";
      dump(id1.get_params());
      std::cout << "\n";
      BOOST_REQUIRE(false);
    }
  }
  catch (const std::exception& err)
  {
    BOOST_REQUIRE(false);
  }
}

BOOST_AUTO_TEST_CASE(test_2)
{
  int num_err = 0;
  const unsigned num_tests = 100;
  const char* stored_query_id = "fmi::test::query";
  std::mt19937 rng;
  std::uniform_real_distribution<double> rn(0.0, 1.0);
  for (unsigned i = 0; i < num_tests; i++) {
    int k = 0;
    std::multimap<std::string, Value> params;
    while (true) {
      double val = rn(rng);
      std::string name = "A" + std::to_string(++k);
      if (val > 0.99) {
	break;
      } else if (val > 0.7) {
	params.insert(std::make_pair(name, Value((val - 0.7)/0.29)));
      } else if (val > 0.3) {
	params.insert(std::make_pair(name, Value(k*k-25)));
      } else {
	params.insert(std::make_pair(name, Value("B"+ std::to_string(k*k))));
      }
      const double c_bbox[4] = {21.0, 60.0, 23.0, 62.0};
      FeatureID id1(stored_query_id, params);
      id1.add_param("bbox", c_bbox, c_bbox + 4);
      boost::shared_ptr<FeatureID> id2 = FeatureID::create_from_id(id1.get_id());
      if (id1.get_params() != id2->get_params()) {
	num_err++;
      }
    }
    if (i > 0 && (i % 100) == 0) { std::cout << i << "  " << params.size() << "  " << num_err << std::endl; }
  }
  std::cout << "Finished" << std::endl;
  BOOST_REQUIRE_EQUAL(num_err, 0);
}
