// =====================================================================
// Camera.cpp —— 摄像机实现（移植自 Python core/Camera.py）
// 坐标变换使用运行时屏幕尺寸（窗口缩放/最大化后保持点击与世界坐标一致）
// =====================================================================
#include "Camera.h"
#include <algorithm>
#include <cmath>

void Camera::init() {
    // Python: 初始定位地图中心（使用当前屏幕尺寸）
    x = (cfg::GRID_WIDTH * cfg::TILE_SIZE) / 2.0f - screenW / 2.0f;
    y = (cfg::GRID_HEIGHT * cfg::TILE_SIZE) / 2.0f - screenH / 2.0f;
    targetX = x;
    targetY = y;
}

void Camera::update(float dt) {
    // 帧率无关的指数平滑：把"每帧固定插值 0.1"换算成"每秒速率 CAMERA_SMOOTH_RATE"。
    //   每帧系数 k = 1 - exp(-rate * dt)
    // 60 帧时 k≈0.095（与旧手感一致）；144 帧时自动变小，总位移不随帧率变化。
    const float k = 1.0f - std::exp(-cfg::CAMERA_SMOOTH_RATE * dt);
    x += (targetX - x) * k;
    y += (targetY - y) * k;
    // 钳制到地图左上角（Python只钳制>=0）
    x = std::max(0.0f, x);
    y = std::max(0.0f, y);
}

void Camera::move(float dx, float dy, float dt) {
    // Python: 移动速度除以缩放，缩小时世界移动更慢
    // speedScale：启动菜单中的摄像机速度档位（慢0.6 / 标准1.0 / 快1.6）
    // 速度是"像素/秒"，乘 dt 后与帧率无关（旧写法按帧跑，144Hz 下会快 2.4 倍）
    const float speed = cfg::CAMERA_SPEED_PER_SEC * speedScale * dt;
    targetX += dx * speed / zoom;
    targetY += dy * speed / zoom;
}

sf::Vector2f Camera::worldToScreen(float wx, float wy) const {
    return {wx * zoom + (screenW / 2.0f - x * zoom),
            wy * zoom + (screenH / 2.0f - y * zoom)};
}

sf::Vector2f Camera::screenToWorld(float sx, float sy) const {
    return {(sx - (screenW / 2.0f - x * zoom)) / zoom,
            (sy - (screenH / 2.0f - y * zoom)) / zoom};
}
