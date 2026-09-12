// Own bounded CPU ray queries against observed FFP triangles. Not game collision.
// Queries are two-sided and ignore alpha; a hit is not proof of RTX occlusion.
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <numeric>
#include <vector>
struct V {float x[3]{};float& operator[](int i){return x[i];}float operator[](int i)const{return x[i];}};
static V sub(V a,V b){return {{a[0]-b[0],a[1]-b[1],a[2]-b[2]}};}
static V cross(V a,V b){return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
static float dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct Tri {V p[3];uint64_t hash;uint32_t draw;};
struct Node {V lo,hi;int begin,end,left=-1,right=-1;};
static std::vector<Tri> triangles;
static std::vector<unsigned> order;
static std::vector<Node> nodes;
static int Build(int begin,int end) {
  Node n;n.begin=begin;n.end=end;for(int k=0;k<3;++k){n.lo[k]=FLT_MAX;n.hi[k]=-FLT_MAX;}
  for(int i=begin;i<end;++i)for(auto p:triangles[order[i]].p)for(int k=0;k<3;++k){n.lo[k]=(std::min)(n.lo[k],p[k]);n.hi[k]=(std::max)(n.hi[k],p[k]);}
  const int id=int(nodes.size());nodes.push_back(n);
  if(end-begin>8){int axis=0;for(int k=1;k<3;++k)if(n.hi[k]-n.lo[k]>n.hi[axis]-n.lo[axis])axis=k;
    const int mid=(begin+end)/2;std::nth_element(order.begin()+begin,order.begin()+mid,order.begin()+end,[axis](unsigned a,unsigned b){
      const auto& x=triangles[a];const auto& y=triangles[b];return x.p[0][axis]+x.p[1][axis]+x.p[2][axis]<y.p[0][axis]+y.p[1][axis]+y.p[2][axis];});
    const int left=Build(begin,mid),right=Build(mid,end);nodes[id].left=left;nodes[id].right=right;
  }return id;
}
static bool Box(const Node& n,V origin,V direction,float distance) {
  float near=0,far=distance;
  for(int k=0;k<3;++k){if(std::abs(direction[k])<1e-12f){if(origin[k]<n.lo[k]||origin[k]>n.hi[k])return false;continue;}
    float a=(n.lo[k]-origin[k])/direction[k],b=(n.hi[k]-origin[k])/direction[k];if(a>b)std::swap(a,b);
    near=(std::max)(near,a);far=(std::min)(far,b);if(near>far)return false;}return true;
}
static float Triangle(const Tri& t,V origin,V direction) {
  const auto e1=sub(t.p[1],t.p[0]),e2=sub(t.p[2],t.p[0]),p=cross(direction,e2);const float det=dot(e1,p);
  if(std::abs(det)<1e-8f)return FLT_MAX;const auto s=sub(origin,t.p[0]);const float u=dot(s,p)/det;
  if(u<0||u>1)return FLT_MAX;const auto q=cross(s,e1);const float v=dot(direction,q)/det;
  if(v<0||u+v>1)return FLT_MAX;const float distance=dot(e2,q)/det;return distance>.01f?distance:FLT_MAX;
}
struct Hit {float distance=25000;int triangle=-1;};
static void Ray(int node,V origin,V direction,Hit& hit) {
  const auto& n=nodes[node];if(!Box(n,origin,direction,hit.distance))return;
  if(n.left>=0){Ray(n.left,origin,direction,hit);Ray(n.right,origin,direction,hit);return;}
  for(int i=n.begin;i<n.end;++i){const auto index=order[i];const float distance=Triangle(triangles[index],origin,direction);
    if(distance<hit.distance){hit.distance=distance;hit.triangle=int(index);}}
}
static void Fail(const char* message){fprintf(stderr,"%s\n",message);std::exit(1);}
template<class T> static T Read(std::ifstream& stream){T value{};if(!stream.read(reinterpret_cast<char*>(&value),sizeof(value)))Fail("truncated geometry");return value;}
int main(int argc,char** argv){
  if(argc!=3)Fail("usage: analyze_light_geometry geometry.bin origins.txt");
  // Independent known ray cases qualify distance, sidedness and miss behavior.
  Tri test{{{{-1,-1,2}},{{1,-1,2}},{{0,1,2}}},0,0};
  if(Triangle(test,{{0,0,0}},{{0,0,1}})!=2||Triangle(test,{{0,0,3}},{{0,0,-1}})!=1||Triangle(test,{{3,0,0}},{{0,0,1}})!=FLT_MAX)Fail("ray self check");
  std::ifstream input(argv[1],std::ios::binary);if(Read<uint32_t>(input)!=0x57475031)Fail("geometry magic");const auto frame=Read<uint32_t>(input);
  unsigned records=0;
  for(;;){const auto kind=Read<uint32_t>(input);if(!kind){const auto draws=Read<uint32_t>(input),count=Read<uint32_t>(input),limited=Read<uint32_t>(input);
      if(draws!=records||count!=triangles.size()||limited||input.peek()!=EOF)Fail("incomplete geometry footer");break;}
    if(kind!=1||++records>4096)Fail("draw bound");const auto draw=Read<uint32_t>(input),count=Read<uint32_t>(input);Read<uint32_t>(input);
    const auto hash=Read<uint64_t>(input);const auto world=Read<std::array<float,16>>(input);
    if(count>32768||triangles.size()+count>1800000)Fail("triangle bound");
    for(unsigned i=0;i<count;++i){Tri t{};t.hash=hash;t.draw=draw;
      for(auto& p:t.p){const auto v=Read<V>(input);for(int k=0;k<3;++k){p[k]=v[0]*world[k]+v[1]*world[4+k]+v[2]*world[8+k]+world[12+k];if(!std::isfinite(p[k]))Fail("nonfinite position");}}
      triangles.push_back(t);}}
  if(triangles.empty())Fail("no observed triangles");order.resize(triangles.size());std::iota(order.begin(),order.end(),0);Build(0,int(order.size()));
  std::ifstream points(argv[2]);if(!points)Fail("origins unavailable");unsigned id=0;V origin{};
  printf("{\"frame\":%u,\"draws\":%u,\"triangles\":%zu,\"bvhNodes\":%zu,\"origins\":[",frame,records,triangles.size(),nodes.size());
  bool first=true;unsigned pointCount=0;
  while(points>>id){if(!(points>>origin[0]>>origin[1]>>origin[2]))Fail("invalid origin");
    for(float x:origin.x)if(!std::isfinite(x))Fail("nonfinite origin");
    if(++pointCount>64)Fail("origin bound");if(!first)printf(",");first=false;
    printf("{\"id\":%u,\"position\":[%.9g,%.9g,%.9g],\"rays\":[",id,origin[0],origin[1],origin[2]);
    for(int ray=0;ray<6;++ray){V direction{};direction[ray/2]=ray%2?-1.f:1.f;Hit hit;Ray(0,origin,direction,hit);if(ray)printf(",");
      printf("{\"axis\":%d,\"sign\":%.0f,\"distance\":%.9g",ray/2,direction[ray/2],hit.distance);
      if(hit.triangle>=0){const auto& tri=triangles[hit.triangle];printf(",\"draw\":%u,\"hash\":\"%016llX\",\"triangle\":[",tri.draw,tri.hash);
        for(int v=0;v<3;++v)printf("%s[%.9g,%.9g,%.9g]",v?",":"",tri.p[v][0],tri.p[v][1],tri.p[v][2]);printf("]");}printf("}");}printf("]}");
  }if(!points.eof()||!pointCount)Fail("invalid origins");printf("]}\n");return 0;
}
