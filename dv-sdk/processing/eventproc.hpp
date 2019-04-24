#ifndef DV_PROCESSING_EVENTPROC_HPP
#define DV_PROCESSING_EVENTPROC_HPP

#include "core.hpp"
#include <opencv2/opencv.hpp>

namespace dv {

    /**
     * Function that creates perfect hash for 2d coordinates.
     * @param x x coordinate
     * @param y y coordinate
     * @return a 64 bit hash that uniquely identifies the coordinates
     */
    inline long coordinateHash(const coord_t x, const coord_t y) {
        return ((long)x << 32) | y;
    }

    /**
     * Extracts only the events that are within the defined region of interest.
     * This function copies the events from the in EventStore into the given
     * out EventStore, if they intersect with the given region of interest rectangle.
     * @param in The EventStore to operate on. Won't be modified.
     * @param out The EventStore to put the ROI events into. Will get modified.
     * @param roi The rectangle with the region of interest.
     */
    void roiFilter(const EventStore &in, EventStore &out, const cv::Rect &roi) {
        // in-place filtering is not supported
        assert(&in != &out);

        for (const Event &event : in) {
            if (roi.contains(cv::Point(event.x(), event.y()))) {
                out.addEvent(event);
            }
        }
    }

    /**
     * Projects the event coordinates onto a smaller range. The x- and y-coordinates the divided
     * by xFactor and yFactor respectively and floored to the next integer. This forms the new
     * coordinates of the event. Due to the nature of this, it can happen that multiple events
     * end up happening simultaneously at the same location. This is still a valid event stream,
     * as time keeps monotonically increasing, but is something that is unlikely to be generated
     * by an event camera.
     * @param in The EventStore to operate on. Won't get modified
     * @param out The outgoing EventStore to store the projected events on
     * @param xDivision Division factor for the x-coordinate for the events
     * @param yDivision Division factor for the y-coordinate of the events
     */
    void subsample(const EventStore &in, EventStore &out, double xDivision, double yDivision) {
        // in-place filtering is not supported
        assert(&in != &out);

        for (const Event &event : in) {
            out.addEvent(Event(event.timestamp(), (coord_t)(event.x()/xDivision), (coord_t)(event.y()/yDivision), event.polarity()));
        }
    }

    /**
     * Filters events by polarity. Only events that exhibit the same polarity as given in
     * polarity are kept.
     * @param in Incoming EventStore to operate on. Won't get modified.
     * @param out The outgoing EventStore to store the kept events on
     * @param polarity The polarity of the events that should be kept
     */
    void polarityFilter(const EventStore &in, EventStore &out, bool polarity) {
        // in-place filtering is not supported
        assert(&in != &out);

        for (const Event &event : in) {
            if (event.polarity() == polarity) {
                out.addEvent(event);
            }
        }
    }


    /**
    * Stateless per-pixel rate limiting. Limits the number of events per location
    * (spatially, in pixel) per second. Only operates on the given package, does not keep state.
    * To have a stateful rate limiter (over multiple packages) use the class `RateLimitFilter`.
    * to the given value. The first event at every location is kept, subsequently only events are kept
    * if they happen after the refractory period (1/rate). All events get copied, the incoming
    * packet is not modified.
    * @param in The incoming EventStore to work on.
    * @param out The EventStore to copy the kept events into.
    * @param rate The rate in "Events per second per pixel"
    */
    void rateLimitFilter(const EventStore &in, EventStore &out, double rate) {
        // in-place filtering is not supported
        assert(&in != &out);
        double period = (1./rate) * TIME_SCALE;

        std::unordered_map<long, time_t> occurrenceMap = {};
        for (const Event &event : in) {
            long hash = coordinateHash(event.x(), event.y());
            if (occurrenceMap.find(hash) == occurrenceMap.end() ||
                (double)(event.timestamp() - occurrenceMap[hash]) >= period) {
                occurrenceMap[hash] = event.timestamp();
                out.addEvent(event);
            }
        }
    }


    /**
     * Stateful per-pixel rate limiting filter. Reduces the number of events per pixel
     * to a defined value per second. The first event at every location is kept, subsequent events
     * are only kept if they arise after 1/rate seconds after the last kept event.
     */
    class RateLimitFilter {
    private:
        double rate_;
        TimeMat lastEmitSurface_;

    public:
        /**
         * Crates a new stateful RateLimitFilter with the provided dimensions. The
         * filter keeps track of the last emitted events at every location.
         * @param rows The height of the expected event data. This is one more than the maximum
         * y coordinate of expected data
         * @param cols the width of the expected event data. This is one more than the maximum
         * x coordinate of the expected data
         * @param rate The rate of events (in events/second) that should pass the filter
         */
        RateLimitFilter(coord_t rows, coord_t cols, double rate)
                : rate_(rate), lastEmitSurface_(TimeMat(cv::Size(cols, rows))) {}

        /**
        * Crates a new stateful RateLimitFilter with the provided dimensions. The
        * filter keeps track of the last emitted events at every location.
        * @param size The height and width of the expected event data.
        * @param rate The rate of events (in events/second) that should pass the filter
        */
        RateLimitFilter(const cv::Size &size, double rate)
        : RateLimitFilter(size.height, size.height, rate) {};

        /**
         * Filters the incoming packet with using the given rate and the state of the
         * filter.
         * @param in the incoming package, won't get modified
         * @param out the outgoing EventStore to where the kept events get added.
         */
        void filter(const EventStore &in, EventStore &out) {
            double period = (1./rate_) * TIME_SCALE;

            for (const Event &event : in) {
                if ((double)(event.timestamp() - lastEmitSurface_.at(event.y(), event.x()))  > period) {
                    out.addEvent(event);
                    lastEmitSurface_.at(event.y(), event.x()) = event.timestamp();
                }
            }
        }
    };

    /**
     * Computes and returns a rectangle with dimensions such that all the events
     * in the given `EventStore` fall into the bounding box.
     * @param packet The EventStore to work on
     * @return The smallest possible rectangle that contains all the events in packet.
     */
    cv::Rect boundingRect(const EventStore &packet) {
        if (packet.isEmpty()) {
            return cv::Rect(0, 0, 0, 0);
        }

        coord_t minX = std::numeric_limits<coord_t>::max();
        coord_t maxX = 0;
        coord_t minY = std::numeric_limits<coord_t>::max();
        coord_t maxY = 0;

        for (const Event &event : packet) {
            minX = std::min(event.x(), minX);
            maxX = std::max(event.x(), maxX);
            minY = std::min(event.y(), minY);
            maxY = std::max(event.y(), maxY);
        }

        return cv::Rect(minX, minY, maxX - minX, maxY - minY);
    }
}


#endif //DV_PROCESSING_EVENTPROC_HPP
