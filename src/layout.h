// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//                           MOTIVATION AND TUTORIAL
//
// If you want to put in a single heap allocation N doubles followed by M ints,
// it's easy if N and M are known at compile time.
//
//   struct S {
//     double a[N];
//     int b[M];
//   };
//
//   S* p = new S;
//
// But what if N and M are known only in run time? Class template Layout to the
// rescue! It's a portable generalization of the technique known as struct hack.
//
//   // This object will tell us everything we need to know about the memory
//   // layout of double[N] followed by int[M]. It's structurally identical to
//   // size_t[2] that stores N and M. It's very cheap to create.
//   const Layout<double, int> layout(N, M);
//
//   // Allocate enough memory for both arrays. `AllocSize()` tells us how much
//   // memory is needed. We are free to use any allocation function we want as
//   // long as it returns aligned memory.
//   std::unique_ptr<unsigned char[]> p(new unsigned char[layout.AllocSize()]);
//
//   // Obtain the pointer to the array of doubles.
//   // Equivalent to `reinterpret_cast<double*>(p.get())`.
//   //
//   // We could have written layout.Pointer<0>(p) instead. If all the types are
//   // unique you can use either form, but if some types are repeated you must
//   // use the index form.
//   double* a = layout.Pointer<double>(p.get());
//
//   // Obtain the pointer to the array of ints.
//   // Equivalent to `reinterpret_cast<int*>(p.get() + N * 8)`.
//   int* b = layout.Pointer<int>(p);
//
// If we are unable to specify sizes of all fields, we can pass as many sizes as
// we can to `Partial()`. In return, it'll allow us to access the fields whose
// locations and sizes can be computed from the provided information.
// `Partial()` comes in handy when the array sizes are embedded into the
// allocation.
//
//   // size_t[1] containing N, size_t[1] containing M, double[N], int[M].
//   using L = Layout<size_t, size_t, double, int>;
//
//   unsigned char* Allocate(size_t n, size_t m) {
//     const L layout(1, 1, n, m);
//     unsigned char* p = new unsigned char[layout.AllocSize()];
//     *layout.Pointer<0>(p) = n;
//     *layout.Pointer<1>(p) = m;
//     return p;
//   }
//
//   void Use(unsigned char* p) {
//     // First, extract N and M.
//     // Specify that the first array has only one element. Using `prefix` we
//     // can access the first two arrays but not more.
//     constexpr auto prefix = L::Partial(1);
//     size_t n = *prefix.Pointer<0>(p);
//     size_t m = *prefix.Pointer<1>(p);
//
//     // Now we can get pointers to the payload.
//     const L layout(1, 1, n, m);
//     double* a = layout.Pointer<double>(p);
//     int* b = layout.Pointer<int>(p);
//   }
//
// The layout we used above combines fixed-size with dynamically-sized fields.
// This is quite common. Layout is optimized for this use case and generates
// optimal code. All computations that can be performed at compile time are
// indeed performed at compile time.
//
// Efficiency tip: The order of fields matters. In `Layout<T1, ..., TN>` try to
// ensure that `alignof(T1) >= ... >= alignof(TN)`. This way you'll have no
// padding in between arrays.
//
// You can manually override the alignment of an array by wrapping the type in
// `Aligned<T, N>`. `Layout<..., Aligned<T, N>, ...>` has exactly the same API
// and behavior as `Layout<..., T, ...>` except that the first element of the
// array of `T` is aligned to `N` (the rest of the elements follow without
// padding). `N` cannot be less than `alignof(T)`.
//
// `AllocSize()` and `Pointer()` are the most basic methods for dealing with
// memory layouts. Check out the reference or code below to discover more.
//
//                            EXAMPLE
//
//   // Immutable move-only string with sizeof equal to sizeof(void*). The
//   // string size and the characters are kept in the same heap allocation.
//   class CompactString {
//    public:
//     CompactString(const char* s = "") {
//       const size_t size = strlen(s);
//       // size_t[1] followed by char[size + 1].
//       const L layout(1, size + 1);
//       p_.reset(new unsigned char[layout.AllocSize()]);
//       // If running under ASAN, mark the padding bytes, if any, to catch
//       // memory errors.
//       layout.PoisonPadding(p_.get());
//       // Store the size in the allocation.
//       *layout.Pointer<size_t>(p_.get()) = size;
//       // Store the characters in the allocation.
//       memcpy(layout.Pointer<char>(p_.get()), s, size + 1);
//     }
//
//     size_t size() const {
//       // Equivalent to reinterpret_cast<size_t&>(*p).
//       return *L::Partial().Pointer<size_t>(p_.get());
//     }
//
//     const char* c_str() const {
//       // Equivalent to reinterpret_cast<char*>(p.get() + sizeof(size_t)).
//       // The argument in Partial(1) specifies that we have size_t[1] in front
//       // of the characters.
//       return L::Partial(1).Pointer<char>(p_.get());
//     }
//
//    private:
//     // Our heap allocation contains a size_t followed by an array of chars.
//     using L = Layout<size_t, char>;
//     std::unique_ptr<unsigned char[]> p_;
//   };
//
//   int main() {
//     CompactString s = "hello";
//     assert(s.size() == 5);
//     assert(strcmp(s.c_str(), "hello") == 0);
//   }
//
//                               DOCUMENTATION
//
// The interface exported by this file consists of:
// - class `Layout<>` and its public members.
// - The public members of class `internal_layout::LayoutImpl<>`. That class
//   isn't intended to be used directly, and its name and template parameter
//   list are internal implementation details, but the class itself provides
//   most of the functionality in this file. See comments on its members for
//   detailed documentation.
//
// `Layout<T1,... Tn>::Partial(count1,..., countm)` (where `m` <= `n`) returns a
// `LayoutImpl<>` object. `Layout<T1,..., Tn> layout(count1,..., countn)`
// creates a `Layout` object, which exposes the same functionality by inheriting
// from `LayoutImpl<>`.

