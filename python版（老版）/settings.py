#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""游戏配置文件

包含游戏所有的全局配置参数
从 config.py 和 data.py 导入配置，保持向后兼容性
"""

# 从新配置文件导入
from config import *
from data import *

# ========== 向后兼容的旧变量名 ==========

# 塔设置（旧格式，用于兼容现有代码）
TOWER_RANGE = {k: v["range"] for k, v in TOWER_STATS.items()}
TOWER_DAMAGE = {k: v["damage"] for k, v in TOWER_STATS.items()}
TOWER_FIRE_RATE = {k: v["fire_rate"] for k, v in TOWER_STATS.items()}
TOWER_BULLET_SPEED = {k: v["bullet_speed"] for k, v in TOWER_STATS.items()}

# 敌人设置（旧格式，用于兼容现有代码）
ENEMY_HEALTH = {k: v["health"] for k, v in ENEMY_STATS.items()}
ENEMY_SPEED = {k: v["speed"] for k, v in ENEMY_STATS.items()}
ENEMY_REWARD = {k: v["reward"] for k, v in ENEMY_STATS.items()}

# 塔成本（旧格式，用于兼容现有代码）
TOWER_COST = {
    "basic": BUILDING_COSTS["basic_tower"].get("iron_ore", 0),
    "rapid": BUILDING_COSTS["rapid_tower"].get("iron_ore", 0),
    "sniper": BUILDING_COSTS["sniper_tower"].get("iron_ore", 0)
}