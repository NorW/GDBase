#pragma once
#include "pch.h"
#include <vector>
#include <array>
#include <atomic>
#include <thread>
#include <future>
#include <iterator>
#include <optional>


#define GDBASE_OBJECTPOOL_BLOCK_SIZE 1000
#define GDBASE_OBJECTPOOL_MAX_BLOCKS 1000

#include "..//GDBaseTests/TestClasses.h"


namespace GDBase
{
	namespace impl
	{
		class AtomicBoolWrapper;	//Wrapper for atomic_bool thats default and copy constructable so that it may be used in STL structures.
	};

	/*
		ObjectPool
		Data structure that stores a number of objects for reuse.
	*/
	template <class Obj>
	class ObjectPool;

	template <class Obj>
	class ObjectPool
	{
	public:

		explicit ObjectPool<Obj>(size_t initialSize = 1000): currentPosition_(0)
		{
			//Round capacity up to the next block size that can contain initialsize
			capacity_ = (initialSize / GDBASE_OBJECTPOOL_BLOCK_SIZE * GDBASE_OBJECTPOOL_BLOCK_SIZE) + (initialSize % GDBASE_OBJECTPOOL_BLOCK_SIZE > 0 ? GDBASE_OBJECTPOOL_BLOCK_SIZE : 0);

			isInUse_ = std::vector<impl::AtomicBoolWrapper>(capacity_);						//
			poolObjects_ = new Obj* [GDBASE_OBJECTPOOL_MAX_BLOCKS];		//Create block array

			//Create initial blocks
			for (size_t block = 0; block < capacity_ / GDBASE_OBJECTPOOL_BLOCK_SIZE; block++)
			{
				poolObjects_[block] = new Obj[GDBASE_OBJECTPOOL_BLOCK_SIZE];
			}
		}

		explicit ObjectPool<Obj>(Obj& defaultObject, size_t initialSize = 1000) : ObjectPool<Obj>(initialSize)
		{
			//Populate values with defaultObject.
			for (size_t block = 0; block < capacity_ / GDBASE_OBJECTPOOL_BLOCK_SIZE; block++)
			{
				poolObjects_[block] = new Obj[GDBASE_OBJECTPOOL_BLOCK_SIZE];
				for (size_t i = 0; i < GDBASE_OBJECTPOOL_BLOCK_SIZE; i++)
				{
					poolObjects_[block][i] = defaultObject;
				}
			}
		}

		~ObjectPool()
		{
			for (size_t i = 0; i < capacity_ / GDBASE_OBJECTPOOL_BLOCK_SIZE; i++)
			{
				delete[] poolObjects_[i];
			}
			delete[] poolObjects_;
		}

		virtual bool isInUse(size_t index) { return (index < capacity_) && (isInUse_[index].val); }
		virtual Obj& at(size_t index) { return poolObjects_[index / GDBASE_OBJECTPOOL_BLOCK_SIZE][index % GDBASE_OBJECTPOOL_BLOCK_SIZE]; }

		//Reserves one object. Returns the index to the object.
		virtual size_t reserve()
		{
			auto currentPosition = currentPosition_.load();
			bool expected = false;

			if (currentPosition >= capacity_)
			{
				increaseCapacity(capacity_ + GDBASE_OBJECTPOOL_BLOCK_SIZE);
			}

			//Attempt to reserve 
			while (!isInUse_[currentPosition].val.compare_exchange_strong(expected, true))
			{
				if (currentPosition >= capacity_)	//Capacity check
				{
					increaseCapacity(capacity_ + GDBASE_OBJECTPOOL_BLOCK_SIZE);
				}

				if (currentPosition == currentPosition_.load())	//Increment currentPosition_ if it has not changed.
				{
					currentPosition_++;
				}
				expected = false;	//Set expected back to false
				currentPosition = currentPosition_.load();	//set currentPosition
			}

			currentPosition_.compare_exchange_strong(currentPosition, currentPosition + 1);	//Increment currentPosition_ by 1 if its value has not changed.
			return currentPosition;
		}