#ifndef ABSL_CONTAINER_INTERNAL_LAYOUT_H_
#define ABSL_CONTAINER_INTERNAL_LAYOUT_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

#ifdef ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

// for C++20 std::span
#include <boost/beast/core/span.hpp>
#include <fmt/format.h>

#if defined(__GXX_RTTI)
#define ABSL_INTERNAL_HAS_CXA_DEMANGLE
#endif

#ifdef ABSL_INTERNAL_HAS_CXA_DEMANGLE
#include <cxxabi.h>
#endif

namespace absl {
namespace container_internal {

// A type wrapper that instructs `Layout` to use the specific alignment for the
// array. `Layout<..., Aligned<T, N>, ...>` has exactly the same API
// and behavior as `Layout<..., T, ...>` except that the first element of the
// array of `T` is aligned to `N` (the rest of the elements follow without
// padding).
//
// Requires: `N >= alignof(T)` and `N` is a power of 2.
//
// Yuanguo: 类模版没有定义；可以实例化模版类，但不能构造对象；
//          只用于传递类型信息，这足够用了！
//
// 注意：即是不使用Aligned，layout.h实现也会把各个数组对齐到它的元素的size！
//       见test_alignment.cpp
template <class T, size_t N>
struct Aligned;

namespace internal_layout {

// Yuanguo: NotAligned模版及其偏特化，主要是和Type, SizeOf, AlignOf配合使用，限制
//     const Aligned<>的出现
// 为什么不允许const Aligned?

// Yuanguo: NotAligned的主模版
template <class T>
struct NotAligned {};

// Yuanguo：NotAligned的偏特化
//   该特化实现的作用：若用const Aligned<>去实例化它，就静态断言失败！
template <class T, size_t N>
struct NotAligned<const Aligned<T, N>> {
  static_assert(sizeof(T) == 0, "Aligned<T, N> cannot be const-qualified");
};

// Yuanguo: 模版别名；
//   结果类型确定为size_t，有什么作用呢？主要用于展开参数包；见后面对它的使用！
template <size_t>
using IntToSize = size_t;

// Yuanguo: 模版别名；
//   结果类型确定为size_t，有什么作用呢？主要用于展开参数包；见后面对它的使用！
template <class>
using TypeToSize = size_t;

// Yuanguo: Type的参数有3中情况：
//
//   - Type<Aligned<>>       ：匹配Type偏特化，提取Aligned的内部类型；注意：和主模版无关，不继承NotAligned<T>
//   - Type<const Aligned<>> : 匹配Type主模版，继承NotAligned<T>，且T  = const Aligned，进而匹配NotAligned的偏特化，触发静态断言！
//   - Type<其它>            ：匹配Type主模版，继承NotAligned<T>，且T != const Aligned，进而匹配NotAligned的主模版，不触发发静断言！作用也是提取内部类型！

// Yuanguo: Type主模版
template <class T>
struct Type : NotAligned<T> {
  using type = T;
};

// Yuanguo: Type偏特化
template <class T, size_t N>
struct Type<Aligned<T, N>> {
  using type = T;
};

// Yuanguo: SizeOf/AlignOf和NotAligned的关系，类似于Type和NotAligned的关系！

template <class T>
struct SizeOf : NotAligned<T>, std::integral_constant<size_t, sizeof(T)> {};

template <class T, size_t N>
struct SizeOf<Aligned<T, N>> : std::integral_constant<size_t, sizeof(T)> {};

// Note: workaround for https://gcc.gnu.org/PR88115
template <class T>
struct AlignOf : NotAligned<T> {
  static constexpr size_t value = alignof(T);
};

template <class T, size_t N>
struct AlignOf<Aligned<T, N>> {
  static_assert(N % alignof(T) == 0,
                "Custom alignment can't be lower than the type's alignment");
  static constexpr size_t value = N;
};

// Does `Ts...` contain `T`?
template <class T, class... Ts>
using Contains = std::disjunction<std::is_same<T, Ts>...>;

template <class From, class To>
using CopyConst =
    typename std::conditional_t<std::is_const_v<From>, const To, To>;

// Note: We're not qualifying this with absl:: because it doesn't compile under
// MSVC.

// Yuanguo: boost::beast::span
//   类似于golang中的slice，所以这里命名为SliceType；
//   也类似于C++20 的 std::span。
//   它允许高效地传递和操作数据，而无需复制底层存储；
//
//        // 1. 从数组创建 span
//        int arr[] = {1, 2, 3, 4, 5};
//        boost::beast::span<int> array_span(arr, 5); // 指针 + 大小
//        // 输出：array_span[2] = 3
//        std::cout << "array_span[2] = " << array_span[2] << std::endl;
//
//        // 2. 从容器（如 std::vector）创建 span
//        std::vector<double> vec = {3.14, 2.718, 1.618};
//        boost::beast::span<double> vector_span(vec.data(), vec.size());
//        // 输出：3.14 2.718 1.618
//        for (const auto& num : vector_span) {
//            std::cout << num << " ";
//        }
//        std::cout << std::endl; // 输出 3.14 2.718 1.618
//
//        // 3. 获取子视图
//        auto sub_span = array_span.subspan(1, 3); // 从索引1开始，长度3
//        // 输出：2 3 4
//        for (auto num : sub_span) {
//            std::cout << num << " ";
//        }
//
template <class T>
using SliceType = boost::beast::span<T>;

// This namespace contains no types. It prevents functions defined in it from
// being found by ADL.
namespace adl_barrier {

template <class Needle, class... Ts>
constexpr size_t Find(Needle, Needle, Ts...) {
  static_assert(!Contains<Needle, Ts...>(), "Duplicate element type");
  return 0;
}

template <class Needle, class T, class... Ts>
constexpr size_t Find(Needle, T, Ts...) {
  return adl_barrier::Find(Needle(), Ts()...) + 1;
}

constexpr bool IsPow2(size_t n) { return !(n & (n - 1)); }

// Returns `q * m` for the smallest `q` such that `q * m >= n`.
// Requires: `m` is a power of two. It's enforced by IsLegalElementType below.
constexpr size_t Align(size_t n, size_t m) { return (n + m - 1) & ~(m - 1); }

constexpr size_t Min(size_t a, size_t b) { return b < a ? b : a; }

constexpr size_t Max(size_t a) { return a; }

template <class... Ts>
constexpr size_t Max(size_t a, size_t b, Ts... rest) {
  return adl_barrier::Max(b < a ? a : b, rest...);
}

template <class T>
std::string TypeName() {
  std::string out;
  int status = 0;
  char* demangled = nullptr;
#ifdef ABSL_INTERNAL_HAS_CXA_DEMANGLE
  demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
#endif
  if (status == 0 && demangled != nullptr) {  // Demangling succeeded.
    out = fmt::format("<{}>", demangled);
    free(demangled);
  } else {
#if defined(__GXX_RTTI) || defined(_CPPRTTI)
    out = fmt::format("<{}>", typeid(T).name());
#endif
  }
  return out;
}

}  // namespace adl_barrier

template <bool C>
using EnableIf = typename std::enable_if_t<C, int>;

// Can `T` be a template argument of `Layout`?
template <class T>
using IsLegalElementType = std::integral_constant<
    bool, !std::is_reference_v<T> && !std::is_volatile_v<T> &&
              !std::is_reference_v<typename Type<T>::type> &&
              !std::is_volatile_v<typename Type<T>::type> &&
              adl_barrier::IsPow2(AlignOf<T>::value)>;

template <class Elements, class SizeSeq, class OffsetSeq>
class LayoutImpl;

/*
 * Yuanguo: 一个object包含N个字段，可以看做包含N个数组：若某个字段不是数组，可以看做长度（元素个数）为1的数组！
 *
 * int main()
 * {
 *   // 4个数组的长度（元素个数）都知道。
 *   using L1 = LayoutImpl<std::tuple<double, float, int, char>, std::make_index_sequence<4>, std::make_index_sequence<4>>;
 *
 *   std::cout << L1::NumTypes << std::endl;    //打印：4  (把NumTypes改成public才能运行)
 *   std::cout << L1::NumSizes << std::endl;    //打印：4  (把NumSizes改成public才能运行)
 *   std::cout << L1::NumOffsets << std::endl;  //打印：4  (把NumOffsets改成public才能运行)
 *
 *   L1 layout1(4,3,2,1);
 *   //    +----------------------+ <------------------ offset 0
 *   //    |                      |
 *   //    |  doubles             | 4个double
 *   //    |                      |
 *   //    |                      |
 *   //    +----------------------+ <------------------ offset 32
 *   //    |                      |
 *   //    |  floats              | 3个float
 *   //    |                      |
 *   //    +----------------------+ <------------------ offset 44
 *   //    |  ints                | 2个int
 *   //    |                      |
 *   //    +----------------------+ <------------------ offset 53
 *   //    |  chars               | 1个char
 *   //    +----------------------+
 *   std::cout << layout1.Offset<double>() << std::endl; //打印：0
 *   std::cout << layout1.Offset<float>() << std::endl;  //打印：32
 *   std::cout << layout1.Offset<int>() << std::endl;    //打印：44
 *   std::cout << layout1.Offset<char>() << std::endl;   //打印：52
 *
 *   // 4个数组，其中前2个数组的长度（元素个数）已知，后2个未知！
 *   // std::make_index_sequence<3>必须为3，否则静态断言失败：
 *   //       static_assert(NumOffsets == adl_barrier::Min(NumTypes, NumSizes + 1),
 *   // 因为，知道2个数组的长度（元素个数），就知道第3个数组的offset！如下图layout2。
 *   using L2 = LayoutImpl<std::tuple<double, float, int, char>, std::make_index_sequence<2>, std::make_index_sequence<3>>;
 *
 *   std::cout << L2::NumTypes << std::endl;    //打印：4  (把NumTypes改成public才能运行)
 *   std::cout << L2::NumSizes << std::endl;    //打印：2  (把NumSizes改成public才能运行)
 *   std::cout << L2::NumOffsets << std::endl;  //打印：3  (把NumOffsets改成public才能运行)
 *
 *   L2 layout2(4,3);
 *   //    +----------------------+ <------------------ offset 0
 *   //    |                      |
 *   //    |  doubles             | 4个double
 *   //    |                      |
 *   //    |                      |
 *   //    +----------------------+ <------------------ offset 32
 *   //    |                      |
 *   //    |  floats              | 3个float
 *   //    |                      |
 *   //    +----------------------+ <------------------ offset 44
 *   //    |                      |
 *   //    |  ints                | 不知道多少个int
 *   //    |                      |
 *   //    +----------------------+ <--不知道offset！
 *   //    |                      |
 *   //    |  chars               | 不知道多少个char
 *   //    |                      |
 *   //    +----------------------+
 *   std::cout << layout2.Offset<double>() << std::endl;  //打印：0
 *   std::cout << layout2.Offset<float>() << std::endl;   //打印：32
 *   std::cout << layout2.Offset<int>() << std::endl;     //打印：44
 *
 *   // 编译错误：
 *   //     error: static assertion failed: Index out of bounds
 *   //     static_assert(N < NumOffsets, "Index out of bounds");
 *   // std::cout << layout2.Offset<char>() << std::endl;
 *
 *   return 0;
 * }
 *
 * 从上面的逻辑可以看到：NumOffsets = min(NumTypes, NumSizes+1)
 *   - 当所有数组长度（元素个数）已知：NumOffsets = NumTypes
 *   - 当部分数组长度（元素个数）已知：NumOffsets = NumSizes+1
 *
 * 类实例L1和L2有哪些信息：
 *   - 各个数组的类型；
 *   - 几个数组的长度是已知的；注意，只知道**几个**，并不知道它们的长度（L1/L2的对象才知道）!
 *
 * L1/L2的对象有哪些信息：
 *   - 已知长度的数组的实际长度；
 *
 * 有了已知长度的数组的实际长度，就可以算出各个数组的offset（前n个）!
 *
 * 注意：
 *   - 第3个形参其实是冗余的：要么等于第一个的长度，要么等于第2个加1；是否可以简化？
 *   - std::make_index_sequence()生成的2个序列有什么用呢？其实需要的是序列的长度；为什么不直接写成这样？
 *
 *         //主模版
 *         template <class Elements, size_t NumSizes, size_t NumOffsets>
 *         class LayoutImpl;
 *
 *         //偏特化
 *         template <class... Elements, size_t NumSizes, size_t NumOffsets>
 *         class LayoutImpl<std::tuple<Elements...>, NumSizes, NumOffsets> { ... };
 *
 * 不对！这2个序列在不同的地方会被展开使用！
 *
 * SizeSeq序列（{0, 1, 2, ...}）在这些地方被展开为0, 1, 2, ...
 *   - LayoutImpl构造函数；
 *   - 函数Sizes() const {}
 *   - 函数Slices(Char* p) const {}
 *
 * OffsetSeq序列（{0, 1, 2, ...}）在这些地方被展开为0, 1, 2, ...
 *   - 函数Offsets() const {}
 *   - 函数Pointers(Char* p) const {}
 *   - 函数DebugString() const {}
 **/

// Public base class of `Layout` and the result type of `Layout::Partial()`.
//
// `Elements...` contains all template arguments of `Layout` that created this
// instance.
//
// `SizeSeq...` is `[0, NumSizes)` where `NumSizes` is the number of arguments
// passed to `Layout::Partial()` or `Layout::Layout()`.
//
// `OffsetSeq...` is `[0, NumOffsets)` where `NumOffsets` is
// `Min(sizeof...(Elements), NumSizes + 1)` (the number of arrays for which we
// can compute offsets).
template <class... Elements, size_t... SizeSeq, size_t... OffsetSeq>
class LayoutImpl<std::tuple<Elements...>, std::index_sequence<SizeSeq...>,
                 std::index_sequence<OffsetSeq...>> {
 //Yuanguo: 为了测试test_basic.cpp，把private改成public (只需改此1处)
 //private:
 public:
  static_assert(sizeof...(Elements) > 0, "At least one field is required");
  static_assert(std::conjunction_v<IsLegalElementType<Elements>...>,
                "Invalid element type (see IsLegalElementType)");

  enum {
    NumTypes = sizeof...(Elements),
    NumSizes = sizeof...(SizeSeq),
    NumOffsets = sizeof...(OffsetSeq),
  };

  // These are guaranteed by `Layout`.
  //
  // Yuanguo: 若已知前2个数组的长度（元素个数），那么已知offset的个数必然为3 ！
  static_assert(NumOffsets == adl_barrier::Min(NumTypes, NumSizes + 1),
                "Internal error");
  static_assert(NumTypes > 0, "Internal error");

  // Returns the index of `T` in `Elements...`. Results in a compilation error
  // if `Elements...` doesn't contain exactly one instance of `T`.
  template <class T>
  static constexpr size_t ElementIndex() {
    static_assert(Contains<Type<T>, Type<typename Type<Elements>::type>...>(),
                  "Type not found");
    return adl_barrier::Find(Type<T>(),
                             Type<typename Type<Elements>::type>()...);
  }

  // Yuanguo: 获取第N个数组的元素类型的alignment；可以这样使用(需要把ElementAlignment改成public)；可这样使用：
  //          std::cout << L1::ElementAlignment<3>::value << std::endl;
  template <size_t N>
  using ElementAlignment =
      AlignOf<typename std::tuple_element<N, std::tuple<Elements...>>::type>;

 public:
  // Element types of all arrays packed in a tuple.
  using ElementTypes = std::tuple<typename Type<Elements>::type...>;

  // Element type of the Nth array.
  //
  // Yuanguo: 获取第N个数组的元素的类型，可这样使用：
  //          std::cout << typeid(L1::ElementType<3>).name() << std::endl;
  template <size_t N>
  using ElementType = typename std::tuple_element<N, ElementTypes>::type;

  //Yuanguo: 这个语法比较难理解；
  //  IntToSize<SizeSeq>... 对SizeSeq展开为：IntToSize<0>, IntToSize<1>, ...即size_t, size_t, ...
  //  sizes是一个序列，例如：4, 3, 2
  constexpr explicit LayoutImpl(IntToSize<SizeSeq>... sizes)
      : size_{sizes...} {}

  // Alignment of the layout, equal to the strictest alignment of all elements.
  // All pointers passed to the methods of layout must be aligned to this value.
  //
  // Yuanguo: 返回最大aligment；
  //          例如包含4个数组，double[], float[], int[], char[]，则返回8；
  static constexpr size_t Alignment() {
    return adl_barrier::Max(AlignOf<Elements>::value...);
  }

  /*
   * Yuanguo: 获取第N个数组的offset；
   *
   * template <size_t N, EnableIf<N == 0> = 0> constexpr size_t Offset() const { return 0; }
   * template <size_t N, EnableIf<N != 0> = 0> constexpr size_t Offset() const { return adl_barrier::Align(...); }
   *
   * 这2个模版函数是使用SFINAE机制来控制函数模板的重载。拆开来看：
   *
   * 当N==0时，下面的那个模版替换失败（因为std::enable_if<false>没有type），被SFINAE机制静默丢弃（不会导致编译错误）
   * 当N!=0时，上面的那个模版替换失败（因为std::enable_if<false>没有type），被SFINAE机制静默丢弃（不会导致编译错误）
   *
   * 所以，最终替换产生的4个函数，2个被丢弃(std::enable_if<false, int>根本没有type，所以替换失败)：
   *
   * template <size_t N, std::enable_if<false, int>::type = 0> constexpr size_t Offset() const { return adl_barrier::Align(...); }
   * template <size_t N, std::enable_if<false, int>::type = 0> constexpr size_t Offset() const { return 0; }
   *
   * 2个被保留：
   *
   * template <size_t N, int = 0> constexpr size_t Offset() const { return 0; }
   * template <size_t N, int = 0> constexpr size_t Offset() const { return adl_barrier::Align(...); }
   *
   *
   * 注意：int = 0 是省略了模版变量名，并且提供默认值。之所以这样，是因为int参数（及其值）对函数没有意义，它的作用
   * 就在于上面说的SFINAE机制！
   *
   * 这也导致可读性很差。从C++17起，可以利用if constexpr替换成如下实现（可读性更好）：
   *
   *     template <size_t N>
   *     constexpr size_t Offset() const {
   *       if constexpr(N == 0) {
   *         return 0;
   *       } else {
   *         static_assert(N < NumOffsets, "Index out of bounds");
   *         return adl_barrier::Align(
   *             Offset<N - 1>() + SizeOf<ElementType<N - 1>>() * size_[N - 1],
   *             ElementAlignment<N>::value);
   *       }
   *     }
   **/

  // Offset in bytes of the Nth array.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   assert(x.Offset<0>() == 0);   // The ints starts from 0.
  //   assert(x.Offset<1>() == 16);  // The doubles starts from 16.
  //
  // Requires: `N <= NumSizes && N < sizeof...(Ts)`.
  template <size_t N, EnableIf<N == 0> = 0>
  constexpr size_t Offset() const {
    return 0;
  }

  template <size_t N, EnableIf<N != 0> = 0>
  constexpr size_t Offset() const {
    static_assert(N < NumOffsets, "Index out of bounds");
    return adl_barrier::Align(
        Offset<N - 1>() + SizeOf<ElementType<N - 1>>() * size_[N - 1],
        ElementAlignment<N>::value);
  }

  // Offset in bytes of the array with the specified element type. There must
  // be exactly one such array and its zero-based index must be at most
  // `NumSizes`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   assert(x.Offset<int>() == 0);      // The ints starts from 0.
  //   assert(x.Offset<double>() == 16);  // The doubles starts from 16.
  //
  // Yuanguo: 获取类型为T的数组的offset； 要求：
  //          - 必须有且只有1个数组是给定类型！
  //          - 它的offset**可计算** （例如前2个数组的长度已知，则前3个数组的offset可计算）！
  template <class T>
  constexpr size_t Offset() const {
    return Offset<ElementIndex<T>()>();
  }

  // Offsets in bytes of all arrays for which the offsets are known.
  //
  // Yuanguo: 获取所有**可计算**的offsets!（例如前2个数组的长度已知，则前3个数组的offset可计算）
  //          就是展开参数包，对每一个调用上面的函数（注意是编译期）
  constexpr std::array<size_t, NumOffsets> Offsets() const {
    return {{Offset<OffsetSeq>()...}};
  }

  // The number of elements in the Nth array. This is the Nth argument of
  // `Layout::Partial()` or `Layout::Layout()` (zero-based).
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   assert(x.Size<0>() == 3);
  //   assert(x.Size<1>() == 4);
  //
  // Requires: `N < NumSizes`.
  //
  // Yuanguo: 获取第N个数组的长度（元素个数）!
  template <size_t N>
  constexpr size_t Size() const {
    static_assert(N < NumSizes, "Index out of bounds");
    return size_[N];
  }

  // The number of elements in the array with the specified element type.
  // There must be exactly one such array and its zero-based index must be
  // at most `NumSizes`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   assert(x.Size<int>() == 3);
  //   assert(x.Size<double>() == 4);
  //
  // Yuanguo: 获取类型为T的数组的长度（元素个数）；要求：
  //          - 必须有且只有1个数组是给定类型！
  //          - 它的长度是已知的！
  template <class T>
  constexpr size_t Size() const {
    return Size<ElementIndex<T>()>();
  }

  // The number of elements of all arrays for which they are known.
  //
  // Yuanguo: 获取所有已知长度的数组的长度（元素个数）；
  //          就是展开参数包，对每一个调用上面的函数（注意是编译期）！
  constexpr std::array<size_t, NumSizes> Sizes() const {
    return {{Size<SizeSeq>()...}};
  }

  // Pointer to the beginning of the Nth array.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //   int* ints = x.Pointer<0>(p);
  //   double* doubles = x.Pointer<1>(p);
  //
  // Requires: `N <= NumSizes && N < sizeof...(Ts)`.
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Yuanguo: 要构造一个对象包含int[3]数组和double[4]数组，布局是：int[3], 4 bytes of padding, double[4]，过程是这样的：
  //   - 构造layout：
  //         using L1 = LayoutImpl<std::tuple<int, double>, std::make_index_sequence<2>, std::make_index_sequence<2>>;
  //         L1 x(3,4)；
  //   - 对象空间（可能如下分配，也可能从文件读取的内容等等；可能是非const的，也可能是const的）：
  //         unsigned char* p = new unsigned char[x.AllocSize()];
  //   - int[3]和double[4]数组的起始地址分别是：
  //         int* ints = x.Pointer<0>(p);
  //         double* doubles = x.Pointer<1>(p);
  //   - 读写ints和doubles；
  //
  // 所以，Pointer的入参p是一个[const] [signed|unsigned] char类型的指针！
  //
  // 具体实现：
  //   - CopyConst<...>保证：当入参p是const指针，返回的也是const指针；当入参p是非const指针，返回的也是非const指针；
  //                         并且，类型是第N个数组的元素的类型的指针！
  //   - using C = ...：去掉const修饰；
  //   - static_assert保证：把p的const修饰去掉后，必须是char, unsigned char或者signed char！
  //   - assert保证：入参p要对齐到**最大类型**的size；例如int和double的最大类型是double，size是8，即要对齐到8的倍数；
  //                 这样无论各个数组的顺序如何，第一个数组的offset一定为0！
  //   - return：第N个数组的起始地址；
  template <size_t N, class Char>
  CopyConst<Char, ElementType<N>>* Pointer(Char* p) const {
    using C = typename std::remove_const<Char>::type;
    static_assert(
        std::is_same<C, char>() || std::is_same<C, unsigned char>() ||
            std::is_same<C, signed char>(),
        "The argument must be a pointer to [const] [signed|unsigned] char");
    constexpr size_t alignment = Alignment();
    (void)alignment;
    assert(reinterpret_cast<uintptr_t>(p) % alignment == 0);
    return reinterpret_cast<CopyConst<Char, ElementType<N>>*>(p + Offset<N>());
  }

  // Pointer to the beginning of the array with the specified element type.
  // There must be exactly one such array and its zero-based index must be at
  // most `NumSizes`.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //   int* ints = x.Pointer<int>(p);
  //   double* doubles = x.Pointer<double>(p);
  //
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Yuanguo: 获取类型为T的数组的起始地址； 要求：
  //          - 必须有且只有1个数组是给定类型！
  //          - 它的offset**可计算**！（例如前2个数组的长度已知，则前3个数组的offset可计算）
  // 其实是调用上一个实现；ElementIndex<T>()在**编译时**获取类型T的index；
  template <class T, class Char>
  CopyConst<Char, T>* Pointer(Char* p) const {
    return Pointer<ElementIndex<T>()>(p);
  }

  // Pointers to all arrays for which pointers are known.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //
  //   int* ints;
  //   double* doubles;
  //   std::tie(ints, doubles) = x.Pointers(p);
  //
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Note: We're not using ElementType alias here because it does not compile
  // under MSVC.
  //
  // Yuanguo: 假设包含3个数组，类型分别是T0, T1, T2；且假设p没有const修饰！
  //
  // 返回类型拆解：
  //   - 先看typename std::tuple_element<OffsetSeq, ElementTypes>::type
  //       - 对OffsetSeq展开，得到std::tuple_element<i, ElementTypes>::type,  i=0, 1, 2
  //       - std::tuple_element<i, ElementTypes>::type就是第i个数组的元素的类型；
  //       - 所以结果是 T0, T1, T2
  //   - CopyConst<Char, T0, T1, T2>*...
  //       - 结果是：[const] T0*, [const] T1*, [const] T2*
  //       - 是否有const取决于Char* p；假设p没有const修饰；那么最终结果是：T0*, T1*, T2*
  //   - 外层std::tuple<>：实例化类模版，得到std::tuple<T0*, T1*, T2*>
  //   - 所以，返回类型是：std::tuple<T0*, T1*, T2*>
  //
  // 函数实现拆解：
  //   - std::tuple<CopyConst<Char, ElementType<OffsetSeq>>*...>：和返回类型等价。
  //     这样写更简洁，返回类型也可以写成这样!
  //   - Pointer<OffsetSeq>(p)...：对OffsetSeq展开，得到Pointer<0>(p), Pointer<1>(p), Pointer<2>(p)；
  //     调用前面的Pointer<size_t N, class Char>函数（注意：Char模版参数省略，由编译器自动推导）
  template <class Char>
  std::tuple<CopyConst<
      Char, typename std::tuple_element<OffsetSeq, ElementTypes>::type>*...>
  Pointers(Char* p) const {
    return std::tuple<CopyConst<Char, ElementType<OffsetSeq>>*...>(
        Pointer<OffsetSeq>(p)...);
  }

  // The Nth array.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //   Span<int> ints = x.Slice<0>(p);
  //   Span<double> doubles = x.Slice<1>(p);
  //
  // Requires: `N < NumSizes`.
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Yuanguo: 返回第N个数组的slice（整个数组）；假设第N个数组类型是Tn，且p有const修饰！
  //
  // 返回类型拆解：
  //   - ElementType<N>是Tn；
  //   - CopyConst<Char, ElementType<N>>是const Tn；
  //   - 返回类型是SliceType<const Tn>
  // 函数实现拆解：
  //   - SliceType<>和返回类型一样，所以简化为：SliceType<const Tn>(Pointer<N>(p), Size<N>);
  //   - 两个参数分别是：
  //       - Pointer<N>(p) 第N个数组的起始地址 ；
  //       - Size<N>: 第N个数组的长度（元素个数）；
  //   - 所以，就是构造第N个数组的slice（整个数组）
  template <size_t N, class Char>
  SliceType<CopyConst<Char, ElementType<N>>> Slice(Char* p) const {
    return SliceType<CopyConst<Char, ElementType<N>>>(Pointer<N>(p), Size<N>());
  }

  // The array with the specified element type. There must be exactly one
  // such array and its zero-based index must be less than `NumSizes`.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //   Span<int> ints = x.Slice<int>(p);
  //   Span<double> doubles = x.Slice<double>(p);
  //
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Yuanguo: 获取类型为T的数组的slice（整个数组）； 要求：
  //          - 必须有且只有1个数组是给定类型！
  //          - 它的size（元素个数）必须已知！
  // 其实是调用上一个实现；ElementIndex<T>()在**编译时**获取类型T的index；
  template <class T, class Char>
  SliceType<CopyConst<Char, T>> Slice(Char* p) const {
    return Slice<ElementIndex<T>()>(p);
  }

  // All arrays with known sizes.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];
  //
  //   Span<int> ints;
  //   Span<double> doubles;
  //   std::tie(ints, doubles) = x.Slices(p);
  //
  // Requires: `p` is aligned to `Alignment()`.
  //
  // Note: We're not using ElementType alias here because it does not compile
  // under MSVC.
  //
  // Yuanguo: 获取所有已知长度的数组。和前面Pointers函数类似。
  //
  // 假设包含5个数组，前3个的长度（元素个数）已知，且类型分别是T0, T1, T2；
  // 假设p有const修饰；
  //
  // 返回类型拆解：
  //   - typename std::tuple_element<SizeSeq, ElementTypes>::type：对SizeSeq展开，得到std::tuple_element<i, ElementTypes>::type, i=0,1,2；即：
  //     T0, T1, T2
  //   - CopyConst<Char, T0, T1, T2>：得到const T0, const T1, const T2；假设p有const修饰；
  //   - SliceType<...>：得到SliceType<const T0>, SliceType<const T1>, SliceType<const T2>
  //   - 所以，返回类型是：std::tuple<SliceType<const T0>, SliceType<const T1>, SliceType<const T2>>
  //
  // 函数实现拆解：
  //   - std::tuple<SliceType<CopyConst<Char, ElementType<SizeSeq>>>...>：和返回类型等价。
  //     这样写更简洁，返回类型也可以写成这样!
  //   - Slice<SizeSeq>(p)...：对SizeSeq展开，得到Slice<0>(p), Slice<1>(p), Slice<2>(p)；
  //     调用前面的Slice<size_t N, class Char>函数（注意：Char模版参数省略，由编译器自动推导）
  template <class Char>
  std::tuple<SliceType<CopyConst<
      Char, typename std::tuple_element<SizeSeq, ElementTypes>::type>>...>
  Slices(Char* p) const {
    // Workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63875 (fixed
    // in 6.1).
    (void)p;
    return std::tuple<SliceType<CopyConst<Char, ElementType<SizeSeq>>>...>(
        Slice<SizeSeq>(p)...);
  }

  // The size of the allocation that fits all arrays.
  //
  //   // int[3], 4 bytes of padding, double[4].
  //   Layout<int, double> x(3, 4);
  //   unsigned char* p = new unsigned char[x.AllocSize()];  // 48 bytes
  //
  // Requires: `NumSizes == sizeof...(Ts)`.
  //
  // Yuanguo: 计算所有数组占用的空间之和！
  //   要求：所有数组的长度（元素个数）已知！
  //   注意：已经考虑了对齐问题，即各个数组对齐到元素类型的size!
  //
  //   实现：最后一个数组的offset + 最后一个数组元素大小 * 长度（元素个数）
  constexpr size_t AllocSize() const {
    static_assert(NumTypes == NumSizes, "You must specify sizes of all fields");
    return Offset<NumTypes - 1>() +
           SizeOf<ElementType<NumTypes - 1>>() * size_[NumTypes - 1];
  }

  //Yuanguo: PoisonPadding是利用AddressSanitizer（ASAN）来标记填充区域（padding），
  //  旨在检测非法内存访问！
  //
  // 实现上，是编译期递归；
  //   第一个PoisonPadding()是递归出口（N=0才替换成功；N>0替换失败，根据SFINAE原则被移除）！
  //   第二个PoisonPadding() （N>0才替换成功；N=0替换失败，根据SFINAE原则被移除）:
  //     - 编译期递归调用PoisonPadding<Char, N-1>
  //     - 标记内存：从第N-1个数组的结尾，到第N个数组的开始

  // If built with --config=asan, poisons padding bytes (if any) in the
  // allocation. The pointer must point to a memory block at least
  // `AllocSize()` bytes in length.
  //
  // `Char` must be `[const] [signed|unsigned] char`.
  //
  // Requires: `p` is aligned to `Alignment()`.
  template <class Char, size_t N = NumOffsets - 1, EnableIf<N == 0> = 0>
  void PoisonPadding(const Char* p) const {
    Pointer<0>(p);  // verify the requirements on `Char` and `p`
  }

  template <class Char, size_t N = NumOffsets - 1, EnableIf<N != 0> = 0>
  void PoisonPadding(const Char* p) const {
    static_assert(N < NumOffsets, "Index out of bounds");
    (void)p;
#ifdef ADDRESS_SANITIZER
    PoisonPadding<Char, N - 1>(p);
    // The `if` is an optimization. It doesn't affect the observable behaviour.
    if (ElementAlignment<N - 1>::value % ElementAlignment<N>::value) {
      size_t start =
          Offset<N - 1>() + SizeOf<ElementType<N - 1>>() * size_[N - 1];
      ASAN_POISON_MEMORY_REGION(p + start, Offset<N>() - start);
    }
#endif
  }

  // Human-readable description of the memory layout. Useful for debugging.
  // Slow.
  //
  //   // char[5], 3 bytes of padding, int[3], 4 bytes of padding, followed
  //   // by an unknown number of doubles.
  //   auto x = Layout<char, int, double>::Partial(5, 3);
  //   assert(x.DebugString() ==
  //          "@0<char>(1)[5]; @8<int>(4)[3]; @24<double>(8)");
  //
  // Each field is in the following format: @offset<type>(sizeof)[size] (<type>
  // may be missing depending on the target platform). For example,
  // @8<int>(4)[3] means that at offset 8 we have an array of ints, where each
  // int is 4 bytes, and we have 3 of those ints. The size of the last field may
  // be missing (as in the example above). Only fields with known offsets are
  // described. Type names may differ across platforms: one compiler might
  // produce "unsigned*" where another produces "unsigned int *".
  std::string DebugString() const {
    const auto offsets = Offsets();
    const size_t sizes[] = {SizeOf<ElementType<OffsetSeq>>()...};
    const std::string types[] = {
        adl_barrier::TypeName<ElementType<OffsetSeq>>()...};
    std::string res = fmt::format("@0{}({})", types[0], sizes[0]);
    for (size_t i = 0; i != NumOffsets - 1; ++i) {
      res += fmt::format("[{}]; @({})", size_[i], offsets[i + 1], types[i + 1], sizes[i + 1]);
    }
    // NumSizes is a constant that may be zero. Some compilers cannot see that
    // inside the if statement "size_[NumSizes - 1]" must be valid.
    int last = static_cast<int>(NumSizes) - 1;
    if (NumTypes == NumSizes && last >= 0) {
      res += fmt::format("[{}]", size_[last]);
    }
    return res;
  }

 private:
  // Arguments of `Layout::Partial()` or `Layout::Layout()`.
  size_t size_[NumSizes > 0 ? NumSizes : 1];
};

// Yuanguo: 模板别名：
//   注意1：模版别名也可以有模版参数（本例中的NumSizes, Ts）
//   注意2：NumSizes不一定等于sizeof...(Ts)
//          - NumSizes == sizeof...(Ts) ：所有数组长度（元素个数）已知。例如：
//            LayoutType<4, double,float,int,char> = LayoutImpl<tuple<double,float,int,char>, index_sequence<0,1,2,3>, index_sequence<0,1,2,3>>;
//          - NumSizes <  sizeof...(Ts) ：部分数组长度（元素个数）已知。例如：
//            LayoutType<2, double,float,int,char> = LayoutImpl<tuple<double,float,int,char>, index_sequence<0,1>, index_sequence<0,1,2>>;
//            即：前2个数组长度已知，则前3个数组的offset就已知！
template <size_t NumSizes, class... Ts>
using LayoutType = LayoutImpl<
    std::tuple<Ts...>, std::make_index_sequence<NumSizes>,
    std::make_index_sequence<adl_barrier::Min(sizeof...(Ts), NumSizes + 1)>>;

}  // namespace internal_layout

// Descriptor of arrays of various types and sizes laid out in memory one after
// another. See the top of the file for documentation.
//
// Check out the public API of internal_layout::LayoutImpl above. The type is
// internal to the library but its methods are public, and they are inherited
// by `Layout`.
template <class... Ts>
class Layout : public internal_layout::LayoutType<sizeof...(Ts), Ts...> {
 public:
  static_assert(sizeof...(Ts) > 0, "At least one field is required");
  static_assert(
      std::conjunction_v<internal_layout::IsLegalElementType<Ts>...>,
      "Invalid element type (see IsLegalElementType)");

