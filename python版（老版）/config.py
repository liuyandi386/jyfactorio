#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""配置文件

包含游戏中所有的常量配置参数
"""

# ========== 游戏窗口设置 ==========
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
SCREEN_TITLE = "Factorio风格塔防游戏"

# ========== 地图设置 ==========
TILE_SIZE = 32
MAP_WIDTH = 50
MAP_HEIGHT = 50

# ========== 摄像机设置 ==========
CAMERA_SPEED = 5
ZOOM_MIN = 0.5
ZOOM_MAX = 2.0
ZOOM_SPEED = 0.1

# ========== 游戏设置 ==========
FPS = 60
GAME_SPEED = 1.0
INITIAL_GOLD = 500

# ========== 波次设置 ==========
WAVE_INTERVAL = 10
WAVE_ENEMIES = 5
INITIAL_COUNTDOWN = 180

# ========== 电力设置 ==========
POWER_RANGE = 150
GENERATOR_POWER = 100

# ========== 路径点（敌人行走路线） ==========
PATH_POINTS = [
    (0, 15),
    (10, 15),
    (10, 5),
    (25, 5),
    (25, 20),
    (40, 20),
    (40, 35),
    (20, 35),
    (20, 45),
    (49, 45)
]