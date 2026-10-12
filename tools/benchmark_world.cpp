// Developer benchmark: CPU generation time of complete streamed world windows (PERF-002).
// Usage: trainsim_world_benchmark [quality] [first-km] [last-km] [step-km]
#include "trainsim/render/WorldGeometry.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc,char** argv)
{
    using namespace TrainSim;
    const int quality=argc>1 ? std::atoi(argv[1]) : 2;
    const double first=argc>2 ? std::atof(argv[2]) : 50.0;
    const double last=argc>3 ? std::atof(argv[3]) : 60.0;
    const double step=argc>4 ? std::atof(argv[4]) : 0.25;
    const auto route=Sim::Route::NorthernValleyExpress();
    Render::WorldGeometry geometry;
    std::vector<double> samples;
    double largestMb=0,largestWorldMb=0;
    for (double km=first;km<=last+1e-9;km+=step)
    {
        World::StreamingWorld world(route);
        world.update(km*1000.0);
        const double anchor=(static_cast<double>(world.currentChunk())+0.5)*world.chunkLength();
        // Thread CPU time, so other processes on a shared host do not distort comparisons.
        timespec before{},after{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID,&before);
        geometry.build(route,world,anchor,quality,750.0f);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID,&after);
        samples.push_back(static_cast<double>(after.tv_sec-before.tv_sec)*1000.0
                          +static_cast<double>(after.tv_nsec-before.tv_nsec)/1.0e6);
        largestMb=std::max(largestMb,static_cast<double>(geometry.counters.largestMaterialBytes_)/1048576.0);
        largestWorldMb=std::max(largestWorldMb,static_cast<double>(geometry.counters.worldGpuBytes_)/1048576.0);
    }
    std::sort(samples.begin(),samples.end());
    double total=0;
    for (double sample : samples) total+=sample;
    std::cout<<std::fixed<<std::setprecision(1)
             <<"windows="<<samples.size()<<" cpu_mean_ms="<<total/static_cast<double>(samples.size())
             <<" p50_ms="<<samples[samples.size()/2]<<" max_ms="<<samples.back()
             <<" largest_material_mb="<<largestMb<<" largest_window_mb="<<largestWorldMb<<'\n';
}
