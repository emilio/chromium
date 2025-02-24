// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceTraits_h
#define TraceTraits_h

#include "platform/heap/GCInfo.h"
#include "platform/heap/Heap.h"
#include "platform/heap/StackFrameDepth.h"
#include "platform/heap/Visitor.h"
#include "platform/heap/WrapperVisitor.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/HashTable.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"

namespace blink {

template <typename T>
class CrossThreadPersistent;
template <typename T>
class CrossThreadWeakPersistent;
template <typename T>
class Member;
template <typename T>
class TraceEagerlyTrait;
template <typename T>
class TraceTrait;
template <typename T>
class WeakMember;
template <typename T>
class WeakPersistent;

// "g++ -Os" reasonably considers the mark() eager-tracing specialization
// as an inlinable method. Its optimization pipeline will however trigger
// unconditional uses of that inlining inside trace() methods, i.e., without
// consideration for resulting code size, so one for each use of
// "visitor->trace(..)". This results in an unwanted amount of extra code
// across all trace methods. Address the issue indirectly by turning off
// inlining for the method. See crbug.com/681991 for further details.
//
// TODO(sof): revisit with later g++ versions, or when g++ is no
// longer used for production builds.
#if !defined(__clang__) && defined(__GNUC__)
#define NOINLINE_GXX_ONLY NOINLINE
#else
#define NOINLINE_GXX_ONLY
#endif

template <typename T, bool = NeedsAdjustAndMark<T>::value>
class AdjustAndMarkTrait;

template <typename T>
class AdjustAndMarkTrait<T, false> {
  STATIC_ONLY(AdjustAndMarkTrait);

 public:
  template <typename VisitorDispatcher>
  static NOINLINE_GXX_ONLY void mark(VisitorDispatcher visitor, const T* t) {
#if DCHECK_IS_ON()
    assertObjectHasGCInfo(const_cast<T*>(t), GCInfoTrait<T>::index());
#endif
    // Default mark method of the trait just calls the two-argument mark
    // method on the visitor. The second argument is the static trace method
    // of the trait, which by default calls the instance method
    // trace(Visitor*) on the object.
    //
    // If the trait allows it, invoke the trace callback right here on the
    // not-yet-marked object.
    if (TraceEagerlyTrait<T>::value) {
      // Protect against too deep trace call chains, and the
      // unbounded system stack usage they can bring about.
      //
      // Assert against deep stacks so as to flush them out,
      // but test and appropriately handle them should they occur
      // in release builds.
      //
      // If you hit this assert, it means that you're creating an object
      // graph that causes too many recursions, which might cause a stack
      // overflow. To break the recursions, you need to add
      // WILL_NOT_BE_EAGERLY_TRACED_CLASS() to classes that hold pointers
      // that lead to many recursions.
      DCHECK(visitor->heap().stackFrameDepth().isAcceptableStackUse());
      if (LIKELY(visitor->heap().stackFrameDepth().isSafeToRecurse())) {
        if (visitor->ensureMarked(t)) {
          TraceTrait<T>::trace(visitor, const_cast<T*>(t));
        }
        return;
      }
    }
    visitor->mark(const_cast<T*>(t), &TraceTrait<T>::trace);
  }
};

template <typename T>
class AdjustAndMarkTrait<T, true> {
  STATIC_ONLY(AdjustAndMarkTrait);

