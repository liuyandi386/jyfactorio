#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""弹药制造机类

自动合成弹药的大箱子，每秒检测一次配方，有材料就自动合成并输出
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, InventoryComponent
from settings import TILE_SIZE, COLORS, RECIPES
from sprites import get_sprite


class AmmoFactory(Entity):
    """弹药制造机类 - 自动合成的大箱子"""

    def __init__(self, x: float, y: float, direction: int = 2):
        """初始化弹药制造机

        Args:
            x: 初始x位置
            y: 初始y位置
            direction: 输出方向（0-上, 1-右, 2-下, 3-左）
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 输出方向
        self.output_direction = direction % 4

        # 库存组件（大箱子，无限容量）
        self.inventory = InventoryComponent(max_slots=100, max_stack_size=99999)

        # 合成计时器（每秒检测一次）
        self.craft_timer = 0.0
        self.craft_interval = 1.0  # 每秒检测一次

        # 弹药配方
        self.recipe = RECIPES["ammo"]

        # 初始化外观
        self._init_image()

    def _init_image(self) -> None:
        """初始化弹药制造机外观"""
        # 使用贴图生成器创建贴图，传入方向参数
        self.image = get_sprite('ammo_factory', direction=self.output_direction)

    def get_output_position(self) -> tuple:
        """获取输出位置（世界坐标）

        Returns:
            输出位置(x, y)
        """
        tile_x = int(self.x // TILE_SIZE)
        tile_y = int(self.y // TILE_SIZE)

        if self.output_direction == 0:  # 上
            return (tile_x * TILE_SIZE, (tile_y - 1) * TILE_SIZE)
        elif self.output_direction == 1:  # 右
            return ((tile_x + 1) * TILE_SIZE, tile_y * TILE_SIZE)
        elif self.output_direction == 2:  # 下
            return (tile_x * TILE_SIZE, (tile_y + 1) * TILE_SIZE)
        else:  # 左
            return ((tile_x - 1) * TILE_SIZE, tile_y * TILE_SIZE)
    
    def rotate_direction(self) -> None:
        """旋转输出方向"""
        self.output_direction = (self.output_direction + 1) % 4
        self._init_image()

    def update(self, delta_time: float) -> None:
        """更新弹药制造机状态

        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)

        # 更新合成计时器
        self.craft_timer += delta_time

        # 每秒检测一次配方
        if self.craft_timer >= self.craft_interval:
            self.craft_timer = 0.0
            self._try_craft()

    def _try_craft(self) -> bool:
        """尝试合成弹药

        Returns:
            是否成功合成
        """
        # 检查是否有足够的原材料
        for item, amount in self.recipe["ingredients"].items():
            if self.inventory.get_item_count(item) < amount:
                return False

        # 消耗原材料
        for item, amount in self.recipe["ingredients"].items():
            self.inventory.remove_item(item, amount)

        # 产出弹药
        for item, amount in self.recipe["output"].items():
            self.inventory.add_item(item, amount)

        return True

    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染弹药制造机

        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)

        # 显示库存中的弹药数量
        ammo_count = self.inventory.get_item_count("ammo")
        if ammo_count > 0:
            screen_x, screen_y = camera.world_to_screen(self.x, self.y)
            font = pygame.font.SysFont('Arial', max(8, int(12 * camera.zoom)))
            text = font.render(f"{ammo_count}", True, (255, 255, 0))
            text_rect = text.get_rect(bottomright=(screen_x + self.width * camera.zoom - 2, screen_y + self.height * camera.zoom - 2))
            screen.blit(text, text_rect)