  // The result type of `Partial()` with `NumSizes` arguments.
  template <size_t NumSizes>
  using PartialType = internal_layout::LayoutType<NumSizes, Ts...>;

  // `Layout` knows the element types of the arrays we want to lay out in
  // memory but not the number of elements in each array.
  // `Partial(size1, ..., sizeN)` allows us to specify the latter. The
  // resulting immutable object can be used to obtain pointers to the
  // individual arrays.
  //
  // It's allowed to pass fewer array sizes than the number of arrays. E.g.,
  // if all you need is to the offset of the second array, you only need to
  // pass one argument -- the number of elements in the first array.
  //
  //   // int[3] followed by 4 bytes of padding and an unknown number of
  //   // doubles.
  //   auto x = Layout<int, double>::Partial(3);
  //   // doubles start at byte 16.
  //   assert(x.Offset<1>() == 16);
  //
  // If you know the number of elements in all arrays, you can still call
  // `Partial()` but it's more convenient to use the constructor of `Layout`.
  //
  //   Layout<int, double> x(3, 5);
  //
  // Note: The sizes of the arrays must be specified in number of elements,
  // not in bytes.
  //
  // Requires: `sizeof...(Sizes) <= sizeof...(Ts)`.
  // Requires: all arguments are convertible to `size_t`.
  //
  // Yuanguo:
  //
  //   假设有4个数组:
  //       char[3], 1B-padding, int[6], 4B-padding, double[], float[]；
  //
  //   其中2个数组长度已知（char[3]和int[6]），另2个未知（double[]和float[]）；
  //   这种情况下，无法构造Layout实例的对象：
  //
  //         //实例化Layout类模板，成功！
  //         using L = Layout<char, int, double, float>;
  //
  //         //编译失败！
  //         L x(3, 6);
  //
  //   为什么失败呢？
  //
  //   - 首先，L的基类是：
  //         LayoutImpl<std::tuple<char, int, double, float>, std::index_sequence<0, 1, 2, 3>, std::index_sequence<0, 1, 2, 3>>;
  //     这表明4个数组的长度都应该已知！
  //
  //   - 其次，看Layout的构造函数：Layout(internal_layout::TypeToSize<Ts>... sizes)
  //     它展开是：Layout(TypeToSize<char, int, double, float>... sizes) {...}
  //               Layout(size_t, size_t, size_t, size_t) {...}
  //     是要有4个参数才行（还是要求所有数组长度已知）！
  //
  //         //编译成功
  //         L x(3, 6, 5, 5);
  //
  //   也就是说，要想使用Layout，必须所有数组长度都是已知的。显然很不方便！！！Partial就是用来解决这个问题:
  //   不要调用L的构造函数，而调用其Partial()静态函数：
  //
  //         //编译成功
  //         auto x = L::Partial(3, 6);
  //
  //   这构造一个PartialType的对象（实际是LayoutImpl的对象）；真实类型是：
  //
  //         PartialType<NumSizes=2>                     =>
  //         LayoutType<2, char, int, double, float>     =>
  //         LayoutImpl<std::tuple<char, int, double, float>, std::index_sequence<0, 1>, std::index_sequence<0, 1, 2>>
  //
  //   不嫌麻烦的化，写成这样也可以：
  //
  //         //编译成功
  //         LayoutImpl<std::tuple<char, int, double, float>, std::index_sequence<0, 1>, std::index_sequence<0, 1, 2>> x = L::Partial(3, 6);
  //
  //   这就是Partial()的作用：**可以把它看作另一个构造函数**！！！
  //
  //   有了它，可以实现类似“序列化- 反序列化"的功能，见src/serialize.cpp
  template <class... Sizes>
  static constexpr PartialType<sizeof...(Sizes)> Partial(Sizes&&... sizes) {
    static_assert(sizeof...(Sizes) <= sizeof...(Ts));
    return PartialType<sizeof...(Sizes)>(std::forward<Sizes>(sizes)...);
  }

  // Creates a layout with the sizes of all arrays specified. If you know
  // only the sizes of the first N arrays (where N can be zero), you can use
  // `Partial()` defined above. The constructor is essentially equivalent to
  // calling `Partial()` and passing in all array sizes; the constructor is
  // provided as a convenient abbreviation.
  //
  // Note: The sizes of the arrays must be specified in number of elements,
  // not in bytes.
  constexpr explicit Layout(internal_layout::TypeToSize<Ts>... sizes)
      : internal_layout::LayoutType<sizeof...(Ts), Ts...>(sizes...) {}
};

}  // namespace container_internal
}  // namespace absl

#endif  // ABSL_CONTAINER_INTERNAL_LAYOUT_H_
