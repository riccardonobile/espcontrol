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
    assert(grid.large&&grid.diagram.col==2&&grid.diagram.cols==3&&grid.diagram.rows==rows);
    assert(grid.back.col==0);for(auto cell:grid.values)assert(cell.col+cell.cols<=2);
    assert(grid.values[3].cols==2);
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
  // Compiled S3 Roboto-18 envelope for a long Dutch SOC/unavailable row:
  // Battery needs 198 px, while Grid/Home need 142 px. Their own widths preserve
  // positive tangent runs rather than letting the Battery caption pinch elbows.
  const auto compact_long=flow_geometry(436,302,FlowFamily::SQUARE,198,69,142);
  for(size_t i:{0,2,3,4}){
    const auto &p=compact_long.paths[i];
    assert(compact_long.arcs[i].radius>0);
    assert(i==0||i==3 ? p[3].x>p[2].x : p[3].x<p[2].x);
  }
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
  // Approved renderer/radius stays fixed. Only endpoints/port spacing change;
  // portrait explicitly opts out, including node centers and scale.
  for(auto size:{std::array<int,2>{600,500},{660,620}}){
    const auto old=flow_geometry(size[0],size[1],FlowFamily::SQUARE,210,90,170);
    const auto refined=flow_geometry(size[0],size[1],FlowFamily::SQUARE,210,90,170,true);
    for(size_t n=0;n<4;++n)assert(same(old.nodes[n],refined.nodes[n]));
    assert(refined.ports[0][2].x-refined.ports[0][1].x==old.ports[0][2].x-old.ports[0][1].x+3);
    assert(refined.ports[3][1].y==refined.nodes[3].y-45-8);
    assert(refined.ports[0][1].y==refined.nodes[0].y+45+8);
    for(size_t i:{0,2,3,4})assert(refined.arcs[i].radius==old.arcs[i].radius&&refined.arcs[i].radius==32);
    for(size_t j=0;j<FLOW_POINT_COUNT;++j)assert(same(refined.paths[4][j],refined.paths[6][FLOW_POINT_COUNT-1-j]));
    for(size_t n=0;n<4;++n)for(auto port:refined.ports[n]){
      const auto node=refined.nodes[n];
      const double gap=n==0?port.y-node.y-45:n==3?node.y-45-port.y:n==1?node.x-85-port.x:port.x-node.x-85;
      assert(gap==8);
    }
  }
  const auto portrait=flow_geometry(436,584,FlowFamily::PORTRAIT,198,69,142,false);
  const auto portrait_default=flow_geometry(436,584,FlowFamily::PORTRAIT,198,69,142);
  for(size_t n=0;n<4;++n){assert(same(portrait.nodes[n],portrait_default.nodes[n]));for(size_t k=0;k<3;++k)assert(same(portrait.ports[n][k],portrait_default.ports[n][k]));}
  // Production content regions/native compiled-font envelopes. The same
  // geometry owns all four squares, the radial distance and fixed ports.
  for(auto profile:{std::array<int,8>{722,713,176,75,114,698,689,1}, {572,521,158,69,103,560,509,1},
                    {710,655,238,102,151,656,433,0}, {722,713,260,75,190,698,689,1}, {572,521,187,69,138,560,509,1}}){
    const int w=profile[0],h=profile[1];
    const auto family=profile[7]?FlowFamily::LANDSCAPE:FlowFamily::SQUARE;
    const double scale=flow_render_scale(profile[5],profile[6],family);
    const auto square=flow_geometry(w,h,family,profile[2],profile[3],profile[4],true,true,scale);
    const double e=square.square_size,half=e/2,d=square.radial_distance;
    assert(e>0&&e>=profile[3]);
    assert(d==(std::min(w,h)-e)/2);
    const auto legacy=flow_geometry(w,h,FlowFamily::SQUARE,profile[2],profile[3],profile[4],true);
    assert(square.port_spacing==legacy.port_spacing);
    const double old_spacing=std::min(21.0,std::min({profile[2]/2.0,profile[3]/2.0,profile[4]/2.0})/3+3);
    assert(square.port_spacing==old_spacing+2);
    for(auto node:square.nodes){
      assert(std::abs(std::hypot(node.x-w/2.0,node.y-h/2.0)-d)<1e-8);
      assert(node.x-half>=0&&node.x+half<=w&&node.y-half>=0&&node.y+half<=h);
    }
    for(size_t n=0;n<4;++n)for(size_t port=0;port<3;++port){
      const auto p=square.ports[n][port],node=square.nodes[n];
      const double gap=n==0?p.y-node.y-half:n==3?node.y-half-p.y:n==1?node.x-half-p.x:p.x-node.x-half;
      assert(std::abs(gap-8)<1e-8);
      const double offset=n==0||n==3?p.x-node.x:p.y-node.y;
      assert(std::abs(offset-(static_cast<int>(port)-1)*square.port_spacing)<1e-8);
    }
    // Test painted straights, complete circular elbows and arrowhead strokes
    // against EVERY envelope, not merely endpoint centreline distances.
    const int stroke=(std::max<int>(2,std::lround(3.2*square.scale))+1)&~1;
    // Neighbouring incoming arrowheads at Home/Battery cannot overlap even
    // with the full painted stroke/cap footprint included.
    assert(square.port_spacing>11*square.scale+stroke);
    auto clearance=[&](Point p){
      for(auto node:square.nodes){
        const double dx=std::max(std::abs(p.x-node.x)-half,0.0);
        const double dy=std::max(std::abs(p.y-node.y)-half,0.0);
        assert(std::hypot(dx,dy)-stroke/2.0>=5.99);
      }
    };
    auto segment=[&](Point a,Point b){for(int i=0;i<=100;++i)clearance({a.x+(b.x-a.x)*i/100,a.y+(b.y-a.y)*i/100});};
    for(size_t i=0;i<7;++i){
      const auto &path=square.paths[i];const auto &arc=square.arcs[i];
      segment(path[0],arc.radius?path[1]:path[3]);
      if(arc.radius){
        assert(arc.radius==32);segment(path[2],path[3]);
        for(int a=arc.start_angle;a<=arc.end_angle;++a){const double angle=a*std::acos(-1)/180;
          clearance({arc.center.x+arc.radius*std::cos(angle),arc.center.y+arc.radius*std::sin(angle)});}
      }
      segment(square.heads[i][0],square.heads[i][1]);segment(square.heads[i][1],square.heads[i][2]);
      for(size_t j=0;j<FLOW_POINT_COUNT;++j)assert(same(square.paths[4][j],square.paths[6][FLOW_POINT_COUNT-1-j]));
    }
  }
  // Portrait's measured 434x583 placement region cannot retain its approved
  // node centres with equal squares: Grid's old left edge is 8.68 px, while
  // a 158 px square at that centre would extend to -18.82 px. Keep that
  // profile's rectangular boxes and centres; only symmetric port spacing grows.
  const auto approved_portrait=flow_geometry(434,583,FlowFamily::PORTRAIT,158,69,103);
  assert(approved_portrait.square_size==0);
  assert(same(approved_portrait.nodes[0],{217,43.18}));
  assert(same(approved_portrait.nodes[2],{60.18,291.5}));
  assert(approved_portrait.port_spacing==13.5);
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
