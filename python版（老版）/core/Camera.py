#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""摄像机类

负责管理游戏视角的移动和缩放
"""

import pygame
from settings import CAMERA_SPEED, ZOOM_MIN, ZOOM_MAX, ZOOM_SPEED, SCREEN_WIDTH, SCREEN_HEIGHT, MAP_WIDTH, MAP_HEIGHT, TILE_SIZE


class Camera:
    """摄像机类"""
    
    def __init__(self):
        """初始化摄像机"""
        # 摄像机位置（世界坐标）- 初始化为地图中心
        self.x = (MAP_WIDTH * TILE_SIZE) // 2 - SCREEN_WIDTH // 2
        self.y = (MAP_HEIGHT * TILE_SIZE) // 2 - SCREEN_HEIGHT // 2
        
        # 缩放比例
        self.zoom = 1.0
        
        # 目标位置（用于平滑移动）- 初始化为地图中心
        self.target_x = self.x
        self.target_y = self.y
        
        # 移动速度
        self.speed = CAMERA_SPEED
    
    def update(self, delta_time: float) -> None:
        """更新摄像机位置
        
        Args:
            delta_time: 帧时间间隔
        """
        # 平滑移动到目标位置
        self.x += (self.target_x - self.x) * 0.1
        self.y += (self.target_y - self.y) * 0.1
        
        # 确保摄像机不超出地图边界
        self.x = max(0, self.x)
        self.y = max(0, self.y)
    
    def move(self, dx: float, dy: float) -> None:
        """移动摄像机
        
        Args:
            dx: x方向移动量
            dy: y方向移动量
        """
        self.target_x += dx * self.speed / self.zoom
        self.target_y += dy * self.speed / self.zoom
    
    def zoom_in(self) -> None:
        """放大摄像机"""
        self.zoom = min(self.zoom + ZOOM_SPEED, ZOOM_MAX)
    
    def zoom_out(self) -> None:
        """缩小摄像机"""
        self.zoom = max(self.zoom - ZOOM_SPEED, ZOOM_MIN)
    
    def get_offset(self) -> tuple:
        """获取屏幕偏移量
        
        Returns:
            屏幕中心相对于摄像机位置的偏移量
        """
        return (
            SCREEN_WIDTH // 2 - self.x * self.zoom,
            SCREEN_HEIGHT // 2 - self.y * self.zoom
        )
    
    def world_to_screen(self, world_x: float, world_y: float) -> tuple:
        """将世界坐标转换为屏幕坐标
        
        Args:
            world_x: 世界x坐标
            world_y: 世界y坐标
        
        Returns:
            屏幕坐标(x, y)
        """
        offset_x, offset_y = self.get_offset()
        return (
            world_x * self.zoom + offset_x,
            world_y * self.zoom + offset_y
        )
    
    def screen_to_world(self, screen_x: float, screen_y: float) -> tuple:
        """将屏幕坐标转换为世界坐标
        
        Args:
            screen_x: 屏幕x坐标
            screen_y: 屏幕y坐标
        
        Returns:
            世界坐标(x, y)
        """
        offset_x, offset_y = self.get_offset()
        return (
            (screen_x - offset_x) / self.zoom,
            (screen_y - offset_y) / self.zoom
        )
    
    def apply(self, rect: pygame.Rect) -> pygame.Rect:
        """将矩形从世界坐标转换到屏幕坐标
        
        Args:
            rect: 世界坐标中的矩形
        
        Returns:
            屏幕坐标中的矩形
        """
        x, y = self.world_to_screen(rect.x, rect.y)
        return pygame.Rect(
            x, y,
            rect.width * self.zoom,
            rect.height * self.zoom
        )