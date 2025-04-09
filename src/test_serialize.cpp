#include <iostream>
#include <utility>

#include <string.h>
#include <assert.h>

#include "layout.h"
#include "aligned_alloc.h"

using namespace absl::container_internal;

struct MyCompactFoo
{
  using L = Layout<size_t, size_t, float, double>;

  size_t*  num_floats; //固定长度为1，存储floats数组的长度；
  size_t*  num_doubles;//固定长度为1，存储doubles数组的长度；
  float*   floats;
  double*  doubles;
};

unsigned char* create(float* floats, size_t num_floats, double* doubles, size_t num_doubles)
{
  //此时4个数组的长度都已知；
  const MyCompactFoo::L layout(1, 1, num_floats, num_doubles);
 
  //要求：
  //   对齐到最大元素的alignment；
  //   所有数组长度已知；
  unsigned char* p = (unsigned char*)aligned_alloc_posix(MyCompactFoo::L::Alignment(), layout.AllocSize()); 

  *layout.Pointer<0>(p) = num_floats;
  *layout.Pointer<1>(p) = num_doubles;

  memcpy((void*)layout.Pointer<float>(p),  (void*)floats,  sizeof(float) * num_floats);
  memcpy((void*)layout.Pointer<double>(p), (void*)doubles, sizeof(double) * num_doubles);

  return p;
}

void use(unsigned char* p)
{
  // 从设计上，知道前2个数组的长度都固定为1，所以，先构造一个“部分”layout；
  constexpr auto partial = MyCompactFoo::L::Partial(1, 1);
  size_t num_floats  = *partial.Pointer<0>(p);
  size_t num_doubles = *partial.Pointer<1>(p);

  // 从“部分”layout获取到了全局layout（4个数组的长度都知道了），构造全局layout；
  const MyCompactFoo::L layout(1, 1, num_floats, num_doubles);

  float*  floats  = layout.Pointer<2>(p);
  double* doubles = layout.Pointer<3>(p);

  std::cout << "floats  : ";
  for (size_t i=0; i<num_floats; ++i) {
    std::cout << floats[i] << " ";
  }
  std::cout << std::endl;

  std::cout << "doubles : ";
  for (size_t i=0; i<num_doubles; ++i) {
    std::cout << doubles[i] << " ";
  }
  std::cout << std::endl;
}

int main()
{
  //step-1: 生成MyCompactFoo对象；
  float  f[3] = {1.1, 2.2, 3.3};
  double d[4] = {4.4, 5.5, 6.6, 7.7};

  unsigned char* foo = create(f, 3, d, 4);

  //step-2: 通过网络发送，或者保存到文件；

  //step-3: 从网络接收，或者从文件读取；
  unsigned char* foo_read_from_file = foo;

  //step-4: 使用，读或者修改！以打印为例：
  //输出：
  //  floats  : 1.1 2.2 3.3
  //  doubles : 4.4 5.5 6.6 7.7
  use(foo_read_from_file);

  return 0;
}
