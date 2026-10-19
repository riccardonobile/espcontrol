#include <cassert>
#include <cmath>
#include "power_flow_layout.h"
#include "power_card_data.h"

using namespace espcontrol::power;
bool same(Point a, Point b) { return std::abs(a.x-b.x)<1e-8 && std::abs(a.y-b.y)<1e-8; }
int main() {
  static_assert(FLOW_POINT_COUNT==4);
  static_assert(sizeof(FlowGeometry)<=1536);
  assert(flow_coordinate<int32_t>(1.49)==1&&flow_coordinate<int32_t>(1.51)==2);
  assert(flow_coordinate<float>(1.25)==1.25f);
  for(auto size:{std::array<int,2>{304,430},{464,430},{704,670},{1264,750}}){
    for(int back_bottom:{40,60,80}){
      const auto v=flow_viewport(size[0],size[1],8,back_bottom);
      assert(v.x+v.width/2.0==size[0]/2.0);
      assert(v.y+v.height/2.0==size[1]/2.0);
      assert(v.y>=back_bottom&&v.y+v.height<=size[1]-back_bottom);
    }
  }
  using espcontrol::modal::LayoutFamily;
  for(int rows:{3,4}){
    espcontrol::modal::DeviceProfile profile;
    profile.layout_family=rows==3?LayoutFamily::WIDE_LANDSCAPE:LayoutFamily::LARGE_LANDSCAPE;
    const auto grid=flow_dashboard_grid(profile,5,rows,rows==3?1024:1280,rows==3?600:800);
    assert(grid.large&&grid.diagram.cols==3&&grid.diagram.rows==rows);
    int occupied[4][5]{};
    auto occupy=[&](FlowGridCell cell){for(int y=cell.row;y<cell.row+cell.rows;++y)for(int x=cell.col;x<cell.col+cell.cols;++x){assert(y<rows&&x<5);assert(++occupied[y][x]==1);}};
    occupy(grid.diagram);occupy(grid.back);for(auto cell:grid.values)occupy(cell);
    for(int y=0;y<rows;++y)for(int x=0;x<5;++x)assert(occupied[y][x]==1);
    assert(!flow_dashboard_grid(profile,5,rows,600,1024).large);
    assert(!flow_dashboard_grid(profile,3,rows,1024,600).large);
    profile.layout_family=LayoutFamily::LARGE_SQUARE;
    assert(!flow_dashboard_grid(profile,5,rows,1280,800).large);
  }
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
        const auto head=g.heads[i][0], tip=g.heads[i][1];
        assert(std::abs(std::hypot(head.x-tip.x,head.y-tip.y)/g.scale - std::hypot(11,5.5))<1e-7);
        for (auto p:g.paths[i]) assert(p.x>=0 && p.x<=size[0] && p.y>=0 && p.y<=size[1]);
      }
      assert(g.heads[6][0].y < g.heads[6][1].y); // arrow into Battery points down
      assert(g.heads[4][0].x > g.heads[4][1].x); // arrow into Grid points left
      assert(std::abs((g.nodes[0].y+g.nodes[3].y)/2-size[1]/2.0)<1e-8);
      assert(std::abs((g.nodes[1].x+g.nodes[2].x)/2-size[0]/2.0)<1e-8);
    }
  }
  // Continuous circular elbows with exact axis-aligned tangent transitions.
  const auto smooth=flow_geometry(1200,650,FlowFamily::LANDSCAPE,220,140);
  for(size_t i:{0,2,3,4}){
    const auto &p=smooth.paths[i];const auto &a=smooth.arcs[i];
    assert(a.radius>0&&a.radius<=32&&a.end_angle-a.start_angle==90);
    assert(p[0].x==p[1].x&&p[2].y==p[3].y);
    assert(std::abs(std::hypot(p[1].x-a.center.x,p[1].y-a.center.y)-a.radius)<1e-8);
    assert(std::abs(std::hypot(p[2].x-a.center.x,p[2].y-a.center.y)-a.radius)<1e-8);
    assert(p[1].y==a.center.y&&p[2].x==a.center.x);
  }
  assert(same(smooth.arcs[4].center,smooth.arcs[6].center));
  assert(smooth.arcs[4].radius==smooth.arcs[6].radius);
  assert(smooth.arcs[4].start_angle==smooth.arcs[6].start_angle);
  assert(smooth.arcs[4].end_angle==smooth.arcs[6].end_angle);
  // Outward placement uses all actual content bounds, retaining the topology.
  const auto g=flow_geometry(520,430,FlowFamily::SQUARE);
  assert(g.nodes[0].y<83 && g.nodes[2].x<102);
  const auto wide=flow_geometry(1200,650,FlowFamily::LANDSCAPE,220,140);
  assert(wide.nodes[2].x<130 && wide.nodes[1].x>1070);
  assert(wide.nodes[0].y<90 && wide.nodes[3].y>560);
  assert(same(wide.paths[6].front(),wide.paths[4].back()));
  // Real content/font envelopes: every path stays outside node text bounds.
  for (auto box : {std::array<int,4>{264,300,110,80}, {440,350,140,101},
                    {680,550,210,145}, {1200,650,240,150}}) {
    const auto actual=flow_geometry(box[0],box[1],flow_family(box[0],box[1]),box[2],box[3]);
    for (auto &path:actual.paths) for(auto p:path) for(auto n:actual.nodes)
      assert(!(p.x>n.x-box[2]/2.0 && p.x<n.x+box[2]/2.0 &&
               p.y>n.y-box[3]/2.0 && p.y<n.y+box[3]/2.0));
  }
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
