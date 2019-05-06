#ifndef DV_PROCESSING_CORE_HPP
#define DV_PROCESSING_CORE_HPP

#include "../data/event.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>

#define TIME_SCALE 1e6
#define PARTIAL_SHARDING_COUNT 1000

namespace dv {

using time_t     = int64_t;
using coord_t    = int16_t;
using slicejob_t = unsigned int;

/**
 * __INTERNAL USE ONLY__
 * Compares an events timestamp to that of a timestamp.
 */
class EventTimeComparator {
public:
	bool operator()(const dv::Event &evt, const time_t time) const {
		return evt.timestamp() < time;
	}

	bool operator()(const time_t time, const Event &evt) const {
		return time < evt.timestamp();
	}
};

/**
 * __INTERNAL USE ONLY__
 * Internal event container class that holds a shard of events.
 * A `PartialEventData` holds a shared pointer to an `EventPacketT`, which
 * is the underlying data structure. The underlying data can either be const,
 * in which case no addition is allowed, or non const, in which addition
 * of new data is allowed. Slicing is allowed in both cases, as it only modifies
 * the control structure.
 * All the events in the partial have to
 * be monotonically increasing in time. A `PartialEventData` can be sliced
 * both from the front as well as from the back. By doing so, the memory
 * footprint of the structure is not modified, just the internal bookkeeping
 * pointers are readjusted. The `PartialEventData` keeps track of lowest as
 * well as highest times of events in the structure.
 *
 * The data `PartialEventData` points to can be shared between multiple
 * `PartialEventData`, each with potentially different slicings.
 */
class PartialEventData {
	using iterator = dv::cvector<const Event>::iterator;

private:
	bool referencesConstData_;
	size_t start_;
	size_t length_;
	time_t lowestTime_;
	time_t highestTime_;
	std::shared_ptr<const dv::EventPacketT> data_;

public:
	/**
	 * Creates a new `PartialEventData` shard. Allocates new memory on the
	 * heap to keep the data. Upon constructions, the newly created object
	 * is the sole owner of the data.
	 */
	PartialEventData() :
		referencesConstData_(false),
		start_(0),
		length_(0),
		data_(std::make_shared<const dv::EventPacketT>()) {
	}

	/**
	 * Creates a new `PartialEventData` shard from existing const data. Copies the
	 * supplied shared_ptr into the structure, acquiring shared ownership of
	 * the supplied data.
	 * @param data The shared pointer to the data to which we want to obtain shared
	 * ownership
	 */
	explicit PartialEventData(std::shared_ptr<const dv::EventPacketT> data) :
		referencesConstData_(true),
		start_(0),
		length_(data->events.size()),
		lowestTime_(data->events.front().timestamp()),
		highestTime_(data->events.back().timestamp()),
		data_(data) {
	}

	/**
	 * Copy constructor.
	 * Creates a shallow copy of `other` without copying the actual
	 * data over. As slicing does not alter the underlying data,
	 * the new copy may be sliced without affecting the orignal object.
	 * @param other
	 */
	PartialEventData(const PartialEventData &other) = default;

	/**
	 * Returns an iterator to the first element that is bigger than
	 * the supplied timestamp. If every element is bigger than the supplied
	 * time, an iterator to the first element is returned (same as `begin()`).
	 * If all elements have a smaller timestamp than the supplied, the end
	 * iterator is returned (same as `end()`).
	 * @param time The requested time. The iterator will be the first element
	 * with a timestamp larger than this time.
	 * @return An iterator to the first element larger than the supplied time.
	 */
	iterator iteratorAtTime(time_t time) {
		auto comparator    = EventTimeComparator();
		auto sliceEventItr = std::lower_bound(begin(), end(), time, comparator);
		return sliceEventItr;
	}

	/**
	 * Returns an iterator to the first element of the `PartialEventData`.
	 * The iterator is according to the current slice and not to the
	 * underlying datastore. E.g. when slicing the shard from the front,
	 * the `begin()` will change.
	 * @return
	 */
	iterator begin() {
		return data_->events.begin() + start_;
	}

	/**
	 * Returns an iterator to one after the last element of the `PartialEventData`.
	 * The iterator is according to the current slice and not to the
	 * underlying datastore. E.g. when slicing the shard from the back,
	 * the result of `end()` will change.
	 * @return
	 */
	iterator end() {
		return data_->events.begin() + start_ + length_;
	}

	/**
	 * Slices off `number` events from the front of the `PartialEventData`.
	 * This operation just adjust the bookkeeping of the datastructure
	 * without actually modifying the underlying data representation.
	 * If there are not enough events left, a `range_error` exception is thrown.
	 *
	 * Other instances of `PartialEventData` which share the same underlying
	 * data are not affected by this.
	 * @param number amount of events to be removed from the front.
	 */
	void sliceFront(size_t number) {
		if (number > length_) {
			throw std::range_error("Can not slice more than length from PartialEventData.");
		}

		start_      = start_ + number;
		length_     = length_ - number;
		lowestTime_ = (data_->events)[start_].timestamp();
	}

	/**
	 * Slices off `number` events from the back of the `PartialEventData`.
	 * This operation just adjust the bookkeeping of the datastructure
	 * without actually modifying the underlying data representation.
	 * If there are not enough events left, a `range_error` exception is thrown.
	 *
	 * Other instances of `PartialEventData` which share the same underlying
	 * data are not affected by this.
	 * @param number amount of events to be removed from the back.
	 */
	void sliceBack(size_t number) {
		if (number > length_) {
			throw std::range_error("Can not slice more than length from PartialEventData.");
		}

		length_      = length_ - number;
		highestTime_ = (data_->events)[start_ + length_ - 1].timestamp();
	}

