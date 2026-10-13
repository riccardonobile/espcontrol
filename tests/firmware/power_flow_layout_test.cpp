#include <cassert>
#include <cmath>
#include "power_flow_layout.h"
#include "power_card_data.h"

using namespace espcontrol::power;
bool same(Point a, Point b) { return std::abs(a.x-b.x)<1e-8 && std::abs(a.y-b.y)<1e-8; }
int main() {
  for (auto family : {FlowFamily::LANDSCAPE, FlowFamily::SQUARE, FlowFamily::PORTRAIT}) {
    for (auto size : {std::array<int,2>{264,300}, {430,260}, {440,430}, {740,380}, {680,1040}}) {
      const auto g = flow_geometry(size[0], size[1], family);
      assert(g.scale > 0);
      assert(same(g.paths[0].front(), g.ports[0][2]));
      assert(same(g.paths[0].back(), g.ports[1][0]));
      assert(same(g.paths[1].front(), g.ports[0][1]));
      assert(same(g.paths[1].back(), g.ports[3][1]));
      assert(same(g.paths[2].back(), g.ports[2][0]));
      assert(same(g.paths[3].back(), g.ports[1][2]));
      assert(same(g.paths[4].back(), g.ports[2][2]));
      assert(same(g.paths[5].front(), g.ports[2][1]));
      assert(same(g.paths[5].back(), g.ports[1][1]));
      for (size_t j=0;j<FLOW_POINT_COUNT;++j)
        assert(same(g.paths[4][j],g.paths[6][FLOW_POINT_COUNT-1-j]));
      for (size_t i=0;i<7;++i) {
        assert(same(g.heads[i][1],g.paths[i].back()));
        for (auto p:g.paths[i]) assert(p.x>=0 && p.x<=size[0] && p.y>=0 && p.y<=size[1]);
      }
      assert(g.heads[6][0].y < g.heads[6][1].y); // arrow into Battery points down
      assert(g.heads[4][0].x > g.heads[4][1].x); // arrow into Grid points left
      assert(std::abs((g.nodes[0].y+g.nodes[3].y)/2-size[1]/2.0)<1e-8);
      assert(std::abs((g.nodes[1].x+g.nodes[2].x)/2-size[0]/2.0)<1e-8);
    }
  }
  // Golden prototype v0.32 coordinates and quadratic controls.
  const auto g=flow_geometry(520,430,FlowFamily::SQUARE);
  assert(same(g.nodes[0],{260,83})); assert(same(g.nodes[2],{102,215}));
  assert(same(g.ports[0][0],{242,137})); assert(same(g.ports[2][2],{171,233}));
  const double r=std::min({30.0,60*.42,71*.42});
  assert(same(g.paths[4][1],{242,233+r}));
  assert(same(g.paths[4][9],{242-r,233}));
  CardData data;
  data.model.set_polarity({true,false});
  data.state(1,"1"); assert(!data.model.readings[1].valid()); // no assumed W
  data.unit(1,"kW"); assert(data.model.readings[1].value==1000);
  data.state(0,"300");data.unit(0,"W");
  data.state(2,"-1000");data.unit(2,"W");
  data.state(3,"-300");data.unit(3,"W");
  assert(data.model.calculation.validity==CalculationValidity::VALID);
  assert(data.model.calculation.secondary_label()=="Charging · Importing");
  data.unit(0,"kWh"); assert(data.model.calculation.validity==CalculationValidity::UNAVAILABLE);
  assert(data.model.calculation.secondary_label()=="Charging · Importing");
  data.unit(0,"W"); assert(data.model.calculation.validity==CalculationValidity::VALID);
  data.state(4,"78");data.unit(4,"%");assert(data.model.battery_soc.valid());
  data.disconnect();assert(!data.model.battery_soc.valid());assert(data.model.polarity.invert_grid);
  data.state(1,"0");assert(!data.model.readings[1].valid());
  data.unit(1,"W");assert(data.model.readings[1].valid());
  data.state(1,std::string(65,'1'));assert(!data.model.readings[1].valid());
}
