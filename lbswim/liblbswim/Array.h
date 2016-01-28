// -*- mode: C++; -*-

// Multidimensional array template class, where each element is a
// vector. Allows configuration of the memory layout at
// compile time.

#ifndef ARRAY_H
#define ARRAY_H

#include "cuhelp.h"

#include "array.h"
#include <memory>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <assert.h>


template <size_t ND>
struct AoSSpaceLayoutPolicy
{
  typedef array<size_t, ND> ShapeType;
  BOTH static ShapeType MakeStrides(const ShapeType& shape)
  {
    ShapeType strides;
    strides[ND-1] = 1;
    for (int i = int(ND) - 2; i >= 0; --i)
      strides[i] = strides[i+1] * shape[i+1];
    return strides;
  }
};
/*
template <size_t ND>
struct SoASpaceLayoutPolicy
{
  typedef std::array<size_t, ND> ShapeType;
  BOTH static ShapeType MakeStrides(const ShapeType& shape)
  {
    ShapeType strides;
    strides[ND-1] = 1;
    for (int i = int(ND) - 2; i >= 0; --i)
      strides[i] = strides[i+1] * shape[i+1];
    return strides;
  }
};
*/

template <size_t ND>
using SpaceLayoutPolicy = AoSSpaceLayoutPolicy<ND>;

template <class T, size_t ND>
BOTH T Product(const array<T,ND>& arr) {
  T ans = 1;
  for (size_t i = 0; i < ND; ++i) 
    ans *= arr[i];
  return ans;
}

// Helper template class for indexing.
// ND = number of space dimensions.
template <size_t ND>
struct SpaceIndexer
{
  static const size_t ndims;
  typedef array<size_t, ND> ShapeType;
  ShapeType shape;
  ShapeType strides;
  size_t size;
  
  BOTH SpaceIndexer() : shape(), strides(), size(0)
  {
  }
  
  // Construct for a given shape array.
  BOTH SpaceIndexer(const ShapeType& shp) : shape(shp),
					    strides(SpaceLayoutPolicy<ND>::MakeStrides(shp)),
					    size(Product(shp))
				       // size(std::accumulate(shp.begin(), shp.end(), 1,
				       // 			    [](size_t a, size_t b) {return a*b;}))
  {
  }
  
  // Compute 1D index from ND index
  BOTH size_t operator()(const ShapeType& idx) const {
    size_t ijk = 0;
    for (size_t d = 0; d < ND; ++d)
      ijk += strides[d]*idx[d];
    return ijk;
  }
  
  // Helper for subscripting - given a parent array of the next higher
  // dimensionality return an indexer for an array slice taken along
  // the first dimension.
  BOTH static SpaceIndexer ReduceFrom(const SpaceIndexer<ND+1>& parent) {
    SpaceIndexer ans;
    for (size_t i=0; i<ndims; ++i) {
      ans.shape[i] = parent.shape[i+1];
      ans.strides[i] = parent.strides[i+1];
    }
    ans.size = parent.size;
    return ans;
  }
};

// Define static member
template<size_t ND>
const size_t SpaceIndexer<ND>::ndims = ND;

// Forward declare
template <typename T, size_t ND, size_t nElem = 1>
struct Array;

// Wrapper for a single element vector of an Array.
template <typename T, size_t nElem>
struct ElemWrapper
{
  // The pointer to our zeroth element
  T* data;
  // Shared pointer to our base allocation of memory
  // std::shared_ptr<T> baseData;
  T* baseData;
  // number of elements to the next element.
  size_t stride;

public:
  // Get an element
  BOTH T& operator[](const size_t& i) {
    if (i >= nElem)
#ifdef __CUDA_ARCH__
      assert(false);
#else
      throw std::out_of_range("Out of range");
#endif
    return data[i * stride];
  }
  // Get a const element
  BOTH const T& operator[](const size_t& i) const {
    if (i >= nElem)
#ifdef __CUDA_ARCH__
      assert(false);
#else
      throw std::out_of_range("Out of range");
#endif
    return data[i * stride];
  }
};

template<typename T, size_t ND, size_t nElem>
BOTH typename Array<T, ND, nElem>::SubType Helper(Array<T, ND, nElem>& self, const size_t i) {
  Array<T, ND-1, nElem> ans;
  ans.data = self.data + i * self.indexer.strides[0];
  ans.baseData = self.baseData;
  ans.indexer = decltype(ans.indexer)::ReduceFrom(self.indexer);
  return ans;
}