	/**
	 * Slices off all the events that occur before the supplied time.
	 * The resulting data structure has a `lowestTime > time` where time
	 * is the supplied time.
	 *
	 * This operation just adjust the bookkeeping of the datastructure
	 * without actually modifying the underlying data representation.
	 * If there are not enough events left, a `range_error` exception is thrown.
	 *
	 * Other instances of `PartialEventData` which share the same underlying
	 * data are not affected by this.
	 *
	 * @param time the threshold time. All events `<= time` will be sliced off
	 * @return number of events that actually got sliced off as a result of
	 * this operation.
	 */
	size_t sliceTimeFront(time_t time) {
		auto timeItr = iteratorAtTime(time);
		auto index   = static_cast<size_t>(timeItr - begin());
		sliceFront(index);
		return index;
	}

	/**
	 * Slices off all the events that occur after the supplied time.
	 * The resulting data structure has a `lowestTime < time` where time
	 * is the supplied time.
	 *
	 * This operation just adjust the bookkeeping of the datastructure
	 * without actually modifying the underlying data representation.
	 * If there are not enough events left, a `range_error` exception is thrown.
	 *
	 * Other instances of `PartialEventData` which share the same underlying
	 * data are not affected by this.
	 *
	 * @param time the threshold time. All events `> time` will be sliced off
	 * @return number of events that actually got sliced off as a result of
	 * this operation.
	 */
	size_t sliceTimeBack(time_t time) {
		auto timeItr     = iteratorAtTime(time);
		auto index       = static_cast<size_t>(timeItr - begin());
		size_t cutAmount = length_ - index;
		sliceBack(cutAmount);
		return cutAmount;
	}

	/**
	 * __UNSAFE OPERATION__
	 * Copies the data of the supplied event into the underlying data
	 * structure and updates the internal bookkeeping to accommodate the event.
	 *
	 * NOTE: This function does not perform any boundary checks.
	 * Any call to function is expected to have performed the following
	 * boundary checks: `canStoreMoreEvents()` to see if there is space to accommodate
	 * the new event. `getHighestTime()` has to be smaller or equal than
	 * the new event's timestamp, as we require events to be monotonically
	 * increasing.
	 *
	 * @param event
	 */
	void unsafe_addEvent(const Event &event) {
		highestTime_ = event.timestamp();
		if (length_ == 0) {
			lowestTime_ = event.timestamp();
		}
		const dv::cvector<dv::Event> &constVectorRef = data_->events;
		auto vectorRef                               = const_cast<dv::cvector<dv::Event> &>(constVectorRef);
		vectorRef.emplace_back(event);
		length_++;
	}

	/**
	 * The length of the current slice of data. This  value
	 * is at most `PARTIAL_SHARDING_COUNT`, at least 0.
	 *
	 * @return the current length of the slice in number of events.
	 */
	inline size_t getLength() const {
		return length_;
	}

	/**
	 * Gets the lowest timestamp of an event that is represented in this
	 * Partial. The lowest timestamp is always identical to the timestamp
	 * of the first event of the slice.
	 *
	 * @return The timestamp of the first event in the slice.
	 * This is also the lowest time present in this slice.
	 */
	inline time_t getLowestTime() const {
		return lowestTime_;
	}

	/**
	 * Gets the highest timestamp of an event that is represented in this
	 * Partial. The lowest timestamp is always identical to the timestamp
	 * of the last event of the slice.
	 *
	 * @return The timestamp of the last event in the slice. This is also
	 * the highest timestamp present in this slice.
	 */
	inline time_t getHighestTime() const {
		return highestTime_;
	}

	/**
	 * Returns a reference to the element at the given offset of
	 * the slice.
	 * @param offset The offset in the slice of which element
	 * a reference should be obtained
	 * @return A reference to the object at offset offset
	 */
	inline const Event &operator[](size_t offset) const {
		assert(offset <= length_);
		return (data_->events)[start_ + offset];
	}

	/**
	 * Checks if it is safe to add more events to this partial.
	 * It is safe to add more events when the following conditions are fulfilled:
	 * * The partial does not represent const data. In that case, any modification of the underlying buffer
	 *   is impossible.
	 * * The partial does not exceed the sharding count limit
	 * * The partial hasn't been sliced from the back
	 *
	 * If it has been sliced from the back, adding new events would
	 * put them in unreachable space.
	 *
	 * @return true if there is space available to store more events in
	 * this partial.
	 */
	inline bool canStoreMoreEvents() {
		return !referencesConstData_
			   && (data_->events.size() < PARTIAL_SHARDING_COUNT && start_ + length_ == data_->events.size());
	}
};

/**
 * __INTERNAL USE ONLY__
 * Comparator Functor that checks if a given time lies within bounds of the event packet
 */
class PartialEventDataTimeComparator {
private:
	const bool lower_;

public:
	explicit PartialEventDataTimeComparator(bool lower) : lower_(lower) {
	}

