#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""储物桶类

储物桶是独立网格方块，可手动放置、移除。
存在三种状态：空桶、存有物品、桶已满。
禁止存放电力/能量类物品。

特性：
1. 固定容量（默认5个物品）
2. FIFO先进先出输出
3. 禁止存放违禁品（电力等）
4. 可与传送带联动
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent
from settings import TILE_SIZE, COLORS
from sprites import get_sprite

# 储物桶容量（无限）
BUCKET_CAPACITY = 99999


class Bucket(Entity):
    """储物桶类"""
    
    def __init__(self, x: float, y: float, direction: int = 2):
        """初始化储物桶
        
        Args:
            x: x位置
            y: y位置
            direction: 输出方向（0-上, 1-右, 2-下, 3-左）
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        
        # 位置组件
        self.position = PositionComponent(x, y)
        
        # 输出方向（0-上, 1-右, 2-下, 3-左）
        self.output_direction = direction
        
        # 储物桶内的物品列表（FIFO）
        self.items = []
        
        # 容量上限
        self.capacity = BUCKET_CAPACITY
        
        # 输出计时器（控制输出速度）
        self.output_timer = 0.0
        self.output_interval = 0.5  # 每0.5秒输出一个物品
        
        # 动画计时器
        self.animation_timer = 0.0
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化储物桶外观"""
        # 使用贴图生成器创建贴图，传入方向参数
        self.image = get_sprite('bucket', direction=self.output_direction)
    
    def can_accept(self, item_name: str) -> bool:
        """检查是否可以接收物品
        
        Args:
            item_name: 物品名称
        
        Returns:
            是否可以接收
        """
        # 检查是否已满
        if len(self.items) >= self.capacity:
            return False
        
        # 检查是否是违禁品
        forbidden_items = ["electricity", "power", "energy", "eu", "电力", "能量"]
        if item_name.lower() in [i.lower() for i in forbidden_items]:
            return False
        
        return True
    
    def add_item(self, item_name: str) -> bool:
        """添加物品到储物桶
        
        Args:
            item_name: 物品名称
        
        Returns:
            是否成功添加
        """
        if not self.can_accept(item_name):
            return False
        
        self.items.append(item_name)
        
        # 更新外观
        self._init_image()
        return True
    
    def remove_item(self) -> str:
        """取出物品（FIFO）
        
        Returns:
            物品名称，如果没有物品返回None
        """
        if len(self.items) == 0:
            return None
        
        item = self.items.pop(0)
        
        # 更新外观
        self._init_image()
        return item
    
    def get_item_count(self) -> int:
        """获取物品数量
        
        Returns:
            物品数量
        """
        return len(self.items)
    
    def is_full(self) -> bool:
        """检查储物桶是否已满
        
        Returns:
            是否已满
        """
        return len(self.items) >= self.capacity
    
    def is_empty(self) -> bool:
        """检查储物桶是否为空
        
        Returns:
            是否为空
        """
        return len(self.items) == 0
    
    def set_direction(self, direction: int) -> None:
        """设置输出方向
        
        Args:
            direction: 方向（0=上, 1=右, 2=下, 3=左）
        """
        self.output_direction = direction % 4
        self._init_image()
    
    def rotate_direction(self) -> None:
        """旋转输出方向"""
        self.set_direction(self.output_direction + 1)

    def get_items_summary(self) -> dict:
        """获取存储物品汇总信息（用于悬浮窗显示）

        Returns:
            物品名称->数量的字典
        """
        summary = {}
        for item in self.items:
            summary[item] = summary.get(item, 0) + 1
        return summary

    def update(self, delta_time: float) -> None:
        """更新储物桶状态
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        
        # 更新动画和输出计时器
        self.animation_timer += delta_time
        self.output_timer += delta_time
    
    def can_output(self) -> bool:
        """检查是否可以输出物品
        
        Returns:
            是否可以输出
        """
        return not self.is_empty() and self.output_timer >= self.output_interval
    
    def reset_output_timer(self) -> None:
        """重置输出计时器"""
        self.output_timer = 0.0
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染储物桶

        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        # 计算屏幕坐标
        screen_x, screen_y = camera.world_to_screen(self.x, self.y)
        
        # 获取缩放后的贴图
        if self.image:
            scaled_image = pygame.transform.scale(
                self.image, 
                (int(self.width * camera.zoom), int(self.height * camera.zoom))
            )
            screen.blit(scaled_image, (screen_x, screen_y))
        
        # 绘制物品数量（如果有）
        item_count = len(self.items)
        if item_count > 0:
            try:
                small_font = pygame.font.SysFont('Arial', max(6, int(8 * camera.zoom)))
                count_text = small_font.render(f"{item_count}", True, (255, 255, 255))
                count_rect = count_text.get_rect(center=(screen_x + int(self.width * camera.zoom)//2, screen_y + int(self.height * camera.zoom) - 8))
                screen.blit(count_text, count_rect)
            except:
                pass
