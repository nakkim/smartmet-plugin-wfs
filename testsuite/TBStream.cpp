#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/test/included/unit_test.hpp>
#include <BStream.h>
#include <boost/algorithm/hex.hpp>
#include <spine/Exception.h>

using namespace boost::unit_test;

test_suite *init_unit_test_suite(int argc, char *argv[])
{
  const char *name = "BStream tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

using SmartMet::Plugin::WFS::IBStream;
using SmartMet::Plugin::WFS::OBStream;

BOOST_AUTO_TEST_CASE(test_writing_and_reading_bits_1)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading bits (fits in 1 byte)]");

  OBStream out;
  out.put_bit(1);
  out.put_bit(1);
  out.put_bit(0);
  out.put_bit(1);
  out.put_bit(1);
  const std::string s01 = out.raw_data();
  BOOST_REQUIRE_EQUAL(1, (int)s01.length());
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());
  BOOST_REQUIRE_EQUAL(0xD8U, (unsigned)u01[0]);

  IBStream in(u01, 1);
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK_THROW(in.get_bit(), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_bits_2)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading bits (more that 1 byte)]");

  OBStream out;
  out.put_bit(1);
  out.put_bit(1);
  out.put_bit(0);
  out.put_bit(1);
  out.put_bit(1);
  out.put_bit(0);
  out.put_bit(0);
  out.put_bit(0);
  out.put_bit(1);
  const std::string s01 = out.raw_data();
  BOOST_REQUIRE_EQUAL(2, (int)s01.length());
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());
  BOOST_REQUIRE_EQUAL(0xD8U, (unsigned)u01[0]);
  BOOST_REQUIRE_EQUAL(0x80U, (unsigned)u01[1]);

  IBStream in(u01, 2);
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK(in.get_bit());

  IBStream in2(u01, 2);
  BOOST_CHECK_EQUAL(0x0D, in2.get_bits(4));
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_bits_3)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading unsigned integer values]");
  OBStream out;
  out.put_bit(0);
  out.put_bits(0x0BU, 4);
  out.put_bits(0x13U, 5);
  out.put_bits(0x00U, 6);
  // 0
  //  101 1
  //       100 11
  //             00 0000
  //====================
  // 0101 1100 1100 0000
  const std::string s01 = out.raw_data();
  BOOST_REQUIRE_EQUAL(2, (int)s01.length());
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());
  BOOST_REQUIRE_EQUAL(0x5CU, (unsigned)u01[0]);
  BOOST_REQUIRE_EQUAL(0xC0U, (unsigned)u01[1]);

  IBStream in(u01, 2);
  BOOST_CHECK(not in.get_bit());
  BOOST_CHECK_EQUAL(0x05, (int)in.get_bits(3));
  BOOST_CHECK_EQUAL(0x0CC0, (int)in.get_bits(12));
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_unsigned_int)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading unsigned int values]");
  OBStream out;
  out.put_unsigned(15);
  out.put_unsigned(0);
  out.put_unsigned(31);
  out.put_unsigned(23456);
  out.put_unsigned(123456789U);
  out.put_unsigned(1234567890123456789ULL);
  out.put_unsigned(0);
  // 1011 1100  1111 1101  0000 0111  1011 1011
  //===========================================
  // 1011 110
  //         0
  //            1111 110
  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  // std::cout << "Content: ";
  // boost::algorithm::hex(u01, u01 + s01.length(), std::ostream_iterator<char>(std::cout));
  // std::cout << "\n";

  BOOST_REQUIRE_EQUAL(0xBCU, (unsigned)u01[0]);
  IBStream in(u01, s01.length());

  BOOST_REQUIRE_EQUAL(15ULL, in.get_unsigned());
  BOOST_REQUIRE_EQUAL(0ULL, in.get_unsigned());
  BOOST_REQUIRE_EQUAL(31ULL, in.get_unsigned());
  BOOST_REQUIRE_EQUAL(23456ULL, in.get_unsigned());
  BOOST_REQUIRE_EQUAL(123456789ULL, in.get_unsigned());
  BOOST_REQUIRE_EQUAL(1234567890123456789ULL, in.get_unsigned());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_signed_int)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading signed integer values]");
  OBStream out;
  out.put_int(15);
  out.put_int(0);
  out.put_int(-31);
  out.put_int(23456);
  out.put_int(-123456789LL);
  out.put_int(1234567890123456789LL);
  out.put_int(0);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());

  BOOST_REQUIRE_EQUAL(15LL, in.get_int());
  BOOST_REQUIRE_EQUAL(0LL, in.get_int());
  BOOST_REQUIRE_EQUAL(-31LL, in.get_int());
  BOOST_REQUIRE_EQUAL(23456ULL, in.get_int());
  BOOST_REQUIRE_EQUAL(-123456789ULL, in.get_int());
  BOOST_REQUIRE_EQUAL(1234567890123456789LL, in.get_int());
  BOOST_REQUIRE_EQUAL(0LL, in.get_int());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_double_values)
{
  BOOST_TEST_MESSAGE("+[Test writting and reading floating point values]");
  OBStream out;
  out.put_double(M_PI);
  out.put_double(12.0);
  out.put_double(12.625);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());

  BOOST_REQUIRE_EQUAL(M_PI, in.get_double());
  BOOST_REQUIRE_EQUAL(12.0, in.get_double());
  BOOST_REQUIRE_EQUAL(12.625, in.get_double());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_strings)
{
  const char *s1 = "Tämä on esimerkki";
  const char *s2 = "Vielä yksi esimerkki";
  BOOST_TEST_MESSAGE("+[Test writting and reading floating point values]");
  OBStream out;
  out.put_string(s1);
  out.put_string(s2);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());
  BOOST_REQUIRE_EQUAL(std::string(s1), in.get_string());
  BOOST_REQUIRE_EQUAL(std::string(s2), in.get_string());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_ptime_values)
{
  namespace bg = boost::gregorian;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+[Test writting and reading boost::posix_time::ptime values]");

  const pt::ptime t1 = pt::ptime(bg::date(2013, 03, 26));
  const pt::ptime t2 = pt::time_from_string("2013-03-26 09:22:12");

  OBStream out;
  out.put_ptime(t1);
  out.put_ptime(t2);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());
  BOOST_REQUIRE_EQUAL(t1, in.get_ptime());
  BOOST_REQUIRE_EQUAL(t2, in.get_ptime());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_ptime_special_values)
{
  namespace bg = boost::gregorian;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+[Test writting and reading boost::posix_time::ptime values]");

  const pt::ptime t1 = pt::not_a_date_time;
  const pt::ptime t2 = pt::pos_infin;
  const pt::ptime t3 = pt::neg_infin;

  OBStream out;
  out.put_ptime(t1);
  out.put_ptime(t2);
  out.put_ptime(t3);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());
  BOOST_REQUIRE_EQUAL(t1, in.get_ptime());
  BOOST_REQUIRE_EQUAL(t2, in.get_ptime());
  BOOST_REQUIRE_EQUAL(t3, in.get_ptime());
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_much_data)
{
  OBStream out;
  const int SIZE = 8192;
  int i1[SIZE];
  for (int i = 0; i < SIZE; i++)
  {
    i1[i] = i * i;
    out.put_int(i1[i]);
  }

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());
  IBStream in(u01, s01.length());
  int num_err = 0;
  for (int i = 0; i < SIZE; i++)
  {
    int64_t i2 = in.get_int();
    if (i2 != i1[i])
    {
      std::cerr << "\n[" << i << "]: " << i2 << " != " << i1[i] << std::endl;
      num_err++;
    }
  }
  BOOST_CHECK_EQUAL(0, num_err);
}