	/**
	 * Returns true, if the comparator is set to not lower and the given time is higher than the highest
	 * timestamp of the partial, or when it is set to lower and the timestamp is higher than the lowest
	 * timestamp of the partial.
	 * @param partial The partial to be analysed
	 * @param time The time to be compared against
	 * @return true, if time is higher than either lowest or highest timestamp of partial depending on state
	 */
	bool operator()(const PartialEventData &partial, const time_t time) const {
		return lower_ ? partial.getLowestTime() < time : partial.getHighestTime() < time;
	}

	/**
	 * Returns true, if the comparator is set to not lower and the given time is higher than the lowest
	 * timestamp of the partial, or when it is set to lower and the timestamp is higher than the highest
	 * timestamp of the partial.
	 * @param partial The partial to be analysed
	 * @param time The time to be compared against
	 * @return true, if time is higher than either lowest or lowest timestamp of partial depending on state
	 */
	bool operator()(const time_t time, const PartialEventData &partial) const {
		return lower_ ? time < partial.getLowestTime() : time < partial.getHighestTime();
	}
};

/**
 * Iterator for the EventStore class.
 */
class EventStoreIterator {
private:
	const std::vector<PartialEventData> *dataPartialsPtr_;
	/** The current partial (shard) we point to */
	size_t partialIndex_;
	/** The current offset inside the shard we point to */
	size_t offset_;

	/**
	 * Increments the iterator to the next event.
	 * If the iterator goes beyond available data, it remains
	 * at this position.
	 */
	inline void increment() {
		offset_++;
		if (offset_ >= (*dataPartialsPtr_)[partialIndex_].getLength()) {
			offset_ = 0;
			if (partialIndex_ < dataPartialsPtr_->size()) { // increment only to one partial after end
				partialIndex_++;
			}
		}
	}

	/**
	 * Decrements the iterator to the previous event.
	 * If the iterator goes below zero, it remains
	 * at zero.
	 */
	inline void decrement() {
		if (partialIndex_ >= dataPartialsPtr_->size()) {
			partialIndex_ = dataPartialsPtr_->size() - 1;
			offset_       = (*dataPartialsPtr_)[partialIndex_].getLength() - 1;
		}
		else {
			if (offset_ > 0) {
				offset_--;
			}
			else {
				if (partialIndex_ > 0) {
					partialIndex_--;
					offset_ = (*dataPartialsPtr_)[partialIndex_].getLength() - 1;
				}
			}
		}
	}

public:
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type        = const Event;
	using pointer           = const Event *;
	using reference         = const Event &;
	using difference_type   = ptrdiff_t;
	using size_type         = size_t;

	/**
	 * Default constructor. Creates a new iterator at the beginning of
	 * the packet
	 */
	EventStoreIterator() : EventStoreIterator(nullptr, true) {
	}

	/**
	 * Creates a new Iterator either at the beginning or at the end
	 * of the package
	 * @param Pointer to the partials (shards) of the packet
	 * @param front iterator will be at the beginning (true) of the packet,
	 * or at the end (false) of the packet.
	 */
	explicit EventStoreIterator(const std::vector<PartialEventData> *dataPartialsPtr, bool front) :
		dataPartialsPtr_(dataPartialsPtr),
		offset_(0) {
		partialIndex_ = front ? 0 : dataPartialsPtr->size();
	}

	/**
	 * __INTERNAL USE ONLY__
	 * Creates a new iterator at the specific internal position supplied
	 * @param dataPartialsPtr Pointer to the partials (shards) of the packet
	 * @param partialIndex Index pointing to the active shard
	 * @param offset Offset in the active shard
	 */
	EventStoreIterator(const std::vector<PartialEventData> *dataPartialsPtr, size_t partialIndex, size_t offset) :
		dataPartialsPtr_(dataPartialsPtr),
		partialIndex_(partialIndex),
		offset_(offset) {
	}

	/**
	 * @return A reference to the Event at the current iterator position
	 */
	inline reference operator*() const noexcept {
		return (*dataPartialsPtr_)[partialIndex_][offset_];
	}

	/**
	 * @return A pointer to the Event at current iterator position
	 */
	inline pointer operator->() const noexcept {
		return &(this->operator*());
	}

	/**
	 * Increments the iterator by one
	 * @return A reference to the the same iterator, incremented by one
	 */
	EventStoreIterator &operator++() noexcept {
		increment();
		return *this;
	}

	/**
	 * Post-increments the iterator by one
	 * @return A new iterator at the current position. Increments original
	 * iterator by one.
	 */
	const EventStoreIterator operator++(int) noexcept {
		auto currentIterator = EventStoreIterator(dataPartialsPtr_, partialIndex_, offset_);
		increment();
		return currentIterator;
	}

	/**
	 * Increments iterator by a fixed number and returns reference to itself
	 * @param add amount one whishes to increment the iterator
	 * @return reference to itseld incremented by `add`
	 */
	EventStoreIterator &operator+=(size_type add) noexcept {
		for (size_t i = 0; i < add; i++) {
			increment();
		}
		return *this;
	}

	/**
	 * Decrements the iterator by one
	 * @return A reference to the the same iterator, decremented by one
	 */
	EventStoreIterator &operator--() noexcept {
		decrement();
		return *this;
	}

	/**
	 * Post-decrement the iterator by one
	 * @return A new iterator at the current position. Decrements original
	 * iterator by one.
	 */
	const EventStoreIterator operator--(int) noexcept {
		auto currentIterator = EventStoreIterator(dataPartialsPtr_, partialIndex_, offset_);
		decrement();
		return currentIterator;
	}

