#include "QueryBase.h"

namespace bw = SmartMet::Plugin::WFS;

const char *bw::QueryBase::FMI_APIKEY_SUBST = "@FMI_APIKEY@";
const char *bw::QueryBase::FMI_APIKEY_PREFIX_SUBST = "@FMI_APIKEY_PREFIX@";
const char *bw::QueryBase::HOSTNAME_SUBST = "@HOSTNAME@";
const char *bw::QueryBase::PROTOCOL_SUBST = "@PROTOCOL@";

bw::QueryBase::QueryBase() : query_id(1), stale_seconds(0) {}
bw::QueryBase::~QueryBase() {}
void bw::QueryBase::set_query_id(int id)
{
  this->query_id = id;
}
void bw::QueryBase::set_stale_seconds(const int &stale_seconds)
{
  this->stale_seconds = stale_seconds;
}

void bw::QueryBase::set_cached_response(const std::string &response)
{
  cached_response = response;
}
