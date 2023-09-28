#include "loot_generator.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace loot_gen {

unsigned GetRandomItem(unsigned max_item_number) {
    std::random_device device;
    std::mt19937 range(device());
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, max_item_number);
    return distribution(range);
}

unsigned LootGenerator::Generate(TimeInterval time_delta, unsigned loot_count,
                                 unsigned looter_count) {
    time_without_loot_ += time_delta;
    const unsigned loot_shortage = loot_count > looter_count ? 0u : looter_count - loot_count;
    const double ratio = std::chrono::duration<double>{time_without_loot_} / base_interval_;
    const double probability
        = std::clamp((1.0 - std::pow(1.0 - probability_, ratio)) * random_generator_(), 0.0, 1.0);
    const unsigned generated_loot = static_cast<unsigned>(std::round(loot_shortage * probability));
    if (generated_loot > 0) {
        time_without_loot_ = {};
    }
    return generated_loot;
}

} // namespace loot_gen
