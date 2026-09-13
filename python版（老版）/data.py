#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""数据配置文件

包含游戏中所有的数据配置，如资源类型、配方、建筑信息等
"""

# ========== 资源类型 ==========
RESOURCE_TYPES = {
    "iron_ore": {"name": "铁矿石", "color": (120, 120, 140), "mining_time": 2.0},
    "copper_ore": {"name": "铜矿石", "color": (180, 100, 50), "mining_time": 2.0},
    "coal": {"name": "煤矿", "color": (40, 40, 40), "mining_time": 1.5},
    "ammo": {"name": "弹药", "color": (255, 255, 0), "mining_time": 0.0}
}

# ========== 配方设置 ==========
RECIPES = {
    "ammo": {
        "name": "弹药",
        "ingredients": {"iron_ore": 2, "copper_ore": 1},
        "output": {"ammo": 1},
        "craft_time": 2.0
    },
    "power": {
        "name": "电力",
        "ingredients": {"coal": 1},
        "output": {"power": 3000},
        "craft_time": 3.0
    }
}

# ========== 建筑成本 ==========
BUILDING_COSTS = {
    "miner": {"iron_ore": 10, "copper_ore": 5},
    "conveyor": {"iron_ore": 5},
    "chest": {"iron_ore": 8, "wood": 2},
    "bucket": {"iron_ore": 5},
    "generator": {"iron_ore": 20, "copper_ore": 10, "coal": 10},
    "power_pole": {"iron_ore": 5, "copper_ore": 2},
    "power_generator": {"iron_ore": 30, "copper_ore": 15},
    "capacitor": {"iron_ore": 25, "copper_ore": 20},
    "power_wire": {"iron_ore": 3, "copper_ore": 2},
    "splitter": {"iron_ore": 8, "copper_ore": 5},
    "ammo_factory": {"iron_ore": 30, "copper_ore": 15},
    "basic_tower": {"iron_ore": 15, "copper_ore": 5},
    "electric_tower": {"iron_ore": 25, "copper_ore": 20},
    "rapid_tower": {"iron_ore": 20, "copper_ore": 10},
    "sniper_tower": {"iron_ore": 25, "copper_ore": 15}
}

# ========== 建筑名称 ==========
BUILDING_NAMES = {
    "basic_tower": "基础塔",
    "electric_tower": "电力塔",
    "rapid_tower": "速射塔",
    "sniper_tower": "狙击塔",
    "miner": "采矿机",
    "conveyor": "传送带",
    "chest": "箱子",
    "bucket": "储物桶",
    "ammo_factory": "弹药制造机",
    "generator": "发电机",
    "power_pole": "电线杆",
    "power_generator": "燃煤发电机",
    "capacitor": "电容库",
    "power_wire": "电力线缆",
    "splitter": "物品分流器"
}

# ========== 塔配置 ==========
TOWER_STATS = {
    "basic": {
        "range": 150,
        "damage": 20,
        "fire_rate": 1.0,
        "bullet_speed": 10.0
    },
    "rapid": {
        "range": 120,
        "damage": 10,
        "fire_rate": 3.0,
        "bullet_speed": 15.0
    },
    "sniper": {
        "range": 250,
        "damage": 50,
        "fire_rate": 0.5,
        "bullet_speed": 8.0
    },
    "electric": {
        "range": 180,
        "damage": 25,
        "fire_rate": 2.0,
        "bullet_speed": 12.0
    }
}

# ========== 敌人配置 ==========
ENEMY_STATS = {
    "basic": {
        "health": 100,
        "speed": 1.5,
        "reward": 20
    },
    "fast": {
        "health": 60,
        "speed": 3.0,
        "reward": 30
    },
    "tank": {
        "health": 300,
        "speed": 0.8,
        "reward": 50
    }
}

# ========== 颜色配置 ==========
COLORS = {
    "background": (40, 40, 40),
    "tile_grass": (34, 139, 34),
    "tile_path": (139, 119, 101),
    "tile_water": (30, 144, 255),
    "tower_color": (100, 100, 100),
    "enemy_color": (255, 0, 0),
    "bullet_color": (255, 255, 0),
    "text_color": (255, 255, 255),
    "gold_color": (255, 215, 0),
    "iron_ore": (120, 120, 140),
    "copper_ore": (180, 100, 50),
    "coal_ore": (40, 40, 40),
    "miner": (255, 165, 0),
    "conveyor": (100, 149, 237),
    "chest": (139, 90, 43),
    "generator": (70, 70, 70),
    "power_pole": (160, 82, 45)
}