	/**
	 * Decrements iterator by a fixed number and returns reference to itself
	 * @param add amount one whishes to decrement the iterator
	 * @return reference to itseld decremented by `sub`
	 */
	EventStoreIterator &operator-=(size_type sub) noexcept {
		for (size_t i = 0; i < sub; i++) {
			decrement();
		}
		return *this;
	}

	/**
	 * @param rhs iterator to compare to
	 * @return true if both iterators point to the same element
	 */
	bool operator==(const EventStoreIterator &rhs) const noexcept {
		return (partialIndex_ == rhs.partialIndex_) && (offset_ == rhs.offset_);
	}

	/**
	 * @param rhs iterator to compare to
	 * @return true if both iterators point to different elements
	 */
	bool operator!=(const EventStoreIterator &rhs) const noexcept {
		return !(this->operator==(rhs));
	}
};

/**
 * EventStore class.
 * An `EventStore` is a collection of consecutive events, all monotonically
 * increasing in time. EventStore is the basic data structure for handling
 * event data. Event packets hold their data in shards of fixed size.
 * Copying an `EventStore` results in a shallow copy with shared ownership
 * of the shards that are common to both EventStores.
 * EventStores can be sliced by number of events or by time. Slicing creates
 * a shallow copy of the `EventPackage`.
 */
class EventStore {
	using iterator = EventStoreIterator;

protected:
	/** internal list of the shards. */
	std::vector<PartialEventData> dataPartials_;
	/** The exact number-of-events global offsets of the shards */
	std::vector<size_t> partialOffsets_;
	/** The total length of the event package */
	size_t totalLength_ = 0;

	/**
	 * __INTERNAL USE ONLY__
	 * Creates a new EventStore based on the supplied `PartialEventData`
	 * objects. Offsets and meta information is recomputed from the supplied
	 * list. The packet gets shared ownership of all underlying data
	 * of the `PartialEventData` slices in `dataPartials`.
	 * @param dataPartials vector of `PartialEventData` to construct this
	 * package from.
	 */
	explicit EventStore(std::vector<PartialEventData> &dataPartials) {
		this->dataPartials_ = dataPartials;

		// Build up length and offsets
		totalLength_ = 0;
		for (const auto &partial : dataPartials) {
			partialOffsets_.emplace_back(totalLength_);
			totalLength_ += partial.getLength();
		}
	}

public:
	/**
	 * Default constructor.
	 * Creates an empty `EventStore`. This does not allocate any memory
	 * as long as there is no data.
	 */
	EventStore() = default;

	/**
	 * Creates a shallow copy of EventStore. The underlying event data is
	 * is shared between the original and the new object.
	 * If the underlying data is modified, it will affect both packets.
	 * If the new package is sliced, it only affects the new package.
	 * @param other the package to create a shallow copy of
	 */
	EventStore(const EventStore &other) = default;

	/**
	 * Adds a received packet from a module input to the store. This is a shallow operation,
	 * the data of the packet does not get copied. The EventStore gains shared ownership
	 * over the supplied data.
	 * @param packet the packet to use to
	 */
	void addEventPacket(const dv::InputDataWrapper<dv::EventPacket> &packet) {
		if (!dataPartials_.empty() && dataPartials_.back().getHighestTime() > packet.front().timestamp()) {
			std::cerr << "[WARNING] Tried adding event packet to store out of order. Ignoring packet." << std::endl;
			return;
		}

		dataPartials_.emplace_back(PartialEventData(packet.getBasePointer()));
		partialOffsets_.emplace_back(totalLength_);
		totalLength_ += dataPartials_.back().getLength();
	}

	/**
	 * Merges the contents of the supplied Event Store into the current event store. This is
	 * a shallow operation, the data is not copied. The two event stores have to be in ascending order.
	 * @param store the store to be added to this store
	 */
	void addEventStore(EventStore &store) {
		if (getHighestTime() > store.getLowestTime()) {
			std::cerr << "[WARNING] Tried adding event store to store out of order. Ignoring packet." << std::endl;
			return;
		}

		for (PartialEventData &partial : store.dataPartials_) {
			dataPartials_.push_back(partial);
			partialOffsets_.push_back(totalLength_);
			totalLength_ += partial.getLength();
		}
	}

	/**
	 * Creates a new `EventStore` with the data from an `EventPacket`.
	 * This is a shallow operation. No data is copied. The EventStore gains shared
	 * ownership of the supplied data.
	 * This constructor also allows the implicit conversion from `dv::InputWrapper<dv::EventPacket>`
	 * to `dv::EventStore`
	 * @param packet the packet to construct the EventStore from
	 */
	EventStore(const dv::InputDataWrapper<dv::EventPacket> &packet) {
		totalLength_ = 0;
		addEventPacket(packet);
	}

	/**
	 * Adds a single Event to the EventStore. This will potentially
	 * allocate more memory when the currently available shards are exhausted.
	 * Any new memory receives exclusive ownership by this packet.
	 * @param event A reference to the event to be added.
	 */
	void addEvent(const Event &event) {
		PartialEventData *targetPartial = nullptr;

		if (!dataPartials_.empty()) {
			targetPartial = &dataPartials_.back();
			if (targetPartial->getHighestTime() > event.timestamp()) {
				std::cerr << "[WARNING] Tried adding events to store out of time order. Ignoring event." << std::endl;
				return;
			}
			if (!targetPartial->canStoreMoreEvents()) {
				dataPartials_.emplace_back(PartialEventData());
				partialOffsets_.emplace_back(totalLength_);
				targetPartial = &dataPartials_.back();
			}
		}
		else {
			dataPartials_.emplace_back(PartialEventData());
			partialOffsets_.emplace_back(totalLength_);
			targetPartial = &dataPartials_.back();
		}
		targetPartial->unsafe_addEvent(event);
		this->totalLength_++;
	}

