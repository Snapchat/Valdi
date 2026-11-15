#include <gtest/gtest.h>

#include "snap_drawing/cpp/Events/EventQueue.hpp"

#include <vector>

using namespace Valdi;

namespace snap::drawing {

TEST(EventQueueTests, backwardsTimeIsTreatedAsNoElapsedTime) {
    EventQueue queue(TimePoint::fromSeconds(0.0));
    std::vector<double> deltas;
    std::vector<double> times;

    auto record = [&](TimePoint time, Duration delta) {
        times.emplace_back(time.getTime());
        deltas.emplace_back(delta.seconds());
    };

    queue.enqueue(Duration(0), record);
    queue.flush(TimePoint::fromSeconds(1.0));

    queue.enqueue(Duration(0), record);
    queue.flush(TimePoint::fromSeconds(0.5));

    queue.enqueue(Duration(0), record);
    queue.flush(TimePoint::fromSeconds(1.25));

    ASSERT_EQ(std::vector<double>({1.0, 1.0, 1.25}), times);
    ASSERT_EQ(std::vector<double>({1.0, 0.0, 0.25}), deltas);
}

TEST(EventQueueTests, backwardsTimeDoesNotFireFutureEvents) {
    EventQueue queue(TimePoint::fromSeconds(0.0));
    queue.flush(TimePoint::fromSeconds(1.0));

    bool fired = false;
    queue.enqueue(Duration(0.5), [&](TimePoint /*time*/, Duration /*delta*/) { fired = true; });

    queue.flush(TimePoint::fromSeconds(0.9));
    ASSERT_FALSE(fired);

    queue.flush(TimePoint::fromSeconds(1.5));
    ASSERT_TRUE(fired);
}

} // namespace snap::drawing
