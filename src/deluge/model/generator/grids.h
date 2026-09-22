#pragma once
#include <array>
#include <cstdint>
namespace deluge::model::generator::grids {
constexpr uint8_t kParts = 3;
constexpr uint8_t kSteps = 32;
struct Settings {
	uint16_t seed = 1;
	uint8_t x = 128;
	uint8_t y = 128;
	std::array<uint8_t, kParts> density{128, 128, 128};
	uint8_t chaos = 0;
	bool operator==(const Settings&) const = default;
};
struct Hit {
	bool on = false;
	bool accent = false;
};
using Hits = std::array<Hit, kParts>;
// Exact upstream integer interpolation, including the 255-weight / 256 divisor.
uint8_t level(uint8_t step, uint8_t part, uint8_t x, uint8_t y);
class Core {
public:
	void reset(uint16_t seed);
	void beginCycle();
	Hits evaluate(uint8_t step, const Settings& settings) const;

private:
	uint32_t random_ = 1;
	std::array<uint8_t, kParts> perturbation_{};
};
} // namespace deluge::model::generator::grids
