// Developer benchmark: CPU generation time of complete streamed world windows (PERF-002).
// Usage: trainsim_world_benchmark [quality] [first-km] [last-km] [step-km] [threads]
#include "trainsim/render/WorldGeometry.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include <algorithm>
#include <chrono>
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
    const auto threads=argc>5 ? static_cast<std::size_t>(std::atoi(argv[5]))
                              : Render::WorldGeometry::defaultWorkerThreads();
    Render::WorldGeometry geometry(threads);
    std::vector<double> samples,wall;
    double largestMb=0,largestWorldMb=0;
    for (double km=first;km<=last+1e-9;km+=step)
    {
        World::StreamingWorld world(route);
        world.update(km*1000.0);
        const double anchor=(static_cast<double>(world.currentChunk())+0.5)*world.chunkLength();
        // Process CPU time (all generation threads) and wall time of the same build: other
        // processes on a shared host distort wall time, while CPU time shows the work done.
        timespec before{},after{};
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&before);
        const auto wallBefore=std::chrono::steady_clock::now();
        geometry.build(route,world,anchor,quality,750.0f);
        wall.push_back(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-wallBefore).count());
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&after);
        samples.push_back(static_cast<double>(after.tv_sec-before.tv_sec)*1000.0
                          +static_cast<double>(after.tv_nsec-before.tv_nsec)/1.0e6);
        largestMb=std::max(largestMb,static_cast<double>(geometry.counters.largestMaterialBytes_)/1048576.0);
        largestWorldMb=std::max(largestWorldMb,static_cast<double>(geometry.counters.worldGpuBytes_)/1048576.0);
    }
    std::sort(samples.begin(),samples.end());
    std::sort(wall.begin(),wall.end());
    double total=0,wallTotal=0;
    for (double sample : samples) total+=sample;
    for (double sample : wall) wallTotal+=sample;
    std::cout<<std::fixed<<std::setprecision(1)
             <<"threads="<<threads<<" windows="<<samples.size()
             <<" wall_mean_ms="<<wallTotal/static_cast<double>(wall.size())
             <<" wall_max_ms="<<wall.back()
             <<" cpu_mean_ms="<<total/static_cast<double>(samples.size())
             <<" largest_material_mb="<<largestMb<<" largest_window_mb="<<largestWorldMb<<'\n';
}
