#include <iostream>
#include <utility>

#include <string.h>
#include <assert.h>

#include "layout.h"
#include "aligned_alloc.h"

using namespace absl::container_internal;

// 注意：要想没有padding，可以把大类型放在前，小类型放在后；例如：
//       Layout<double, int, short, char>;

int main()
{
  {
    // 自动对齐：
    //
    //     offset:0      offset:4                              offset:16
    //      ^             ^                                     ^
    //      |             |                                     |
    //      +---------+---+------------------------+------------+---------------------
    //      | 3-char  |///|          2-int         |////////////|      4-double  ...
    //      +---------+---+------------------------+------------+---------------------
    //                  ^                             ^
    //                  |                             |
    //                1B-padding                    4B-padding

    using L = Layout<char, int, double>;
    assert(L::Alignment() == 8);

    //打印：Alignment=8
    std::cout << "Alignment=" << L::Alignment() << std::endl;

    L layout(3, 2, 4);
    assert(layout.AllocSize() == 48);

    //打印：AllocSize=48
    std::cout << "AllocSize=" << layout.AllocSize() << std::endl;

    char* p = (char*)aligned_alloc_posix(L::Alignment(), layout.AllocSize());
    assert(reinterpret_cast<uintptr_t>(p) % 8 == 0);

    size_t offset_char   = layout.Offset<0>();
    size_t offset_int    = layout.Offset<1>();
    size_t offset_double = layout.Offset<2>();

    assert(offset_char == 0);
    assert(offset_int == 4);
    assert(offset_double == 16);

    //输出：0 4 16
    std::cout << offset_char << " " << offset_int << " " << offset_double << std::endl;

    char* pchar      = layout.Pointer<char>(p);
    int* pint        = layout.Pointer<int>(p);
    double* pdouble  = layout.Pointer<double>(p);

    assert(reinterpret_cast<uintptr_t>(p) == reinterpret_cast<uintptr_t>(pchar));
    assert(reinterpret_cast<uintptr_t>(pchar) == reinterpret_cast<uintptr_t>(pint)-4);
    assert(reinterpret_cast<uintptr_t>(pchar) == reinterpret_cast<uintptr_t>(pdouble)-16);

    //输出：15978512 15978516 15978528
    std::cout << reinterpret_cast<uintptr_t>(pchar) << " "
      << reinterpret_cast<uintptr_t>(pint)  << " "
      << reinterpret_cast<uintptr_t>(pdouble)
      << std::endl;
  }

  {
    // 手动对齐：
    //
    //     offset:0                offset:32                offset:40
    //      ^                       ^                        ^
    //      |                       |                        |
    //      +---------+-------------+------------------------+---------------------
    //      | 3-char  |//// ... ////|          2-int         |      4-double  ...
    //      +---------+-------------+------------------------+---------------------
    //                  ^                            
    //                  |                            
    //               29B-padding                    

    using L = Layout<char, Aligned<int,32>, double>;
    assert(L::Alignment() == 32);

    //打印：Alignment=32
    std::cout << "Alignment=" << L::Alignment() << std::endl;

    L layout(3, 2, 4);
    assert(layout.AllocSize() == 72);

    //打印：AllocSize=72
    std::cout << "AllocSize=" << layout.AllocSize() << std::endl;

    char* p = (char*)aligned_alloc_posix(L::Alignment(), layout.AllocSize());
    assert(reinterpret_cast<uintptr_t>(p) % 32 == 0);

    size_t offset_char   = layout.Offset<0>();
    size_t offset_int    = layout.Offset<1>();
    size_t offset_double = layout.Offset<2>();

    assert(offset_char == 0);
    assert(offset_int == 32);
    assert(offset_double == 40);

    //输出：0 32 40
    std::cout << offset_char << " " << offset_int << " " << offset_double << std::endl;

    char* pchar      = layout.Pointer<char>(p);
    int* pint        = layout.Pointer<int>(p);
    double* pdouble  = layout.Pointer<double>(p);

    assert(reinterpret_cast<uintptr_t>(p) == reinterpret_cast<uintptr_t>(pchar));
    assert(reinterpret_cast<uintptr_t>(pchar) == reinterpret_cast<uintptr_t>(pint)-32);
    assert(reinterpret_cast<uintptr_t>(pchar) == reinterpret_cast<uintptr_t>(pdouble)-40);

    //输出：9580672 9580704 9580712
    std::cout << reinterpret_cast<uintptr_t>(pchar) << " "
      << reinterpret_cast<uintptr_t>(pint)  << " "
      << reinterpret_cast<uintptr_t>(pdouble)
      << std::endl;
  }

  return 0;
}
