#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电容库（电力系统实体）

储存电力，充当电网的缓冲。
充电：电网产电过剩时储存
放电：电网电力不足时释放
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent
from settings import TILE_SIZE


class Capacitor(Entity):
    """电容库 - 储存和释放电力

    属性:
        capacity: 最大容量（EU）
        energy: 当前储存电量（EU）
        max_input_eut: 最大输入速率（EU/t）
        max_output_eut: 最大输出速率（EU/t）
    """

    # 电容库配置
    DEFAULT_CAPACITY = 50000       # 默认最大容量 50000 EU
    DEFAULT_MAX_INPUT = 64        # 最大充电速率 64 EU/t
    DEFAULT_MAX_OUTPUT = 64       # 最大放电速率 64 EU/t

    def __init__(self, x: float, y: float):
        """初始化电容库

        Args:
            x: 世界坐标X
            y: 世界坐标Y
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 电容属性
        self.capacity = self.DEFAULT_CAPACITY      # 最大容量
        self.energy = 0.0                          # 当前电量
        self.max_input_eut = self.DEFAULT_MAX_INPUT    # 最大输入
        self.max_output_eut = self.DEFAULT_MAX_OUTPUT  # 最大输出

        # 外观
        self.image = None
        self._init_image()

    def _init_image(self) -> None:
        """初始化电容库外观"""
        size = TILE_SIZE
        surf = pygame.Surface((size, size), pygame.SRCALPHA)

        # 深色外壳
        pygame.draw.rect(surf, (40, 45, 55), (2, 2, size - 4, size - 4), border_radius=3)
        pygame.draw.rect(surf, (60, 65, 75), (4, 4, size - 8, size - 8), border_radius=2)

        # 电源指示灯（顶部）
        center = size // 2
        pygame.draw.circle(surf, (100, 100, 105),
                          (center, 6), 3)
        pygame.draw.circle(surf, (150, 150, 155),
                          (center, 6), 2)

        # 电力储存指示条（侧面）
        for i in range(3):
            y = center - 3 + i * 5
            pygame.draw.rect(surf, (0, 120, 220),
                           (size - 8, y, 4, 3), border_radius=1)

        # 正负极标识
        pygame.draw.circle(surf, (255, 50, 50),
                          (center - 6, size - 6), 2)   # + 红
        pygame.draw.circle(surf, (30, 30, 200),
                          (center + 6, size - 6), 2)   # - 蓝

        self.image = surf

    def get_energy_ratio(self) -> float:
        """获取电量占比

        Returns:
            当前电量占最大容量的比例（0.0 ~ 1.0）
        """
        if self.capacity <= 0:
            return 0.0
        return min(self.energy / self.capacity, 1.0)

    def is_full(self) -> bool:
        """检查电容是否已满"""
        return self.energy >= self.capacity

    def is_empty(self) -> bool:
        """检查电容是否为空"""
        return self.energy <= 0.0

    def charge(self, amount_eu: float) -> float:
        """充电（存储电力）

        Args:
            amount_eu: 要存储的电量（EU）

        Returns:
            实际存储的电量
        """
        if amount_eu <= 0 or self.is_full():
            return 0.0

        # 限制输入速率
        max_charge = min(amount_eu, self.max_input_eut)
        space = self.capacity - self.energy
        actual = min(max_charge, space)
        self.energy += actual
        return actual

    def discharge(self, amount_eu: float) -> float:
        """放电（释放电力）

        Args:
            amount_eu: 请求释放的电量（EU）

        Returns:
            实际释放的电量
        """
        if amount_eu <= 0 or self.is_empty():
            return 0.0

        # 限制输出速率
        max_discharge = min(amount_eu, self.max_output_eut)
        actual = min(max_discharge, self.energy)
        self.energy -= actual
        return actual

    def update(self, delta_time: float) -> None:
        """更新电容库状态

        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)

    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染电容库

        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)

        # 绘制电量条
        screen_x, screen_y = camera.world_to_screen(
            self.x, self.y + self.height - 4
        )
        bar_width = int(self.width * camera.zoom)
        bar_height = max(2, int(3 * camera.zoom))
        ratio = self.get_energy_ratio()

        # 背景
        pygame.draw.rect(screen, (30, 30, 30),
                        (screen_x, screen_y, bar_width, bar_height))
        # 电量填充
        energy_color = (0, 150, 255) if ratio > 0.3 else (200, 50, 50)
        pygame.draw.rect(screen, energy_color,
                        (screen_x, screen_y, int(bar_width * ratio), bar_height))

        # 显示电量文字
        try:
            font = pygame.font.SysFont('Arial', max(6, int(8 * camera.zoom)))
            text = font.render(
                f"{int(self.energy)}/{self.capacity}",
                True, (255, 255, 255)
            )
            text_x = screen_x + (bar_width - text.get_width()) // 2
            text_y = screen_y - max(8, int(10 * camera.zoom))
            screen.blit(text, (text_x, text_y))
        except Exception:
            pass