	/**
	 * Returns the total size of the EventStore.
	 * @return The total size (in events) of the packet.
	 */
	inline size_t size() const noexcept {
		return totalLength_;
	}

	/**
	 * Returns a new EventStore which is a shallow representation of
	 * a slice of this EventStore. The slice is from `start` (number of,
	 * events, minimum 0, maximum `getLength()`) and has a length of `length`.
	 *
	 * As a slice is a shallow representation, no EventData gets copied by
	 * this operation. The resulting EventStore receives shared ownership
	 * over the relevant parts of the data. Should the original EventStore
	 * get out of scope, memory that is not relevant to the sliced EventStore
	 * will get freed.
	 *
	 * @param start The start index of the slice (in number of events)
	 * @param length The desired length of the slice (in number of events)
	 * @return A new EventStore object which references to the sliced,
	 * shared data. No Event data is copied.
	 */
	EventStore slice(size_t start, size_t length) const {
		if (start + length > totalLength_) {
			throw std::range_error("Slice exceeds EventStore range");
		}

		if (length == 0) {
			return EventStore();
		}

		std::vector<PartialEventData> newPartials;
		auto lowerPartial = std::upper_bound(partialOffsets_.begin(), partialOffsets_.end(), start);
		auto upperPartial = std::lower_bound(partialOffsets_.begin(), partialOffsets_.end(), start + length);
		auto lowIndex     = static_cast<size_t>(lowerPartial - partialOffsets_.begin()) - 1;
		auto highIndex    = static_cast<size_t>(upperPartial - partialOffsets_.begin());
		for (size_t i = lowIndex; i < highIndex; i++) {
			newPartials.emplace_back(dataPartials_[i]);
		}
		size_t frontSliceAmount = start - partialOffsets_[lowIndex];
		size_t backSliceAmount  = partialOffsets_[highIndex - 1] + newPartials.back().getLength() - (start + length);
		newPartials.front().sliceFront(frontSliceAmount);
		newPartials.back().sliceBack(backSliceAmount);

		if (newPartials.front().getLength() <= 0) {
			newPartials.erase(newPartials.begin());
		}

		if (newPartials.back().getLength() <= 0) {
			newPartials.erase(newPartials.end() - 1);
		}

		return EventStore(newPartials);
	}

	/**
	 * Returns a new EventStore which is a shallow representation of
	 * a slice of this EventStore. The slice is from `start` (number of,
	 * events, minimum 0, maximum `getLength()`) and goes to the end of the
	 * EventStore. This method slices off the front of an EventStore.
	 *
	 * As a slice is a shallow representation, no EventData gets copied by
	 * this operation. The resulting EventStore receives shared ownership
	 * over the relevant parts of the data. Should the original EventStore
	 * get out of scope, memory that is not relevant to the sliced EventStore
	 * will get freed.
	 *
	 * @param start The start index of the slice (in number of events). The
	 * slice will be from this index to the end of the packet.
	 * @return A new EventStore object which references to the sliced,
	 * shared data. No Event data is copied.
	 */
	EventStore slice(size_t start) const {
		return slice(start, totalLength_ - start);
	}

	/**
	 * Returns a new EventStore which is a shallow representation of
	 * a slice of this EventStore. The slice is from a specific startTime (in
	 * event timestamps, microseconds) to a specific endTime (event timestamps,
	 * microseconds). The actual size (in events) of the resulting packet
	 * depends on the event rate in the requested time interval. The resulting
	 * packet may be empty, if there is no event that happend in the requested
	 * interval.
	 *
	 * As a slice is a shallow representation, no EventData gets copied by
	 * this operation. The resulting EventStore receives shared ownership
	 * over the relevant parts of the data. Should the original EventStore
	 * get out of scope, memory that is not relevant to the sliced EventStore
	 * will get freed.
	 * @param startTime The start time of the required slice
	 * @param endTime The end time of the required time
	 * @param retStart parameter that will get set to the actual index (in
	 * number of events) at which the start of the slice occured.
	 * @param retEnd parameter that will get set to the actual index (in
	 * number of events) at which the end of the slice occured
	 * @return A new EventStore object that is a shallow representation
	 * to the sliced, shared data. No data is copied over.
	 */
	EventStore sliceTime(time_t startTime, time_t endTime, size_t &retStart, size_t &retEnd) const {
		// we find the relevant partials and slice the first and last one to fit
		std::vector<PartialEventData> newPartials;

		auto lowerPartial = std::lower_bound(
			dataPartials_.begin(), dataPartials_.end(), startTime, PartialEventDataTimeComparator(false));
		auto upperPartial = std::upper_bound(
			dataPartials_.begin(), dataPartials_.end(), endTime, PartialEventDataTimeComparator(true));

		size_t newLength = 0;
		for (auto it = lowerPartial; it < upperPartial; it++) {
			newLength += it->getLength();
			newPartials.emplace_back(*it);
		}

		if (newLength == 0) {
			retStart = isEmpty() ? 0 : partialOffsets_[static_cast<size_t>(lowerPartial - dataPartials_.begin())];
			retEnd   = retStart;

			return EventStore();
		}

		size_t cutFront = newPartials.front().sliceTimeFront(startTime);
		size_t cutBack  = newPartials.back().sliceTimeBack(endTime);
		newLength       = newLength - cutFront - cutBack;

		if (newPartials.front().getLength() <= 0) {
			newPartials.erase(newPartials.begin());
		}

		if (!newPartials.empty() && newPartials.back().getLength() <= 0) {
			newPartials.erase(newPartials.end() - 1);
		}

		retStart = partialOffsets_[static_cast<size_t>(lowerPartial - dataPartials_.begin())] + cutFront;
		retEnd   = retStart + newLength;

		return EventStore(newPartials);
	}

