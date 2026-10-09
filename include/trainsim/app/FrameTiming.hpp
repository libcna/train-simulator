#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace TrainSim::App {
struct FrameTimingSummary
{
    std::size_t windowSamples = 0;
    std::uint64_t totalSamples = 0;
    double meanMs = 0, p50Ms = 0, p95Ms = 0, p99Ms = 0, windowMaxMs = 0, allMaxMs = 0;
};

// Bounded draw-to-draw wall-time history, independent of simulated time and GPU APIs.
// Recording allocates nothing; sorting happens only when explicitly requesting a report.
class FrameTiming
{
public:
    static constexpr std::size_t Capacity = 2048;
    void record(double milliseconds) noexcept
    {
        if (!std::isfinite(milliseconds) || milliseconds < 0) return;
        samples_[cursor_] = milliseconds;
        cursor_ = (cursor_+1)%Capacity;
        count_ = std::min(count_+1,Capacity);
        ++total_;
        allMax_ = std::max(allMax_,milliseconds);
    }
    [[nodiscard]] FrameTimingSummary summary() const
    {
        FrameTimingSummary result;
        result.windowSamples = count_;
        result.totalSamples = total_;
        result.allMaxMs = allMax_;
        if (!count_) return result;
        auto sorted = samples_;
        std::sort(sorted.begin(),sorted.begin()+static_cast<std::ptrdiff_t>(count_));
        const auto percentile = [&](double fraction) {
            const auto rank = static_cast<std::size_t>(std::ceil(fraction*static_cast<double>(count_)));
            return sorted[rank-1];
        };
        result.meanMs = std::accumulate(sorted.begin(),
            sorted.begin()+static_cast<std::ptrdiff_t>(count_),0.0)/static_cast<double>(count_);
        result.p50Ms = percentile(0.50);
        result.p95Ms = percentile(0.95);
        result.p99Ms = percentile(0.99);
        result.windowMaxMs = sorted[count_-1];
        return result;
    }
private:
    std::array<double,Capacity> samples_{};
    std::size_t cursor_ = 0, count_ = 0;
    std::uint64_t total_ = 0;
    double allMax_ = 0;
};
} // namespace TrainSim::App
