#pragma once

#include "pch.h"
#include <vector>
#include <array>
#include <atomic>
#include <thread>
#include <future>
#include <iterator>
#include <optional>

#include "ObjectPool.h"

#define GDBASE_INVALID_ID (size_t)-1

namespace GDBase
{
	//Class declarations
	namespace impl
	{
		template <class Obj>
		class InternalPoolObj;		//Internal object wrapper used in AutoObjectPool to track ownership of object pool objects.
	}

	template <class Obj>
	class PoolObject;		//Object wrapper for ObjectPool objects. Treat as a smart pointer.

	template <class Obj>
	class AutoObjectPool;		//Object Pool

	template <class Obj>
	class PoolObject
	{
	public:
		PoolObject() : object_(nullptr) {}
		PoolObject(impl::InternalPoolObj<Obj>& obj) : object_(&obj) { if(object_ != nullptr) object_->incrementCounter(); }		//Increment counter on creation
		PoolObject(PoolObject<Obj>& obj) : object_(obj.object_) { if(object_ != nullptr) object_->incrementCounter(); }			//

		~PoolObject() { object_->decrementCounter(); }							//Decrement reference counter on deletion

		PoolObject<Obj> operator=(const PoolObject<Obj>& other) { object_ = other.object_; object_->incrementCounter(); }
		auto& operator*() { return **object_; }
		auto operator->() { return object_->getPointer(); }

		const auto getID() { return object_ != nullptr ? object_->Index() : GDBASE_INVALID_ID; }

	private:
		impl::InternalPoolObj<Obj>* object_;	//Pointer to internal pool object. Can not use a reference due to the existence of = operator.
	};

	//Class definitions
	/*
		AutoObjectPool
		An object pool that automatically releases (does not destroy) objects when they are no longer referenced.
		References to objects are handled through PoolObject objects.
	*/
	template <class Obj>
	class AutoObjectPool : protected ObjectPool<impl::InternalPoolObj<Obj>>	//Protected inheritence to hide base class functions.
	{
	public:
		/*
			AutoObjectPool Constructor
			@defaultObject	What to initialize objects as.
			@initialSize	Initial size of the object pool rounded up to the nearest block size specified by GDBASE_OBJECTPOOL_BLOCK_SIZE
		*/
		explicit AutoObjectPool<Obj>(Obj& defaultObject = Obj(), size_t initialSize = 1000) : ObjectPool<impl::InternalPoolObj<Obj>>(initialSize), defaultObject_(defaultObject)
		{
			//Set internal pool object values created by ObjectPool constructor.
			for (size_t block = 0; block < capacity_ / GDBASE_OBJECTPOOL_BLOCK_SIZE; block++)
			{
				for (size_t i = 0; i < GDBASE_OBJECTPOOL_BLOCK_SIZE; i++)
				{
					poolObjects_[block][i].setValues(this, block * GDBASE_OBJECTPOOL_BLOCK_SIZE + i);
				}
			}
		}

		//Reserves and returns a PoolObject with default values.
		virtual PoolObject<Obj> makePoolObject()
		{
			auto index = reserve(); //Reserve object
			return PoolObject<Obj>(poolObjects_[index / GDBASE_OBJECTPOOL_BLOCK_SIZE][index % GDBASE_OBJECTPOOL_BLOCK_SIZE]);
		}


		virtual void makePoolObjects(std::vector<PoolObject<Obj>>& objects, size_t nObjects)
		{
			objects.reserve(nObjects);							//Reserve space in objects
			auto ids = ObjectPool::reserveMultiple(nObjects);	//Reserve IDs
			for (auto id : ids)
			{
				objects.push_back(PoolObject<Obj>(poolObjects_[id / GDBASE_OBJECTPOOL_BLOCK_SIZE][id % GDBASE_OBJECTPOOL_BLOCK_SIZE]));
			}
		}
		
		/*
			Currently gives MSB6006 error CL.exe exited with code 2.

		virtual void makePoolObjects(PoolObject<Obj>*& objects, size_t nObjects)
		{
			objects = new PoolObject<Obj>[nObjects];
			size_t* ids;
			ObjectPool::reserveMultiple(ids, nObjects);
			size_t id;
			for (size_t i = 0; i < nObjects; i++)
			{
				id = ids[i];
				objects[i] = (PoolObject<Obj>)PoolObject<Obj>(poolObjects_[id / GDBASE_OBJECTPOOL_BLOCK_SIZE][id % GDBASE_OBJECTPOOL_BLOCK_SIZE]);
			}
		}*/

		virtual bool isInUse(size_t index) { return ObjectPool::isInUse(index); }	//Returns whether object at index is in use or not.

		Obj& getDefaultObject() { return defaultObject_; }
		Obj& setDefaultObject(Obj& newObj) { defaultObject_ = newObj; }

		/*
			Called when object at index is no longer referenced.
			Sets pointer to first free object to index if index specifies an earlier position and mark object as unused.
			When overloading, it is recommended to call the base function after resetting values if at all.
		*/
		virtual void resetObject(size_t index)
		{
			auto curPosition = currentPosition_.load();
			isInUse_[index].val.store(false);	//Set flag to unused.
			while ((index < curPosition) && !currentPosition_.compare_exchange_strong(curPosition, index)){}	//Set currentPosition_ if index is lower.
		}

	protected:
		Obj defaultObject_;									//Default state of the objects in the object pool.

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
					poolObjects_[block] = new impl::InternalPoolObj<Obj>[GDBASE_OBJECTPOOL_BLOCK_SIZE];

					for (size_t index = 0; index < GDBASE_OBJECTPOOL_BLOCK_SIZE; index++)
					{
						poolObjects_[block][index].setValues(this, block * GDBASE_OBJECTPOOL_BLOCK_SIZE + index);
					}
				}

				capacity_ = newCapacity;	//Update capacity
			}
		}	//capacityLock_ released on lock destruction.
	};

	template <class Obj>
	class impl::InternalPoolObj
	{
	public:
		//Constructors set the ObjectPool that owns the object and starts the reference counter at 0.
		InternalPoolObj() :owner_(0), id_(GDBASE_INVALID_ID), count_(0) {}

		InternalPoolObj(AutoObjectPool<Obj>* owner, size_t index) : owner_(owner), id_(index), count_(0), object_(owner->getDefaultObject()) {}

		InternalPoolObj(const InternalPoolObj<Obj>& defaultValue) : InternalPoolObj(defaultValue.owner_, defaultValue.id_) {}

		~InternalPoolObj() {}

		void setValues(AutoObjectPool<Obj>* owner, size_t index)
		{
			owner_ = owner;
			id_ = index;
			object_ = owner->getDefaultObject();
		}

		void incrementCounter() { ++count_; }	//Increment the reference counter
		void decrementCounter() { if (--count_ < 1) { owner_->resetObject(id_); } }	//Decrement the reference counter, if counter reaches 0 (no references) reset object.

		Obj& operator*() { return object_; }
		Obj* operator->() { return &object_; }

		Obj* getPointer() { return &object_; }

		const auto& Owner() { return *owner_; }

		const auto Index() { return id_; }

	private:
		Obj object_;				//The object stored.
		size_t id_;					//Object ID (index of the object in object pool)
		AutoObjectPool<Obj>* owner_;	//ObjectPool that owns this object.
		std::atomic_int count_;		//Reference counter. When the counter reaches 0, this object will reset itself to the default object specified by owner_.
	};
};