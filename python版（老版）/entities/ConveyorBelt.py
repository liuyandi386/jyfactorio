#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""虚空动力传送带类

虚空动力传送带是一种无能源、无应力、无电线、无传动连接的物流方块。
放置即永久运行，不需要任何外部驱动。
用于运输游戏内除电力/能量类以外的所有物品。

核心规则：
1. 不需要任何能源，放置后自动永久激活
2. 仅支持上、下、左、右四个正方向
3. 可运输除电力/能量类以外的所有物品
4. 前方阻塞时物品排队，不穿墙、不重叠
5. 多段传送带自动衔接
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent
from settings import TILE_SIZE, COLORS
from sprites import get_sprite


class ConveyorBelt(Entity):
    """虚空动力传送带类"""
    
    # 方向定义
    DIRECTION_UP = 0
    DIRECTION_RIGHT = 1
    DIRECTION_DOWN = 2
    DIRECTION_LEFT = 3
    
    # 移动速度（每秒移动的格子数）
    SPEED = 2.0  # 每秒2格
    
    def __init__(self, x: float, y: float, direction: int = 1):
        """初始化虚空动力传送带
        
        Args:
            x: 初始x位置
            y: 初始y位置
            direction: 方向（0=上, 1=右, 2=下, 3=左）
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        
        # 位置组件
        self.position = PositionComponent(x, y)
        
        # 方向（仅支持四个正方向）
        self.direction = direction % 4
        
        # 永远运行
        self.running = True
        
        # 当前传送带上的物品
        self.item = None  # {"name": "iron_ore", "progress": 0.0}
        
        # 下一个传送带
        self.next_belt = None
        
        # 阻塞状态
        self.is_blocked = False

        # 是否被实体阻塞（机器/箱子在前方）
        self.blocks_transfer = False
        self.blocks_transfer_target = None  # 前方的机器/箱子实体

        # 动画计时器
        self.animation_timer = 0.0
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化传送带外观"""
        # 使用贴图生成器创建贴图
        self.image = get_sprite('conveyor', direction=self.direction)
    
    def set_direction(self, direction: int) -> None:
        """设置方向
        
        Args:
            direction: 方向（0=上, 1=右, 2=下, 3=左）
        """
        self.direction = direction % 4
        self._init_image()
    
    def can_transport(self, item_name: str) -> bool:
        """检查物品是否可以运输
        
        Args:
            item_name: 物品名称
        
        Returns:
            是否可以运输（除电力外都可以）
        """
        # 禁止运输的物品类型
        forbidden_items = ["electricity", "power", "energy", "eu"]
        return item_name.lower() not in forbidden_items
    
    def can_accept_item(self) -> bool:
        """检查是否可以接受物品
        
        Returns:
            是否可以接受
        """
        return self.item is None
    
    def insert_item(self, item_name: str) -> bool:
        """插入物品到传送带
        
        Args:
            item_name: 物品名称
        
        Returns:
            是否成功插入
        """
        # 检查是否可以运输
        if not self.can_transport(item_name):
            return False
        
        # 检查是否已有物品
        if self.item is not None:
            return False
        
        # 插入物品
        self.item = {
            "name": item_name,
            "progress": 0.0
        }
        return True
    
    def set_next_belt(self, belt) -> None:
        """设置下一个传送带
        
        Args:
            belt: 下一个传送带实体（None表示没有）
        """
        self.next_belt = belt
    
    def has_item(self) -> bool:
        """检查是否有物品"""
        return self.item is not None
    
    def _get_output_position(self) -> tuple:
        """获取输出位置（下一格的瓦片坐标）
        
        Returns:
            输出位置(tile_x, tile_y)
        """
        tile_x = int(self.x // TILE_SIZE)
        tile_y = int(self.y // TILE_SIZE)
        
        if self.direction == 0:  # 上
            return (tile_x, tile_y - 1)
        elif self.direction == 1:  # 右
            return (tile_x + 1, tile_y)
        elif self.direction == 2:  # 下
            return (tile_x, tile_y + 1)
        else:  # 左
            return (tile_x - 1, tile_y)
    
    def _check_blocked(self, game_map) -> bool:
        """检查前方是否阻塞
        
        Args:
            game_map: 游戏地图对象
        
        Returns:
            是否阻塞
        """
        if self.item is None:
            return False
        
        output_pos = self._get_output_position()
        tile_x, tile_y = output_pos
        
        # 检查边界
        if tile_x < 0 or tile_x >= game_map.width or tile_y < 0 or tile_y >= game_map.height:
            return True
        
        # 检查是否是障碍物（非草地）
        if not game_map.is_walkable(tile_x, tile_y):
            return True
        
        # 检查下一个传送带是否已满
        if self.next_belt and self.next_belt.has_item():
            return True
        
        return False
    
    def update(self, delta_time: float, game_map=None) -> None:
        """更新传送带状态
        
        Args:
            delta_time: 帧时间间隔
            game_map: 游戏地图对象（用于阻塞检测）
        """
        super().update(delta_time)
        
        # 更新动画
        self.animation_timer += delta_time
        
        # 如果没有物品，不需要处理
        if self.item is None:
            return
        
        # 检查阻塞状态
        if game_map:
            self.is_blocked = self._check_blocked(game_map)
        
        # 如果阻塞，物品不动
        if self.is_blocked:
            return
        
        # 更新物品进度
        self.item["progress"] += self.SPEED * delta_time
        
        # 物品到达终点
        if self.item["progress"] >= 1.0:
            # 如果前方有机器/箱子阻塞，不自动传递，等待外部处理
            if not self.blocks_transfer:
                # 尝试传递给下一个传送带
                if self.next_belt and not self.next_belt.has_item():
                    self.next_belt.insert_item(self.item["name"])
                # 无论是否传递成功，当前格子清空
                self.item = None
            # 否则不清空物品，等待外部（_update_conveyor_to_chest）处理传递
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染传送带
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)
        
        # 绘制传送带上的物品
        if self.item:
            # 计算物品显示位置（中心）
            center_x = self.x + TILE_SIZE // 2 - 4
            center_y = self.y + TILE_SIZE // 2 - 4
            
            # 转换到屏幕坐标
            screen_x, screen_y = camera.world_to_screen(center_x, center_y)
            size = int(8 * camera.zoom)
            
            # 绘制物品
            color = self._get_item_color(self.item["name"])
            pygame.draw.rect(screen, color, (screen_x, screen_y, size, size))
            pygame.draw.rect(screen, (255, 255, 255), (screen_x, screen_y, size, size), 1)
        
        # 绘制阻塞状态指示
        if self.is_blocked:
            screen_x, screen_y = camera.world_to_screen(self.x + 4, self.y + 4)
            size = int(4 * camera.zoom)
            pygame.draw.circle(screen, (255, 100, 100), (int(screen_x + size), int(screen_y + size)), size)
    
    def _get_item_color(self, item_name: str) -> tuple:
        """获取物品颜色
        
        Args:
            item_name: 物品名称
        
        Returns:
            颜色元组
        """
        colors = {
            "iron_ore": COLORS["iron_ore"],
            "copper_ore": COLORS["copper_ore"],
            "coal": COLORS["coal_ore"],
            "ammo": (255, 255, 0),
            "wood": (139, 90, 43),
            "stone": (128, 128, 128),
            "iron_ingot": (192, 192, 192),
            "copper_ingot": (210, 105, 30),
            "steel": (100, 100, 100),
            "gear": (169, 169, 169),
            "circuit": (0, 200, 100),
            "battery": (255, 165, 0)
        }
        return colors.get(item_name, (200, 200, 200))