	/**
	 * Returns a new EventStore which is a shallow representation of
	 * a slice of this EventStore. The slice is from a specific startTime (in
	 * event timestamps, microseconds) to a specific endTime (event timestamps,
	 * microseconds). The actual size (in events) of the resulting packet
	 * depends on the event rate in the requested time interval. The resulting
	 * packet may be empty, if there is no event that happend in the requested
	 * interval.
	 *
	 * As a slice is a shallow representation, no EventData gets copied by
	 * this operation. The resulting EventStore receives shared ownership
	 * over the relevant parts of the data. Should the original EventStore
	 * get out of scope, memory that is not relevant to the sliced EventStore
	 * will get freed.
	 * @param startTime The start time of the required slice
	 * @param endTime The end time of the required time
	 * @return A new EventStore object that is a shallow representation
	 * to the sliced, shared data. No data is copied over.
	 */
	EventStore sliceTime(time_t startTime, time_t endTime) const {
		size_t retStart, retEnd;
		return sliceTime(startTime, endTime, retStart, retEnd);
	}

	/**
	 * Returns a new EventStore which is a shallow representation of
	 * a slice of this EventStore. The slice is from a specific startTime (in
	 * event timestamps, microseconds) to the end of the packet.
	 * The actual size (in events) of the resulting packet
	 * depends on the event rate in the requested time interval. The resulting
	 * packet may be empty, if there is no event that happend in the requested
	 * interval.
	 *
	 * As a slice is a shallow representation, no EventData gets copied by
	 * this operation. The resulting EventStore receives shared ownership
	 * over the relevant parts of the data. Should the original EventStore
	 * get out of scope, memory that is not relevant to the sliced EventStore
	 * will get freed.
	 * @param startTime The start time of the required slice
	 * @return A new EventStore object that is a shallow representation
	 * to the sliced, shared data. No data is copied over.
	 */
	EventStore sliceTime(time_t startTime) const {
		return sliceTime(startTime, getHighestTime() + 1); // + 1 to include the events that happen at the last time.
	}

	/**
	 * Returns an iterator to the begin of the EventStore
	 * @return an interator to the begin of the EventStore
	 */
	iterator begin() const noexcept {
		return (iterator(&dataPartials_, true));
	}

	/**
	 * Returns an iterator to the end of the EventStore
	 * @return  an iterator to the end of the EventStore
	 */
	iterator end() const noexcept {
		return (iterator(&dataPartials_, false));
	}

	/**
	 * Returns a reference to the first element of the packet
	 * @return a reference to the first element to the packet
	 */
	const Event &front() const {
		return *iterator(&dataPartials_, true);
	}

	/**
	 * Returns a reference to the last element of the packet
	 * @return a reference to the last element to the packet
	 */
	const Event &back() const {
		iterator it(&dataPartials_, false);
		it -= 1;
		return *it;
	}

	/**
	 * Returns the timestamp of the first event in the packet.
	 * This is also the lowest timestamp in the packet, as
	 * the events are required to be monotonic.
	 * @return The lowest timestamp present in the packet. 0 if the packet is
	 * empty.
	 */
	inline time_t getLowestTime() const {
		if (isEmpty()) {
			return 0;
		}
		return dataPartials_.front().getLowestTime();
	}

	/**
	 * Returns the timestamp of the last event in the packet.
	 * This is also the highest timestamp in the packet, as
	 * the events are required to be monotonic.
	 * @return The highest timestamp present in the packet. 0 if the packet
	 * is empty
	 */
	inline time_t getHighestTime() const {
		if (isEmpty()) {
			return 0;
		}
		return dataPartials_.back().getHighestTime();
	}

	/**
	 * Returns the total length (in number of events) of the packet
	 * @return the total number of events present in the packet.
	 */
	inline size_t getTotalLength() const {
		return totalLength_;
	}

	/**
	 * Returns true if the packet is empty (does not contain any events).
	 * @return Returns true if the packet is empty (does not contain any events).
	 */
	inline bool isEmpty() const {
		return totalLength_ == 0;
	}
};

/**
 * The EventStreamSlicer is a class that takes on incoming events, stores
 * them in a minimal way and invokes functions at individual
 * periods.
 */
class EventStreamSlicer {
	/**
	 * __INTERNAL USE ONLY__
	 * A single job of the EventStreamSlicer
	 */
	class SliceJob {
	public:
		enum SliceType { NUMBER, TIME };

	private:
		SliceType type_;
		const std::function<void(EventStore &)> callback_;
		time_t timeInterval_;
		size_t numberInterval_;
		time_t lastCallEndTime_;

	public:
		size_t lastCallEnd;

