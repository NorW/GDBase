#include "pch.h"
#include "CppUnitTest.h"
#include "../GDBase/ObjectPool.h"
#include "../GDBase/AutoObjectPool.h"
#include "TestClasses.h"
#include <iostream>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using GDBase::AutoObjectPool;

using std::cout;
using std::endl;

namespace GDBaseTests
{
	GDBaseTests::Logger log;

	class ObjWrapper
	{
	public:
		GDBase::PoolObject<std::string> obj;
		explicit ObjWrapper(GDBase::PoolObject<std::string> a): obj(a) {}
	};

	TEST_CLASS(ObjectPool)
	{
	public:
		TEST_METHOD(TestInit)
		{
			GDBase::ObjectPool<std::string> pool;
			auto pool2 = new GDBase::ObjectPool<std::string>();
		}

		TEST_METHOD(TestReserve)
		{
			auto pool = new GDBase::ObjectPool<std::string>();
			auto obj1 = pool->reserve();
			auto obj2 = pool->reserve();
			auto obj3 = pool->reserve();
			auto obj4 = pool->reserve();
			auto obj5 = pool->reserve();

			Assert::AreEqual(pool->isInUse(obj1), true);
			Assert::AreEqual(pool->isInUse(obj2), true);
			Assert::AreEqual(pool->isInUse(obj3), true);
			Assert::AreEqual(pool->isInUse(obj4), true);
			Assert::AreEqual(pool->isInUse(obj5), true);
			Assert::AreEqual(pool->isInUse(6), false);
		}
		
		TEST_METHOD(TestResize)
		{
			auto pool = new GDBase::ObjectPool<std::string>();
			for (int i = 0; i < 3000; i++)
			{
				pool->reserve();
			}
			Assert::AreEqual(pool->isInUse(2993), true);
		}
		
		TEST_METHOD(TestDestruct)
		{
			auto pool = new GDBase::ObjectPool<std::string>();
			delete pool;
		}

		TEST_METHOD(TestRelease)
		{
			GDBase::ObjectPool<std::string> pool;
			pool.reserve();
			pool.reserve();
			pool.reserve();
			pool.reserve();
			pool.reserve();
			pool.reserve();
			pool.reserve();

			pool.release(1);
			pool.release(4);

			Assert::AreEqual(pool.isInUse(0), true);
			Assert::AreEqual(pool.isInUse(1), false);
			Assert::AreEqual(pool.isInUse(2), true);
			Assert::AreEqual(pool.isInUse(3), true);
			Assert::AreEqual(pool.isInUse(4), false);
			Assert::AreEqual(pool.isInUse(5), true);
			Assert::AreEqual(pool.isInUse(6), true);
			Assert::AreEqual(pool.isInUse(7), false);
			Assert::AreEqual(pool.isInUse(8), false);
			Assert::AreEqual(pool.isInUse(9), false);
			Assert::AreEqual(pool.isInUse(10), false);
		}

		TEST_METHOD(TestReserveMultipleMarked)
		{
			GDBase::ObjectPool<std::string> pool;
			auto ids = pool.reserveMultiple(400);
			auto ids2 = pool.reserveMultiple(300);

			for (int i = 0; i < 700; i++)
			{
				Assert::AreEqual(pool.isInUse(i), true);
			}
			Assert::AreEqual(pool.isInUse(700), false);
		}

		TEST_METHOD(TestReserveMultipleIDs)
		{
			GDBase::ObjectPool<std::string> pool;
			auto ids = pool.reserveMultiple(400);
			auto ids2 = pool.reserveMultiple(300);

			for (size_t i = 0; i < 400; i++)
			{
				Assert::AreEqual(ids[i], i);
			}

			for (size_t i = 0; i < 300; i++)
			{
				Assert::AreEqual(ids2[i], i + 400);
			}
		}

		TEST_METHOD(TestReserveMultipleMass)
		{
			GDBase::ObjectPool<std::string> pool;
			auto ids = pool.reserveMultiple(5000);
		}

		TEST_METHOD(TestReserveMultipleArrayMass)
		{
			GDBase::ObjectPool<std::string> pool;
			size_t* ids;
			pool.reserveMultiple(ids, 5000);
		}

		TEST_METHOD(TestReserveMultipleNoncontinuousMarked)
		{
			GDBase::ObjectPool<std::string> pool;
			pool.reserveMultiple(100);

			for (size_t i = 0; i < 100; i += 4)
			{
				pool.release(i);
			}

			pool.reserveMultiple(25);

			for (size_t i = 0; i < 100; i++)
			{
				Assert::AreEqual(pool.isInUse(i), true);
			}
			Assert::AreEqual(pool.isInUse(100), false);
		}
		
		TEST_METHOD(TestReserveMultipleNoncontinuousIDs)
		{
			GDBase::ObjectPool<std::string> pool;
			auto ids = pool.reserveMultiple(100);

			for (size_t i = 0; i < 100; i += 4)
			{
				pool.release(i);
			}

			auto ids2 = pool.reserveMultiple(25);

			for (size_t i = 0; i < 25; i++)
			{
				Assert::AreEqual(ids2[i], i * 4);
			}
		}
	};

