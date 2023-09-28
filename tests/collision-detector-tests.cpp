#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/collision_detector.h"

#include <sstream>

using namespace std::literals;

namespace Catch {
    template<>
    struct StringMaker<collision_detector::GatheringEvent> {
        static std::string convert(collision_detector::GatheringEvent const& value) {
            std::ostringstream tmp;
            tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

            return tmp.str();
        }
    };
} // namespace Catch

template<typename Range>
struct GatheringContainerMatcher : Catch::Matchers::MatcherGenericBase {
    GatheringContainerMatcher(Range range)
            : range_{std::move(range)} 
    {
    }

    GatheringContainerMatcher(GatheringContainerMatcher&&) = default;

    template <typename OtherRange>
    bool match(OtherRange other) const {
        if(range_.size() != other.size()) 
            return false;
            
        for(size_t i = 0; i < range_.size(); ++i) {
            if(range_[i] != other[i])
                return false;
        }

        return true;
    }

    std::string describe() const override {
        return "Is equal of: "s + Catch::rangeToString(range_);
    }

private:
    Range range_;
};

template<typename Range>
GatheringContainerMatcher<Range> IsEqualEvents(Range&& range) {
    return GatheringContainerMatcher{std::forward<Range>(range)};
}

class TestItemGathererPrivider : public collision_detector::ItemGathererProvider {
    using Item = collision_detector::Item;
    using Gatherer = collision_detector::Gatherer; 
public: 
    TestItemGathererPrivider(std::vector<Item> items, std::vector<Gatherer> gatherers) 
            : items_(items) 
            , gatherers_(gatherers)
        {
        }
    
    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        if(idx > items_.size() - 1) 
            throw std::out_of_range("GetItem with idx out of range");
        return items_[idx];    
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        if(idx > gatherers_.size() - 1) 
            throw std::out_of_range("GatGatherer with idx out of range");
        return gatherers_[idx];
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

SCENARIO("Gathering events") {
    
    GIVEN("gatherer, 2 items in path and 1 item not in path") {
        collision_detector::Gatherer gatherer{{0., 0.}, {0., 1.5}, 0.4};
        collision_detector::Item item_in_path_1{{0.19, 1.5}, 0.0};
        collision_detector::Item item_in_path_2{{0.19, 1}, 0.0};
        collision_detector::Item item_not_in_path{{0.41, 1.5}, 0.0};

        WHEN("1 item on gatherer path") {
            
            TestItemGathererPrivider provider{{item_in_path_1}, {gatherer}};
            THEN("gatherer find item") {
                std::vector<collision_detector::GatheringEvent> events = collision_detector::FindGatherEvents(provider);
                CHECK(events.size() == 1);
            }
        }
        WHEN("1 item not on gatherer path") {
            
            TestItemGathererPrivider provider{{item_not_in_path},{gatherer}};
            std::vector<collision_detector::GatheringEvent> events = collision_detector::FindGatherEvents(provider);

            THEN("gatherer don't find item") {
                CHECK(events.size() == 0);
            }
        }
        WHEN("many items gather path") {
            TestItemGathererPrivider provider{{item_in_path_1, item_in_path_2}, {gatherer}};
            std::vector<collision_detector::GatheringEvent> events = collision_detector::FindGatherEvents(provider);

            THEN("they say in order by cross time") {
                REQUIRE(events.size() == 2);
                CHECK(events[0].time < events[1].time);
            }
            THEN("events with correct data") {
                using Catch::Matchers::WithinRel;
                CHECK_THAT(events[0].time, WithinRel(2./3, 1e-5));
                CHECK_THAT(events[1].time, WithinRel(1., 1e-5));
                CHECK(events[0].item_id == 1);
                CHECK(events[1].item_id == 0);
                CHECK(events[0].gatherer_id == 0);
                CHECK(events[0].gatherer_id == 0);
                CHECK_THAT(events[0].sq_distance, WithinRel(0.19*0.19, 1e-5));
                CHECK_THAT(events[1].sq_distance, WithinRel(0.19*0.19, 1e-5));          
            }
        }

    }
}
