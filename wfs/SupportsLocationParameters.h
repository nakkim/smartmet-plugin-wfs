#pragma once

#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include "RequiresGeoEngine.h"
#include "StoredQueryConfig.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsLocationParameters.h"
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <engines/geonames/Engine.h>
#include <spine/Value.h>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *  @brief Base class for stored query location parameter support
 */
class SupportsLocationParameters : protected virtual SupportsExtraHandlerParams,
                                   protected virtual StoredQueryParamRegistry,
                                   protected virtual RequiresGeoEngine
{
 public:
  enum location_options_t
  {
    SUPPORT_KEYWORDS = 2,
    INCLUDE_GEOIDS = 4,
    INCLUDE_FMISIDS = 8,
    INCLUDE_WMOS = 16,
    INCLUDE_LPNNS = 32
  };

 protected:
  /**
   *    @brief Constructor
   *
   *    @param config Stored query configuration file to use
   *    @param options Options which specify what to support. See enum location_options_t
   *            for the meaning of separate bits
   */
    SupportsLocationParameters(
        SmartMet::Spine::Reactor* reactor,
        boost::shared_ptr<StoredQueryConfig> config,
        unsigned options);

 public:
  virtual ~SupportsLocationParameters();

  void get_location_options(
      const RequestParameterMap &param_values,
      const std::string &language,
      std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const;

  void get_geoids(
      const RequestParameterMap &param,
      const std::string &language_requested,
      std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const;

  void get_fmisids(
      const RequestParameterMap &param,
      const std::string &language_requested,
      std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const;

  void get_wmos(const RequestParameterMap &param,
                const std::string &language_requested,
                std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const;

  void get_lpnns(const RequestParameterMap &param,
                const std::string &language_requested,
                std::list<std::pair<std::string, SmartMet::Spine::LocationPtr> > *locations) const;

  // This is a temporary hack because geoengine has not full support for languages.
  // the method change "fin" to "fi" or "eng" to "en"
  static void engOrFinToEnOrFi(std::string &language);

 private:
  const bool support_keywords;
  const bool include_fmisids;
  const bool include_geoids;
  const bool include_wmos;
  const bool include_lpnns;

 protected:
  static const char *P_PLACES;
  static const char *P_LATLONS;
  static const char *P_FMISIDS;
  static const char *P_GEOIDS;
  static const char *P_WMOS;
  static const char *P_LPNNS;
  static const char *P_MAX_DISTANCE;
  static const char *P_KEYWORD;
  static const char *P_KEYWORD_OVERWRITABLE;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
