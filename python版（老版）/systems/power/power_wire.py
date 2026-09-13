#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电线系统（电力系统实体）

玩家手动放置的电线，用于连接建筑形成电力网络。
每条电线有4个独立方向面（上/右/下/左），每面可独立配置：
  - NONE: 不连接
  - INPUT: 输入电力
  - TRANSFER: 传输/中继
  - OUTPUT: 输出电力
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent
from settings import TILE_SIZE


# 面配置常量
FACE_NONE = 0
FACE_INPUT = 1
FACE_TRANSFER = 2
FACE_OUTPUT = 3

DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

# 方向名称和颜色
FACE_LABELS = {FACE_NONE: "N", FACE_INPUT: "I", FACE_TRANSFER: "T", FACE_OUTPUT: "O"}
FACE_COLOR_MAP = {
    FACE_NONE: (70, 70, 75),
    FACE_INPUT: (0, 140, 255),
    FACE_TRANSFER: (180, 180, 40),
    FACE_OUTPUT: (255, 140, 0)
}

# 默认面配置（全是TRANSFER，可任意连接）
DEFAULT_WIRE_FACES = [FACE_TRANSFER, FACE_TRANSFER, FACE_TRANSFER, FACE_TRANSFER]


class PowerWire(Entity):
    """电线实体 - 连接建筑，传输电力

    属性:
        position: 位置组件
        connected_objects: 已连接的对象列表
        faces: 4方向面配置 [UP, RIGHT, DOWN, LEFT]
        connection_radius: 自动连接半径
    """

    DEFAULT_CONNECTION_RADIUS = 128   # 自动连接半径（像素）

    def __init__(self, x: float, y: float):
        """初始化电线

        Args:
            x: 世界坐标X
            y: 世界坐标Y
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 已连接的对象列表
        self.connected_objects = []

        # 4方向面配置 [UP, RIGHT, DOWN, LEFT]
        self.faces = list(DEFAULT_WIRE_FACES)

        # 连接半径
        self.connection_radius = self.DEFAULT_CONNECTION_RADIUS

        # 外观
        self.image = None
        self.refresh_appearance()

    def refresh_appearance(self):
        """刷新电线外观 - 在4个方向显示面颜色指示"""
        size = TILE_SIZE
        surf = pygame.Surface((size, size), pygame.SRCALPHA)

        half = size // 2
        third = size // 3
        indicator_size = max(2, size // 10)
        line_thickness = max(2, size // 12)

        # 绘制每个方向的面指示条和颜色方块
        face_positions = {
            DIR_UP:    (half - line_thickness, 0, line_thickness * 2, third),
            DIR_RIGHT: (size - third, half - line_thickness, third, line_thickness * 2),
            DIR_DOWN:  (half - line_thickness, size - third, line_thickness * 2, third),
            DIR_LEFT:  (0, half - line_thickness, third, line_thickness * 2),
        }
        indicator_positions = {
            DIR_UP:    (half, third),
            DIR_RIGHT: (size - third, half),
            DIR_DOWN:  (half, size - third),
            DIR_LEFT:  (third, half),
        }

        for d in range(4):
            mode = self.faces[d]
            color = FACE_COLOR_MAP.get(mode, (70, 70, 75))

            # 方向线缆
            rx, ry, rw, rh = face_positions[d]
            pygame.draw.rect(surf, color, (rx, ry, rw, rh))

            # 面颜色指示方块
            ix, iy = indicator_positions[d]
            pygame.draw.rect(surf, color,
                           (ix - indicator_size, iy - indicator_size,
                            indicator_size * 2, indicator_size * 2),
                           border_radius=1)

        # 中心端子
        center = half
        term_r = size // 5
        pygame.draw.circle(surf, (160, 160, 170), (center, center), term_r)
        pygame.draw.circle(surf, (30, 30, 35), (center, center), term_r - 2)

        # 边框
        pygame.draw.rect(surf, (50, 50, 55), (0, 0, size, size), 1)

        self.image = surf

    def cycle_face(self, direction: int) -> None:
        """切换指定方向面的模式: NONE→INPUT→TRANSFER→OUTPUT→NONE

        Args:
            direction: DIR_UP / DIR_RIGHT / DIR_DOWN / DIR_LEFT
        """
        if 0 <= direction <= 3:
            self.faces[direction] = (self.faces[direction] + 1) % 4
            self.refresh_appearance()

    def get_face(self, direction: int) -> int:
        """获取指定方向面的模式

        Args:
            direction: DIR_UP / DIR_RIGHT / DIR_DOWN / DIR_LEFT

        Returns:
            面模式 FACE_NONE / FACE_INPUT / FACE_TRANSFER / FACE_OUTPUT
        """
        if 0 <= direction <= 3:
            return self.faces[direction]
        return FACE_NONE

    def can_connect_to(self, obj) -> bool:
        """检查是否可以连接到指定对象

        Args:
            obj: 目标对象（发电机/电容库/电力塔/其他电线）

        Returns:
            是否可以连接
        """
        if obj is self:
            return False

        dx = self.x - obj.x
        dy = self.y - obj.y
        distance = (dx * dx + dy * dy) ** 0.5
        return distance <= self.connection_radius

    def connect(self, obj) -> bool:
        """连接到指定对象

        Args:
            obj: 目标对象

        Returns:
            是否连接成功
        """
        if not self.can_connect_to(obj):
            return False

        if obj not in self.connected_objects:
            self.connected_objects.append(obj)
            return True
        return False

    def disconnect(self, obj) -> None:
        """断开与指定对象的连接

        Args:
            obj: 目标对象
        """
        if obj in self.connected_objects:
            self.connected_objects.remove(obj)

    def disconnect_all(self) -> None:
        """断开所有连接"""
        self.connected_objects.clear()

    def update(self, delta_time: float) -> None:
        """更新电线状态"""
        super().update(delta_time)

    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染电线及其连接线

        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)

        center_x, center_y = camera.world_to_screen(
            self.x + self.width // 2,
            self.y + self.height // 2
        )

        for obj in self.connected_objects:
            obj_center_x, obj_center_y = camera.world_to_screen(
                obj.x + obj.width // 2,
                obj.y + obj.height // 2
            )

            pygame.draw.line(
                screen, (80, 80, 90),
                (int(center_x), int(center_y)),
                (int(obj_center_x), int(obj_center_y)),
                max(1, int(2 * camera.zoom))
            )

    def draw_connection_radius(self, screen: pygame.Surface, camera):
        """绘制连接范围（调试用）"""
        center_x, center_y = camera.world_to_screen(
            self.x + self.width // 2,
            self.y + self.height // 2
        )
        radius = int(self.connection_radius * camera.zoom)
        surf = pygame.Surface((radius * 2, radius * 2), pygame.SRCALPHA)
        pygame.draw.circle(surf, (0, 255, 0, 30),
                          (radius, radius), radius, 1)
        screen.blit(surf, (center_x - radius, center_y - radius))