 public:
  template <typename VisitorDispatcher>
  static void mark(VisitorDispatcher visitor, const T* self) {
    if (!self)
      return;
    self->adjustAndMark(visitor);
  }
};

template <typename T, bool isTraceable>
struct TraceIfEnabled;

template <typename T>
struct TraceIfEnabled<T, false> {
  STATIC_ONLY(TraceIfEnabled);
  template <typename VisitorDispatcher>
  static void trace(VisitorDispatcher, T&) {
    static_assert(!WTF::IsTraceable<T>::value, "T should not be traced");
  }
};

template <typename T>
struct TraceIfEnabled<T, true> {
  STATIC_ONLY(TraceIfEnabled);
  template <typename VisitorDispatcher>
  static void trace(VisitorDispatcher visitor, T& t) {
    static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
    visitor->trace(t);
  }
};

template <bool isTraceable,
          WTF::WeakHandlingFlag weakHandlingFlag,
          WTF::ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceCollectionIfEnabled;

template <WTF::ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceCollectionIfEnabled<false,
                                WTF::NoWeakHandlingInCollections,
                                strongify,
                                T,
                                Traits> {
  STATIC_ONLY(TraceCollectionIfEnabled);
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher, T&) {
    static_assert(!WTF::IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    return false;
  }
};

template <bool isTraceable,
          WTF::WeakHandlingFlag weakHandlingFlag,
          WTF::ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceCollectionIfEnabled {
  STATIC_ONLY(TraceCollectionIfEnabled);
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, T& t) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      weakHandlingFlag == WTF::WeakHandlingInCollections,
                  "Traits should be traced");
    return WTF::TraceInCollectionTrait<weakHandlingFlag, strongify, T,
                                       Traits>::trace(visitor, t);
  }
};

// The TraceTrait is used to specify how to mark an object pointer and
// how to trace all of the pointers in the object.
//
// By default, the 'trace' method implemented on an object itself is
// used to trace the pointers to other heap objects inside the object.
//
// However, the TraceTrait can be specialized to use a different
// implementation. A common case where a TraceTrait specialization is
// needed is when multiple inheritance leads to pointers that are not
// to the start of the object in the Blink garbage-collected heap. In
// that case the pointer has to be adjusted before marking.
template <typename T>
class TraceTrait {
  STATIC_ONLY(TraceTrait);

 public:
  static void trace(Visitor*, void* self);

  static void markWrapperNoTracing(const WrapperVisitor*, const void*);
  static void traceMarkedWrapper(const WrapperVisitor*, const void*);
  static HeapObjectHeader* heapObjectHeader(const void*);

  template <typename VisitorDispatcher>
  static void mark(VisitorDispatcher visitor, const T* t) {
    AdjustAndMarkTrait<T>::mark(visitor, t);
  }

 private:
  static const T* ToWrapperTracingType(const void* t) {
    static_assert(CanTraceWrappers<T>::value,
                  "T should be able to trace wrappers. See "
                  "dispatchTraceWrappers in WrapperVisitor.h");
    static_assert(!NeedsAdjustAndMark<T>::value,
                  "wrapper tracing is not supported within mixins");
#if DCHECK_IS_ON()
    DCHECK(HeapObjectHeader::fromPayload(t)->checkHeader());
#endif
    return reinterpret_cast<const T*>(t);
  }
};

template <typename T>
class TraceTrait<const T> : public TraceTrait<T> {};

template <typename T>
void TraceTrait<T>::trace(Visitor* visitor, void* self) {
  static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
  static_cast<T*>(self)->trace(visitor);
}

template <typename T>
void TraceTrait<T>::markWrapperNoTracing(const WrapperVisitor* visitor,
                                         const void* t) {
  const T* traceable = ToWrapperTracingType(t);
  DCHECK(!heapObjectHeader(traceable)->isWrapperHeaderMarked());
  visitor->markWrapperHeader(heapObjectHeader(traceable));
}

template <typename T>
void TraceTrait<T>::traceMarkedWrapper(const WrapperVisitor* visitor,
                                       const void* t) {
  const T* traceable = ToWrapperTracingType(t);
  DCHECK(heapObjectHeader(t)->isWrapperHeaderMarked());
  // The term *mark* is misleading here as we effectively trace through the
  // API boundary, i.e., tell V8 that an object is alive. Actual marking
  // will be done in V8.
  traceable->markAndDispatchTraceWrappers(visitor);
}

template <typename T>
HeapObjectHeader* TraceTrait<T>::heapObjectHeader(const void* t) {
  return HeapObjectHeader::fromPayload(ToWrapperTracingType(t));
}

