#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电力塔适配器

将游戏内置的 electric_tower 接入新的工业电力系统。
电力塔需要从电网获取电力才能运作。
"""

import pygame


class PowerTower:
    """电力塔适配器 - 将 electric_tower 接入电力网络

    封装内置 Tower（electric 类型），使其能从 PowerNetwork 获取电力。

    属性:
        tower: 内置 Tower 实例
        power_required: 所需 EU/t
        powered: 是否已供电
        range: 覆盖范围（从 Tower 继承）
    """

    # 电力塔消耗配置
    DEFAULT_POWER_REQUIRED = 8  # 默认消耗 8 EU/t

    def __init__(self, tower):
        """初始化电力塔适配器

        Args:
            tower: 内置 Tower 实例（tower_type == "electric"）
        """
        self.tower = tower

        # 电力属性
        self.power_required = self.DEFAULT_POWER_REQUIRED  # 所需 EU/t
        self.powered = False       # 是否已供电
        self.range = tower.range   # 攻击范围（继承自 Tower）

    def set_powered(self, powered: bool) -> None:
        """设置供电状态

        Args:
            powered: 是否供电
        """
        self.powered = powered
        if hasattr(self.tower, 'power'):
            self.tower.power.set_powered(powered)
            # 工业电网供电时直接填满电力（连续供电模式）
            if powered:
                self.tower.power.current_power = self.tower.power.max_power
                self.tower.power.is_powered = True
            else:
                self.tower.power.is_powered = False
            if hasattr(self.tower, 'is_electric'):
                self.tower.power.is_powered = powered

    def can_attack(self) -> bool:
        """检查是否可以攻击

        Returns:
            是否有足够电力
        """
        return self.powered

    def get_stats(self) -> dict:
        """获取电力塔统计信息

        Returns:
            统计字典
        """
        return {
            "power_required": self.power_required,
            "powered": self.powered,
            "range": self.range
        }

    # 代理属性访问（x, y, width, height 等）
    @property
    def x(self):
        return self.tower.x

    @property
    def y(self):
        return self.tower.y

    @property
    def width(self):
        return self.tower.width

    @property
    def height(self):
        return self.tower.height

    def get_center(self):
        return self.tower.get_center()

    def update(self, delta_time: float) -> None:
        """更新电力塔状态"""
        # Tower 自身的 update 由主游戏循环调用
        pass

    def draw_power_status(self, screen: pygame.Surface, camera) -> None:
        """绘制电力状态指示

        Args:
            screen: 渲染目标
            camera: 摄像机
        """
        screen_x, screen_y = camera.world_to_screen(
            self.tower.x + self.tower.width - 15,
            self.tower.y + 2
        )
        size = max(2, int(4 * camera.zoom))
        font_size = max(6, int(8 * camera.zoom))

        # 供电状态指示灯
        status_color = (0, 255, 0) if self.powered else (255, 0, 0)
        pygame.draw.circle(screen, status_color,
                         (int(screen_x), int(screen_y)), size)

        # 显示 EU/t 消耗
        try:
            font = pygame.font.SysFont('Arial', font_size)
            text = font.render(
                f"{self.power_required}EU/t",
                True, status_color
            )
            text_x = screen_x - text.get_width() - 4
            text_y = screen_y - text.get_height() // 2
            screen.blit(text, (text_x, text_y))
        except Exception:
            pass
