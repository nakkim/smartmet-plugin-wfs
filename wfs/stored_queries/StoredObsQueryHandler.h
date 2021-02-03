#pragma once

#ifndef WITHOUT_OBSERVATION

#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include "StoredQueryHandlerBase.h"
#include "SupportsBoundingBox.h"
#include "SupportsExtraHandlerParams.h"
#include "SupportsLocationParameters.h"
#include "SupportsMeteoParameterOptions.h"
#include "SupportsQualityParameters.h"
#include "SupportsTimeZone.h"
#include "RequiresGeoEngine.h"
#include "RequiresObsEngine.h"
namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredObsQueryHandler : public StoredQueryHandlerBase,
                              protected SupportsLocationParameters,
                              protected SupportsBoundingBox,
                              protected SupportsTimeZone,
                              protected SupportsQualityParameters,
                              protected SupportsMeteoParameterOptions,
                              protected virtual RequiresGeoEngine,
                              protected virtual RequiresObsEngine
{
 public:
  StoredObsQueryHandler(SmartMet::Spine::Reactor* reactor,
                        StoredQueryConfig::Ptr config,
                        PluginImpl& plugin_impl,
                        boost::optional<std::string> template_file_name);

  virtual ~StoredObsQueryHandler();

  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string> &hostname,
                     std::ostream& output) const;

 private:

  struct ParamIndexEntry
  {
	/** Parameter index in obsengine response or -1 for special parameters generated separately */
	int ind;
	
	/** Parameter name */
	std::string name;
	
	/** Sensor number */
	boost::optional<std::string> sensor_name;	
  };

  struct ExtParamIndexEntry
  {
      ParamIndexEntry p;
      boost::optional<ParamIndexEntry> qc;
  };

  /**
   *   @brief Verifies requested parameter names
   *
   *   Throws an exception if on of the following is detected
   *   - duplicate parameter name (case insensitive)
   *   - qc_parameter requested explictitly but QC parameters support is not enabled
   */
  void check_parameter_names(const RequestParameterMap& params,
                             const std::vector<std::string>& param_names) const;

  int lookup_initial_parameter(const std::string& name) const;

  bool add_parameters(const RequestParameterMap& params,
                      const std::vector<std::string>& names,
                      SmartMet::Engine::Observation::Settings& query_params,
                      std::vector<ExtParamIndexEntry>& parameter_index) const;

 private:
  struct GroupRec
  {
    int group_id;
    std::string feature_id;
    std::map<std::string, std::string> param_ids;
  };

  /**
   *   Contains index of result lines for each station
   */
  struct SiteRec
  {
    int group_id;
    int ind_in_group;
    std::vector<int> row_index_vect;
  };

 private:
  /**
   *   @brief The vector of initial parameters which are queried always
   */
  std::vector<SmartMet::Spine::Parameter> initial_bs_param;

  /**
   *    @brief The indexes if always queried parameters
   *    @{
   */
  int fmisid_ind;
  int geoid_ind;
  int lon_ind;
  int lat_ind;
  int height_ind;
  int name_ind;
  int dist_ind;
  int direction_ind;
  int wmo_ind;

  /**
   *  @brief Largest allowed time interval in hours
   */
  double max_hours;

  /**
   *  @brief Max. allowed stations cound per request
   */
  std::size_t max_station_count;

  bool separate_groups;

  /**
   * @brief Allow to turn restrictions off.
   */
  bool sq_restrictions;

  /**
   * @brief Support parameters with "qc_" prefix
   */
  bool m_support_qc_parameters;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
