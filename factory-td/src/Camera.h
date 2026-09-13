#pragma once
// =====================================================================
// Camera.h —— 摄像机
//
// 移植自 Python core/Camera.py：
//   - WASD 移动（速度 CAMERA_SPEED，除以缩放使缩小时移动更慢）
//   - 滚轮缩放 ZOOM_MIN ~ ZOOM_MAX，步长 ZOOM_SPEED
//   - 位置平滑插值(系数0.1)，坐标只钳制 >= 0（地图左上角）
//   - world_to_screen / screen_to_world 坐标变换
// =====================================================================
#include <algorithm>
#include <SFML/System/Vector2.hpp>
#include "GameConfig.h"

class Camera {
public:
    /// 初始化摄像机：定位到地图中心（Python行为）
    void init();

    /// 每帧更新：平滑逼近目标位置并钳制边界
    void update();

    /// 移动摄像机目标（dx/dy为WASD归一化方向）
    void move(float dx, float dy);

    /// 滚轮缩放
    void zoomIn()  { zoom = std::min(zoom + cfg::ZOOM_SPEED, cfg::ZOOM_MAX); }
    void zoomOut() { zoom = std::max(zoom - cfg::ZOOM_SPEED, cfg::ZOOM_MIN); }

    /// 更新屏幕尺寸（窗口缩放/最大化后调用，坐标变换据此计算）
    void setScreenSize(float w, float h) { screenW = w; screenH = h; }

    /// 世界坐标 → 屏幕坐标
    sf::Vector2f worldToScreen(float wx, float wy) const;
    /// 屏幕坐标 → 世界坐标
    sf::Vector2f screenToWorld(float sx, float sy) const;

    // 摄像机状态（屏幕中心对应的世界坐标）
    float x = 0.0f, y = 0.0f;
    float targetX = 0.0f, targetY = 0.0f;
    float zoom = 1.0f;
    // 当前屏幕尺寸（默认窗口化尺寸，运行时随窗口缩放更新）
    float screenW = cfg::SCREEN_WIDTH;
    float screenH = cfg::SCREEN_HEIGHT;
    // 移动速度倍率（来自启动菜单的"摄像机速度"设置：慢0.6/标准1.0/快1.6）
    float speedScale = 1.0f;
};
