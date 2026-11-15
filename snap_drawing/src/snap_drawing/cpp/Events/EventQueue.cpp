//
//  EventQueue.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/7/20.
//

#include "snap_drawing/cpp/Events/EventQueue.hpp"
#include <algorithm>

namespace snap::drawing {

EventQueue::EventQueue(TimePoint initialTime) : _lastTime(initialTime) {}

void EventQueue::flush(TimePoint currentTime) {
    // Platform frame times can step backwards: Android's Choreographer subtracts a frame interval
    // from callback times during buffer stuffing recovery without re-checking monotonicity.
    // Treat a backwards step as no elapsed time so animations never run in reverse.
    if (currentTime < _lastTime) {
        currentTime = _lastTime;
    }
    auto delta = currentTime - _lastTime;
    _lastTime = currentTime;

    collectNextEvents(currentTime);

    for (const auto& event : _nextEvents) {
        if (event.callback) {
            event.callback(currentTime, delta);
        }
    }

    _nextEvents.clear();
}

EventId EventQueue::enqueue(Duration delay, EventCallback&& callback) {
    return enqueue(_lastTime + delay, std::move(callback));
}

EventId EventQueue::enqueue(TimePoint time, EventCallback&& callback) {
    auto sequence = ++_sequence;
    Event event(EventId(), time, std::move(callback));

    auto it =
        std::upper_bound(_pendingEvents.begin(), _pendingEvents.end(), event, [=](const Event& a, const Event& b) {
            return a.time < b.time;
        });
    auto index = it - _pendingEvents.begin();
    auto enqueuedEvent = _pendingEvents.emplace(it, std::move(event));

    auto eventId = EventId(static_cast<uint32_t>(index), sequence);
    enqueuedEvent->id = eventId;

    return eventId;
}

bool EventQueue::cancel(EventId eventId) {
    return cancelFromPendingEvents(eventId) || cancelFromProcessingEvents(eventId);
}

bool EventQueue::cancelFromPendingEvents(EventId eventId) {
    auto index = static_cast<size_t>(eventId.index);
    if (index >= _pendingEvents.size()) {
        return false;
    }

    auto& event = _pendingEvents[index];
    if (event.id != eventId) {
        return false;
    }

    return event.cancel();
}

bool EventQueue::cancelFromProcessingEvents(EventId eventId) {
    for (auto& nextEvent : _nextEvents) {
        if (nextEvent.id == eventId) {
            return nextEvent.cancel();
        }
    }

    return false;
}

void EventQueue::collectNextEvents(TimePoint currentTime) {
    while (!_pendingEvents.empty() && currentTime >= _pendingEvents.front().time) {
        _nextEvents.emplace_back(std::move(_pendingEvents.front()));
        _pendingEvents.pop_front();
    }
}

void EventQueue::clear() {
    _nextEvents.clear();
    _pendingEvents.clear();
}

bool EventQueue::isEmpty() const {
    return _pendingEvents.empty();
}

} // namespace snap::drawing
