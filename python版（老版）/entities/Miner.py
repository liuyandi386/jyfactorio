#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""采矿机类

Factorio风格的采矿机，自动采集资源矿点
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, InventoryComponent, PowerComponent, ProductionComponent
from settings import TILE_SIZE, COLORS, RESOURCE_TYPES
from sprites import get_sprite


class Miner(Entity):
    """采矿机类"""
    
    def __init__(self, x: float, y: float, direction: int = 2):
        """初始化采矿机

        Args:
            x: 初始x位置
            y: 初始y位置
            direction: 输出方向 (0-上, 1-右, 2-下, 3-左)
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 库存组件（存储采集到的资源）
        self.inventory = InventoryComponent(max_slots=5, max_stack_size=50)

        # 电力组件（需要电力才能工作）
        self.power = PowerComponent(
            max_power=50,
            power_generation=0,
            power_consumption=10
        )

        # 生产组件
        self.production = ProductionComponent(
            production_time=2.0,
            input_items={},
            output_items={}  # 输出物品在update中动态设置
        )

        # 目标矿点
        self.target_ore = None

        # 输出方向（0-上, 1-右, 2-下, 3-左）
        self.output_direction = direction
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化采矿机外观"""
        # 使用贴图生成器创建贴图，传入方向参数
        self.image = get_sprite('miner', direction=self.output_direction)
    
    def set_target_ore(self, ore_deposit) -> None:
        """设置目标矿点
        
        Args:
            ore_deposit: 矿点实体
        """
        self.target_ore = ore_deposit
        
        # 更新生产组件
        if ore_deposit:
            self.production.production_time = ore_deposit.mining_time
            self.production.output_items = {ore_deposit.ore_type: 1}
    
    def set_output_direction(self, direction: int) -> None:
        """设置输出方向
        
        Args:
            direction: 方向（0-上, 1-右, 2-下, 3-左）
        """
        self.output_direction = direction % 4
        self._init_image()  # 重新绘制
    
    def rotate_direction(self) -> None:
        """旋转输出方向"""
        self.set_output_direction(self.output_direction + 1)
    
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
    
    def update(self, delta_time: float) -> None:
        """更新采矿机状态
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        
        if self.target_ore is None:
            return
        
        # 检查是否有电力
        has_power = self.power.is_powered
        
        # 检查库存是否已满
        if self.inventory.is_full():
            return
        
        # 更新生产
        output = self.production.update(delta_time, has_power)
        
        # 如果有产出，添加到库存
        if output:
            for item_name, amount in output.items():
                self.inventory.add_item(item_name, amount)
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染采矿机
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)
        
        # 绘制电力状态指示
        if not self.power.is_powered:
            screen_x, screen_y = camera.world_to_screen(self.x, self.y - 8)
            size = int(4 * camera.zoom)
            pygame.draw.circle(screen, (255, 0, 0), (int(screen_x + size), int(screen_y)), size)
        
        # 绘制生产进度
        if self.production.is_producing:
            screen_x, screen_y = camera.world_to_screen(self.x, self.y + self.height + 2)
            bar_width = self.width * camera.zoom
            bar_height = 3 * camera.zoom
            progress = self.production.get_progress_percentage()
            pygame.draw.rect(screen, (0, 0, 0), (screen_x, screen_y, bar_width, bar_height))
            pygame.draw.rect(screen, (0, 255, 0), (screen_x, screen_y, bar_width * progress, bar_height))