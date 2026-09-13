#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""资源矿点类

Factorio风格的资源矿点，可以无限开采
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, InventoryComponent
from settings import TILE_SIZE, RESOURCE_TYPES, COLORS
from sprites import get_sprite


class OreDeposit(Entity):
    """资源矿点类"""
    
    def __init__(self, x: float, y: float, ore_type: str = "iron_ore"):
        """初始化资源矿点
        
        Args:
            x: 初始x位置
            y: 初始y位置
            ore_type: 矿石类型（iron_ore/copper_ore/coal）
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        
        # 位置组件
        self.position = PositionComponent(x, y)
        
        # 矿石类型
        self.ore_type = ore_type
        
        # 矿石信息
        self.ore_info = RESOURCE_TYPES.get(ore_type, RESOURCE_TYPES["iron_ore"])
        
        # 无限资源存储
        self.inventory = InventoryComponent(max_slots=1, max_stack_size=999999)
        self.inventory.add_item(ore_type, 999999)
        
        # 开采难度（影响开采速度）
        self.mining_time = self.ore_info["mining_time"]
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化矿点外观"""
        # 根据矿石类型使用对应贴图
        sprite_map = {
            "iron_ore": "iron_ore",
            "copper_ore": "copper_ore",
            "coal": "coal_ore"
        }
        sprite_name = sprite_map.get(self.ore_type, "iron_ore")
        self.image = get_sprite(sprite_name)
    
    def mine(self, amount: int = 1) -> int:
        """开采矿石
        
        Args:
            amount: 开采数量
        
        Returns:
            实际开采到的数量
        """
        return self.inventory.remove_item(self.ore_type, amount)
    
    def get_remaining(self) -> int:
        """获取剩余储量
        
        Returns:
            剩余数量
        """
        return self.inventory.get_item_count(self.ore_type)
    
    def update(self, delta_time: float) -> None:
        """更新矿点状态
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        # 矿点本身不需要每帧更新