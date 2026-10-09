#include "TestHarness.hpp"
#include "trainsim/app/FrameTiming.hpp"
#include <limits>

TEST_CASE("frame timings use nearest-rank percentiles and ignore invalid samples")
{
    TrainSim::App::FrameTiming timings;
    REQUIRE(timings.summary().windowSamples == 0);
    REQUIRE(timings.summary().p95Ms == 0);
    timings.record(-1);
    timings.record(std::numeric_limits<double>::infinity());
    timings.record(std::numeric_limits<double>::quiet_NaN());
    for (int i=100;i>=1;--i) timings.record(i);
    const auto result = timings.summary();
    REQUIRE(result.totalSamples == 100);
    REQUIRE(result.meanMs == 50.5);
    REQUIRE(result.p50Ms == 50);
    REQUIRE(result.p95Ms == 95);
    REQUIRE(result.p99Ms == 99);
    REQUIRE(result.windowMaxMs == 100);
}

TEST_CASE("frame timing window evicts old samples but retains lifetime maximum")
{
    TrainSim::App::FrameTiming timings;
    timings.record(5000);
    for (std::size_t i=0;i<TrainSim::App::FrameTiming::Capacity;++i) timings.record(16);
    const auto result = timings.summary();
    REQUIRE(result.windowSamples == TrainSim::App::FrameTiming::Capacity);
    REQUIRE(result.totalSamples == TrainSim::App::FrameTiming::Capacity+1);
    REQUIRE(result.meanMs == 16);
    REQUIRE(result.p99Ms == 16);
    REQUIRE(result.windowMaxMs == 16);
    REQUIRE(result.allMaxMs == 5000);
}