template <typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapVectorBacking<T, Traits>;

  template <typename VisitorDispatcher>
  static void trace(VisitorDispatcher visitor, void* self) {
    static_assert(!WTF::IsWeak<T>::value,
                  "weakness in HeapVectors and HeapDeques are not supported");
    if (WTF::IsTraceableInCollectionTrait<Traits>::value)
      WTF::TraceInCollectionTrait<
          WTF::NoWeakHandlingInCollections, WTF::WeakPointersActWeak,
          HeapVectorBacking<T, Traits>, void>::trace(visitor, self);
  }

  template <typename VisitorDispatcher>
  static void mark(VisitorDispatcher visitor, const Backing* backing) {
    AdjustAndMarkTrait<Backing>::mark(visitor, backing);
  }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner.  This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries.  When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template <typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapHashTableBacking<Table>;
  using Traits = typename Table::ValueTraits;

  template <typename VisitorDispatcher>
  static void trace(VisitorDispatcher visitor, void* self) {
    if (WTF::IsTraceableInCollectionTrait<Traits>::value ||
        Traits::weakHandlingFlag == WTF::WeakHandlingInCollections)
      WTF::TraceInCollectionTrait<WTF::NoWeakHandlingInCollections,
                                  WTF::WeakPointersActStrong, Backing,
                                  void>::trace(visitor, self);
  }

  template <typename VisitorDispatcher>
  static void mark(VisitorDispatcher visitor, const Backing* backing) {
    AdjustAndMarkTrait<Backing>::mark(visitor, backing);
  }
};

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
class TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static const bool firstIsTraceable = WTF::IsTraceable<T>::value;
  static const bool secondIsTraceable = WTF::IsTraceable<U>::value;
  template <typename VisitorDispatcher>
  static void trace(VisitorDispatcher visitor, std::pair<T, U>* pair) {
    TraceIfEnabled<T, firstIsTraceable>::trace(visitor, pair->first);
    TraceIfEnabled<U, secondIsTraceable>::trace(visitor, pair->second);
  }
};

// If eager tracing leads to excessively deep |trace()| call chains (and
// the system stack usage that this brings), the marker implementation will
// switch to using an explicit mark stack. Recursive and deep object graphs
// are uncommon for Blink objects.
//
// A class type can opt out of eager tracing by declaring a TraceEagerlyTrait<>
// specialization, mapping the trait's |value| to |false| (see the
// WILL_NOT_BE_EAGERLY_TRACED_CLASS() macros below.) For Blink, this is done for
// the small set of GCed classes that are directly recursive.
//
// The TraceEagerlyTrait<T> trait controls whether or not a class
// (and its subclasses) should be eagerly traced or not.
//
// If |TraceEagerlyTrait<T>::value| is |true|, then the marker thread
// should invoke |trace()| on not-yet-marked objects deriving from class T
// right away, and not queue their trace callbacks on its marker stack,
// which it will do if |value| is |false|.
//
// The trait can be declared to enable/disable eager tracing for a class T
// and any of its subclasses, or just to the class T, but none of its
// subclasses.
//
template <typename T>
class TraceEagerlyTrait {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = true;
};

// Disable eager tracing for TYPE, but not any of its subclasses.
#define WILL_NOT_BE_EAGERLY_TRACED_CLASS(TYPE) \
  template <>                                  \
  class TraceEagerlyTrait<TYPE> {              \
    STATIC_ONLY(TraceEagerlyTrait);            \
                                               \
   public:                                     \
    static const bool value = false;           \
  }

template <typename T>
class TraceEagerlyTrait<Member<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<SameThreadCheckedMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<TraceWrapperMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<WeakMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<Persistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<WeakPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<CrossThreadPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename ValueArg, size_t inlineCapacity>
class HeapListHashSetAllocator;
template <typename T, size_t inlineCapacity>
class TraceEagerlyTrait<
    WTF::ListHashSetNode<T, HeapListHashSetAllocator<T, inlineCapacity>>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = false;
};

