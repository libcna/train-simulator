#include "TestHarness.hpp"

int main()
{
    int failures = 0;
    for (const auto& test : TrainSim::Tests::Cases())
    {
        try
        {
            test.run();
            std::cout << "PASS  " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAIL  " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << (TrainSim::Tests::Cases().size() - static_cast<std::size_t>(failures))
              << "/" << TrainSim::Tests::Cases().size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
