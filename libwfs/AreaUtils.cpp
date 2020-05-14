#include "AreaUtils.h"
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/shared_ptr.hpp>
#include <spine/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
std::vector<NFmiPoint> bbox_exclude_point(const NFmiPoint& p1,
                                          const NFmiPoint& p2,
                                          const NFmiPoint& xy,
                                          double w_coeff)
{
  try
  {
    const double x1 = std::min(p1.X(), p2.X());
    const double y1 = std::min(p1.Y(), p2.Y());
    const double x2 = std::max(p1.X(), p2.X());
    const double y2 = std::max(p1.Y(), p2.Y());

    OGRLinearRing r1;
    r1.addPoint(x1, y1);
    r1.addPoint(x1, y2);
    r1.addPoint(x2, y2);
    r1.addPoint(x2, y1);
    r1.closeRings();
    boost::shared_ptr<OGRPolygon> g1(new OGRPolygon);
    g1->addRing(&r1);

    OGRPoint pxy;
    pxy.setX(xy.X());
    pxy.setY(xy.Y());

    if (pxy.Within(g1.get()))
    {
      const double p_off = w_coeff * std::max(1.0, std::min(x2 - x1, y2 - y1));
      const double dxl = xy.X() - x1;
      const double dxr = x2 - xy.X();
      const double dyb = xy.Y() - y1;
      const double dyt = y2 - xy.Y();

      double qx, qy;
      if (std::min(dxl, dxr) < std::min(dyt, dyb))
      {
        qy = xy.Y();
        qx = dxl < dxr ? x1 : x2;
      }
      else
      {
        qx = xy.X();
        qy = dyt < dyb ? y2 : y1;
      }

      const double x3 = std::min(qx, xy.X()) - p_off;
      const double y3 = std::min(qy, xy.Y()) - p_off;
      const double x4 = std::max(qx, xy.X()) + p_off;
      const double y4 = std::max(qy, xy.Y()) + p_off;

      OGRLinearRing r2;

      OGRLinearRing r1;
      r2.addPoint(x3, y3);
      r2.addPoint(x3, y4);
      r2.addPoint(x4, y4);
      r2.addPoint(x4, y3);
      r2.closeRings();
      OGRPolygon g2;
      g2.addRing(&r2);

      OGRGeometry* g3 = g1->Difference(&g2);
      OGRPolygon* g3p = &dynamic_cast<OGRPolygon&>(*g3);
      g1.reset(g3p);
    }

    std::vector<NFmiPoint> result;
    auto* boundary = g1->getExteriorRing();
    int num_points = boundary->getNumPoints();
    for (int i = 0; i < num_points; i++)
    {
      result.push_back(NFmiPoint(boundary->getX(i), boundary->getY(i)));
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void get_latlon_boundary(const NFmiArea* area, OGRPolygon* result, int NP, double w_coeff)
{
  try
  {
    namespace bl = boost::lambda;

    const NFmiRect area_rect = area->XYArea();

    const NFmiPoint lb(area_rect.Left(), area_rect.Bottom());
    const NFmiPoint rt(area_rect.Right(), area_rect.Top());

    static const NFmiPoint north_pole(0.0, 90.0);
    static const NFmiPoint south_pole(0.0, -90.0);

    NFmiPoint np_xy = area->ToXY(north_pole);
    NFmiPoint sp_xy = area->ToXY(south_pole);

    bool np_inside = area_rect.IsInside(np_xy);
    bool sp_inside = area_rect.IsInside(sp_xy);
    if (np_inside and sp_inside)
    {
      throw SmartMet::Spine::Exception(
          BCP, "Area with both north and south pole inside is not supported!");
    }

    NFmiPoint pp = sp_inside ? sp_xy : np_xy;
    const std::vector<NFmiPoint> orig_points = bbox_exclude_point(lb, rt, pp);
    const int num_points = orig_points.size();
    if (num_points < 2)
    {
      throw SmartMet::Spine::Exception(BCP, "INTERNAL ERROR: Not enough points");
    }

    std::vector<NFmiPoint> points;
    for (int i = 0; i < num_points - 1; i++)
    {
      const NFmiPoint& p1 = orig_points.at(i);
      const NFmiPoint& p2 = orig_points.at(i + 1);
      double dx = (p2.X() - p1.X()) / NP;
      double dy = (p2.Y() - p1.Y()) / NP;
      for (int k = (i == 0 ? 0 : 1); k <= NP; k++)
      {
        const NFmiPoint p(p1.X() + k * dx, p1.Y() + k * dy);
        points.push_back(p);
      }
    }

    for (auto it = points.begin(); it != points.end(); ++it)
    {
      *it = area->ToLatLon(*it);
    }

    OGRLinearRing r1;
    for (std::size_t i = 1; i < points.size(); i++)
    {
      const NFmiPoint& p = points[i];
      r1.addPoint(p.X(), p.Y());
    }
    r1.closeRings();

    result->empty();
    result->addRing(&r1);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