BOOST_AUTO_TEST_CASE(test_writing_and_reading_value_map)
{
  using SmartMet::Spine::Value;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+[Test writting and reading value map]");

  std::multimap<std::string, Value> data;
  data.insert(std::make_pair("A", Value(2.0)));
  data.insert(std::make_pair("A", Value(3.0)));
  data.insert(std::make_pair("B", Value(pt::time_from_string("2013-03-25 09:55:54"))));
  data.insert(std::make_pair("C", Value("foobar")));
  data.insert(std::make_pair("D", Value(1)));
  data.insert(std::make_pair("D", Value(1)));
  data.insert(std::make_pair("E", Value(SmartMet::Spine::Point("24.12,61.23,EPSG:4258"))));
  data.insert(
      std::make_pair("F", Value(SmartMet::Spine::BoundingBox("24.12,61.23,25.12,62.23,EPSG:4258"))));

  OBStream out;
  out.put_value_map(data);

  const std::string s01 = out.raw_data();
  const uint8_t *u01 = reinterpret_cast<const uint8_t *>(s01.data());

  IBStream in(u01, s01.length());
  auto test = in.get_value_map();

  if (test != data)
  {
    std::multimap<std::string, SmartMet::Spine::Value>::const_iterator it1, it2;

    it1 = data.begin();
    it2 = test.begin();
    while (it1 != data.end() or it2 != test.end())
    {
      if (it1 != data.end() and it2 != test.end())
      {
        std::cout << (*it1 == *it2 ? "EQUAL     : " : "NOT EQUAL : ");
        std::cout << " GOT ('" << it1->first << "' " << it1->second << ")    "
                  << " EXPECTED ('" << it2->first << "' " << it2->second << ")" << std::endl;
      }
      else if (it1 != data.end())
      {
        std::cout << "GOT UNEXPECTED ('" << it1->first << "' " << it1->second << ")" << std::endl;
      }
      else
      {
        std::cout << "MISSING ('" << it2->first << "' " << it2->second << ")" << std::endl;
      }

      ++it1;
      ++it2;
    }
  }

  BOOST_REQUIRE(test == data);
}