template<typename T, size_t nElem>
BOTH typename Array<T, 1, nElem>::SubType Helper(Array<T, 1, nElem>& self, const size_t i) {
  ElemWrapper<T, nElem> ans;
  ans.data = self.data + i * self.indexer.strides[0];
  ans.baseData = self.baseData;
  ans.stride = self.indexer.size;
  return ans;
}
template<typename T, size_t ND, size_t nElem>
BOTH typename Array<T, ND, nElem>::ConstSubType Helper(const Array<T, ND, nElem>& self, const size_t i) {
  Array<T, ND-1, nElem> ans;
  ans.data = self.data + i * self.indexer.strides[0];
  ans.baseData = self.baseData;
  ans.indexer = decltype(ans.indexer)::ReduceFrom(self.indexer);
  return ans;
}

template<typename T, size_t nElem>
BOTH typename Array<T, 1, nElem>::ConstSubType Helper(const Array<T, 1, nElem>& self, const size_t i) {
  ElemWrapper<const T, nElem> ans;
  ans.data = self.data + i * self.indexer.strides[0];
  ans.baseData = self.baseData;
  ans.stride = self.indexer.size;
  return ans;
}

// Core array class.
template <typename T, size_t ND, size_t nElem>
struct Array
{
  typedef SpaceIndexer<ND> IdxType;
  typedef typename IdxType::ShapeType ShapeType;
  typedef ElemWrapper<T, nElem> WrapType;
  typedef ElemWrapper<const T, nElem> ConstWrapType;
  //typedef std::shared_ptr<T> Ptr;
  
  typedef typename std::conditional<ND==1, WrapType, Array<T, ND-1, nElem> >::type SubType;
  typedef typename std::conditional<ND==1, ConstWrapType, const Array<T, ND-1, nElem> >::type ConstSubType;
  
  IdxType indexer;
  // Pointer to our zeroth element
  T* data;
  // Shared pointer to base memory.
  T* baseData;
  
  // Helper class to call delete[] (instead of delete) on the
  // allocated memory.
  struct Deleter {
    BOTH void operator()(T* ptr) {
      delete[] ptr;
    }
  };
  
  // struct iterator : public std::iterator<std::bidirectional_iterator_tag, WrapType> {
  //   Array& container;
  //   size_t ijk;
  //   iterator(Array& cont, const ShapeType& startPos) : container(cont), ijk(cont.indexer(startPos))
  //   {
  //   }

  //   WrapType operator *()
  //   {
  //     WrapType ans;
  //     return Get<T,ND,nElem>(container, ijk);
  //   }
    
  //   bool operator !=(const iterator& other)const
  //   {
  //     if (container != other.container)
  // 	return true;
  //     if (ijk != other.ijk)
  // 	return true;
  //     return false;
  //   }
    
  //   iterator& operator ++()
  //   {
  //     ++ijk;
  //     return *this;
  //   }
  //   iterator& operator --()
  //   {
  //     --ijk;
  //     return *this;
  //   }
  // };
  
  // Default constructor
  BOTH Array() : indexer(ShapeType()), data(nullptr), baseData(nullptr)
  {
  }
  // Construct a given shape array
  BOTH Array(const ShapeType& shape) : indexer(shape)
  {
    size_t total_size = nElem * indexer.size;
    // baseData = Ptr(new T[total_size], Deleter());
    // data = baseData.get();
    baseData = new T[total_size];
    data = baseData;
  }

  // Subscript to return an array with dimensionality ND-1 or an
  // ElementWrapper, as appropriate.
  BOTH SubType operator[](size_t i) {
    return Helper(*this, i);
  }
  
  BOTH WrapType operator[](const ShapeType& idx) {
    // WrapType ans = {data + indexer(idx),
    // 		    baseData,
    // 		    indexer.size};
    WrapType ans;
    ans.data = data + indexer(idx);
    ans.baseData = baseData;
    ans.stride = indexer.size;
    return ans;
  }
  
  BOTH ConstSubType operator[](size_t i) const {
    return Helper(*this, i);
  }
  
  BOTH ConstWrapType operator[](const ShapeType& idx) const {
    ConstWrapType ans;
    ans.data = data + indexer(idx);
    ans.baseData = baseData;
    ans.stride = indexer.size;
    return ans;
  }
  
  // iterator begin() {
  //   ShapeType zero = {};
  //   return iterator(*this, zero);
  // }
  // iterator end() {
  //   return iterator(*this, indexer.shape);
  // }
  BOTH bool operator !=(const Array& other) {
    return data != other.data;
  }
};

#endif