		/*
			Reserves multiple objects and returns a vector of reserved ids.
			@amount Amount of objects to reserve.
		*/
		virtual void reserveMultiple(size_t*& ids, size_t amount)
		{
			ids = new size_t[amount];
			auto currentPosition = currentPosition_.load();
			bool expected = false;
			size_t toReserve = amount, expectedEnd, index = 0;

			//While objects still need to be reserved.
			while (toReserve > 0)
			{
				do
				{
					if ((currentPosition + toReserve) >= capacity_)	//Capacity check
					{
						increaseCapacity((capacity_ + toReserve) % GDBASE_OBJECTPOOL_BLOCK_SIZE > 0 ? ((capacity_ + toReserve) / GDBASE_OBJECTPOOL_BLOCK_SIZE + GDBASE_OBJECTPOOL_BLOCK_SIZE) : (capacity_ + toReserve));
					}
				} while (!currentPosition_.compare_exchange_strong(currentPosition, currentPosition + toReserve));	//Push marker forward

				expectedEnd = currentPosition + toReserve;
				//Attempt to reserve enough to satisfy amount starting from currentPosition.
				for (auto i = currentPosition; i < expectedEnd; i++)
				{
					expected = false;
					if (isInUse_[i].val.compare_exchange_strong(expected, true))	//If successfully marked, add id to ids.
					{
						toReserve--;
						ids[index++] = i;
					}
				}

				currentPosition = expectedEnd;	//Updated local position.
				if (currentPosition != currentPosition_.load())
				{
					currentPosition = currentPosition_.load();
				}
			}
		}

		/*
			Reserves multiple objects and returns a vector of reserved ids.
			@amount Amount of objects to reserve.
		*/
		virtual std::vector<size_t> reserveMultiple(size_t amount)
		{
			std::vector<size_t> ids;
			auto currentPosition = currentPosition_.load();
			bool expected = false;
			size_t toReserve = amount, expectedEnd;

			//While objects still need to be reserved.
			while (toReserve > 0)
			{
				do 
				{
					if ((currentPosition + toReserve) >= capacity_)	//Capacity check
					{
						increaseCapacity((capacity_ + toReserve) % GDBASE_OBJECTPOOL_BLOCK_SIZE > 0 ? ((capacity_ + toReserve) / GDBASE_OBJECTPOOL_BLOCK_SIZE + GDBASE_OBJECTPOOL_BLOCK_SIZE) : (capacity_ + toReserve));
					}
				} while (!currentPosition_.compare_exchange_strong(currentPosition, currentPosition + toReserve));	//Push marker forward

				expectedEnd = currentPosition + toReserve;
				//Attempt to reserve enough to satisfy amount starting from currentPosition.
				for (auto i = currentPosition; i < expectedEnd; i++)
				{
					expected = false;
					if (isInUse_[i].val.compare_exchange_strong(expected, true))	//If successfully marked, add id to ids.
					{
						toReserve--;
						ids.push_back(i);
					}
				}

				currentPosition = expectedEnd;	//Updated local position.
				if (currentPosition != currentPosition_.load())
				{
					currentPosition = currentPosition_.load();
				}
			}
			return ids;
		}

		//Releases an object from use.
		virtual void release(size_t index)
		{
			auto curPosition = currentPosition_.load();
			isInUse_[index].val.store(false);
			while (index < curPosition && !currentPosition_.compare_exchange_strong(curPosition, index)) {};	//Set currentPosition_ if index is lower.
		}

	protected:
		Obj** poolObjects_;
		std::vector<impl::AtomicBoolWrapper> isInUse_;
		std::mutex capacityLock_;
		size_t capacity_;									//Max capacity of the object pool
		std::atomic_size_t currentPosition_;					//Position of the first free object

		//Increases capacity of object pool to newCapacity. Does nothing if newCapacity is less than the current capacity.
		virtual void increaseCapacity(size_t newCapacity)
		{
			std::lock_guard<std::mutex> capacityGuard(capacityLock_);	//Lock so object pool does not attempt multiple capacity increases at the same time.

			//If capacity is less than new capacity, increase it.
			if (capacity_ < newCapacity)
			{
				//objects_.reserve(newCapacity);
				isInUse_.resize(newCapacity);
				auto nNewBlocks = newCapacity / GDBASE_OBJECTPOOL_BLOCK_SIZE;

				//Create and populate new blocks
				for (size_t block = capacity_ / GDBASE_OBJECTPOOL_BLOCK_SIZE; block < nNewBlocks; block++)
				{
					poolObjects_[block] = new Obj[GDBASE_OBJECTPOOL_BLOCK_SIZE];
				}
				capacity_ = newCapacity;	//Update capacity
			}
		}	//capacityLock_ released on lock destruction.
	};

	class impl::AtomicBoolWrapper
	{
	public:
		std::atomic<bool> val;
		AtomicBoolWrapper()
		{
			val.store(false);
		}

		AtomicBoolWrapper(const AtomicBoolWrapper& other)
		{
			val.store(other.val.load());
		}
	};
};