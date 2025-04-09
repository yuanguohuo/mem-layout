#include <iostream>
#include <utility>

#include "layout.h"
#include "aligned_alloc.h"

using namespace absl::container_internal;
using namespace absl::container_internal::internal_layout;

int main()
{
  //实例化模版：OK
  using AlignedInt = Aligned<int, 8>;

  //创建对象：失败
  // error: aggregate ‘AlignedInt aligned_int’ has incomplete type and cannot be defined
  //AlignedInt aligned_int;

  // Yuanguo: 4个数组的size都知道。
  using L1 = LayoutImpl<std::tuple<double, float, int, char>, std::make_index_sequence<4>, std::make_index_sequence<4>>;

  std::cout << L1::NumTypes << std::endl;    //打印：4  (把NumTypes改成public才能运行)
  std::cout << L1::NumSizes << std::endl;    //打印：4  (把NumSizes改成public才能运行)
  std::cout << L1::NumOffsets << std::endl;  //打印：4  (把NumOffsets改成public才能运行)

  L1 layout1(4,3,2,1);
  //    +----------------------+ <------------------ offset 0
  //    |                      |
  //    |  doubles             | 4个double
  //    |                      |
  //    |                      |
  //    +----------------------+ <------------------ offset 32
  //    |                      |
  //    |  floats              | 3个float
  //    |                      |
  //    +----------------------+ <------------------ offset 44
  //    |  ints                | 2个int
  //    |                      |
  //    +----------------------+ <------------------ offset 53
  //    |  chars               | 1个char
  //    +----------------------+
  std::cout << layout1.Offset<double>() << std::endl; //打印：0
  std::cout << layout1.Offset<float>() << std::endl;  //打印：32
  std::cout << layout1.Offset<int>() << std::endl;    //打印：44
  std::cout << layout1.Offset<char>() << std::endl;   //打印：52


  // Yuanguo: 4个数组，其中前2个的size知道。
  // std::make_index_sequence<3>必须为3，否则静态断言失败：
  //       static_assert(NumOffsets == adl_barrier::Min(NumTypes, NumSizes + 1),
  // 因为，知道2个数组的size，就知道3个数组的offset！如下图layout2。
  using L2 = LayoutImpl<std::tuple<double, float, int, char>, std::make_index_sequence<2>, std::make_index_sequence<3>>;

  std::cout << L2::NumTypes << std::endl;    //打印：4  (把NumTypes改成public才能运行)
  std::cout << L2::NumSizes << std::endl;    //打印：2  (把NumSizes改成public才能运行)
  std::cout << L2::NumOffsets << std::endl;  //打印：3  (把NumOffsets改成public才能运行)

  L2 layout2(4,3);
  //    +----------------------+ <------------------ offset 0
  //    |                      |
  //    |  doubles             | 4个double
  //    |                      |
  //    |                      |
  //    +----------------------+ <------------------ offset 32
  //    |                      |
  //    |  floats              | 3个float
  //    |                      |
  //    +----------------------+ <------------------ offset 44
  //    |                      |
  //    |  ints                | 不知道多少个int
  //    |                      |
  //    +----------------------+ <--不知道offset！
  //    |                      |
  //    |  chars               | 不知道多少个char
  //    |                      |
  //    +----------------------+
  std::cout << layout2.Offset<double>() << std::endl;  //打印：0
  std::cout << layout2.Offset<float>() << std::endl;   //打印：32
  std::cout << layout2.Offset<int>() << std::endl;     //打印：44
  // 编译错误：
  //     error: static assertion failed: Index out of bounds
  //     static_assert(N < NumOffsets, "Index out of bounds");
  // std::cout << layout2.Offset<char>() << std::endl;

  std::cout << "+++++++++++++++++++++++++++ Alignment ++++++++++++++++++++++++++++" << std::endl;

  std::cout << L1::ElementAlignment<0>::value << std::endl;  //打印：8  double的alignment
  std::cout << L1::ElementAlignment<1>::value << std::endl;  //打印：4  float的alignment
  std::cout << L1::ElementAlignment<2>::value << std::endl;  //打印：4  int的alignment
  std::cout << L1::ElementAlignment<3>::value << std::endl;  //打印：1  char的alignment
  std::cout << L1::Alignment() << std::endl;                 //打印：8  char,int,float,double中最大的alignment

  std::cout << "+++++++++++++++++++++++++++ Type +++++++++++++++++++++++++++++++++" << std::endl;

  //若无“typename”关键字，谁知道"type"是std::tuple_element<>的类型成员呢，还是静态数据成员呢？
  using T0 = typename std::tuple_element<0, L1::ElementTypes>::type;
  using T1 = typename std::tuple_element<1, L1::ElementTypes>::type;
  using T2 = typename std::tuple_element<2, L1::ElementTypes>::type;
  using T3 = typename std::tuple_element<3, L1::ElementTypes>::type;

  std::cout << typeid(T0).name() << std::endl;  //打印：d   代表 double
  std::cout << typeid(T1).name() << std::endl;  //打印：f   代表 float
  std::cout << typeid(T2).name() << std::endl;  //打印：i   代表 int
  std::cout << typeid(T3).name() << std::endl;  //打印：c   代表 char

  std::cout << typeid(L1::ElementType<0>).name() << std::endl;  //打印：d   代表 double
  std::cout << typeid(L1::ElementType<1>).name() << std::endl;  //打印：f   代表 float
  std::cout << typeid(L1::ElementType<2>).name() << std::endl;  //打印：i   代表 int
  std::cout << typeid(L1::ElementType<3>).name() << std::endl;  //打印：c   代表 char

  std::cout << "+++++++++++++++++++++++++++ Pointer ++++++++++++++++++++++++++++++" << std::endl;
  char* p = (char*)aligned_alloc_posix(sizeof(double),  53);

  double* pdouble = layout1.Pointer<double>(p);
  float*  pfloat  = layout1.Pointer<float>(p);
  int*    pint    = layout1.Pointer<int>(p);
  char*   pchar   = layout1.Pointer<char>(p);

  for (int i=0; i<4; ++i) {
    pdouble[i] = i+1.5;
  }

  for (int i=0; i<3; ++i) {
    pfloat[i] = i+1.8;
  }

  for (int i=0; i<2; ++i) {
    pint[i] = i+1;
  }

  for (int i=0; i<1; ++i) {
    pchar[i] = i+'a';
  }

  auto pointers = layout1.Pointers(p);

  double* pdouble1 = std::get<0>(pointers);
  float*  pfloat1  = std::get<1>(pointers);
  int*    pint1    = std::get<2>(pointers);
  char*   pchar1   = std::get<3>(pointers);

  std::cout << static_cast<void*>(pdouble) << " " << static_cast<void*>(pdouble1) << std::endl; //打印：0x2603010 0x2603010
  std::cout << static_cast<void*>(pfloat)  << " " << static_cast<void*>(pfloat1)  << std::endl; //打印：0x2603030 0x2603030
  std::cout << static_cast<void*>(pint)    << " " << static_cast<void*>(pint1)    << std::endl; //打印：0x260303c 0x260303c
  std::cout << static_cast<void*>(pchar)   << " " << static_cast<void*>(pchar1)   << std::endl; //打印：0x2603044 0x2603044

  std::cout << "+++++++++++++++++++++++++++ Slice ++++++++++++++++++++++++++++++++" << std::endl;

  auto slices = layout1.Slices(p);

  SliceType<double> double_slice = std::get<0>(slices);
  SliceType<float>  float_slice  = std::get<1>(slices);
  SliceType<int>    int_slice    = std::get<2>(slices);
  SliceType<char>   char_slice   = std::get<3>(slices);

  //打印：1.5 2.5 3.5 4.5
  for (auto e : double_slice)  std::cout << e << " ";
  std::cout << std::endl;

  //打印：1.8 2.8 3.8
  for (auto e : float_slice)  std::cout << e << " ";
  std::cout << std::endl;

  //打印：1 2
  for (auto e : int_slice)  std::cout << e << " ";
  std::cout << std::endl;

  //打印：a
  for (auto e : char_slice)  std::cout << e << " ";
  std::cout << std::endl;

  std::cout << "+++++++++++++++++++++++++++ Partial ++++++++++++++++++++++++++++++" << std::endl;

  using L = Layout<char, int, double, float>;

  //等价于：auto partial = L::Partial(3, 6);
  //LayoutImpl<std::tuple<char, int, double, float>, std::index_sequence<0, 1>, std::index_sequence<0, 1, 2>> partial = L::Partial(3, 6);
  auto partial = L::Partial(3, 6);

  return 0;
}