template <typename T>
struct TraceIfNeeded : public TraceIfEnabled<T, WTF::IsTraceable<T>::value> {
  STATIC_ONLY(TraceIfNeeded);
};

}  // namespace blink

namespace WTF {

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections,
                              strongify,
                              T,
                              Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, T& t) {
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);
    visitor->trace(t);
    return false;
  }
};

// Catch-all for things that have HashTrait support for tracing with weakness.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections, strongify, T, Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, T& t) {
    return Traits::traceInCollection(visitor, t, strongify);
  }
};

// This trace method is used only for on-stack HeapVectors found in
// conservative scanning. On-heap HeapVectors are traced by Vector::trace.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename T,
          typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections,
                              strongify,
                              blink::HeapVectorBacking<T, Traits>,
                              void> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, void* self) {
    // HeapVectorBacking does not know the exact size of the vector
    // and just knows the capacity of the vector. Due to the constraint,
    // HeapVectorBacking can support only the following objects:
    //
    // - An object that has a vtable. In this case, HeapVectorBacking
    //   traces only slots that are not zeroed out. This is because if
    //   the object has a vtable, the zeroed slot means that it is
    //   an unused slot (Remember that the unused slots are guaranteed
    //   to be zeroed out by VectorUnusedSlotClearer).
    //
    // - An object that can be initialized with memset. In this case,
    //   HeapVectorBacking traces all slots including unused slots.
    //   This is fine because the fact that the object can be initialized
    //   with memset indicates that it is safe to treat the zerod slot
    //   as a valid object.
    static_assert(!IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::canClearUnusedSlotsWithMemset ||
                      std::is_polymorphic<T>::value,
                  "HeapVectorBacking doesn't support objects that cannot be "
                  "cleared as unused with memset.");

    // This trace method is instantiated for vectors where
    // IsTraceableInCollectionTrait<Traits>::value is false, but the trace
    // method should not be called. Thus we cannot static-assert
    // IsTraceableInCollectionTrait<Traits>::value but should runtime-assert it.
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);

    T* array = reinterpret_cast<T*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::fromPayload(self);
    ASSERT(header->checkHeader());
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->payloadSize() / sizeof(T);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    // As commented above, HeapVectorBacking can trace unused slots
    // (which are already zeroed out).
    ANNOTATE_CHANGE_SIZE(array, length, 0, length);
#endif
    if (std::is_polymorphic<T>::value) {
      char* pointer = reinterpret_cast<char*>(array);
      for (unsigned i = 0; i < length; ++i) {
        char* element = pointer + i * sizeof(T);
        if (blink::vTableInitialized(element))
          blink::TraceIfEnabled<
              T, IsTraceableInCollectionTrait<Traits>::value>::trace(visitor,
                                                                     array[i]);
      }
    } else {
      for (size_t i = 0; i < length; ++i)
        blink::TraceIfEnabled<
            T, IsTraceableInCollectionTrait<Traits>::value>::trace(visitor,
                                                                   array[i]);
    }
    return false;
  }
};

// This trace method is used only for on-stack HeapHashTables found in
// conservative scanning. On-heap HeapHashTables are traced by HashTable::trace.
template <ShouldWeakPointersBeMarkedStrongly strongify, typename Table>
struct TraceInCollectionTrait<NoWeakHandlingInCollections,
                              strongify,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;

  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, void* self) {
    static_assert(strongify == WTF::WeakPointersActStrong,
                  "An on-stack HeapHashTable needs to be visited strongly.");

    DCHECK(IsTraceableInCollectionTrait<Traits>::value ||
           Traits::weakHandlingFlag == WeakHandlingInCollections);
    Value* array = reinterpret_cast<Value*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::fromPayload(self);
    ASSERT(header->checkHeader());
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->payloadSize() / sizeof(Value);
    for (size_t i = 0; i < length; ++i) {
      if (!HashTableHelper<
              Value, typename Table::ExtractorType,
              typename Table::KeyTraitsType>::isEmptyOrDeletedBucket(array[i]))
        blink::TraceCollectionIfEnabled<
            IsTraceableInCollectionTrait<Traits>::value,
            Traits::weakHandlingFlag, strongify, Value,
            Traits>::trace(visitor, array[i]);
    }
    return false;
  }
};

