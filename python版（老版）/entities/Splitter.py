#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""物品分流器

类似于电线，为一个方块，四个面状态为：NONE / INPUT / OUTPUT。
连接传送带，实现物品分流和合流。
"""

import pygame
from collections import deque
from entities.Entity import Entity
from components import PositionComponent
from settings import TILE_SIZE


# 面模式常量
SPLIT_FACE_NONE = 0
SPLIT_FACE_INPUT = 1
SPLIT_FACE_OUTPUT = 2

DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

# 方向偏移
DIR_OFFSETS = {
    DIR_UP: (0, -1),
    DIR_RIGHT: (1, 0),
    DIR_DOWN: (0, 1),
    DIR_LEFT: (-1, 0),
}

# 反向映射
OPPOSITE_DIR = {DIR_UP: DIR_DOWN, DIR_DOWN: DIR_UP, DIR_LEFT: DIR_RIGHT, DIR_RIGHT: DIR_LEFT}

# 面颜色
FACE_COLORS = {
    SPLIT_FACE_NONE: (70, 70, 75),
    SPLIT_FACE_INPUT: (0, 180, 80),
    SPLIT_FACE_OUTPUT: (220, 80, 40),
}

FACE_LABELS = {SPLIT_FACE_NONE: "NONE", SPLIT_FACE_INPUT: "IN", SPLIT_FACE_OUTPUT: "OUT"}
FACE_CHARS = {SPLIT_FACE_NONE: "N", SPLIT_FACE_INPUT: "I", SPLIT_FACE_OUTPUT: "O"}

# 默认面配置（全NONE，需手动配置）
DEFAULT_SPLIT_FACES = [SPLIT_FACE_NONE] * 4

# 内部队列最大容量
MAX_QUEUE_SIZE = 100


class Splitter(Entity):
    """物品分流器 - 连接传送带，分流/合流物品

    属性:
        faces: 4方向面配置 [UP, RIGHT, DOWN, LEFT]
        item_queue: 内部物品队列（FIFO）
        transfer_cooldown: 传输冷却时间
    """

    TRANSFER_INTERVAL = 0.15   # 传输间隔（秒）

    def __init__(self, x: float, y: float):
        """初始化分流器

        Args:
            x: 世界坐标X
            y: 世界坐标Y
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 4方向面配置 [UP, RIGHT, DOWN, LEFT]
        self.faces = list(DEFAULT_SPLIT_FACES)

        # 内部物品队列
        self.item_queue = deque()

        # 传输冷却
        self.transfer_timer = 0.0

        # 均分输出索引（轮询OUTPUT面）
        self._output_index = 0

        # 外观
        self.image = None
        self.refresh_appearance()

    def refresh_appearance(self):
        """刷新分流器外观"""
        size = TILE_SIZE
        surf = pygame.Surface((size, size), pygame.SRCALPHA)

        half = size // 2
        third = size // 3
        line_thickness = max(2, size // 12)
        indicator_size = max(2, size // 10)

        # 每个方向的面色块和指示
        face_positions = {
            DIR_UP:    (half - line_thickness, 0, line_thickness * 2, third),
            DIR_RIGHT: (size - third, half - line_thickness, third, line_thickness * 2),
            DIR_DOWN:  (half - line_thickness, size - third, line_thickness * 2, third),
            DIR_LEFT:  (0, half - line_thickness, third, line_thickness * 2),
        }
        indicator_positions = {
            DIR_UP:    (half, third - indicator_size),
            DIR_RIGHT: (size - third + indicator_size, half),
            DIR_DOWN:  (half, size - third + indicator_size),
            DIR_LEFT:  (third - indicator_size, half),
        }

        for d in range(4):
            mode = self.faces[d]
            color = FACE_COLORS.get(mode, (70, 70, 75))

            # 方向线缆区域
            rx, ry, rw, rh = face_positions[d]
            pygame.draw.rect(surf, color, (rx, ry, rw, rh))

            # 面指示三角（指向性）
            ix, iy = indicator_positions[d]
            isize = indicator_size * 2
            if mode == SPLIT_FACE_INPUT:
                # 输入：向中心的三角
                points = [
                    (half, half),
                    (ix - indicator_size, iy - indicator_size),
                    (ix + indicator_size, iy + indicator_size),
                ]
            elif mode == SPLIT_FACE_OUTPUT:
                # 输出：向外侧的三角
                points = [
                    (ix, iy),
                    (half - indicator_size, half - indicator_size),
                    (half + indicator_size, half + indicator_size),
                ]
            else:
                # NONE：方块
                points = None

            if points:
                # 简化：用方块代替三角
                pygame.draw.rect(surf, color,
                               (ix - indicator_size, iy - indicator_size,
                                isize, isize), border_radius=1)

        # 中心核心
        center = half
        core_r = size // 4
        pygame.draw.circle(surf, (120, 120, 130), (center, center), core_r)
        pygame.draw.circle(surf, (30, 30, 35), (center, center), core_r - 2)

        # 中心文字"S"
        try:
            font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', max(9, size // 6))
            label = font.render("S", True, (255, 255, 255))
            lr = label.get_rect(center=(center, center))
            surf.blit(label, lr)
        except Exception:
            pass

        # 边框
        pygame.draw.rect(surf, (60, 60, 65), (0, 0, size, size), 1)

        # 队列指示：显示内部物品数
        if len(self.item_queue) > 0:
            count_text = str(len(self.item_queue)) if len(self.item_queue) < 10 else "+"
            try:
                cfont = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', max(8, size // 7))
                c_surf = cfont.render(count_text, True, (255, 255, 0))
                cr = c_surf.get_rect(topright=(size - 2, 2))
                surf.blit(c_surf, cr)
            except Exception:
                pass

        self.image = surf

    def cycle_face(self, direction: int) -> None:
        """切换指定方向面的模式: NONE→INPUT→OUTPUT→NONE

        Args:
            direction: DIR_UP / DIR_RIGHT / DIR_DOWN / DIR_LEFT
        """
        if 0 <= direction <= 3:
            self.faces[direction] = (self.faces[direction] + 1) % 3
            self.refresh_appearance()

    def get_face(self, direction: int) -> int:
        """获取指定方向面的模式

        Args:
            direction: DIR_UP / DIR_RIGHT / DIR_DOWN / DIR_LEFT

        Returns:
            面模式 SPLIT_FACE_NONE / SPLIT_FACE_INPUT / SPLIT_FACE_OUTPUT
        """
        if 0 <= direction <= 3:
            return self.faces[direction]
        return SPLIT_FACE_NONE

    def can_accept_item(self) -> bool:
        """检查是否可以接受物品"""
        return len(self.item_queue) < MAX_QUEUE_SIZE

    def add_item(self, item_name: str) -> bool:
        """添加物品到内部队列

        Args:
            item_name: 物品名称

        Returns:
            是否成功添加
        """
        if len(self.item_queue) >= MAX_QUEUE_SIZE:
            return False
        self.item_queue.append(item_name)
        self.refresh_appearance()
        return True

    def pop_item(self):
        """从队列弹出一个物品

        Returns:
            物品名称或None
        """
        if self.item_queue:
            item = self.item_queue.popleft()
            self.refresh_appearance()
            return item
        return None

    def has_items(self) -> bool:
        """检查是否有物品"""
        return len(self.item_queue) > 0

    def update(self, delta_time: float) -> None:
        """更新分流器状态"""
        super().update(delta_time)
        self.transfer_timer += delta_time

    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染分流器"""
        super().draw(screen, camera)
