# GDBase
Collection of classes for game development.

ObjectPool
  A generic thread safe implementation of an object pool.

AutoObjectPool
  A generic thread safe implementation of an object pool that uses reference counting to manage its members.
  Users requesting objects from this class get a PoolObject object that handles reference counting.
  An object inside the object pool will be automatically released when all PoolObjects referencing the specific object are destroyed.