// This specialization of TraceInCollectionTrait is for the backing of
// HeapListHashSet.  This is for the case that we find a reference to the
// backing from the stack.  That probably means we have a GC while we are in a
// ListHashSet method since normal API use does not put pointers to the backing
// on the stack.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename NodeContents,
          size_t inlineCapacity,
          typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
struct TraceInCollectionTrait<
    NoWeakHandlingInCollections,
    strongify,
    blink::HeapHashTableBacking<HashTable<
        ListHashSetNode<NodeContents,
                        blink::HeapListHashSetAllocator<T, inlineCapacity>>*,
        U,
        V,
        W,
        X,
        Y,
        blink::HeapAllocator>>,
    void> {
  using Node =
      ListHashSetNode<NodeContents,
                      blink::HeapListHashSetAllocator<T, inlineCapacity>>;
  using Table = HashTable<Node*, U, V, W, X, Y, blink::HeapAllocator>;

  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, void* self) {
    Node** array = reinterpret_cast<Node**>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::fromPayload(self);
    ASSERT(header->checkHeader());
    size_t length = header->payloadSize() / sizeof(Node*);
    for (size_t i = 0; i < length; ++i) {
      if (!HashTableHelper<Node*, typename Table::ExtractorType,
                           typename Table::KeyTraitsType>::
              isEmptyOrDeletedBucket(array[i])) {
        traceListHashSetValue(visitor, array[i]->m_value);
        // Just mark the node without tracing because we already traced
        // the contents, and there is no need to trace the next and
        // prev fields since iterating over the hash table backing will
        // find the whole chain.
        visitor->markNoTracing(array[i]);
      }
    }
    return false;
  }
};

// Key value pairs, as used in HashMap.  To disambiguate template choice we have
// to have two versions, first the one with no special weak handling, then the
// one with weak handling.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename Key,
          typename Value,
          typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections,
                              strongify,
                              KeyValuePair<Key, Value>,
                              Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self) {
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);
    blink::TraceCollectionIfEnabled<
        IsTraceableInCollectionTrait<typename Traits::KeyTraits>::value,
        NoWeakHandlingInCollections, strongify, Key,
        typename Traits::KeyTraits>::trace(visitor, self.key);
    blink::TraceCollectionIfEnabled<
        IsTraceableInCollectionTrait<typename Traits::ValueTraits>::value,
        NoWeakHandlingInCollections, strongify, Value,
        typename Traits::ValueTraits>::trace(visitor, self.value);
    return false;
  }
};

