#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""地图类

负责游戏地图的生成、渲染和管理
"""

import pygame
from settings import TILE_SIZE, MAP_WIDTH, MAP_HEIGHT, COLORS, PATH_POINTS, SCREEN_WIDTH, SCREEN_HEIGHT
from sprites import get_sprite


class GameMap:
    """游戏地图类"""
    
    def __init__(self):
        """初始化地图"""
        # 地图尺寸（瓦片数）- 使用配置的完整尺寸
        self.width = MAP_WIDTH
        self.height = MAP_HEIGHT
        
        # 瓦片类型：0-草地，1-路径，2-水
        # 使用延迟生成策略，只在需要时生成瓦片
        self.tiles = {}
        
        # 初始化路径
        self._generate_path()
        
        # 不需要限制地图尺寸，支持无限滚动
        # 移除原来的尺寸限制代码
        
        # 创建一个小的缓存表面用于常用区域
        self.cache_surface = pygame.Surface((50 * TILE_SIZE, 50 * TILE_SIZE))
        self.cache_dirty = True
    
    def _generate_path(self) -> None:
        """生成敌人行走路径"""
        for i in range(len(PATH_POINTS) - 1):
            start_x, start_y = PATH_POINTS[i]
            end_x, end_y = PATH_POINTS[i + 1]
            
            # 绘制水平路径段
            min_x = min(start_x, end_x)
            max_x = max(start_x, end_x)
            for x in range(min_x, max_x + 1):
                if 0 <= x < self.width and 0 <= start_y < self.height:
                    self.tiles[(x, start_y)] = 1
            
            # 绘制垂直路径段
            min_y = min(start_y, end_y)
            max_y = max(start_y, end_y)
            for y in range(min_y, max_y + 1):
                if 0 <= end_x < self.width and 0 <= y < self.height:
                    self.tiles[(end_x, y)] = 1
    
    def _render_map(self) -> None:
        """渲染地图表面"""
        for x in range(self.width):
            for y in range(self.height):
                tile_type = self.tiles[x][y]
                
                # 根据瓦片类型设置颜色
                if tile_type == 0:
                    color = COLORS["tile_grass"]
                elif tile_type == 1:
                    color = COLORS["tile_path"]
                else:
                    color = COLORS["tile_water"]
                
                # 绘制瓦片
                rect = pygame.Rect(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
                pygame.draw.rect(self.surface, color, rect)
                
                # 添加网格线
                pygame.draw.rect(self.surface, (20, 20, 20), rect, 1)
    
    def get_tile_type(self, x: int, y: int) -> int:
        """获取指定位置的瓦片类型
        
        Args:
            x: 瓦片x坐标
            y: 瓦片y坐标
        
        Returns:
            瓦片类型：0-草地，1-路径，2-水
        """
        # 检查边界
        if x < 0 or x >= self.width or y < 0 or y >= self.height:
            return 2  # 超出边界视为水域
        
        # 从字典获取瓦片类型，默认为草地
        return self.tiles.get((x, y), 0)
    
    def is_walkable(self, x: int, y: int) -> bool:
        """检查指定位置是否可行走
        
        Args:
            x: 瓦片x坐标
            y: 瓦片y坐标
        
        Returns:
            是否可行走
        """
        return self.get_tile_type(x, y) == 0  # 只有草地可行走
    
    def is_path(self, x: int, y: int) -> bool:
        """检查指定位置是否是路径
        
        Args:
            x: 瓦片x坐标
            y: 瓦片y坐标
        
        Returns:
            是否是路径
        """
        return self.get_tile_type(x, y) == 1
    
    def can_place_tower(self, world_x: float, world_y: float) -> bool:
        """检查指定世界坐标是否可以放置塔
        
        Args:
            world_x: 世界x坐标
            world_y: 世界y坐标
        
        Returns:
            是否可以放置塔
        """
        tile_x = int(world_x // TILE_SIZE)
        tile_y = int(world_y // TILE_SIZE)
        
        # 检查是否在地图范围内
        if tile_x < 0 or tile_x >= self.width or tile_y < 0 or tile_y >= self.height:
            return False
        
        # 检查是否是草地（使用新的 get_tile_type 方法）
        return self.get_tile_type(tile_x, tile_y) == 0
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染地图
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        # 获取摄像机偏移
        offset_x, offset_y = camera.get_offset()
        
        # 计算可见区域（扩大范围以支持滚动）
        tiles_per_screen_x = int(SCREEN_WIDTH / (TILE_SIZE * camera.zoom)) + 2
        tiles_per_screen_y = int(SCREEN_HEIGHT / (TILE_SIZE * camera.zoom)) + 2
        
        # 计算起始瓦片位置（考虑负数偏移）
        start_x = int(-offset_x / (TILE_SIZE * camera.zoom)) - 1
        start_y = int(-offset_y / (TILE_SIZE * camera.zoom)) - 1
        
        # 限制在地图范围内
        start_x = max(0, start_x)
        start_y = max(0, start_y)
        end_x = min(self.width, start_x + tiles_per_screen_x + 1)
        end_y = min(self.height, start_y + tiles_per_screen_y + 1)
        
        # 渲染可见区域的瓦片
        for x in range(start_x, end_x):
            for y in range(start_y, end_y):
                # 使用新的 get_tile_type 方法获取瓦片类型
                tile_type = self.get_tile_type(x, y)
                
                # 根据瓦片类型选择贴图
                if tile_type == 0:  # 草地
                    sprite = get_sprite('grass')
                elif tile_type == 1:  # 路径
                    sprite = get_sprite('path')
                else:  # 水或其他
                    sprite = get_sprite('grass')
                
                # 计算屏幕坐标
                screen_x = x * TILE_SIZE * camera.zoom + offset_x
                screen_y = y * TILE_SIZE * camera.zoom + offset_y
                
                # 缩放贴图
                scaled_sprite = pygame.transform.scale(
                    sprite,
                    (int(TILE_SIZE * camera.zoom), int(TILE_SIZE * camera.zoom))
                )
                
                # 绘制贴图
                screen.blit(scaled_sprite, (screen_x, screen_y))
                
                # 添加网格线（可选，可以注释掉）
                pygame.draw.rect(screen, (20, 20, 20), (screen_x, screen_y, TILE_SIZE * camera.zoom, TILE_SIZE * camera.zoom), 1)
    
    def get_world_size(self) -> tuple:
        """获取世界地图尺寸（像素）
        
        Returns:
            地图尺寸(width, height)
        """
        return (self.width * TILE_SIZE, self.height * TILE_SIZE)