	TEST_CLASS(AutoObjectPoolTests)
	{
	public:
		TEST_METHOD(TestInit)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);

			AutoObjectPool<std::string> pool2(v);
		}

		TEST_METHOD(TestInitSize)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v, 10);

			AutoObjectPool<std::string> pool2(v, 100);
		}
		TEST_METHOD(TestMakeObject)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);
			{
				auto obj = pool->makePoolObject();
				Assert::AreEqual(obj.getID(), (size_t)0);
				auto obj2 = pool->makePoolObject();
				Assert::AreEqual(obj2.getID(), (size_t)1);
				auto obj3 = pool->makePoolObject();
				Assert::AreEqual(obj3.getID(), (size_t)2);
				auto obj4 = pool->makePoolObject();
				Assert::AreEqual(obj4.getID(), (size_t)3);
				log.log(obj.getID());
				log.log(obj2.getID());
				log.log(obj3.getID());
				log.log(obj4.getID());

				Assert::AreNotEqual(obj.getID(), obj2.getID());
				Assert::AreNotEqual(obj.getID(), obj3.getID());
				Assert::AreNotEqual(obj.getID(), obj4.getID());
				Assert::AreNotEqual(obj2.getID(), obj3.getID());
				Assert::AreNotEqual(obj2.getID(), obj4.getID());
				Assert::AreNotEqual(obj3.getID(), obj4.getID());
			}
			int a = 0;
		}
		
		TEST_METHOD(TestMakePoolObjectsVectorMarked)
		{
			auto pool = new AutoObjectPool<std::string>();
			std::vector<GDBase::PoolObject<std::string>> vec;
			pool->makePoolObjects(vec, 1000);
			for(size_t i = 0; i < 1000; i++)
			{
				Assert::AreEqual(pool->isInUse(i), true);
			}
			Assert::AreEqual(pool->isInUse(1000), false);
		}
		
		/*
		TEST_METHOD(TestMakePoolObjectsArrayMarked)
		{
			auto pool = new AutoObjectPool<std::string>();
			GDBase::PoolObject<std::string>* objects;
			pool->makePoolObjects(objects, 1000);
			for (size_t i = 0; i < 1000; i++)
			{
				Assert::AreEqual(pool->isInUse(i), true);
			}
			Assert::AreEqual(pool->isInUse(1000), false);
		}*/
		
		TEST_METHOD(TestMakePoolObjectsVectorIDs)
		{
			auto pool = new AutoObjectPool<std::string>();
			std::vector<GDBase::PoolObject<std::string>> vec;
			pool->makePoolObjects(vec, 1000);
			for(size_t i = 0; i < 1000; i++)
			{
				Assert::AreEqual(vec[i].getID(), i);
			}
		}
		
		/*
		TEST_METHOD(TestMakePoolObjectsArrayIDs)
		{
			auto pool = new AutoObjectPool<std::string>();
			GDBase::PoolObject<std::string>* objects;
			pool->makePoolObjects(objects, 1000);
			for (size_t i = 0; i < 1000; i++)
			{
				Assert::AreEqual(objects[i].getID(), i);
			}
		}*/
		
		TEST_METHOD(TestDestroyObject)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);
			ObjWrapper* a = new ObjWrapper(pool->makePoolObject());
			ObjWrapper* b = new ObjWrapper(pool->makePoolObject());
			
			delete a;
			delete b;
		}
		
		TEST_METHOD(TestReuse)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);
			ObjWrapper* a = new ObjWrapper(pool->makePoolObject());
			Assert::AreEqual(a->obj.getID(), (size_t)0);
			ObjWrapper* b = new ObjWrapper(pool->makePoolObject());
			Assert::AreEqual(b->obj.getID(), (size_t)1);

			delete a;
			
			ObjWrapper* c = new ObjWrapper(pool->makePoolObject());
			a = new ObjWrapper(pool->makePoolObject());

			Assert::AreEqual(c->obj.getID(), (size_t)0);
			Assert::AreEqual(b->obj.getID(), (size_t)1);
			log.log(a->obj.getID());
			Assert::AreEqual(a->obj.getID(), (size_t)2);
		}


		TEST_METHOD(TestResize)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v, 10);
			std::vector<GDBase::PoolObject<std::string>> objects;

			for (size_t i = 0; i < 3000; i++)
			{
				objects.push_back(pool->makePoolObject());
				Assert::AreEqual(objects[i].getID(), i);
			}
		}

		TEST_METHOD(TestDestructor)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);
			delete pool;
		}

		TEST_METHOD(TestIsInUse)
		{
			std::string v = "asdf";
			auto pool = new AutoObjectPool<std::string>(v);
			auto obj = pool->makePoolObject();
			Assert::AreEqual(pool->isInUse(0), true);
			Assert::AreEqual(pool->isInUse(1), false);
		}
	};
}
