#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""工业电力系统模块

采用 EU/t 单位，完整流程：
  燃煤发电机(消耗煤炭/测试无限燃料) → EU/t 电力 → 电线传输 → 电容库储存 → 电力塔消耗

模块：
  - PowerGenerator: 燃煤发电机实体（消耗煤炭，产生 EU/t）
  - Capacitor: 电容库实体（储存电力）
  - PowerWire: 电线实体（连接建筑，传输电力，支持方向面配置）
  - PowerNetwork: 电力网络计算（每tick统计发电/储存/消耗）
  - PowerManager: 电力管理器（管理所有电网）
  - PowerTower: 电力塔适配器（接入电网的电力塔）
"""

# 电线方向面配置常量（测试程序用）
FACE_NONE = 0
FACE_INPUT = 1
FACE_TRANSFER = 2
FACE_OUTPUT = 3

DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

DIR_OFFSETS = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}
DIR_OPPOSITE = {0: 2, 1: 3, 2: 0, 3: 1}

from .generator import PowerGenerator
from .capacitor import Capacitor
from .power_wire import PowerWire
from .power_network import PowerNetwork
from .power_manager import PowerManager
from .power_tower import PowerTower
