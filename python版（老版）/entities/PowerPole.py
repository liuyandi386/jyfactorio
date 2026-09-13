#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电线杆类

Factorio风格的电线杆，用于扩展电力网络
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, PowerComponent
from settings import TILE_SIZE, COLORS, POWER_RANGE
from sprites import get_sprite


class PowerPole(Entity):
    """电线杆类"""
    
    def __init__(self, x: float, y: float):
        """初始化电线杆
        
        Args:
            x: 初始x位置
            y: 初始y位置
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        
        # 位置组件
        self.position = PositionComponent(x, y)
        
        # 电力组件（用于分配电力）
        self.power = PowerComponent(
            max_power=0,
            power_generation=0,
            power_consumption=0
        )
        
        # 连接半径
        self.connection_radius = POWER_RANGE
        
        # 连接的电线杆列表
        self.connected_poles = []
        
        # 连接的用电建筑
        self.connected_buildings = []
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化电线杆外观"""
        # 使用贴图生成器创建贴图
        self.image = get_sprite('power_pole')
    
    def connect_to_pole(self, pole) -> bool:
        """连接到另一个电线杆
        
        Args:
            pole: 另一个电线杆
        
        Returns:
            是否成功连接
        """
        if pole == self:
            return False
        
        # 检查距离
        distance = self.position.distance_to(pole.position, self.width, self.height)
        if distance > self.connection_radius:
            return False
        
        # 添加连接
        if pole not in self.connected_poles:
            self.connected_poles.append(pole)
            return True
        
        return False
    
    def connect_building(self, building) -> bool:
        """连接用电建筑
        
        Args:
            building: 用电建筑实体
        
        Returns:
            是否成功连接
        """
        # 检查建筑是否有电力组件
        if not hasattr(building, 'power'):
            return False
        
        # 检查距离
        if hasattr(building, 'position'):
            distance = self.position.distance_to(building.position, self.width, self.height)
        else:
            dx = self.x - building.x
            dy = self.y - building.y
            distance = (dx ** 2 + dy ** 2) ** 0.5
        
        if distance > self.connection_radius:
            return False
        
        # 添加连接
        if building not in self.connected_buildings:
            self.connected_buildings.append(building)
            building.power.power_grid_id = id(self)
            return True
        
        return False
    
    def disconnect_building(self, building) -> None:
        """断开与建筑的连接
        
        Args:
            building: 要断开的建筑
        """
        if building in self.connected_buildings:
            self.connected_buildings.remove(building)
            building.power.power_grid_id = None
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染电线杆
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)
        
        # 绘制连接线到其他电线杆
        center_x, center_y = camera.world_to_screen(
            self.x + self.width // 2,
            self.y + self.height // 2
        )
        
        for pole in self.connected_poles:
            pole_center_x, pole_center_y = camera.world_to_screen(
                pole.x + pole.width // 2,
                pole.y + pole.height // 2
            )
            
            # 绘制电线
            pygame.draw.line(
                screen,
                (150, 150, 150),
                (int(center_x), int(center_y)),
                (int(pole_center_x), int(pole_center_y)),
                max(1, int(2 * camera.zoom))
            )
        
        # 绘制连接范围（调试用）
        # radius = int(self.connection_radius * camera.zoom)
        # pygame.draw.circle(screen, (0, 255, 0, 30), (int(center_x), int(center_y)), radius, 1)