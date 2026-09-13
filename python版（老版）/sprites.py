#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""贴图生成模块

使用 pygame 绘制精美的工业风格方块和机器贴图
所有贴图尺寸：32x32 像素
风格：工业扁平化 + 像素艺术
"""

import pygame
import os
from settings import TILE_SIZE

# ========== 颜色定义 ==========
# 金属色系
METAL_LIGHT = (192, 192, 192)      # 亮银
METAL_BASE = (128, 128, 128)       # 基础灰
METAL_DARK = (64, 64, 64)          # 深灰
METAL_SHADOW = (32, 32, 32)        # 阴影

# 钢铁蓝系
STEEL_LIGHT = (100, 149, 237)      # 亮钢蓝
STEEL_BASE = (70, 130, 180)        # 钢蓝
STEEL_DARK = (50, 90, 140)         # 深钢蓝

# 铜色系
COPPER_LIGHT = (210, 140, 80)      # 亮铜
COPPER_BASE = (184, 115, 51)       # 铜
COPPER_DARK = (140, 80, 30)        # 深铜

# 金色系
GOLD_LIGHT = (255, 223, 100)       # 亮金
GOLD_BASE = (255, 215, 0)          # 金
GOLD_DARK = (218, 165, 32)         # 深金

# 警告色系
WARNING_YELLOW = (255, 193, 7)     # 警告黄
WARNING_ORANGE = (255, 152, 0)     # 警告橙
WARNING_RED = (244, 67, 54)        # 警告红

# 电力色系
ELECTRIC_BLUE = (0, 150, 255)      # 电蓝
ELECTRIC_CYAN = (0, 255, 255)      # 电青
ELECTRIC_GLOW = (100, 200, 255)    # 电光

# 地面色系
GRASS_LIGHT = (100, 200, 80)       # 亮草绿
GRASS_BASE = (76, 175, 80)         # 草绿
GRASS_DARK = (56, 142, 60)         # 深草绿

DIRT_LIGHT = (160, 120, 80)        # 亮土
DIRT_BASE = (121, 85, 72)          # 土色
DIRT_DARK = (93, 64, 55)           # 深土

# 矿石颜色
IRON_COLOR = (120, 120, 140)       # 铁矿石
COPPER_ORE_COLOR = (180, 100, 50)  # 铜矿石
COAL_COLOR = (40, 40, 40)          # 煤矿


def _create_surface(size: int = TILE_SIZE) -> pygame.Surface:
    """创建透明贴图表面"""
    surf = pygame.Surface((size, size), pygame.SRCALPHA)
    return surf


def _draw_base_block(surf: pygame.Surface, color: tuple, shadow: tuple = None) -> None:
    """绘制基础方块（带阴影和高光）"""
    size = surf.get_width()
    
    # 主体
    pygame.draw.rect(surf, color, (1, 1, size-2, size-2), border_radius=2)
    
    # 顶部高光
    light_color = tuple(min(255, c + 40) for c in color)
    pygame.draw.line(surf, light_color, (2, 2), (size-3, 2), 1)
    pygame.draw.line(surf, light_color, (2, 2), (2, size-3), 1)
    
    # 底部阴影
    if shadow is None:
        shadow = tuple(max(0, c - 40) for c in color)
    pygame.draw.line(surf, shadow, (2, size-2), (size-3, size-2), 1)
    pygame.draw.line(surf, shadow, (size-2, 2), (size-2, size-3), 1)
    
    # 边框
    border = tuple(max(0, c - 60) for c in color)
    pygame.draw.rect(surf, border, (0, 0, size, size), 1, border_radius=2)


def _draw_circular_base(surf: pygame.Surface, color: tuple) -> None:
    """绘制圆形底座"""
    size = surf.get_width()
    center = size // 2
    radius = size // 2 - 2
    
    # 阴影
    pygame.draw.circle(surf, (30, 30, 30), (center+1, center+2), radius)
    
    # 主体
    pygame.draw.circle(surf, color, (center, center), radius)
    
    # 高光
    light = tuple(min(255, c + 50) for c in color)
    pygame.draw.circle(surf, light, (center-2, center-2), radius-3, 1)
    
    # 边框
    border = tuple(max(0, c - 50) for c in color)
    pygame.draw.circle(surf, border, (center, center), radius, 1)


def _draw_direction_arrow(surf: pygame.Surface, direction: int, color: tuple = (255, 255, 255)) -> None:
    """绘制方向箭头"""
    size = surf.get_width()
    center = size // 2
    
    # 箭头点
    if direction == 0:    # 上
        points = [(center, 4), (center-4, 10), (center+4, 10)]
    elif direction == 1:  # 右
        points = [(size-4, center), (size-10, center-4), (size-10, center+4)]
    elif direction == 2:  # 下
        points = [(center, size-4), (center-4, size-10), (center+4, size-10)]
    else:                 # 左
        points = [(4, center), (10, center-4), (10, center+4)]
    
    pygame.draw.polygon(surf, color, points)
    # 箭头边框
    dark = tuple(max(0, c - 60) for c in color)
    pygame.draw.polygon(surf, dark, points, 1)


# ========== 塔类贴图 ==========

def create_basic_tower(direction: int = 0) -> pygame.Surface:
    """创建基础塔贴图
    
    Args:
        direction: 炮管方向（0-上, 1-右上, 2-右, 3-右下, 4-下, 5-左下, 6-左, 7-左上）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 圆形底座
    _draw_circular_base(surf, METAL_BASE)
    
    # 炮塔主体
    tower_color = (100, 100, 100)
    pygame.draw.rect(surf, tower_color, (center-6, center-6, 12, 12), border_radius=1)
    pygame.draw.rect(surf, METAL_LIGHT, (center-5, center-5, 10, 10), border_radius=1)
    
    # 根据方向绘制炮管
    barrel_length = center - 4
    barrel_width = 4
    
    # 计算炮管角度（每个方向45度）
    angle = direction * 45
    
    # 炮管起点在中心
    start_x = center
    start_y = center
    
    # 根据方向计算炮管终点
    import math
    rad = math.radians(angle)
    end_x = center + int(barrel_length * math.sin(rad))
    end_y = center - int(barrel_length * math.cos(rad))
    
    # 炮管颜色
    barrel_color = (80, 80, 80)
    
    # 绘制炮管（粗线条）
    pygame.draw.line(surf, barrel_color, (start_x, start_y), (end_x, end_y), barrel_width)
    pygame.draw.line(surf, METAL_LIGHT, (start_x, start_y), (end_x, end_y), barrel_width // 2)
    
    # 炮口（终点处的小方块）
    muzzle_radius = 3
    pygame.draw.circle(surf, (40, 40, 40), (end_x, end_y), muzzle_radius)
    
    return surf


def create_rapid_tower(direction: int = 0) -> pygame.Surface:
    """创建速射塔贴图（加特林风格）
    
    Args:
        direction: 炮管方向（0-上, 1-右上, 2-右, 3-右下, 4-下, 5-左下, 6-左, 7-左上）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 圆形底座
    _draw_circular_base(surf, COPPER_BASE)
    
    # 旋转炮塔
    pygame.draw.circle(surf, COPPER_DARK, (center, center), 7)
    pygame.draw.circle(surf, COPPER_LIGHT, (center, center), 5)
    
    # 根据方向绘制多根炮管（加特林风格）
    import math
    barrel_length = center - 6
    for i in range(3):
        # 三根炮管围绕中心分布
        offset_angle = (i - 1) * 20
        angle = direction * 45 + offset_angle
        rad = math.radians(angle)
        end_x = center + int(barrel_length * math.sin(rad))
        end_y = center - int(barrel_length * math.cos(rad))
        
        barrel_color = COPPER_BASE if i % 2 == 0 else COPPER_LIGHT
        pygame.draw.line(surf, barrel_color, (center, center), (end_x, end_y), 3)
    
    # 炮口火焰色（主方向）
    angle = direction * 45
    rad = math.radians(angle)
    muzzle_x = center + int(barrel_length * math.sin(rad))
    muzzle_y = center - int(barrel_length * math.cos(rad))
    pygame.draw.circle(surf, WARNING_ORANGE, (muzzle_x, muzzle_y), 2)
    
    return surf


def create_sniper_tower(direction: int = 0) -> pygame.Surface:
    """创建狙击塔贴图
    
    Args:
        direction: 炮管方向（0-上, 1-右上, 2-右, 3-右下, 4-下, 5-左下, 6-左, 7-左上）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 三角形底座（稳定）
    points = [(center, 4), (size-4, size-4), (4, size-4)]
    pygame.draw.polygon(surf, STEEL_DARK, points)
    pygame.draw.polygon(surf, STEEL_BASE, [(center, 6), (size-6, size-6), (6, size-6)])
    
    # 根据方向绘制长炮管
    import math
    barrel_length = center + 4
    
    angle = direction * 45
    rad = math.radians(angle)
    end_x = center + int(barrel_length * math.sin(rad))
    end_y = center - int(barrel_length * math.cos(rad))
    
    # 炮管
    pygame.draw.line(surf, STEEL_DARK, (center, center), (end_x, end_y), 4)
    pygame.draw.line(surf, STEEL_LIGHT, (center, center), (end_x, end_y), 2)
    
    # 瞄准镜（在炮塔顶部）
    scope_y = center - 4
    pygame.draw.circle(surf, (50, 50, 50), (center, scope_y), 4)
    pygame.draw.circle(surf, ELECTRIC_CYAN, (center, scope_y), 2)
    
    # 支架
    pygame.draw.line(surf, STEEL_DARK, (center-6, size-4), (center, center), 2)
    pygame.draw.line(surf, STEEL_DARK, (center+6, size-4), (center, center), 2)
    
    return surf


def create_electric_tower(direction: int = 0) -> pygame.Surface:
    """创建电力塔贴图
    
    Args:
        direction: 放电方向（0-上, 1-右上, 2-右, 3-右下, 4-下, 5-左下, 6-左, 7-左上）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 圆形底座
    _draw_circular_base(surf, (30, 30, 50))
    
    # 塔身
    pygame.draw.rect(surf, (60, 60, 100), (center-5, center-5, 10, 14), border_radius=2)
    pygame.draw.rect(surf, (80, 80, 140), (center-4, center-4, 8, 12), border_radius=1)
    
    # 电力线圈
    coil_y = 4
    for i in range(3):
        y = coil_y + i * 5
        pygame.draw.ellipse(surf, ELECTRIC_BLUE, (center-6, y, 12, 4))
        pygame.draw.ellipse(surf, ELECTRIC_CYAN, (center-4, y+1, 8, 2))
    
    # 根据方向绘制放电针
    import math
    needle_length = center - 2
    
    angle = direction * 45
    rad = math.radians(angle)
    end_x = center + int(needle_length * math.sin(rad))
    end_y = center - int(needle_length * math.cos(rad))
    
    # 放电针
    pygame.draw.line(surf, METAL_LIGHT, (center, center), (end_x, end_y), 2)
    pygame.draw.circle(surf, ELECTRIC_GLOW, (end_x, end_y), 3)
    
    # 闪电符号（在塔身上）
    pygame.draw.polygon(surf, WARNING_YELLOW, [
        (center-2, center+2), (center+1, center+2), 
        (center-1, center+6), (center+2, center+6),
        (center, center+10), (center-3, center+6)
    ])
    
    return surf


# ========== 机器类贴图 ==========

def create_miner(direction: int = 2) -> pygame.Surface:
    """创建采矿机贴图
    
    Args:
        direction: 输出方向（0-上, 1-右, 2-下, 3-左）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 主体方块
    _draw_base_block(surf, WARNING_YELLOW, (200, 150, 0))
    
    # 根据方向绘制钻头位置
    drill_color = METAL_DARK
    if direction == 0:  # 上
        pygame.draw.rect(surf, drill_color, (center-4, 4, 8, 8))
        pygame.draw.rect(surf, METAL_BASE, (center-3, 5, 6, 6))
        for i in range(3):
            x = center - 3 + i * 3
            pygame.draw.line(surf, (20, 20, 20), (x, 10), (x, 4), 2)
    elif direction == 1:  # 右
        pygame.draw.rect(surf, drill_color, (size-12, center-4, 8, 8))
        pygame.draw.rect(surf, METAL_BASE, (size-11, center-3, 6, 6))
        for i in range(3):
            y = center - 3 + i * 3
            pygame.draw.line(surf, (20, 20, 20), (size-10, y), (size-4, y), 2)
    elif direction == 2:  # 下
        pygame.draw.rect(surf, drill_color, (center-4, size-12, 8, 8))
        pygame.draw.rect(surf, METAL_BASE, (center-3, size-11, 6, 6))
        for i in range(3):
            x = center - 3 + i * 3
            pygame.draw.line(surf, (20, 20, 20), (x, size-10), (x, size-4), 2)
    else:  # 左
        pygame.draw.rect(surf, drill_color, (4, center-4, 8, 8))
        pygame.draw.rect(surf, METAL_BASE, (5, center-3, 6, 6))
        for i in range(3):
            y = center - 3 + i * 3
            pygame.draw.line(surf, (20, 20, 20), (10, y), (4, y), 2)
    
    # 顶部指示灯
    pygame.draw.circle(surf, WARNING_RED, (center, 6), 3)
    pygame.draw.circle(surf, (255, 100, 100), (center, 6), 2)
    
    # 输出方向箭头（白色）
    arrow_color = (255, 255, 255)
    _draw_direction_arrow(surf, direction, arrow_color)
    
    return surf


def create_ammo_factory(direction: int = 2) -> pygame.Surface:
    """创建弹药制造机贴图
    
    Args:
        direction: 输出方向（0-上, 1-右, 2-下, 3-左）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 大型箱体
    _draw_base_block(surf, (100, 80, 60), (60, 40, 20))
    
    # 军工条纹
    stripe_color = (80, 60, 40)
    pygame.draw.rect(surf, stripe_color, (3, 3, size-6, 4))
    pygame.draw.rect(surf, stripe_color, (3, size-7, size-6, 4))
    
    # 根据方向绘制输出口
    if direction == 0:  # 上
        pygame.draw.rect(surf, METAL_DARK, (center-4, 2, 8, 6), border_radius=1)
        pygame.draw.rect(surf, WARNING_ORANGE, (center-2, 4, 4, 3))
    elif direction == 1:  # 右
        pygame.draw.rect(surf, METAL_DARK, (size-8, center-4, 6, 8), border_radius=1)
        pygame.draw.rect(surf, WARNING_ORANGE, (size-6, center-2, 3, 4))
    elif direction == 2:  # 下
        pygame.draw.rect(surf, METAL_DARK, (center-4, size-8, 8, 6), border_radius=1)
        pygame.draw.rect(surf, WARNING_ORANGE, (center-2, size-6, 4, 3))
    else:  # 左
        pygame.draw.rect(surf, METAL_DARK, (2, center-4, 6, 8), border_radius=1)
        pygame.draw.rect(surf, WARNING_ORANGE, (4, center-2, 3, 4))
    
    # 齿轮装饰
    pygame.draw.circle(surf, METAL_BASE, (center, center+2), 5)
    pygame.draw.circle(surf, METAL_LIGHT, (center, center+2), 3)
    for i in range(4):
        import math
        angle = i * 90
        rad = math.radians(angle)
        x1 = center + int(3 * math.cos(rad))
        y1 = center + 2 + int(3 * math.sin(rad))
        x2 = center + int(6 * math.cos(rad))
        y2 = center + 2 + int(6 * math.sin(rad))
        pygame.draw.line(surf, METAL_DARK, (x1, y1), (x2, y2), 2)
    
    # 输出方向箭头（红色）
    arrow_color = (255, 100, 100)
    _draw_direction_arrow(surf, direction, arrow_color)
    
    return surf


def create_generator(direction: int = 2) -> pygame.Surface:
    """创建发电机贴图
    
    Args:
        direction: 输出方向（0-上, 1-右, 2-下, 3-左）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 深色箱体
    _draw_base_block(surf, (50, 50, 50), (20, 20, 20))
    
    # 通风口
    vent_color = (30, 30, 30)
    for i in range(3):
        y = 6 + i * 4
        pygame.draw.rect(surf, vent_color, (4, y, size-8, 2))
        pygame.draw.rect(surf, (80, 80, 80), (4, y, size-8, 1))
    
    # 烟囱
    pygame.draw.rect(surf, (60, 60, 60), (center+4, 2, 6, 8))
    pygame.draw.rect(surf, (80, 80, 80), (center+5, 2, 4, 7))
    
    # 电力符号
    pygame.draw.polygon(surf, ELECTRIC_GLOW, [
        (center-4, center+4), (center-1, center+4),
        (center-2, center+8), (center+1, center+8),
        (center, center+12), (center-3, center+8)
    ])
    
    # 状态灯
    pygame.draw.circle(surf, (0, 255, 0), (center-4, 6), 2)
    
    # 输出方向箭头（绿色）
    arrow_color = (0, 255, 0)
    _draw_direction_arrow(surf, direction, arrow_color)
    
    return surf


def create_power_pole() -> pygame.Surface:
    """创建电线杆贴图"""
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 木质电线杆
    pole_color = (139, 90, 43)
    pygame.draw.rect(surf, pole_color, (center-2, 2, 4, size-4))
    
    # 木纹
    pygame.draw.line(surf, (160, 110, 60), (center-1, 2), (center-1, size-4), 1)
    
    # 横梁
    pygame.draw.rect(surf, (120, 80, 40), (2, 6, size-4, 3))
    
    # 绝缘子
    pygame.draw.circle(surf, (200, 200, 200), (4, 6), 2)
    pygame.draw.circle(surf, (200, 200, 200), (size-4, 6), 2)
    
    # 电线连接点（高亮）
    pygame.draw.circle(surf, ELECTRIC_GLOW, (4, 6), 1)
    pygame.draw.circle(surf, ELECTRIC_GLOW, (size-4, 6), 1)
    
    # 底座
    pygame.draw.rect(surf, (100, 70, 40), (center-4, size-6, 8, 4))
    
    return surf


# ========== 传送带贴图 ==========

def create_conveyor(direction: int) -> pygame.Surface:
    """创建传送带贴图
    
    Args:
        direction: 0-上, 1-右, 2-下, 3-左
    """
    surf = _create_surface()
    size = TILE_SIZE
    
    # 传送带底座
    base_color = (80, 80, 80)
    pygame.draw.rect(surf, base_color, (0, 0, size, size))
    
    # 侧边护栏
    rail_color = (60, 60, 60)
    if direction in [0, 2]:  # 垂直
        pygame.draw.rect(surf, rail_color, (0, 0, 4, size))
        pygame.draw.rect(surf, rail_color, (size-4, 0, 4, size))
        # 高光
        pygame.draw.rect(surf, (100, 100, 100), (1, 0, 2, size))
    else:  # 水平
        pygame.draw.rect(surf, rail_color, (0, 0, size, 4))
        pygame.draw.rect(surf, rail_color, (0, size-4, size, 4))
        pygame.draw.rect(surf, (100, 100, 100), (0, 1, size, 2))
    
    # 传送带纹理
    belt_color = (100, 100, 100)
    if direction in [0, 2]:  # 垂直
        pygame.draw.rect(surf, belt_color, (6, 0, size-12, size))
        # 滚轮纹理
        for y in range(4, size, 6):
            pygame.draw.line(surf, (70, 70, 70), (6, y), (size-6, y), 1)
    else:  # 水平
        pygame.draw.rect(surf, belt_color, (0, 6, size, size-12))
        for x in range(4, size, 6):
            pygame.draw.line(surf, (70, 70, 70), (x, 6), (x, size-6), 1)
    
    # 方向箭头
    arrow_color = (180, 180, 180)
    _draw_direction_arrow(surf, direction, arrow_color)
    
    # 中央传送物品区域
    center = size // 2
    pygame.draw.rect(surf, (120, 120, 120), (center-3, center-3, 6, 6), border_radius=1)
    
    return surf


# ========== 储物桶贴图 ==========

def create_bucket(direction: int = 2) -> pygame.Surface:
    """创建储物桶贴图
    
    Args:
        direction: 输出方向（0-上, 1-右, 2-下, 3-左）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 桶身（梯形）
    points = [
        (center-8, 4),   # 顶部左
        (center+8, 4),   # 顶部右
        (center+10, size-4),  # 底部右
        (center-10, size-4)   # 底部左
    ]
    pygame.draw.polygon(surf, (100, 149, 237), points)
    pygame.draw.polygon(surf, (70, 130, 180), points, 2)
    
    # 桶口
    pygame.draw.ellipse(surf, (120, 170, 255), (center-8, 2, 16, 6))
    pygame.draw.ellipse(surf, (80, 130, 200), (center-8, 2, 16, 6), 1)
    
    # 桶内阴影
    pygame.draw.polygon(surf, (80, 120, 200), [
        (center-6, 8), (center+6, 8),
        (center+8, size-6), (center-8, size-6)
    ])
    
    # 侧面把手
    pygame.draw.rect(surf, (60, 100, 160), (2, center-4, 3, 8), border_radius=1)
    pygame.draw.rect(surf, (60, 100, 160), (size-5, center-4, 3, 8), border_radius=1)
    
    # 输出方向箭头（橙色）
    arrow_color = (255, 152, 0)  # 橙色
    _draw_direction_arrow(surf, direction, arrow_color)
    
    return surf


# ========== 地面贴图 ==========

def create_grass() -> pygame.Surface:
    """创建草地贴图"""
    surf = _create_surface()
    size = TILE_SIZE
    
    # 基础绿色
    surf.fill(GRASS_BASE)
    
    # 添加随机草丛纹理
    import random
    random.seed(12345)  # 固定种子确保一致性
    
    for _ in range(8):
        x = random.randint(2, size-4)
        y = random.randint(2, size-4)
        color = random.choice([GRASS_LIGHT, GRASS_DARK, GRASS_BASE])
        pygame.draw.line(surf, color, (x, y), (x, y+4), 1)
        pygame.draw.line(surf, color, (x+2, y+1), (x+2, y+3), 1)
    
    # 边缘阴影
    pygame.draw.line(surf, GRASS_DARK, (0, size-1), (size, size-1), 1)
    
    return surf


def create_path() -> pygame.Surface:
    """创建路径贴图（敌人行走路线）"""
    surf = _create_surface()
    size = TILE_SIZE
    
    # 基础土色
    surf.fill(DIRT_BASE)
    
    # 添加路径纹理（脚印/压痕）
    import random
    random.seed(54321)
    
    # 压痕
    for _ in range(6):
        x = random.randint(4, size-8)
        y = random.randint(4, size-8)
        pygame.draw.ellipse(surf, DIRT_DARK, (x, y, 6, 4))
        pygame.draw.ellipse(surf, DIRT_LIGHT, (x+1, y, 4, 2))
    
    # 边缘（略微高起）
    pygame.draw.rect(surf, DIRT_LIGHT, (0, 0, size, 2))
    pygame.draw.rect(surf, DIRT_LIGHT, (0, 0, 2, size))
    
    # 中心路径线（稍亮）
    center = size // 2
    pygame.draw.line(surf, DIRT_LIGHT, (4, center-2), (size-4, center-2), 1)
    pygame.draw.line(surf, DIRT_LIGHT, (4, center+2), (size-4, center+2), 1)
    
    return surf


# ========== 资源矿点贴图 ==========

def create_iron_ore() -> pygame.Surface:
    """创建铁矿石贴图"""
    surf = _create_surface()
    size = TILE_SIZE
    
    # 地面基础
    pygame.draw.rect(surf, DIRT_BASE, (0, 0, size, size))
    
    # 岩石
    rock_color = (100, 100, 120)
    highlight = (130, 130, 150)
    shadow = (70, 70, 90)
    
    # 主岩石
    pygame.draw.ellipse(surf, rock_color, (4, 6, size-8, size-10))
    pygame.draw.ellipse(surf, highlight, (6, 7, size-12, 6))
    pygame.draw.ellipse(surf, shadow, (4, size-8, size-8, 4))
    
    # 金属光泽
    pygame.draw.ellipse(surf, (150, 150, 170), (8, 10, 6, 4))
    pygame.draw.ellipse(surf, (140, 140, 160), (size-12, 14, 5, 3))
    
    # 裂缝
    pygame.draw.line(surf, (60, 60, 80), (10, 12), (14, 18), 1)
    pygame.draw.line(surf, (60, 60, 80), (size-10, 14), (size-14, 20), 1)
    
    return surf


def create_copper_ore() -> pygame.Surface:
    """创建铜矿石贴图"""
    surf = _create_surface()
    size = TILE_SIZE
    
    # 地面基础
    pygame.draw.rect(surf, DIRT_BASE, (0, 0, size, size))
    
    # 铜色岩石
    rock_color = (180, 100, 50)
    highlight = (210, 130, 80)
    shadow = (140, 70, 30)
    
    pygame.draw.ellipse(surf, rock_color, (3, 5, size-6, size-8))
    pygame.draw.ellipse(surf, highlight, (5, 6, size-10, 5))
    pygame.draw.ellipse(surf, shadow, (3, size-7, size-6, 4))
    
    # 铜光泽
    pygame.draw.ellipse(surf, (230, 160, 100), (7, 9, 7, 4))
    pygame.draw.ellipse(surf, (220, 150, 90), (size-11, 13, 6, 3))
    
    # 绿色铜锈
    pygame.draw.ellipse(surf, (100, 140, 100), (12, 16, 4, 3))
    pygame.draw.ellipse(surf, (80, 120, 80), (size-10, 10, 3, 2))
    
    return surf


def create_coal_ore() -> pygame.Surface:
    """创建煤矿贴图"""
    surf = _create_surface()
    size = TILE_SIZE
    
    # 地面基础
    pygame.draw.rect(surf, DIRT_DARK, (0, 0, size, size))
    
    # 黑色煤炭
    coal_color = (30, 30, 30)
    highlight = (60, 60, 60)
    
    # 多块煤炭
    coals = [
        (6, 8, 10, 8),
        (16, 6, 8, 10),
        (8, 16, 12, 8),
        (size-10, 14, 8, 8)
    ]
    
    for x, y, w, h in coals:
        pygame.draw.ellipse(surf, coal_color, (x, y, w, h))
        pygame.draw.ellipse(surf, highlight, (x+1, y+1, w-2, 3))
    
    # 光泽
    pygame.draw.ellipse(surf, (80, 80, 80), (8, 10, 4, 2))
    pygame.draw.ellipse(surf, (70, 70, 70), (18, 8, 3, 2))
    
    return surf


# ========== 敌人贴图 ==========

def create_enemy(enemy_type: str = "basic") -> pygame.Surface:
    """创建敌人贴图
    
    Args:
        enemy_type: 敌人类型（basic/fast/tank）
    """
    surf = _create_surface()
    size = TILE_SIZE
    center = size // 2
    
    # 根据类型设置颜色
    colors = {
        "basic": ((255, 50, 50), (200, 30, 30)),      # 红色敌人
        "fast": ((255, 165, 0), (200, 120, 0)),       # 橙色快速敌人
        "tank": ((100, 50, 50), (70, 30, 30))         # 深红坦克敌人
    }
    
    main_color, dark_color = colors.get(enemy_type, colors["basic"])
    
    # 根据类型绘制不同形状
    if enemy_type == "basic":
        # 圆形敌人（基础）
        pygame.draw.circle(surf, dark_color, (center, center), center - 3)
        pygame.draw.circle(surf, main_color, (center, center), center - 5)
        
        # 添加眼睛（愤怒的表情）
        pygame.draw.circle(surf, (255, 255, 255), (center - 4, center - 4), 3)
        pygame.draw.circle(surf, (255, 255, 255), (center + 4, center - 4), 3)
        pygame.draw.circle(surf, (0, 0, 0), (center - 4, center - 4), 2)
        pygame.draw.circle(surf, (0, 0, 0), (center + 4, center - 4), 2)
        
        # 添加边框
        pygame.draw.circle(surf, (20, 20, 20), (center, center), center - 3, 2)
        
    elif enemy_type == "fast":
        # 菱形敌人（快速）
        points = [
            (center, 3),              # 上
            (size - 3, center),       # 右
            (center, size - 3),       # 下
            (3, center)               # 左
        ]
        pygame.draw.polygon(surf, dark_color, points)
        
        # 内部菱形
        inner_points = [
            (center, 6),
            (size - 6, center),
            (center, size - 6),
            (6, center)
        ]
        pygame.draw.polygon(surf, main_color, inner_points)
        
        # 添加眼睛
        pygame.draw.circle(surf, (255, 255, 255), (center - 3, center - 2), 2)
        pygame.draw.circle(surf, (255, 255, 255), (center + 3, center - 2), 2)
        pygame.draw.circle(surf, (0, 0, 0), (center - 3, center - 2), 1)
        pygame.draw.circle(surf, (0, 0, 0), (center + 3, center - 2), 1)
        
        # 边框
        pygame.draw.polygon(surf, (20, 20, 20), points, 2)
        
    elif enemy_type == "tank":
        # 六边形敌人（坦克）
        import math
        points = []
        for i in range(6):
            angle = math.radians(60 * i - 30)
            x = center + int((center - 3) * math.cos(angle))
            y = center + int((center - 3) * math.sin(angle))
            points.append((x, y))
        pygame.draw.polygon(surf, dark_color, points)
        
        # 内部六边形
        inner_points = []
        for i in range(6):
            angle = math.radians(60 * i - 30)
            x = center + int((center - 6) * math.cos(angle))
            y = center + int((center - 6) * math.sin(angle))
            inner_points.append((x, y))
        pygame.draw.polygon(surf, main_color, inner_points)
        
        # 添加装甲纹理
        pygame.draw.circle(surf, dark_color, (center, center), 4)
        pygame.draw.circle(surf, (50, 20, 20), (center, center), 3)
        
        # 边框
        pygame.draw.polygon(surf, (20, 20, 20), points, 2)
    
    return surf


# ========== 贴图缓存 ==========

# ========== 贴图路径配置 ==========
ASSETS_DIR = os.path.join(os.path.dirname(__file__), "assets", "sprites")

# 方向名称映射（8方向）
DIR_NAMES_8 = {
    0: "up",
    1: "up_right",
    2: "right",
    3: "down_right",
    4: "down",
    5: "down_left",
    6: "left",
    7: "up_left"
}

# 方向名称映射（4方向）
DIR_NAMES_4 = {
    0: "up",
    1: "right",
    2: "down",
    3: "left"
}

# 方向名称映射（4方向逆时针旋转90度）
DIR_NAMES_4_ROTATED = {
    0: "left",    # 上 → 逆时针90° → 左
    1: "up",      # 右 → 逆时针90° → 上
    2: "right",   # 下 → 逆时针90° → 右
    3: "down"     # 左 → 逆时针90° → 下
}

# 贴图路径映射
SPRITE_PATH_MAP = {
    # 塔类（8方向）
    'basic_tower': ('towers', 'tower_basic', DIR_NAMES_8),
    'rapid_tower': ('towers', 'tower_rapid', DIR_NAMES_8),
    'sniper_tower': ('towers', 'tower_sniper', DIR_NAMES_8),
    'electric_tower': ('towers', 'tower_electric', DIR_NAMES_8),
    # 机器类（4方向）
    'miner': ('machines', 'machine_miner', DIR_NAMES_4_ROTATED),
    'ammo_factory': ('machines', 'machine_ammo_factory', DIR_NAMES_4_ROTATED),
    'generator': ('machines', 'machine_generator', DIR_NAMES_4_ROTATED),
    # 储物桶（4方向）
    'bucket': ('containers', 'container_bucket', DIR_NAMES_4),
    # 传送带（4方向，特殊命名）
    'conveyor': ('conveyors', 'conveyor', None),
    # 无方向机器
    'power_pole': ('machines', 'machine_power_pole', None),
    # 矿石
    'iron_ore': ('ores', 'ore_iron', None),
    'copper_ore': ('ores', 'ore_copper', None),
    'coal_ore': ('ores', 'ore_coal', None),
    # 地面
    'grass': ('terrain', 'terrain_grass', None),
    'path': ('terrain', 'terrain_path', None),
    # 敌人
    'enemy_basic': ('enemies', 'enemy_basic', None),
    'enemy_fast': ('enemies', 'enemy_fast', None),
    'enemy_tank': ('enemies', 'enemy_tank', None),
    # 物品
    'item_ammo': ('items', 'item_ammo', None),
}

_sprite_cache = {}

def get_sprite(name: str, **kwargs) -> pygame.Surface:
    """获取贴图（带缓存）
    
    优先从外部文件加载，文件不存在时回退到动态生成
    
    Args:
        name: 贴图名称
        **kwargs: 额外参数（如 direction）
    
    Returns:
        贴图表面
    """
    cache_key = name
    if 'direction' in kwargs:
        cache_key += f"_dir{kwargs['direction']}"
    elif 'enemy_type' in kwargs:
        cache_key += f"_enemy{kwargs['enemy_type']}"
    
    if cache_key in _sprite_cache:
        return _sprite_cache[cache_key]
    
    # 尝试从外部文件加载
    sprite = _load_sprite_from_file(name, **kwargs)
    if sprite:
        _sprite_cache[cache_key] = sprite
        return sprite
    
    # 文件不存在，回退到动态生成
    sprite = _generate_sprite(name, **kwargs)
    _sprite_cache[cache_key] = sprite
    return sprite


def _load_sprite_from_file(name: str, **kwargs) -> pygame.Surface:
    """从外部文件加载贴图
    
    Args:
        name: 贴图名称
        **kwargs: 额外参数（如 direction）
    
    Returns:
        贴图表面，如果加载失败返回 None
    """
    if name not in SPRITE_PATH_MAP:
        return None
    
    dir_name, file_prefix, dir_names = SPRITE_PATH_MAP[name]
    
    # 构建文件名
    if dir_names and 'direction' in kwargs:
        direction = kwargs['direction']
        if direction in dir_names:
            filename = f"{file_prefix}_{dir_names[direction]}.png"
        else:
            return None
    elif name == 'conveyor' and 'direction' in kwargs:
        direction = kwargs['direction']
        filename = f"{file_prefix}_dir{direction}_1.png"
    else:
        filename = f"{file_prefix}.png"
    
    # 构建完整路径
    filepath = os.path.join(ASSETS_DIR, dir_name, filename)
    
    if os.path.exists(filepath):
        try:
            return pygame.image.load(filepath).convert_alpha()
        except Exception as e:
            print(f"警告: 加载贴图失败 {filepath}: {e}")
            return None
    
    return None


def _generate_sprite(name: str, **kwargs) -> pygame.Surface:
    """动态生成贴图（回退方案）"""
    sprite_map = {
        'basic_tower': lambda: create_basic_tower(kwargs.get('direction', 0)),
        'rapid_tower': lambda: create_rapid_tower(kwargs.get('direction', 0)),
        'sniper_tower': lambda: create_sniper_tower(kwargs.get('direction', 0)),
        'electric_tower': lambda: create_electric_tower(kwargs.get('direction', 0)),
        'miner': lambda: create_miner((kwargs.get('direction', 2) + 3) % 4),
        'ammo_factory': lambda: create_ammo_factory((kwargs.get('direction', 2) + 3) % 4),
        'generator': lambda: create_generator((kwargs.get('direction', 2) + 3) % 4),
        'power_pole': create_power_pole,
        'conveyor': lambda: create_conveyor(kwargs.get('direction', 0)),
        'bucket': lambda: create_bucket(kwargs.get('direction', 2)),
        'iron_ore': create_iron_ore,
        'copper_ore': create_copper_ore,
        'coal_ore': create_coal_ore,
        'grass': create_grass,
        'path': create_path,
        'enemy': lambda: create_enemy(kwargs.get('enemy_type', 'basic')),
    }
    
    if name not in sprite_map:
        surf = _create_surface()
        pygame.draw.rect(surf, (255, 0, 255), (0, 0, TILE_SIZE, TILE_SIZE))
        return surf
    
    return sprite_map[name]()


def preload_all_sprites() -> None:
    """预加载所有贴图"""
    # 预加载塔类（8方向）
    for tower_name in ['basic_tower', 'rapid_tower', 'sniper_tower', 'electric_tower']:
        for direction in range(8):
            get_sprite(tower_name, direction=direction)
    
    # 预加载机器类（4方向）
    for machine_name in ['miner', 'ammo_factory', 'generator', 'bucket']:
        for direction in range(4):
            get_sprite(machine_name, direction=direction)
    
    # 预加载无方向机器
    get_sprite('power_pole')
    
    # 预加载传送带（4方向）
    for direction in range(4):
        get_sprite('conveyor', direction=direction)
    
    # 预加载矿石
    get_sprite('iron_ore')
    get_sprite('copper_ore')
    get_sprite('coal_ore')
    
    # 预加载地面
    get_sprite('grass')
    get_sprite('path')
    
    # 预加载敌人
    get_sprite('enemy_basic')
    get_sprite('enemy_fast')
    get_sprite('enemy_tank')
    
    # 预加载物品
    get_sprite('item_ammo')


def clear_cache() -> None:
    """清除贴图缓存"""
    _sprite_cache.clear()
