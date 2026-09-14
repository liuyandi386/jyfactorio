#pragma once
// =====================================================================
// PlayerSystem.h —— 玩家系统（输入交互 + 建筑放置/拆除 + 资源）
//
//   键盘：WASD移动摄像机 / 1-5,7-9,0,-,=,\ 选择建筑 / TAB循环 /
//         R旋转(空) / 空格暂停 / Z,U生成敌人 / F5存档 / F9读档 /
//         DEL拆除 / ESC关闭面编辑器
//   鼠标：左键放置 / 右键旋转采矿机输出面、打开电线/机器面配置编辑器、
//         组装机配方菜单 / 滚轮缩放（管道/分流器自动链接，无右键交互）
// =====================================================================
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

class Game;

/// 玩家系统
class PlayerSystem {
public:
    /// 处理世界交互事件（UI未消费时调用）
    static void handleEvent(Game& g, const sf::Event& e);

    /// 每帧更新摄像机（WASD）；dt = 帧耗时(秒)，保证高刷新率下速度一致
    static void updateCamera(Game& g, float dt);

    /// 每帧更新放置预览数据
    static void updatePreview(Game& g);

private:
    /// 左键放置
    static void handleLeftClick(Game& g, sf::Vector2f world, sf::Vector2f screen);
    /// 右键交互（旋转/面编辑器/配方菜单）
    static void handleRightClick(Game& g, sf::Vector2f world, sf::Vector2f screen);
};
