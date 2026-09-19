#pragma once
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
namespace openece::communications::detail {
inline constexpr std::uint64_t link_bits = 0x4c494e4b42495453ULL;
inline constexpr std::uint64_t link_noise = 0x4c494e4b4e4f4953ULL;
inline constexpr std::uint64_t ber_bits = 0x4245525f42495453ULL;
inline constexpr std::uint64_t ber_noise = 0x4245525f4e4f4953ULL;
constexpr std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
constexpr std::uint64_t derive_seed(std::uint64_t base, std::uint64_t tag, std::uint64_t point) {
    return mix(base ^ tag ^ mix(point));
}
class Random {
  public:
    Random(std::uint64_t base, std::uint64_t tag, std::uint64_t point = 0)
        : engine_(derive_seed(base, tag, point)) {}
    std::uint64_t word() { return engine_(); }
    std::uint8_t bit() { return static_cast<std::uint8_t>(word() >> 63); }
    static double open_uniform(std::uint64_t word) {
        return (static_cast<double>(word >> 12) + 0.5) * 0x1p-52;
    }
    double gaussian() {
        if (cached_) {
            cached_ = false;
            return spare_;
        }
        const double u1 = open_uniform(word()), u2 = open_uniform(word());
        const double radius = std::sqrt(-2 * std::log(u1));
        const double angle = 2 * std::numbers::pi * u2;
        spare_ = radius * std::sin(angle);
        cached_ = true;
        return radius * std::cos(angle);
    }

  private:
    std::mt19937_64 engine_;
    bool cached_ = false;
    double spare_ = 0;
};
} // namespace openece::communications::detail
