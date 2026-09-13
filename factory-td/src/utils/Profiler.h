#pragma once
// =====================================================================
// Profiler.h —— Tracy 打点封装 + 并行执行封装
//
// 1. Tracy：CMake 启用 TRACY_ENABLE 后，FT_PROFILE / FT_FRAME 展开为打点
//    宏；未启用时为零开销空操作。
// 2. 并行执行：ft::parForEach 在 MSVC/带TBB的GCC 上使用
//    std::execution::par；未找到并行后端（FT_NO_PAR）时退化为串行
//    std::for_each，保证任何平台都能编译运行。
// =====================================================================
#include <algorithm>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define FT_PROFILE ZoneScoped      // 函数级打点
#define FT_FRAME   FrameMark       // 帧标记
#else
#define FT_PROFILE                 // 空实现
#define FT_FRAME
#endif

namespace ft {
#ifdef FT_NO_PAR
/// 并行后端不可用：退化为串行
template <class It, class F>
void parForEach(It first, It last, F f) {
    std::for_each(first, last, f);
}
#else
#include <execution>
/// 并行 for_each（std::execution::par）
template <class It, class F>
void parForEach(It first, It last, F f) {
    std::for_each(std::execution::par, first, last, f);
}
#endif
} // namespace ft
