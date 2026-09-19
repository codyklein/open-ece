#include <chrono>
#include <iostream>
#include <openece/communications/ber.hpp>
using namespace openece::communications;
int main() {
    for (auto modulation : {Modulation::bpsk, Modulation::qpsk})
        for (auto point_count : {std::size_t{1}, limits::ber_points}) {
            auto count = point_count == 1 ? limits::bits_per_ber_point
                                          : limits::aggregate_ber_bits / point_count;
            // Current limits yield even counts for both modulations; do not silently round.
            BerRequest request{modulation, std::vector<double>(point_count, 4), count, 41, 73};
            BerExperiment run(request);
            const auto begin = std::chrono::steady_clock::now();
            run.run();
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
            std::uint64_t errors = 0, bits = 0;
            for (const auto& result : run.results()) {
                errors += result.bit_errors;
                bits += result.bits_tested;
            }
            if (!run.complete() || bits != count * point_count)
                return 1;
            std::cout << (modulation == Modulation::bpsk ? "BPSK" : "QPSK")
                      << " points=" << point_count << " bits=" << bits << " errors=" << errors
                      << " seconds=" << seconds << '\n';
        }
}
