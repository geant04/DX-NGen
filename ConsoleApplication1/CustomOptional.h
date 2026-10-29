#pragma once
#include <utility>

namespace CustomDataStructurePractice
{
	template<typename T>
	class UniquePtr
	{
	private:
		T* ptr = nullptr;

	public:
		// Constructor
		UniquePtr() {};
		UniquePtr(T* p) : ptr(p) {};

		// Destructor
		~UniquePtr() { Reset(); };

		// Copy Constructor
		UniquePtr( const UniquePtr& copy ) = delete;
		// Copy Assignment
		UniquePtr& operator=( const UniquePtr& copy ) = delete;

		// Move Constructor
		UniquePtr( UniquePtr&& parent )
		{
			if ( parent.ptr != nullptr )
			{
				ptr = parent.Release();
			}
		}

		// Move Assignment
		UniquePtr& operator=( UniquePtr&& parent )
		{
			// confused by these references with this move reference?
			if ( this != &parent )
			{
				Reset( parent.Release() );
			}
			return *this;
		}

		// Releases data from ownership
		T* Release()
		{
			return std::exchange( ptr, nullptr );
		}

		// Destroys data of old data, replaces ptr with p
		void Reset( T* p = nullptr )
		{
			T* oldPtr = std::exchange( ptr, p );
			if ( oldPtr )
			{
				delete ptr;
			}
		}

		T* get() const { return ptr };
		T& operator*() const { return *ptr };
	};










}