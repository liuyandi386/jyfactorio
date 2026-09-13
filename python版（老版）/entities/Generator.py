#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""发电机类

Factorio风格的燃煤发电机，消耗煤炭产生电力
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, InventoryComponent, PowerComponent
from settings import TILE_SIZE, COLORS, RECIPES
from sprites import get_sprite


class Generator(Entity):
    """燃煤发电机类"""

    def __init__(self, x: float, y: float, direction: int = 2):
        """初始化发电机

        Args:
            x: 初始x位置
            y: 初始y位置
            direction: 输出方向（0-上, 1-右, 2-下, 3-左）
        """
        super().__init__(x, y, TILE_SIZE * 2, TILE_SIZE * 2)

        # 位置组件
        self.position = PositionComponent(x, y)

        # 输出方向
        self.output_direction = direction % 4

        # 库存组件（存储燃料）
        self.inventory = InventoryComponent(max_slots=1, max_stack_size=50)

        # 电力组件
        self.power = PowerComponent(
            max_power=200,
            power_generation=0,
            power_consumption=0
        )

        # 燃烧进度
        self.burn_progress = 0.0

        # 当前电力输出（每秒）
        self.current_power_output = 0.0

        # 动画计时器
        self.animation_timer = 0.0

        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化发电机外观"""
        # 使用贴图生成器创建贴图，传入方向参数
        self.image = get_sprite('generator', direction=self.output_direction)

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
            return ((tile_x + 2) * TILE_SIZE, tile_y * TILE_SIZE)
        elif self.output_direction == 2:  # 下
            return ((tile_x + 1) * TILE_SIZE, (tile_y + 2) * TILE_SIZE)
        else:  # 左
            return ((tile_x - 1) * TILE_SIZE, (tile_y + 1) * TILE_SIZE)

    def add_fuel(self, amount: int = 1) -> int:
        """添加燃料
        
        Args:
            amount: 燃料数量
        
        Returns:
            实际添加的数量
        """
        return self.inventory.add_item("coal", amount)
    
    def has_fuel(self) -> bool:
        """检查是否有燃料
        
        Returns:
            是否有燃料
        """
        return self.inventory.get_item_count("coal") > 0
    
    def update(self, delta_time: float) -> None:
        """更新发电机状态
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        
        # 更新动画
        self.animation_timer += delta_time
        
        # 获取电力配方
        power_recipe = RECIPES["power"]
        
        # 检查是否有燃料
        if self.has_fuel():
            # 更新燃烧进度
            self.burn_progress += delta_time
            
            # 检查是否完成一次燃烧周期
            if self.burn_progress >= power_recipe["craft_time"]:
                # 消耗1个煤炭
                self.inventory.remove_item("coal", 1)
                
                # 产生EU电力
                power_output = power_recipe["output"]["power"]
                self.power.add_power(power_output)
                
                # 重置燃烧进度
                self.burn_progress = 0.0
            
            # 当前电力输出 = 总电力 / 燃烧时间
            self.current_power_output = power_recipe["output"]["power"] / power_recipe["craft_time"]
        else:
            # 没有燃料，停止发电
            self.current_power_output = 0.0
            self.burn_progress = 0.0
        
        # 更新电力产生（基于当前输出）
        if self.current_power_output > 0:
            self.power.generate_power(delta_time)
    
    def get_power_output(self) -> float:
        """获取当前电力输出
        
        Returns:
            当前可用电力
        """
        return self.current_power_output
    
    def get_rotation_speed(self) -> float:
        """获取转速（RPM）
        
        Returns:
            当前转速
        """
        if self.has_fuel():
            return 60.0  # 默认转速60 RPM
        return 0.0
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染发电机
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)
        
        # 绘制运行状态指示
        if self.has_fuel():
            # 旋转的涡轮效果
            screen_x, screen_y = camera.world_to_screen(
                self.x + self.width // 2 - 4,
                self.y + self.height // 2 + 6
            )
            size = int(8 * camera.zoom)
            
            # 简单的闪烁效果表示运行
            alpha = int(128 + 127 * (0.5 + 0.5 * pygame.math.Vector2(1, 0).rotate(self.animation_timer * 360).x))
            color = (alpha, alpha, 0)
            pygame.draw.circle(screen, color, (int(screen_x), int(screen_y)), size)
        else:
            # 无燃料警告
            screen_x, screen_y = camera.world_to_screen(self.x + 4, self.y + 4)
            size = int(4 * camera.zoom)
            pygame.draw.circle(screen, (255, 0, 0), (int(screen_x + size), int(screen_y + size)), size)
        
        # 绘制电力输出指示
        if self.current_power_output > 0:
            font = pygame.font.SysFont('SimHei, Microsoft YaHei, sans-serif', 12)
            text = font.render(f"{int(self.current_power_output)} EU/s", True, (0, 255, 0))
            screen_x, screen_y = camera.world_to_screen(self.x, self.y - 12)
            screen.blit(text, (screen_x, screen_y))