		/**
		 * __INTERNAL USE ONLY__
		 * Creates a new SliceJob of a certain type, interval and callback
		 * @param type The type of periodicity. Can be either NUMBER or TIME
		 * @param interval The interval (either in time or number) at which
		 * the job should be executed
		 * @param callback The callback function to call on execution.
		 */
		SliceJob(const SliceType type, const size_t interval, std::function<void(EventStore &)> callback) :
			type_(type),
			callback_(std::move(callback)),
			lastCallEndTime_(0),
			lastCallEnd(0) {
			this->timeInterval_   = (type == TIME ? interval : 0);
			this->numberInterval_ = (type == NUMBER ? interval : 0);
		}

		/**
		 * __INTERNAL USE ONLY__
		 * This function establishes how much fresh data is availble
		 * and how often the callback can be executed on this fresh data.
		 * it then creates slices of the data and executes the callback as
		 * often as possible.
		 * @param packet the storage packet to slice on.
		 */
		void run(EventStore &packet) {
			if (packet.getTotalLength() == 0) {
				return;
			}

			if (type_ == NUMBER) {
				while (packet.getTotalLength() - lastCallEnd >= numberInterval_) {
					EventStore slice = packet.slice(lastCallEnd, numberInterval_);
					lastCallEnd      = lastCallEnd + numberInterval_;
					callback_(slice);
				}
			}

			if (type_ == TIME) {
				if (lastCallEndTime_ == 0) { // initialize with the lowest time
					lastCallEndTime_ = packet.getLowestTime();
				}
				while (packet.getHighestTime() - lastCallEndTime_ >= timeInterval_) {
					size_t lastCallStart;
					EventStore slice = packet.sliceTime(
						lastCallEndTime_, lastCallEndTime_ + timeInterval_, lastCallStart, lastCallEnd);
					lastCallEndTime_ = lastCallEndTime_ + timeInterval_;
					callback_(slice);
				}
			}
		}
	};

private:
	/** Global storage packet that holds just as many events
	 * as minimally required for all outstanding calls */
	EventStore storePacket_;

	/** List of all the sliceJobs */
	std::vector<SliceJob> sliceJobs_;

	/**
	 * Should get called as soon as there is fresh data available.
	 * It loops through all jobs and determines if they can run on the new data.
	 * The jobs get executed as often as possible. Afterwards, all data that has
	 * been processed by all jobs gets discarded.
	 */
	void evaluate() {
		// run jobs
		for (auto &job : sliceJobs_) {
			job.run(storePacket_);
		}

		// find  border of the end of last call
		size_t lowerBound = storePacket_.getTotalLength();
		for (auto &job : sliceJobs_) {
			lowerBound = std::min(lowerBound, job.lastCallEnd);
		}

		// discard fully processed events and readjust call boundaries of jobs
		storePacket_ = storePacket_.slice(lowerBound);
		for (auto &job : sliceJobs_) {
			job.lastCallEnd = job.lastCallEnd - lowerBound;
		}
	}

public:
	EventStreamSlicer() = default;

	/**
	 * Adds a single event to the slicer buffer and evaluate jobs.
	 * @param evt the event to be added to the buffer
	 */
	void addEvent(const Event &evt) {
		storePacket_.addEvent(evt);
		evaluate();
	}

	/**
	 * Adds full EventStore to the buffer and evaluates jobs.
	 * This function copies the data over.
	 * @param evtPacket the EventStore to be added to the buffer
	 */
	void addEventStore(EventStore &evtStore) {
		storePacket_.addEventStore(evtStore);
		evaluate();
	}

	/**
	 * Adds the an `EventPacket` to the buffer
	 * and evaluate the jobs. The content of the packet does not get copied.
	 * The slicer obtaines shared ownership ower the provided data
	 * @param packet the libcaer packet to add the the buffer
	 */
	void addEventPacket(const InputDataWrapper<dv::EventPacket> &packet) {
		storePacket_.addEventPacket(packet);
		evaluate();
	}

	/**
	 * Adds a number-of-events triggered job to the Slicer. A job is defined
	 * by its interval and callback function. The slicer calls the callback
	 * function every `n` events, with the corresponding data.
	 * The (cpu) time interval between individual calls to the function depends
	 * on the physical event rate as well as the bulk sizes of the incoming
	 * data.
	 * @param n the interval (in number of events) in which the callback
	 * should be called
	 * @param callback the callback function that gets called on the data
	 * every interval
	 * @return A handle to uniquely identify the job.
	 */
	slicejob_t doEveryNumberOfEvents(size_t n, std::function<void(const EventStore &)> callback) {
		sliceJobs_.emplace_back(SliceJob(SliceJob::SliceType::NUMBER, n, callback));
		return (slicejob_t) sliceJobs_.size();
	}

	/**
	 * Adds an event-timestamp-interval triggered job to the Slicer.
	 * A job is defined by its interval and callback function. The slicer
	 * calls the callback whenever the timestamp difference of an incoming
	 * event to the last time the function was called is bigger than the
	 * interval. As the timing is based on event times rather than CPU time,
	 * the actual time periods are not guranteed, especially with a low event
	 * rate.
	 * The (cpu) time interval between individual calls to the function depends
	 * on the physical event rate as well as the bulk sizes of the incoming
	 * data.
	 * @param time the interval (in event-time) in which the callback
	 * should be called
	 * @param callback the callback function that gets called on the data
	 * every interval
	 * @return A handle to uniquely identify the job.
	 */
	slicejob_t doEveryTimeInterval(time_t time, std::function<void(const EventStore &)> callback) {
		sliceJobs_.emplace_back(SliceJob(SliceJob::SliceType::TIME, time, callback));
		return (slicejob_t) sliceJobs_.size();
	}
};

/**
 * OpenCV Mat does not support a 64 integer data type. Since event data timestamps
 * are inherently 64 bits, they can't be stored in an OpenCV Mat. This class provides
 * a type safe alternative, without many of the OpenCV convenience functions.
 */
class TimeMat {
private:
	std::shared_ptr<time_t> data_;