template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename Key,
          typename Value,
          typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections,
                              strongify,
                              KeyValuePair<Key, Value>,
                              Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self) {
    // This is the core of the ephemeron-like functionality.  If there is
    // weakness on the key side then we first check whether there are
    // dead weak pointers on that side, and if there are we don't mark the
    // value side (yet).  Conversely if there is weakness on the value side
    // we check that first and don't mark the key side yet if we find dead
    // weak pointers.
    // Corner case: If there is weakness on both the key and value side,
    // and there are also strong pointers on the both sides then we could
    // unexpectedly leak.  The scenario is that the weak pointer on the key
    // side is alive, which causes the strong pointer on the key side to be
    // marked.  If that then results in the object pointed to by the weak
    // pointer on the value side being marked live, then the whole
    // key-value entry is leaked.  To avoid unexpected leaking, we disallow
    // this case, but if you run into this assert, please reach out to Blink
    // reviewers, and we may relax it.
    constexpr bool keyIsWeak =
        Traits::KeyTraits::weakHandlingFlag == WeakHandlingInCollections;
    constexpr bool valueIsWeak =
        Traits::ValueTraits::weakHandlingFlag == WeakHandlingInCollections;
    const bool keyHasStrongRefs =
        IsTraceableInCollectionTrait<typename Traits::KeyTraits>::value;
    const bool valueHasStrongRefs =
        IsTraceableInCollectionTrait<typename Traits::ValueTraits>::value;
    static_assert(
        !keyIsWeak || !valueIsWeak || !keyHasStrongRefs || !valueHasStrongRefs,
        "this configuration is disallowed to avoid unexpected leaks");
    if ((valueIsWeak && !keyIsWeak) ||
        (valueIsWeak && keyIsWeak && !valueHasStrongRefs)) {
      // Check value first.
      bool deadWeakObjectsFoundOnValueSide = blink::TraceCollectionIfEnabled<
          IsTraceableInCollectionTrait<typename Traits::ValueTraits>::value,
          Traits::ValueTraits::weakHandlingFlag, strongify, Value,
          typename Traits::ValueTraits>::trace(visitor, self.value);
      if (deadWeakObjectsFoundOnValueSide)
        return true;
      return blink::TraceCollectionIfEnabled<
          IsTraceableInCollectionTrait<typename Traits::KeyTraits>::value,
          Traits::KeyTraits::weakHandlingFlag, strongify, Key,
          typename Traits::KeyTraits>::trace(visitor, self.key);
    }
    // Check key first.
    bool deadWeakObjectsFoundOnKeySide = blink::TraceCollectionIfEnabled<
        IsTraceableInCollectionTrait<typename Traits::KeyTraits>::value,
        Traits::KeyTraits::weakHandlingFlag, strongify, Key,
        typename Traits::KeyTraits>::trace(visitor, self.key);
    if (deadWeakObjectsFoundOnKeySide)
      return true;
    return blink::TraceCollectionIfEnabled<
        IsTraceableInCollectionTrait<typename Traits::ValueTraits>::value,
        Traits::ValueTraits::weakHandlingFlag, strongify, Value,
        typename Traits::ValueTraits>::trace(visitor, self.value);
  }
};

// Nodes used by LinkedHashSet.  Again we need two versions to disambiguate the
// template.
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename Value,
          typename Allocator,
          typename Traits>
struct TraceInCollectionTrait<NoWeakHandlingInCollections,
                              strongify,
                              LinkedHashSetNode<Value, Allocator>,
                              Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor,
                    LinkedHashSetNode<Value, Allocator>& self) {
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);
    return TraceInCollectionTrait<
        NoWeakHandlingInCollections, strongify, Value,
        typename Traits::ValueTraits>::trace(visitor, self.m_value);
  }
};

template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename Value,
          typename Allocator,
          typename Traits>
struct TraceInCollectionTrait<WeakHandlingInCollections,
                              strongify,
                              LinkedHashSetNode<Value, Allocator>,
                              Traits> {
  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor,
                    LinkedHashSetNode<Value, Allocator>& self) {
    return TraceInCollectionTrait<
        WeakHandlingInCollections, strongify, Value,
        typename Traits::ValueTraits>::trace(visitor, self.m_value);
  }
};

// ListHashSetNode pointers (a ListHashSet is implemented as a hash table of
// these pointers).
template <ShouldWeakPointersBeMarkedStrongly strongify,
          typename Value,
          size_t inlineCapacity,
          typename Traits>
struct TraceInCollectionTrait<
    NoWeakHandlingInCollections,
    strongify,
    ListHashSetNode<Value,
                    blink::HeapListHashSetAllocator<Value, inlineCapacity>>*,
    Traits> {
  using Node =
      ListHashSetNode<Value,
                      blink::HeapListHashSetAllocator<Value, inlineCapacity>>;

  template <typename VisitorDispatcher>
  static bool trace(VisitorDispatcher visitor, Node* node) {
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);
    traceListHashSetValue(visitor, node->m_value);
    // Just mark the node without tracing because we already traced the
    // contents, and there is no need to trace the next and prev fields
    // since iterating over the hash table backing will find the whole
    // chain.
    visitor->markNoTracing(node);
    return false;
  }
};

}  // namespace WTF

#endif
