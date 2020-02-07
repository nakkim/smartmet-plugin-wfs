#include "FeatureID.h"
#include "BStream.h"
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>
#include <macgyver/Base64.h>
#include <macgyver/StringConversion.h>
#include <openssl/sha.h>
#include <spine/Exception.h>
#include <cstring>
#include <sstream>

using SmartMet::Plugin::WFS::FeatureID;
using SmartMet::Spine::Value;

namespace bw = SmartMet::Plugin::WFS;

namespace
{
const char* magic = "SDGHWGFWLBFVLDSBVDSvdlvdNnc";
const unsigned FEATURE_ID_REVISION = 1;
}  // namespace

std::string FeatureID::prefix = "WFS-";

FeatureID::FeatureID(std::string stored_query_id,
                     const std::multimap<std::string, SmartMet::Spine::Value> params,
                     unsigned seq_id)
    : stored_query_id(stored_query_id), params(params), seq_id(seq_id)
{
}

FeatureID::~FeatureID() {}

boost::shared_ptr<FeatureID> FeatureID::create_from_id(const std::string& id)
{
  try
  {
    const int prefix_len = prefix.length();
    if (id.substr(0, prefix_len) != prefix)
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::FeatureID::create_feature_id(): invalid feature id '" << id
          << "': must begin WITH '" << prefix << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    std::ostringstream tmp;
    BOOST_FOREACH (char c, id.substr(4))
    {
      switch (c)
      {
        case '.':
          tmp << '+';
          break;
        case '_':
          tmp << '/';
          break;
        case '-':
          tmp << '=';
          break;
        default:
          tmp << c;
          break;
      }
    }

    std::string raw_id = Fmi::Base64::decode(tmp.str());
    if (raw_id.length() < SHA_DIGEST_LENGTH + 1)
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::FeatureID::create_feature_id(): invalid feature id '" << id
          << "': not enough data";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    const std::string sha = raw_id.substr(0, SHA_DIGEST_LENGTH);
    raw_id = raw_id.substr(SHA_DIGEST_LENGTH);

    unsigned char md[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, magic, strlen(magic));
    SHA1_Update(&ctx, raw_id.c_str(), raw_id.length());
    SHA1_Final(md, &ctx);
    auto result_sha = std::string(md, md + SHA_DIGEST_LENGTH);
    if (result_sha != sha)
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::FeatureID::create_feature_id(): invalid feature id '" << id
          << "': hash error";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    bw::IBStream input(reinterpret_cast<const uint8_t*>(raw_id.c_str()), raw_id.length());

    const std::string prefix = input.get_string();
    if (prefix != "SQ")
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::FeatureID::create_feature_id(): invalid feature ID '" << id
          << "': incorrect type";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    unsigned revision = input.get_unsigned();
    if (revision != FEATURE_ID_REVISION)
    {
      std::ostringstream msg;
      msg << "SmartMet::Plugin::WFS::FeatureID::create_feature_id(): invalid feature ID '" << id
          << "': revision mismatch";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    try
    {
      const std::string stored_query_id = input.get_string();
      // Sequence ID is here only to provide a way to make FeatureID unique
      // in case of several identical stored queries in the same request
      // There is no need to actualy store it here
      input.get_unsigned();
      const std::multimap<std::string, SmartMet::Spine::Value> params = input.get_value_map();

      boost::shared_ptr<FeatureID> result(new FeatureID(stored_query_id, params));
      return result;
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Failed to parse feature ID!", nullptr);
      exception.addParameter("Feature ID", id);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string FeatureID::get_id() const
{
  try
  {
    bw::OBStream stream;
    stream.put_string("SQ");
    stream.put_unsigned(FEATURE_ID_REVISION);
    stream.put_string(stored_query_id);
    stream.put_unsigned(seq_id);
    stream.put_value_map(params);

    const std::string raw_data = stream.raw_data();
    unsigned char md[20];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, magic, strlen(magic));
    SHA1_Update(&ctx, raw_data.c_str(), raw_data.length());
    SHA1_Final(md, &ctx);

    std::string raw_id(md, md + 20);
    raw_id += raw_data;

    const std::string id = Fmi::Base64::encode(raw_id);
    std::ostringstream tmp;
    tmp << prefix;
    BOOST_FOREACH (char c, id)
    {
      switch (c)
      {
        case '+':
          tmp << '.';
          break;
        case '/':
          tmp << '_';
          break;
        case '=':
          tmp << '-';
          break;
        default:
          tmp << c;
          break;
      }
    }

    return tmp.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void FeatureID::erase_param(const std::string& name)
{
  try
  {
    params.erase(name);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