	void addImpl(time_t a, TimeMat &target) const {
		auto length = ((unsigned int) cols * (unsigned int) rows);
		for (unsigned int i = 0; i < length; i++) {
			time_t oldVal           = data_.get()[i];
			(target.data_.get())[i] = (a < 0 && -a > oldVal) ? 0 : oldVal + (time_t) a;
		}
	}

public:
	/**
	 * Dummy constructor
	 * Constructs a new, empty TimeMat without any data allocated to it.
	 */
	TimeMat() = default;

	/**
	 * Creates a new TimeMat of the given size. The Mat is zero initialized.
	 * @param size
	 */
	explicit TimeMat(const cv::Size size) :
		data_(std::shared_ptr<time_t>(
			new time_t[(unsigned long) (size.width * size.height)]{0}, [](const time_t *p) { delete[] p; })),
		rows(size.height),
		cols(size.width) {
	}

	/**
	 * Copy constructor, constructs a new TimeMat with shared ownership of the data.
	 * @param other The TimeMat to be copied. The data is not copied but takes shared ownership.
	 */
	TimeMat(const TimeMat &other) = default;

	/**
	 * The height of the TimeMat.
	 */
	coord_t rows = 0;

	/**
	 * The width of the TimeMat.
	 */
	coord_t cols = 0;

	/**
	 * The size of the TimeMat.
	 * @return
	 */
	inline cv::Size size() const {
		return cv::Size(cols, rows);
	}

	/**
	 * Returns true if the TimeMat has zero size. In this case, it was not
	 * allocated with a size.
	 * @return true if the TimeMat does not have a size > 0
	 */
	inline bool empty() {
		return rows == 0 || cols == 0;
	}

	/**
	 * Returns a reference to the element at the given coordinates.
	 * The element can both be read from as well as written to.
	 * @param y The y coordinate of the element to be accessed.
	 * @param x The x coordinate of the element to be accessed.
	 * @return A referemce to the element at the requested coordinats.
	 */
	inline time_t &at(coord_t y, coord_t x) const {
		return data_.get()[(unsigned long) (y * cols + x)];
	}

	/**
	 * Creates a new OpenCV matrix of the type given and copies the time
	 * data into this OpenCV matrix. The data in the TimeMat is of unsigned
	 * 64bit integer type. There is no OpenCV type that can hold the full
	 * range of these values. Make sure that the range of the values in the
	 * TimeMat can be mapped onto the OpenCV type requested.
	 * @tparam T The type of the OpenCV Mat to be generated.
	 * @return An OpenCV Mat of the requested type.
	 */
	template<class T> cv::Mat getOCVMat() const {
		cv::Mat mat(rows, cols, cv::DataType<T>::type);

		for (coord_t y = 0; y < rows; y++) {
			for (coord_t x = 0; x < cols; x++) {
				mat.at<T>(y, x) = (T) at(y, x);
			}
		}
		return mat;
	}

	/**
	 * Adds a constant to the TimeMat.
	 * Values are bounds checked to 0. If the new time would become negative,
	 * it is set to 0.
	 * @tparam T The type of the constant. Accepts any numeric type.
	 * @param s The constant to be added
	 * @return A new TimeMat with the changed times
	 */
	template<typename T> inline TimeMat operator+(const T &s) const {
		TimeMat tm(cv::Size(cols, rows));
		addImpl((const time_t) s, tm);
		return tm;
	}

	/**
	 * Adds a constant to the TimeMat.
	 * Values are bounds checked to 0. If the new time would become negative,
	 * it is set to 0.
	 * @tparam T The type of the constant. Accepts any numeric type.
	 * @param s The constant to be added
	 * @return A reference to the TimeMat
	 */
	template<typename T> inline TimeMat &operator+=(const T &s) {
		addImpl((const time_t) s, *this);
		return *this;
	}

	/**
	 * Subtracts a constant to the TimeMat.
	 * Values are bounds checked to 0. If the new time would become negative,
	 * it is set to 0.
	 * @tparam T The type of the constant. Accepts any numeric type.
	 * @param s The constant to be subtracted
	 * @return A reference to the TimeMat
	 */
	template<typename T> inline TimeMat operator-(const T &s) const {
		TimeMat tm(cv::Size(cols, rows));
		addImpl((const time_t) -s, tm);
		return tm;
	}

	/**
	 * Subtracts a constant to the TimeMat.
	 * Values are bounds checked to 0. If the new time would become negative,
	 * it is set to 0.
	 * @tparam T The type of the constant. Accepts any numeric type.
	 * @param s The constant to be subtracted
	 * @return A reference to the TimeMat
	 */
	template<typename T> inline TimeMat &operator-=(const T &s) {
		addImpl((const time_t) -s, *this);
		return *this;
	}
};

} // namespace dv

#endif // DV_PROCESSING_CORE_HPP
