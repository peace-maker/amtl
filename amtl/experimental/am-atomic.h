// vim: set sts=8 ts=2 sw=2 tw=99 et:
//
// Copyright (C) 2013-2016, David Anderson and AlliedModders LLC
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//  * Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//  * Neither the name of AlliedModders LLC nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef _include_amtl_experimental_atomic_h_
#define _include_amtl_experimental_atomic_h_

#include <amtl/am-utility.h>
#include <amtl/am-atomics.h>

#if defined(_MSC_VER)
// On Windows, we shortcut and use C++11 <atomic>. Normally we would not use
// STL as we are afforded more flexibility with our own containers: however,
// there are two factors pushing us in this direction for Windows. One, linking
// the STL in embedded libraries it not as painful. Second, Microsoft has
// deprecated compiler intrinsics. We might as well try biting the bullet in
// this case.
# define KE_USE_CXX11_ATOMICS
# include <atomic>
#endif

namespace ke {

// We support two of the C++11 memory ordering models.
enum class MemoryOrdering
{
  // No synchronization.
  Relaxed,

  // Sequentially consistent (issue a full memory barrier before and after
  // loads and stores).
  SeqCst
};

namespace impl {

#if defined(KE_USE_CXX11_ATOMICS)

template <MemoryOrdering Order>
struct StlOrdering;

template <>
struct StlOrdering<MemoryOrdering::Relaxed>
{
  static const std::memory_order Load = std::memory_order_relaxed;
  static const std::memory_order Store = std::memory_order_relaxed;
  static const std::memory_order ReadModifyWrite = std::memory_order_relaxed;
};

template <>
struct StlOrdering<MemoryOrdering::SeqCst>
{
  static const std::memory_order Load = std::memory_order_seq_cst;
  static const std::memory_order Store = std::memory_order_seq_cst;
  static const std::memory_order ReadModifyWrite = std::memory_order_seq_cst;
};

template <typename T, MemoryOrdering Order>
struct AtomicValueImpl
{
  typedef StlOrdering<Order> StlOrder;

 public:
  explicit AtomicValueImpl(T value)
   : value_(value)
  {}

  T exchange(T newValue) {
    return value_.exchange(newValue, StlOrder::ReadModifyWrite);
  }

  T get() const {
    return value_.load(StlOrder::Load);
  }

  void set(T value) {
    value_.store(value, StlOrder::Store);
  }

 private:
  std::atomic<T> value_;
};

#else // KE_USE_CXX11_ATOMICS

template <MemoryOrdering Order>
struct Synchronize;

template <>
struct Synchronize<MemoryOrdering::Relaxed>
{
  static void FenceBeforeLoad()
  {}
  static void FenceAfterLoad()
  {}
  static void FenceBeforeStore()
  {}
  static void FenceAfterStore()
  {}
};

# if defined(__GNUC__)
template <>
struct Synchronize<MemoryOrdering::SeqCst>
{
  static void FenceBeforeLoad() {
    __sync_synchronize();
  }
  static void FenceAfterLoad() {
    __sync_synchronize();
  }
  static void FenceBeforeStore() {
    __sync_synchronize();
  }
  static void FenceAfterStore() {
    __sync_synchronize();
  }
};

template <typename T, MemoryOrdering Order>
struct AtomicValueImpl
{
  typedef Synchronize<Order> Model;

 public:
  explicit AtomicValueImpl(T value)
   : value_(value)
  {}

  T exchange(T newValue) {
    // AMTL implements atomics with full sequential ordering only, so we need
    // more than the documented acquire barrier for this intrinsic.
    Model::FenceBeforeStore();
    return __sync_lock_test_and_set(&value_, newValue);
  }

  T get() const {
    Model::FenceBeforeLoad();
    T value = value_;
    Model::FenceAfterLoad();
    return value;
  }

  void set(T value) {
    Model::FenceBeforeStore();
    value_ = value;
    Model::FenceAfterStore();
  }

 private:
  T value_;
};
# endif // __GNUC__
#endif  // !KE_USE_CXX11_ATOMICS

} // namespace impl

template <typename T,
          MemoryOrdering Order = MemoryOrdering::SeqCst>
class Atomic;

template <MemoryOrdering Order>
class Atomic<bool, Order> final : private impl::AtomicValueImpl<int32_t, Order>
{
  typedef typename impl::AtomicValueImpl<int32_t, Order> Base;

 public:
  Atomic()
   : Base(0)
  {}
  explicit Atomic(bool value)
   : Base(value ? 1 : 0)
  {}

  operator bool() const {
    return !!Base::get();
  }

  // Retrieve the current value of the variable, then store the new given value.
  bool exchange(bool value) {
    return !!Base::exchange(value ? 1 : 0);
  }

 private:
  Atomic(const Atomic& other) = delete;
  void operator =(const Atomic& other) = delete;
};

} // namespace ke

#endif // _include_amtl_experimental_atomic_h_
