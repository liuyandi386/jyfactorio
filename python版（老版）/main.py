#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""游戏主入口

整合所有模块，实现Factorio风格的塔防游戏
UI风格：采用工业扁平化浅色主题风格
版本: alpha v1.0.4
"""

import pygame
import sys

sys.path.insert(0, '.')

# ========== 游戏版本 ==========
VERSION = "alpha v1.0.4"

from settings import *
from core.Game import Game
from core.Scene import Scene
from core.Camera import Camera
from core.PowerGrid import PowerGrid
from maps.GameMap import GameMap
from maps.GridManager import GridManager, GRID_WIDTH, GRID_HEIGHT, CELL_BUCKET, BUCKET_CAPACITY, DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT
from entities.Tower import Tower
from entities.Enemy import Enemy
from entities.Bullet import Bullet
from entities.OreDeposit import OreDeposit
from entities.Miner import Miner
from entities.ConveyorBelt import ConveyorBelt
from entities.Bucket import Bucket
from entities.Generator import Generator
from entities.PowerPole import PowerPole
from entities.AmmoFactory import AmmoFactory
from entities.Splitter import Splitter, SPLIT_FACE_NONE, SPLIT_FACE_INPUT, SPLIT_FACE_OUTPUT, DIR_UP as S_DIR_UP, DIR_RIGHT as S_DIR_RIGHT, DIR_DOWN as S_DIR_DOWN, DIR_LEFT as S_DIR_LEFT, OPPOSITE_DIR as S_OPPOSITE_DIR, DIR_OFFSETS as S_DIR_OFFSETS, FACE_COLORS as S_FACE_COLORS, FACE_LABELS as S_FACE_LABELS

from ui.GameUI import GameUI
from save_system import save_game, load_game, has_save

# 工业电力系统
from systems.power import (PowerManager, PowerGenerator, Capacitor, PowerWire, PowerTower,
                           FACE_NONE, FACE_INPUT, FACE_TRANSFER, FACE_OUTPUT,
                           DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT,
                           DIR_OFFSETS, DIR_OPPOSITE)


# ========== UI 颜色主题 (来自conveyor_test) ==========
UI_BG = (43, 43, 43)           # #2b2b2b 主背景
UI_BG_DARK = (26, 26, 26)      # #1a1a1a 深色背景
UI_BG_MEDIUM = (51, 51, 51)    # #333 中等背景
UI_ACCENT = (76, 175, 80)       # 绿色强调
UI_TEXT = (255, 255, 255)      # 白色文字
UI_TEXT_DIM = (200, 200, 200)  # 灰色文字
UI_BORDER = (80, 80, 100)      # 边框色

# 方向颜色
DIR_COLORS = {
    0: (76, 175, 80),    # 上-绿色
    1: (233, 30, 99),    # 右-粉色
    2: (33, 1, 243),   # 下-蓝色
    3: (255, 152, 0)     # 左-橙色
}


class GameScene(Scene):
    """游戏主场景"""

    def __init__(self, game):
        """初始化游戏场景

        Args:
            game: 游戏主对象引用
        """
        super().__init__(game)

        # ========== 核心系统 ==========
        self.camera = Camera()
        self.map = GameMap()
        self.power_grid = PowerGrid()

        # 工业电力系统管理器
        self.power_manager = PowerManager()

        # ========== 传送带和储物桶系统 (来自conveyor_test) ==========
        self.grid_manager = GridManager(self)

        # ========== 游戏UI (采用conveyor_test风格) ==========
        self.game_ui = GameUI(self)

        # ========== 精灵组 ==========
        self.towers = pygame.sprite.Group()
        self.enemies = pygame.sprite.Group()
        self.bullets = pygame.sprite.Group()
        self.ore_deposits = pygame.sprite.Group()
        self.miners = pygame.sprite.Group()
        self.conveyors = pygame.sprite.Group()
        
        self.buckets = pygame.sprite.Group()
        self.generators = pygame.sprite.Group()
        self.power_poles = pygame.sprite.Group()
        self.ammo_factories = pygame.sprite.Group()

        # 工业电力系统精灵组
        self.power_generators = pygame.sprite.Group()     # 燃煤发电机
        self.power_capacitors = pygame.sprite.Group()     # 电容库
        self.power_wires = pygame.sprite.Group()          # 电线
        
        self.splitters = pygame.sprite.Group()             # 物品分流器

        # ========== 建筑放置状态 ==========
        self.selected_building = None
        self.building_category = "tower"
        self.is_previewing = False
        self.preview_position = None
        self.preview_direction = 0

        # ========== 电线面配置编辑 ==========
        self.face_edit_target = None      # 正在编辑面配置的实体 (PowerWire/Splitter)
        self.face_edit_type = None        # "power_wire" / "splitter"
        self.face_edit_buttons = {}       # 方向按钮rects方向按钮 {direction: pygame.Rect}

        # ========== 玩家资源 (无限资源模式) ==========
        self.player_inventory = {
            "iron_ore": 999999,
            "copper_ore": 999999,
            "coal": 999999,
            "ammo": 999999
        }

        # ========== 波次管理 ==========
        self.current_wave = 1
        self.enemies_spawned = 0
        self.enemies_per_wave = WAVE_ENEMIES
        self.spawn_timer = 0
        self.spawn_interval = 1.0

        # ========== 游戏状态 ==========
        self.game_over = False
        self.paused = False
        self.countdown_active = True
        self.countdown_time = INITIAL_COUNTDOWN
        self.game_state = "countdown"
        self.wave_timer = 0

        # ========== 按键状态 ==========
        self.keys = {'w': False, 'a': False, 's': False, 'd': False}

        # ========== 初始化 ==========
        self._generate_ore_deposits()

    def _generate_ore_deposits(self):
        """生成初始矿点分布"""
        import random
        random.seed(42)

        for _ in range(8):
            x = random.randint(2, 20) * TILE_SIZE
            y = random.randint(2, 48) * TILE_SIZE
            if not self.map.is_path(int(x // TILE_SIZE), int(y // TILE_SIZE)):
                ore = OreDeposit(x, y, "iron_ore")
                self.ore_deposits.add(ore)

        for _ in range(6):
            x = random.randint(22, 40) * TILE_SIZE
            y = random.randint(2, 48) * TILE_SIZE
            if not self.map.is_path(int(x // TILE_SIZE), int(y // TILE_SIZE)):
                ore = OreDeposit(x, y, "copper_ore")
                self.ore_deposits.add(ore)

        for _ in range(5):
            x = random.randint(30, 48) * TILE_SIZE
            y = random.randint(2, 48) * TILE_SIZE
            if not self.map.is_path(int(x // TILE_SIZE), int(y // TILE_SIZE)):
                ore = OreDeposit(x, y, "coal")
                self.ore_deposits.add(ore)

    def on_enter(self):
        """进入场景时调用"""
        pass

    def handle_event(self, event):
        """处理事件"""
        if self.game_over:
            return

        # 优先处理UI事件
        if self.game_ui.handle_event(event):
            return

        if event.type == pygame.KEYDOWN:
            key = event.key

            if key == pygame.K_w:
                self.keys['w'] = True
            elif key == pygame.K_a:
                self.keys['a'] = True
            elif key == pygame.K_s:
                self.keys['s'] = True
            elif key == pygame.K_d:
                self.keys['d'] = True
            elif key == pygame.K_SPACE:
                self.paused = not self.paused
            elif key == pygame.K_r:
                self._rotate_building()
            elif key == pygame.K_1:
                self.game_ui.select_building("basic_tower")
            elif key == pygame.K_2:
                self.game_ui.select_building("electric_tower")
            elif key == pygame.K_3:
                self.game_ui.select_building("miner")
            elif key == pygame.K_4:
                self.game_ui.select_building("conveyor")
            elif key == pygame.K_5:
                self.game_ui.select_building("bucket")
            elif key == pygame.K_7:
                self.game_ui.select_building("ammo_factory")
            elif key == pygame.K_8:
                self.game_ui.select_building("generator")
            elif key == pygame.K_9:
                self.game_ui.select_building("power_pole")
            elif key == pygame.K_0:
                self.game_ui.select_building("power_generator")
            elif key == pygame.K_MINUS:
                self.game_ui.select_building("capacitor")
            elif key == pygame.K_EQUALS:
                self.game_ui.select_building("power_wire")
            elif key == pygame.K_BACKSLASH:
                self.game_ui.select_building("splitter")
            elif key == pygame.K_ESCAPE:
                self.face_edit_target = None
                self.face_edit_type = None
            
            elif key == pygame.K_TAB:
                self._cycle_building()
            elif key == pygame.K_u:
                # 按U键手动生成一个敌人
                self.spawn_enemy()
            elif key == pygame.K_F5:
                # 按F5保存游戏
                self.save_game()
            elif key == pygame.K_F9:
                # 按F9加载游戏
                self.load_game()
            elif key == pygame.K_DELETE:
                # Delete键删除鼠标指向的建筑并返还材料
                self._delete_entity_at_cursor()

        elif event.type == pygame.KEYUP:
            key = event.key
            if key == pygame.K_w:
                self.keys['w'] = False
            elif key == pygame.K_a:
                self.keys['a'] = False
            elif key == pygame.K_s:
                self.keys['s'] = False
            elif key == pygame.K_d:
                self.keys['d'] = False

        elif event.type == pygame.MOUSEBUTTONDOWN:
            # 面配置编辑器激活时优先处理
            if self.face_edit_target is not None:
                if event.button == 1:
                    self._handle_face_editor_click(event.pos)
                return
            # 方向选择悬浮窗激活时，鼠标点击只应由悬浮窗处理
            if self.game_ui.direction_popup_active:
                return
            if event.button == 1:
                self._handle_left_click(event.pos)
            elif event.button == 3:
                self._handle_right_click(event.pos)
            elif event.button == 4:
                self.camera.zoom_in()
            elif event.button == 5:
                self.camera.zoom_out()

    def _cycle_building(self):
        """循环切换建筑类型"""
        buildings = [
            "basic_tower", "electric_tower", "rapid_tower", "sniper_tower",
            "miner", "conveyor", "bucket", "ammo_factory",
            "generator", "power_pole", "power_generator", "capacitor", "power_wire", "splitter"
        ]

        if self.selected_building is None:
            self.selected_building = buildings[0]
        else:
            try:
                idx = buildings.index(self.selected_building)
                self.selected_building = buildings[(idx + 1) % len(buildings)]
            except ValueError:
                self.selected_building = buildings[0]

        self.game_ui.select_building(self.selected_building)

    def _rotate_building(self):
        """旋转当前选中的建筑"""
        pass

    def _handle_left_click(self, pos):
        """处理左键点击"""
        world_x, world_y = self.camera.screen_to_world(pos[0], pos[1])
        
        if self.selected_building is None:
            return

        tile_x = int(world_x // TILE_SIZE)
        tile_y = int(world_y // TILE_SIZE)

        if self.selected_building == "conveyor":
            self._handle_conveyor_click(tile_x, tile_y, pos)
            return

        # 需要方向选择的建筑显示悬浮窗
        buildings_with_direction = ["miner", "bucket", "ammo_factory", 
                                    "basic_tower", "rapid_tower", "sniper_tower", "electric_tower",
                                    "power_generator"]
        if self.selected_building in buildings_with_direction:
            if not self.map.can_place_tower(tile_x * TILE_SIZE, tile_y * TILE_SIZE):
                return
            cost = BUILDING_COSTS.get(self.selected_building, {})
            if not self._can_afford(cost):
                self.game_ui.show_toast("资源不足!")
                return
            # 显示方向选择悬浮窗
            self.game_ui.show_direction_popup(
                self.selected_building,
                (tile_x, tile_y),
                pos
            )
            return

        # 无需方向的建筑直接放置（电容库、电力线缆等）
        buildings_direct = ["capacitor", "power_wire", "splitter"]
        if self.selected_building in buildings_direct:
            if not self.map.can_place_tower(tile_x * TILE_SIZE, tile_y * TILE_SIZE):
                return
            cost = BUILDING_COSTS.get(self.selected_building, {})
            if not self._can_afford(cost):
                self.game_ui.show_toast("资源不足!")
                return
            entity = self._place_building(
                tile_x * TILE_SIZE, tile_y * TILE_SIZE, self.selected_building, 0
            )
            if entity:
                self._deduct_cost(cost)
            return

        if not self.is_previewing:
            if not self.map.can_place_tower(tile_x * TILE_SIZE, tile_y * TILE_SIZE):
                return
            cost = BUILDING_COSTS.get(self.selected_building, {})
            if not self._can_afford(cost):
                self.game_ui.show_toast("资源不足!")
                return
            self.is_previewing = True
            self.preview_position = (tile_x, tile_y)
            self.preview_direction = 0
            return

        if self.is_previewing and self.preview_position:
            preview_tile_x, preview_tile_y = self.preview_position
            dx = tile_x - preview_tile_x
            dy = tile_y - preview_tile_y

            if abs(dx) > abs(dy):
                self.preview_direction = 0 if dx > 0 else 2
            else:
                self.preview_direction = 1 if dy > 0 else 3

            cost = BUILDING_COSTS.get(self.selected_building, {})
            entity = self._place_building(
                preview_tile_x * TILE_SIZE,
                preview_tile_y * TILE_SIZE,
                self.selected_building,
                self.preview_direction
            )

            if entity:  # entity不为None表示成功
                self._deduct_cost(cost)

            self.is_previewing = False
            self.preview_position = None

    def place_building_with_direction(self, building: str, tile_pos: tuple, direction: int):
        """使用指定方向放置建筑（从悬浮窗调用）

        Args:
            building: 建筑类型
            tile_pos: 瓦片位置 (tile_x, tile_y)
            direction: 方向
        """
        tile_x, tile_y = tile_pos

        cost = BUILDING_COSTS.get(building, {})
        entity = self._place_building(
            tile_x * TILE_SIZE,
            tile_y * TILE_SIZE,
            building,
            direction
        )

        if entity:  # entity不为None表示成功
            self._deduct_cost(cost)

    def _handle_conveyor_click(self, tile_x, tile_y, mouse_pos=None):
        """处理传送带放置（支持单格和两点式）
        
        Args:
            tile_x: 瓦片X坐标
            tile_y: 瓦片Y坐标
            mouse_pos: 鼠标在屏幕上的位置，用于计算单格放置方向
        """
        cost = BUILDING_COSTS.get("conveyor", {})
        if not self._can_afford(cost):
            return

        if not (0 <= tile_x < GRID_WIDTH and 0 <= tile_y < GRID_HEIGHT):
            return

        # 检查是否有起点
        if self.grid_manager.placing_start is not None:
            start_x, start_y = self.grid_manager.placing_start
            
            # 如果点击的是起点本身，走单点放置
            if start_x == tile_x and start_y == tile_y:
                self.grid_manager.placing_start = None
                # 单格放置模式：根据鼠标位置计算方向
                if mouse_pos:
                    world_x, world_y = self.camera.screen_to_world(mouse_pos[0], mouse_pos[1])
                    center_x = tile_x * TILE_SIZE + TILE_SIZE // 2
                    center_y = tile_y * TILE_SIZE + TILE_SIZE // 2
                    dx = world_x - center_x
                    dy = world_y - center_y
                    if abs(dx) > abs(dy):
                        direction = DIR_RIGHT if dx > 0 else DIR_LEFT
                    else:
                        direction = DIR_DOWN if dy > 0 else DIR_UP
                else:
                    direction = DIR_DOWN
                success, msg = self.grid_manager.place_conveyor_single(tile_x, tile_y, direction)
                if success:
                    self._place_building(tile_x * TILE_SIZE, tile_y * TILE_SIZE, "conveyor", direction)
                    self._deduct_cost(cost)
                else:
                    self.game_ui.show_toast(msg if msg else "放置失败!")
                return
            
            # 如果点击的不是起点，走两点式放置
            length = abs(tile_x - start_x) + abs(tile_y - start_y)
            total_cost = {k: v * max(1, length) for k, v in cost.items()}

            if not self._can_afford(total_cost):
                self.game_ui.show_toast("资源不足!")
                self.grid_manager.placing_start = None
                return

            success, msg = self.grid_manager.place_conveyor(start_x, start_y, tile_x, tile_y)

            if success:
                end_x = tile_x
                end_y = tile_y
                if start_x == end_x:
                    direction = DIR_DOWN if start_y < end_y else DIR_UP
                    min_y = min(start_y, end_y)
                    max_y = max(start_y, end_y)
                    for y in range(min_y, max_y + 1):
                        self._place_building(start_x * TILE_SIZE, y * TILE_SIZE, "conveyor", direction)
                elif start_y == end_y:
                    direction = DIR_RIGHT if start_x < end_x else DIR_LEFT
                    min_x = min(start_x, end_x)
                    max_x = max(start_x, end_x)
                    for x in range(min_x, max_x + 1):
                        self._place_building(x * TILE_SIZE, start_y * TILE_SIZE, "conveyor", direction)
                else:
                    direction_h = DIR_RIGHT if start_x < end_x else DIR_LEFT
                    direction_v = DIR_DOWN if start_y < end_y else DIR_UP
                    min_x = min(start_x, end_x)
                    max_x = max(start_x, end_x)
                    for x in range(min_x, max_x + 1):
                        self._place_building(x * TILE_SIZE, start_y * TILE_SIZE, "conveyor", direction_h)
                    min_y = min(start_y, end_y)
                    max_y = max(start_y, end_y)
                    for y in range(min_y + 1, max_y + 1):
                        self._place_building(end_x * TILE_SIZE, y * TILE_SIZE, "conveyor", direction_v)
                self._deduct_cost(total_cost)
            else:
                self.game_ui.show_toast(msg if msg else "放置失败!")

            self.grid_manager.placing_start = None
        else:
            # 没有起点，设置为起点
            self.grid_manager.placing_start = (tile_x, tile_y)

    def _handle_right_click(self, pos):
        """处理右键点击"""
        world_x, world_y = self.camera.screen_to_world(pos[0], pos[1])

        for miner in self.miners:
            if miner.rect.collidepoint(world_x, world_y):
                miner.rotate_direction()
                return

        for conveyor in self.conveyors:
            if conveyor.rect.collidepoint(world_x, world_y):
                conveyor.set_direction((conveyor.direction + 1) % 4)
                return

        # 右键电线：打开面配置编辑器
        for wire in self.power_wires:
            if wire.rect.collidepoint(world_x, world_y):
                self.face_edit_target = wire
                self.face_edit_type = "power_wire"
                return

        # 右键分流器：打开面配置编辑器
        for sp in self.splitters:
            if sp.rect.collidepoint(world_x, world_y):
                self.face_edit_target = sp
                self.face_edit_type = "splitter"
                return

    def _can_afford(self, cost):
        """检查是否有足够资源"""
        for item, amount in cost.items():
            if self.player_inventory.get(item, 0) < amount:
                return False
        return True

    def _deduct_cost(self, cost):
        """扣除资源"""
        for item, amount in cost.items():
            self.player_inventory[item] = self.player_inventory.get(item, 0) - amount

    def _is_position_occupied(self, x, y, building_type=None) -> bool:
        """检查位置是否已被占用
        
        Args:
            x: 世界坐标X
            y: 世界坐标Y
            building_type: 建筑类型（可选，用于特殊检查）
        
        Returns:
            True 如果位置已被占用
        """
        # 计算格子中心点
        center_x = x + TILE_SIZE // 2
        center_y = y + TILE_SIZE // 2
        
        # 检查塔
        for tower in self.towers:
            if tower.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查采矿机
        for miner in self.miners:
            if miner.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查传送带（传送带可以在同位置替换方向）
        if building_type != "conveyor":
            for conveyor in self.conveyors:
                if conveyor.rect.collidepoint(center_x, center_y):
                    return True
        
        # 检查储物桶
        for bucket in self.buckets:
            if bucket.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查发电机
        for generator in self.generators:
            if generator.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查电力杆
        for pole in self.power_poles:
            if pole.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查弹药制造机
        for factory in self.ammo_factories:
            if factory.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查燃煤发电机（工业电力系统）
        for pg in self.power_generators:
            if pg.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查电容库（工业电力系统）
        for cap in self.power_capacitors:
            if cap.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查电力线缆（工业电力系统）
        for wire in self.power_wires:
            if wire.rect.collidepoint(center_x, center_y):
                return True
        
        # 检查物品分流器
        for sp in self.splitters:
            if sp.rect.collidepoint(center_x, center_y):
                return True
        
        return False

    def _delete_entity_at_cursor(self):
        """删除鼠标指向的建筑并返还材料"""
        mouse_x, mouse_y = pygame.mouse.get_pos()
        world_x, world_y = self.camera.screen_to_world(mouse_x, mouse_y)

        # 按照从"实体"到"基础设施"的顺序检查
        entity_info = [
            (self.towers, "tower", lambda t: f"{t.tower_type}_tower"),
            (self.miners, "miner", lambda m: "miner"),
            (self.ammo_factories, "factory", lambda f: "ammo_factory"),
            (self.generators, "generator", lambda g: "generator"),
            (self.power_poles, "pole", lambda p: "power_pole"),
            (self.buckets, "bucket", lambda b: "bucket"),
            (self.conveyors, "conveyor", lambda c: "conveyor"),
            (self.power_generators, "power_gen", lambda pg: "power_generator"),
            (self.power_capacitors, "capacitor", lambda c: "capacitor"),
            (self.power_wires, "wire", lambda w: "power_wire"),
            (self.splitters, "splitter", lambda s: "splitter"),
        ]

        for group, entity_type, cost_key_fn in entity_info:
            for entity in list(group):
                if entity.rect.collidepoint(world_x, world_y):
                    # 获取成本键并返还材料
                    cost_key = cost_key_fn(entity)
                    cost = BUILDING_COSTS.get(cost_key, {})
                    for item, amount in cost.items():
                        self.player_inventory[item] = self.player_inventory.get(item, 0) + amount

                    # 电力系统注销
                    if entity_type == "power_gen":
                        self.power_manager.unregister_generator(entity)
                    elif entity_type == "capacitor":
                        self.power_manager.unregister_capacitor(entity)
                    elif entity_type == "wire":
                        self.power_manager.unregister_wire(entity)
                    elif entity_type == "tower":
                        if getattr(entity, 'tower_type', '') == "electric":
                            self.power_manager.unregister_tower(entity)

                    # 从精灵组移除
                    entity.kill()

                    # 传送带连接更新
                    if entity_type == "conveyor":
                        self._update_conveyor_connections()

                    # 刷新电线外观
                    self._rebuild_power_wire_appearances()

                    name = BUILDING_NAMES.get(cost_key, cost_key)
                    self.game_ui.show_toast(f"已拆除 {name}，返还材料")
                    return

    def _place_building(self, x, y, building_type, direction=0):
        """放置建筑"""
        # 检查位置是否已被占用
        if self._is_position_occupied(x, y, building_type):
            self.game_ui.show_toast("该位置已被占用!")
            return False
        
        if building_type in ["basic_tower", "rapid_tower", "sniper_tower", "electric_tower"]:
            tower_type = building_type.replace("_tower", "")
            tower = Tower(x, y, tower_type)
            tower.set_enemies(self.enemies)
            tower.set_bullets(self.bullets)
            # 初始弹药为0，需要通过弹药制造机供给
            self.towers.add(tower)
            self.power_grid.add_consumer(tower)

            # 电力塔接入工业电力系统
            if tower_type == "electric":
                self.power_manager.register_tower(tower)
                self._scan_wire_neighbors_for_tower(tower)
            return tower  # 返回实体对象而不是True

        elif building_type == "miner":
            target_ore = None
            for ore in self.ore_deposits:
                if ore.rect.collidepoint(x + TILE_SIZE // 2, y + TILE_SIZE // 2):
                    target_ore = ore
                    break
            if target_ore is None:
                return None
            miner = Miner(x, y, direction)
            miner.set_target_ore(target_ore)
            self.miners.add(miner)
            self.power_grid.add_consumer(miner)
            return miner  # 返回实体对象

        elif building_type == "conveyor":
            # 检查是否已有传送带在该位置，如果有则替换方向
            center_x = x + TILE_SIZE // 2
            center_y = y + TILE_SIZE // 2
            existing_conveyor = None
            for conveyor in self.conveyors:
                if conveyor.rect.collidepoint(center_x, center_y):
                    existing_conveyor = conveyor
                    break
            
            if existing_conveyor:
                # 替换方向
                existing_conveyor.set_direction(direction)
                return existing_conveyor  # 返回已存在的传送带
            else:
                # 创建新传送带
                conveyor = ConveyorBelt(x, y, direction)
                self.conveyors.add(conveyor)
                self._update_conveyor_connections()
                return conveyor  # 返回实体对象

        elif building_type == "generator":
            generator = Generator(x, y, direction)
            self.generators.add(generator)
            self.power_grid.add_generator(generator)
            return generator  # 返回实体对象

        elif building_type == "power_pole":
            pole = PowerPole(x, y)
            self.power_poles.add(pole)
            self.power_grid.add_pole(pole)
            return pole  # 返回实体对象

        elif building_type == "ammo_factory":
            factory = AmmoFactory(x, y, direction)
            self.ammo_factories.add(factory)
            self.power_grid.add_consumer(factory)
            return factory  # 返回实体对象

        elif building_type == "bucket":
            bucket = Bucket(x, y, direction)
            self.buckets.add(bucket)
            return bucket  # 返回实体对象

        elif building_type == "power_generator":
            pg = PowerGenerator(x, y)
            self.power_generators.add(pg)
            self.power_manager.register_generator(pg)
            self._scan_wire_neighbors(pg)
            return pg

        elif building_type == "capacitor":
            cap = Capacitor(x, y)
            self.power_capacitors.add(cap)
            self.power_manager.register_capacitor(cap)
            self._scan_wire_neighbors(cap)
            return cap

        elif building_type == "power_wire":
            wire = PowerWire(x, y)
            self.power_wires.add(wire)
            self.power_manager.register_wire(wire)
            self._scan_wire_neighbors(wire)
            return wire

        elif building_type == "splitter":
            splitter = Splitter(x, y)
            self.splitters.add(splitter)
            return splitter

        return None  # 返回None表示失败

    def _scan_wire_neighbors_for_tower(self, tower):
        """扫描电力塔相邻格子的电线，将塔加入电线的connected_objects"""
        tile_x = int(tower.x // TILE_SIZE)
        tile_y = int(tower.y // TILE_SIZE)

        # 获取 PowerTower 包装器（网络拓扑使用包装器而非原始Tower）
        power_tower = self.power_manager.registered_towers.get(tower)
        if power_tower is None:
            return

        for dx, dy in [(0, -1), (1, 0), (0, 1), (-1, 0)]:
            nx, ny = tile_x + dx, tile_y + dy
            center_x = nx * TILE_SIZE + TILE_SIZE // 2
            center_y = ny * TILE_SIZE + TILE_SIZE // 2

            for wire in self.power_wires:
                if wire.rect.collidepoint(center_x, center_y):
                    if power_tower not in wire.connected_objects:
                        wire.connected_objects.append(power_tower)

        self.power_manager._rebuild_networks()

    def _scan_wire_neighbors(self, entity):
        """扫描实体相邻格子的电线/设备，建立连接"""
        tile_x = int(entity.x // TILE_SIZE)
        tile_y = int(entity.y // TILE_SIZE)
        
        for dx, dy in [(0, -1), (1, 0), (0, 1), (-1, 0)]:
            nx, ny = tile_x + dx, tile_y + dy
            center_x = nx * TILE_SIZE + TILE_SIZE // 2
            center_y = ny * TILE_SIZE + TILE_SIZE // 2
            
            # 检查相邻电线
            for wire in self.power_wires:
                if wire is entity:
                    continue
                if wire.rect.collidepoint(center_x, center_y):
                    if entity not in wire.connected_objects:
                        wire.connected_objects.append(entity)
                    if hasattr(entity, 'connected_objects') and wire not in entity.connected_objects:
                        entity.connected_objects.append(wire)
            
            # 如果entity是电线，也检查相邻的设备
            if isinstance(entity, PowerWire):
                for pg in self.power_generators:
                    if pg.rect.collidepoint(center_x, center_y) and pg not in entity.connected_objects:
                        entity.connected_objects.append(pg)
                for cap in self.power_capacitors:
                    if cap.rect.collidepoint(center_x, center_y) and cap not in entity.connected_objects:
                        entity.connected_objects.append(cap)
                for twr in self.towers:
                    if twr.rect.collidepoint(center_x, center_y) and twr.tower_type == "electric":
                        # 使用 PowerTower 包装器而非原始 Tower（与 _rebuild_networks 一致）
                        power_twr = self.power_manager.registered_towers.get(twr)
                        if power_twr is not None and power_twr not in entity.connected_objects:
                            entity.connected_objects.append(power_twr)
        
        # 重建网络
        self.power_manager._rebuild_networks()

    def _handle_face_editor_click(self, screen_pos):
        """处理面编辑器中的点击事件（电线/分流器通用）

        Args:
            screen_pos: 屏幕坐标 (x, y)
        """
        target = self.face_edit_target
        if target is None:
            return

        # 检查是否点击了方向按钮
        for d, rect in self.face_edit_buttons.items():
            if rect.collidepoint(screen_pos):
                target.cycle_face(d)
                return

        # 检查是否点击了编辑器外部 → 关闭
        wx, wy = self.camera.world_to_screen(target.x, target.y)
        center_rect = pygame.Rect(wx, wy, int(TILE_SIZE * self.camera.zoom), int(TILE_SIZE * self.camera.zoom))
        if not center_rect.inflate(int(TILE_SIZE * self.camera.zoom * 3), int(TILE_SIZE * self.camera.zoom * 3)).collidepoint(screen_pos):
            self.face_edit_target = None
            self.face_edit_type = None

    def _draw_face_editor(self, screen):
        """绘制面配置编辑器（电线/分流器通用）"""
        target = self.face_edit_target
        if target is None:
            return

        edit_type = self.face_edit_type
        
        # 获取目标屏幕坐标
        wx, wy = self.camera.world_to_screen(target.x, target.y)
        size = int(TILE_SIZE * self.camera.zoom)
        cx = wx + size // 2
        cy = wy + size // 2
        btn_size = max(24, int(28 * self.camera.zoom))
        spacing = size

        # 构建方向按钮位置
        self.face_edit_buttons = {
            DIR_UP:    pygame.Rect(cx - btn_size // 2, cy - spacing - btn_size // 2, btn_size, btn_size),
            DIR_RIGHT: pygame.Rect(cx + spacing - btn_size // 2, cy - btn_size // 2, btn_size, btn_size),
            DIR_DOWN:  pygame.Rect(cx - btn_size // 2, cy + spacing - btn_size // 2, btn_size, btn_size),
            DIR_LEFT:  pygame.Rect(cx - spacing - btn_size // 2, cy - btn_size // 2, btn_size, btn_size),
        }

        # 半透明覆盖
        overlay = pygame.Surface((screen.get_width(), screen.get_height()), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 120))
        screen.blit(overlay, (0, 0))

        # 中心高亮
        hl_color = (255, 255, 0, 80) if edit_type == "power_wire" else (0, 255, 255, 80)
        border_color = (255, 255, 0) if edit_type == "power_wire" else (0, 255, 255)
        hl_surf = pygame.Surface((size, size), pygame.SRCALPHA)
        hl_surf.fill(hl_color)
        screen.blit(hl_surf, (wx, wy))
        pygame.draw.rect(screen, border_color, (wx, wy, size, size), 3)

        # 根据编辑类型选择颜色映射
        if edit_type == "splitter":
            face_colors = {SPLIT_FACE_NONE: (70, 70, 75), SPLIT_FACE_INPUT: (0, 180, 80), SPLIT_FACE_OUTPUT: (220, 80, 40)}
            face_labels = {SPLIT_FACE_NONE: "NONE", SPLIT_FACE_INPUT: "INPUT", SPLIT_FACE_OUTPUT: "OUT"}
            title_text = "分流器方向编辑 - 点击切换 NONE/INPUT/OUTPUT"
        else:
            face_colors = {
                FACE_NONE: (70, 70, 75),
                FACE_INPUT: (0, 140, 255),
                FACE_TRANSFER: (180, 180, 40),
                FACE_OUTPUT: (255, 140, 0),
            }
            face_labels = {FACE_NONE: "NONE", FACE_INPUT: "INP", FACE_TRANSFER: "TRS", FACE_OUTPUT: "OUT"}
            title_text = "电线方向编辑 - 点击切换 NONE/INP/TRS/OUT"

        dir_chars = {DIR_UP: "↑", DIR_RIGHT: "→", DIR_DOWN: "↓", DIR_LEFT: "←"}
        mouse_pos = pygame.mouse.get_pos()

        try:
            font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', max(10, int(12 * self.camera.zoom)))
            font_small = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', max(8, int(9 * self.camera.zoom)))
        except Exception:
            font = pygame.font.Font(None, max(10, int(12 * self.camera.zoom)))
            font_small = pygame.font.Font(None, max(8, int(9 * self.camera.zoom)))

        for d in range(4):
            rect = self.face_edit_buttons[d]
            mode = target.get_face(d)
            color = face_colors.get(mode, (70, 70, 75))
            hover = rect.collidepoint(mouse_pos)

            pygame.draw.rect(screen, color, rect, border_radius=4)
            if hover:
                pygame.draw.rect(screen, (255, 255, 255), rect, 3, border_radius=4)
            else:
                pygame.draw.rect(screen, (180, 180, 180), rect, 1, border_radius=4)

            # 方向箭头
            arrow_surf = font.render(dir_chars[d], True, (230, 230, 240))
            ar = arrow_surf.get_rect(center=rect.center)
            screen.blit(arrow_surf, ar)

            # 模式标签
            label_surf = font_small.render(face_labels[mode], True, (230, 230, 240))
            lr = label_surf.get_rect(center=(rect.centerx, rect.bottom + max(6, int(8 * self.camera.zoom))))
            screen.blit(label_surf, lr)

        # 提示
        tip = font.render(title_text, True, (255, 220, 50))
        tip_rect = tip.get_rect(center=(cx, wy - max(20, int(24 * self.camera.zoom))))
        screen.blit(tip, tip_rect)
        tip2 = font_small.render("[ESC]或点击外部关闭", True, (180, 180, 180))
        tip2_rect = tip2.get_rect(center=(cx, wy + size + max(30, int(40 * self.camera.zoom))))
        screen.blit(tip2, tip2_rect)

    def _rebuild_power_wire_appearances(self) -> None:
        """刷新所有电线的外观贴图"""
        for wire in self.power_wires:
            wire.refresh_appearance()

    def _update_splitters(self):
        """更新所有分流器：从传送带吸收物品 / 均分输出到传送带或机器"""
        for sp in self.splitters:
            if sp.transfer_timer < Splitter.TRANSFER_INTERVAL:
                continue
            sp.transfer_timer = 0.0

            stx = int(sp.x // TILE_SIZE)
            sty = int(sp.y // TILE_SIZE)

            # 收集所有有效的 INPUT 和 OUTPUT 面
            input_faces = []
            output_targets = []  # (face_dir, target_func)

            for face_dir in range(4):
                mode = sp.get_face(face_dir)
                dx, dy = S_DIR_OFFSETS[face_dir]
                tx, ty = stx + dx, sty + dy
                target_cx = tx * TILE_SIZE + TILE_SIZE // 2
                target_cy = ty * TILE_SIZE + TILE_SIZE // 2

                if mode == SPLIT_FACE_INPUT:
                    input_faces.append((face_dir, target_cx, target_cy))
                elif mode == SPLIT_FACE_OUTPUT:
                    output_targets.append((face_dir, target_cx, target_cy))

            # INPUT: 从传送带拉取物品
            for face_dir, target_cx, target_cy in input_faces:
                if not sp.can_accept_item():
                    break
                for conv in self.conveyors:
                    if not conv.rect.collidepoint(target_cx, target_cy):
                        continue
                    expected_dir = S_OPPOSITE_DIR[face_dir]
                    if conv.direction != expected_dir:
                        continue
                    if conv.item and conv.item.get("progress", 0) >= 1.0:
                        item_name = conv.item["name"]
                        conv.item = None
                        sp.add_item(item_name)
                        break

            # OUTPUT: 均分到各个输出面
            if not sp.has_items() or not output_targets:
                continue

            num_outputs = len(output_targets)
            # 轮询：按顺序轮流输出
            idx = sp._output_index % num_outputs
            sp._output_index = (sp._output_index + 1) % num_outputs

            face_dir, target_cx, target_cy = output_targets[idx]

            # 查找目标
            item_name = sp.pop_item()
            if not item_name:
                continue

            delivered = False

            # 传送带
            for conv in self.conveyors:
                if not conv.rect.collidepoint(target_cx, target_cy):
                    continue
                if conv.can_accept_item():
                    conv.insert_item(item_name)
                    delivered = True
                    break
            if delivered:
                continue

            # 塔（接收弹药）
            for tower in self.towers:
                if not tower.rect.collidepoint(target_cx, target_cy):
                    continue
                if not hasattr(tower, 'inventory') or tower.inventory is None:
                    continue
                tower.inventory.add_item(item_name, 1)
                delivered = True
                break
            if delivered:
                continue

            # 弹药制造机
            for factory in self.ammo_factories:
                if not factory.rect.collidepoint(target_cx, target_cy):
                    continue
                factory.inventory.add_item(item_name, 1)
                delivered = True
                break
            if delivered:
                continue

            # 储能桶
            for bucket in self.buckets:
                if not bucket.rect.collidepoint(target_cx, target_cy):
                    continue
                if bucket.can_accept(item_name):
                    bucket.add_item(item_name)
                    delivered = True
                    break

            # 如果没能送达，放回队列
            if not delivered:
                sp.item_queue.appendleft(item_name)

    def _update_conveyor_connections(self):
        """更新传送带连接"""
        for conveyor in self.conveyors:
            tile_x = int(conveyor.x // TILE_SIZE)
            tile_y = int(conveyor.y // TILE_SIZE)

            if conveyor.direction == 0:
                next_x, next_y = tile_x, tile_y - 1
            elif conveyor.direction == 1:
                next_x, next_y = tile_x + 1, tile_y
            elif conveyor.direction == 2:
                next_x, next_y = tile_x, tile_y + 1
            else:
                next_x, next_y = tile_x - 1, tile_y

            next_belt = None
            for other in self.conveyors:
                if int(other.x // TILE_SIZE) == next_x and int(other.y // TILE_SIZE) == next_y:
                    next_belt = other
                    break

            conveyor.set_next_belt(next_belt)

    def _update_miner_output(self, delta_time):
        """更新采矿机输出"""
        for miner in self.miners:
            if miner.inventory.total_items > 0:
                output_x, output_y = miner.get_output_position()

                # 先检查储物桶
                bucket_found = False
                for bucket in self.buckets:
                    if bucket.rect.collidepoint(output_x + TILE_SIZE // 2, output_y + TILE_SIZE // 2):
                        # 尝试输出所有物品到储物桶
                        for item_name in list(miner.inventory.items.keys()):
                            count = miner.inventory.get_item_count(item_name)
                            if count > 0 and bucket.can_accept(item_name):
                                bucket.add_item(item_name)
                                miner.inventory.remove_item(item_name, 1)
                        bucket_found = True
                        break
                
                if not bucket_found:
                    # 检查传送带
                    conveyor_found = False
                    for conveyor in self.conveyors:
                        if conveyor.rect.collidepoint(output_x + TILE_SIZE // 2, output_y + TILE_SIZE // 2):
                            # 尝试输出所有物品到传送带
                            for item_name in list(miner.inventory.items.keys()):
                                count = miner.inventory.get_item_count(item_name)
                                if count > 0 and conveyor.can_accept_item():
                                    conveyor.insert_item(item_name)
                                    miner.inventory.remove_item(item_name, 1)
                                    break  # 传送带一次只能放一个物品
                            conveyor_found = True
                            break
                    
                    if not conveyor_found:
                        # 检查弹药制造机（采矿机可以直接输出到相邻的弹药制造机）
                        for factory in self.ammo_factories:
                            if factory.rect.collidepoint(output_x + TILE_SIZE // 2, output_y + TILE_SIZE // 2):
                                # 尝试输出所有物品到弹药制造机
                                for item_name in list(miner.inventory.items.keys()):
                                    count = miner.inventory.get_item_count(item_name)
                                    if count > 0:
                                        factory.inventory.add_item(item_name, 1)
                                        miner.inventory.remove_item(item_name, 1)
                                break

    def _update_factory_output(self, delta_time):
        """更新弹药制造机输出到传送带或储物桶"""
        for factory in self.ammo_factories:
            # 只输出弹药，不输出原材料
            ammo_count = factory.inventory.get_item_count("ammo")
            if ammo_count > 0:
                output_x, output_y = factory.get_output_position()

                # 检查输出方向前方是否有储物桶
                bucket_found = False
                for bucket in self.buckets:
                    if bucket.rect.collidepoint(output_x + TILE_SIZE // 2, output_y + TILE_SIZE // 2):
                        if bucket.can_accept("ammo"):
                            bucket.add_item("ammo")
                            factory.inventory.remove_item("ammo", 1)
                            bucket_found = True
                        break
                
                # 如果没有储物桶，检查是否有传送带
                if not bucket_found:
                    for conveyor in self.conveyors:
                        if conveyor.rect.collidepoint(output_x + TILE_SIZE // 2, output_y + TILE_SIZE // 2):
                            if conveyor.can_accept_item():
                                conveyor.insert_item("ammo")
                                factory.inventory.remove_item("ammo", 1)
                            break

    def _update_bucket_output(self, delta_time):
        """更新储物桶输出到传送带（基于输出方向）"""
        for bucket in self.buckets:
            # 检查桶是否可以输出
            if not bucket.can_output():
                continue
            
            # 获取桶的位置和输出方向
            bucket_tile_x = int(bucket.x // TILE_SIZE)
            bucket_tile_y = int(bucket.y // TILE_SIZE)
            output_dir = bucket.output_direction
            
            # 根据输出方向确定目标位置
            if output_dir == 0:  # 上
                target_tile_x, target_tile_y = bucket_tile_x, bucket_tile_y - 1
            elif output_dir == 1:  # 右
                target_tile_x, target_tile_y = bucket_tile_x + 1, bucket_tile_y
            elif output_dir == 2:  # 下
                target_tile_x, target_tile_y = bucket_tile_x, bucket_tile_y + 1
            else:  # 左
                target_tile_x, target_tile_y = bucket_tile_x - 1, bucket_tile_y
            
            target_world_x = target_tile_x * TILE_SIZE + TILE_SIZE // 2
            target_world_y = target_tile_y * TILE_SIZE + TILE_SIZE // 2
            
            # 检查该位置是否有传送带
            for conveyor in self.conveyors:
                if conveyor.rect.collidepoint(target_world_x, target_world_y):
                    if conveyor.can_accept_item():
                        # 从桶中取出物品放入传送带
                        item = bucket.remove_item()
                        if item:
                            conveyor.insert_item(item)
                            bucket.reset_output_timer()  # 重置输出计时器
                    break

    def _update_conveyor_to_chest(self):
        """更新传送带到机器/箱子的传输"""
        # 首先更新所有传送带的 blocks_transfer 状态
        for conveyor in self.conveyors:
            conveyor.blocks_transfer = False
            conveyor.blocks_transfer_target = None

            # 获取传送带输出位置（方向前方一格）
            tile_x = int(conveyor.x // TILE_SIZE)
            tile_y = int(conveyor.y // TILE_SIZE)

            if conveyor.direction == 0:  # 上
                target_x, target_y = tile_x, tile_y - 1
            elif conveyor.direction == 1:  # 右
                target_x, target_y = tile_x + 1, tile_y
            elif conveyor.direction == 2:  # 下
                target_x, target_y = tile_x, tile_y + 1
            else:  # 左
                target_x, target_y = tile_x - 1, tile_y

            target_world_x = target_x * TILE_SIZE + TILE_SIZE // 2
            target_world_y = target_y * TILE_SIZE + TILE_SIZE // 2

            # 检查前方是否有机器或箱子
            found_target = False

            # 弹药制造机
            for factory in self.ammo_factories:
                if factory.rect.collidepoint(target_world_x, target_world_y):
                    conveyor.blocks_transfer = True
                    conveyor.blocks_transfer_target = ("factory", factory)
                    found_target = True
                    break

            # 发电机（旧版）
            if not found_target:
                for generator in self.generators:
                    if generator.rect.collidepoint(target_world_x, target_world_y):
                        conveyor.blocks_transfer = True
                        conveyor.blocks_transfer_target = ("generator", generator)
                        found_target = True
                        break
            
            # 燃煤发电机（工业电力系统）
            if not found_target:
                for pg in self.power_generators:
                    if pg.rect.collidepoint(target_world_x, target_world_y):
                        conveyor.blocks_transfer = True
                        conveyor.blocks_transfer_target = ("power_generator", pg)
                        found_target = True
                        break

            # 储物桶
            if not found_target:
                for bucket in self.buckets:
                    if bucket.rect.collidepoint(target_world_x, target_world_y):
                        conveyor.blocks_transfer = True
                        conveyor.blocks_transfer_target = ("bucket", bucket)
                        found_target = True
                        break

            # 塔（接收弹药）
            if not found_target:
                for tower in self.towers:
                    if tower.rect.collidepoint(target_world_x, target_world_y):
                        conveyor.blocks_transfer = True
                        conveyor.blocks_transfer_target = ("tower", tower)
                        found_target = True
                        break

            # 物品分流器（INPUT面）
            if not found_target:
                for sp in self.splitters:
                    if sp.rect.collidepoint(target_world_x, target_world_y):
                        # 检查分流器的对应面是否为INPUT
                        sp_face = S_OPPOSITE_DIR[conveyor.direction]
                        if sp.get_face(sp_face) == SPLIT_FACE_INPUT:
                            conveyor.blocks_transfer = True
                            conveyor.blocks_transfer_target = ("splitter", sp)
                            found_target = True
                            break

        # 然后处理物品传递
        for conveyor in self.conveyors:
            if not conveyor.item or conveyor.item["progress"] < 1.0:
                continue

            item_name = conveyor.item["name"]
            target = conveyor.blocks_transfer_target

            if target:
                target_type, target_entity = target
                transferred = False

                if target_type == "factory":
                    target_entity.inventory.add_item(item_name, 1)
                    transferred = True
                elif target_type == "generator":
                    target_entity.inventory.add_item(item_name, 1)
                    transferred = True
                elif target_type == "power_generator":
                    target_entity.add_fuel(item_name, 1)
                    transferred = True
                elif target_type == "bucket":
                    if target_entity.can_accept(item_name):
                        target_entity.add_item(item_name)
                        transferred = True
                elif target_type == "tower":
                    # 只有弹药可以被塔接收
                    if item_name == "ammo" and hasattr(target_entity, 'inventory'):
                        target_entity.inventory.add_item("ammo", 1)
                        transferred = True
                elif target_type == "splitter":
                    if target_entity.can_accept_item():
                        target_entity.add_item(item_name)
                        transferred = True

                if transferred:
                    conveyor.item = None  # 清空物品

    def spawn_enemy(self):
        """手动生成一个敌人（按U键触发）"""
        # 创建敌人（Enemy类会自动从PATH_POINTS[0]起点开始）
        enemy = Enemy("basic")
        self.enemies.add(enemy)

    def save_game(self):
        """保存游戏"""
        if save_game(self):
            self.game_ui.show_toast("保存成功!")
        else:
            self.game_ui.show_toast("保存失败!")

    def load_game(self):
        """加载游戏"""
        if load_game(self):
            self.game_ui.show_toast("加载成功!")
        else:
            self.game_ui.show_toast("没有存档!")

    def update(self, delta_time):
        """更新场景状态"""
        if self.game_over or self.paused:
            return

        self._update_camera()
        self.power_grid.update(delta_time)

        # 无限资源模式：自动给所有机器供电
        for miner in self.miners:
            miner.power.is_powered = True

        for generator in self.generators:
            generator.update(delta_time)

        for miner in self.miners:
            miner.update(delta_time)

        for factory in self.ammo_factories:
            factory.update(delta_time)

        for bucket in self.buckets:
            bucket.update(delta_time)

        # 更新工业电力系统实体
        for pg in self.power_generators:
            pg.update(delta_time)
        for cap in self.power_capacitors:
            cap.update(delta_time)
        for wire in self.power_wires:
            wire.update(delta_time)

        # 更新工业电力网络（必须在实体 update 之后，确保 is_running 等状态已刷新）
        self.power_manager.update(delta_time)

        # 更新分流器
        for sp in self.splitters:
            sp.update(delta_time)
        self._update_splitters()

        # 先检测传送带前方是否有机器/箱子，设置 blocks_transfer 状态
        self._update_conveyor_to_chest()

        # 然后更新传送带（此时 blocks_transfer 已设置）
        for conveyor in self.conveyors:
            conveyor.update(delta_time, self.map)

        # 更新GridManager（传送带和储物桶系统）
        self.grid_manager.step()

        # 同步倒计时到UI
        self.game_ui.countdown_time = self.countdown_time
        self.game_ui.countdown_active = self.countdown_active

        # 更新UI
        self.game_ui.update(delta_time)

        self._update_miner_output(delta_time)
        self._update_factory_output(delta_time)
        self._update_bucket_output(delta_time)
        self._update_ammo_supply()

        # 更新悬浮窗（鼠标悬停检测）
        self._update_hover_tooltip()

        self.enemies.update(delta_time)

        for enemy in self.enemies:
            if enemy.reached_end:
                self.game_ui.set_lives(self.game_ui.lives - 1)
                enemy.kill()
                if self.game_ui.lives <= 0:
                    self.game_over = True

        self.towers.update(delta_time)
        self.bullets.update(delta_time)

        for enemy in self.enemies:
            if enemy.health <= 0:
                self.game_ui.add_gold(enemy.reward)
                enemy.kill()

        self._update_wave(delta_time)

    def _update_camera(self):
        """更新摄像机位置"""
        dx = dy = 0
        if self.keys['w']:
            dy -= 1
        if self.keys['s']:
            dy += 1
        if self.keys['a']:
            dx -= 1
        if self.keys['d']:
            dx += 1
        if dx != 0 or dy != 0:
            self.camera.move(dx, dy)
        self.camera.update(0)

    def _update_wave(self, delta_time):
        """更新波次状态"""
        if self.game_state == "countdown":
            self.countdown_time -= delta_time
            if self.countdown_time <= 0:
                self.countdown_time = 0
                self.countdown_active = False
                self.game_state = "spawning"
                self.wave_timer = 0
            return

        elif self.game_state == "spawning":
            if self.enemies_spawned < self.enemies_per_wave:
                self.spawn_timer += delta_time
                if self.spawn_timer >= self.spawn_interval:
                    self.spawn_enemy()
                    self.spawn_timer = 0
            elif len(self.enemies) == 0:
                self.game_state = "wave_end"
                self.wave_timer = WAVE_INTERVAL

        elif self.game_state == "wave_end":
            self.wave_timer -= delta_time
            if self.wave_timer <= 0:
                self.current_wave += 1
                self.enemies_spawned = 0
                self.enemies_per_wave = WAVE_ENEMIES + self.current_wave * 2
                self.game_ui.set_wave(self.current_wave)
                self.game_state = "spawning"
                self.wave_timer = 0

    def _update_ammo_supply(self):
        """更新塔的弹药补给"""
        # 暂时禁用箱子弹药补给功能
        pass

    def _update_hover_tooltip(self):
        """更新悬浮窗（鼠标悬停检测）"""
        mouse_x, mouse_y = pygame.mouse.get_pos()
        world_x, world_y = self.camera.screen_to_world(mouse_x, mouse_y)

        found = False

        # 检查采矿机
        for miner in self.miners:
            if miner.rect.collidepoint(world_x, world_y):
                self.game_ui.show_hover_tooltip({
                    "type": "miner",
                    "data": {
                        "ore_type": miner.target_ore.ore_type if miner.target_ore else "未知",
                        "progress": miner.production.progress / miner.production.production_time if miner.production.production_time > 0 else 0,
                        "is_producing": miner.production.is_producing
                    }
                })
                found = True
                break

        # 检查弹药制造机
        if not found:
            for factory in self.ammo_factories:
                if factory.rect.collidepoint(world_x, world_y):
                    self.game_ui.show_hover_tooltip({
                        "type": "ammo_factory",
                        "data": {
                            "iron_ore": factory.inventory.get_item_count("iron_ore"),
                            "copper_ore": factory.inventory.get_item_count("copper_ore"),
                            "ammo": factory.inventory.get_item_count("ammo")
                        }
                    })
                    found = True
                    break

        # 检查发电机
        if not found:
            for generator in self.generators:
                if generator.rect.collidepoint(world_x, world_y):
                    self.game_ui.show_hover_tooltip({
                        "type": "generator",
                        "data": {
                            "fuel": generator.inventory.get_item_count("coal"),
                            "power": generator.current_power_output
                        }
                    })
                    found = True
                    break

        # 检查储物桶
        if not found:
            for bucket in self.buckets:
                if bucket.rect.collidepoint(world_x, world_y):
                    self.game_ui.show_hover_tooltip({
                        "type": "bucket",
                        "data": bucket.get_items_summary()
                    })
                    found = True
                    break

        # 检查塔
        if not found:
            for tower in self.towers:
                if tower.rect.collidepoint(world_x, world_y):
                    tower_info = {
                        "type": "tower",
                        "data": {
                            "tower_type": tower.tower_type,
                            "range": tower.range,
                            "damage": tower.damage,
                            "fire_rate": tower.fire_rate,
                            "is_electric": tower.is_electric,
                            "power": tower.power.current_power if tower.is_electric else 0,
                            "max_power": tower.power.max_power if tower.is_electric else 0,
                            "ammo": tower.inventory.get_item_count("ammo") if tower.inventory else 0,
                            "has_power": tower.has_power(),
                            "has_ammo": tower.has_ammo()
                        }
                    }
                    self.game_ui.show_hover_tooltip(tower_info)
                    found = True
                    break

        # 检查燃煤发电机（工业电力系统）
        if not found:
            for pg in self.power_generators:
                if pg.rect.collidepoint(world_x, world_y):
                    self.game_ui.show_hover_tooltip({
                        "type": "power_generator",
                        "data": {
                            "fuel": pg.inventory.get_item_count("coal"),
                            "output": pg.get_eut_output(),
                            "running": pg.is_running
                        }
                    })
                    found = True
                    break

        # 检查电容库
        if not found:
            for cap in self.power_capacitors:
                if cap.rect.collidepoint(world_x, world_y):
                    self.game_ui.show_hover_tooltip({
                        "type": "capacitor",
                        "data": {
                            "energy": cap.energy,
                            "capacity": cap.capacity
                        }
                    })
                    found = True
                    break

        # 检查电力线缆
        if not found:
            for wire in self.power_wires:
                if wire.rect.collidepoint(world_x, world_y):
                    face_names = {FACE_NONE: "无", FACE_INPUT: "输入", FACE_TRANSFER: "传输", FACE_OUTPUT: "输出"}
                    faces_info = (
                        f"↑{face_names.get(wire.get_face(DIR_UP), '?')} "
                        f"→{face_names.get(wire.get_face(DIR_RIGHT), '?')} "
                        f"↓{face_names.get(wire.get_face(DIR_DOWN), '?')} "
                        f"←{face_names.get(wire.get_face(DIR_LEFT), '?')}"
                    )
                    self.game_ui.show_hover_tooltip({
                        "type": "power_wire",
                        "data": {
                            "mode": faces_info,
                            "connected": len(wire.connected_objects)
                        }
                    })
                    found = True
                    break

        # 检查物品分流器
        if not found:
            for sp in self.splitters:
                if sp.rect.collidepoint(world_x, world_y):
                    face_names = {SPLIT_FACE_NONE: "无", SPLIT_FACE_INPUT: "输入", SPLIT_FACE_OUTPUT: "输出"}
                    faces_info = (
                        f"↑{face_names.get(sp.get_face(S_DIR_UP), '?')} "
                        f"→{face_names.get(sp.get_face(S_DIR_RIGHT), '?')} "
                        f"↓{face_names.get(sp.get_face(S_DIR_DOWN), '?')} "
                        f"←{face_names.get(sp.get_face(S_DIR_LEFT), '?')}"
                    )
                    self.game_ui.show_hover_tooltip({
                        "type": "splitter",
                        "data": {
                            "faces": faces_info,
                            "queued": len(sp.item_queue)
                        }
                    })
                    found = True
                    break

        # 如果没有悬停在任何机器上，隐藏悬浮窗
        if not found:
            self.game_ui.hide_hover_tooltip()

    def render(self, screen):
        """渲染场景"""
        screen.fill(COLORS["background"])

        self.map.draw(screen, self.camera)

        for ore in self.ore_deposits:
            ore.draw(screen, self.camera)

        for bucket in self.buckets:
            bucket.draw(screen, self.camera)

        for generator in self.generators:
            generator.draw(screen, self.camera)

        for factory in self.ammo_factories:
            factory.draw(screen, self.camera)

        for conveyor in self.conveyors:
            conveyor.draw(screen, self.camera)

        for miner in self.miners:
            miner.draw(screen, self.camera)

        for pole in self.power_poles:
            pole.draw(screen, self.camera)

        # 渲染工业电力系统实体
        for pg in self.power_generators:
            pg.draw(screen, self.camera)
        for cap in self.power_capacitors:
            cap.draw(screen, self.camera)
        for wire in self.power_wires:
            wire.draw(screen, self.camera)
        for sp in self.splitters:
            sp.draw(screen, self.camera)

        # 绘制面配置编辑器（覆盖在电线上方）
        self._draw_face_editor(screen)

        # 渲染GridManager（传送带和储物桶网格系统）
        self.grid_manager.draw(screen, self.camera)

        mouse_x, mouse_y = pygame.mouse.get_pos()
        world_x, world_y = self.camera.screen_to_world(mouse_x, mouse_y)

        for tower in self.towers:
            show_range = tower.rect.collidepoint(world_x, world_y)
            tower.draw(screen, self.camera, show_range)

        for enemy in self.enemies:
            enemy.draw(screen, self.camera)

        self._draw_enemy_health_on_hover(screen)

        for bullet in self.bullets:
            bullet.draw(screen, self.camera)

        self._draw_placement_preview(screen)

        # 渲染游戏UI (conveyor_test风格)
        self.game_ui.draw(screen)

        if self.paused:
            font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 72)
            pause_text = font.render("暂停", True, (255, 255, 255))
            text_rect = pause_text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2))
            screen.blit(pause_text, text_rect)

        if self.game_over:
            font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 72)
            game_over_text = font.render("游戏结束", True, (255, 0, 0))
            text_rect = game_over_text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2))
            screen.blit(game_over_text, text_rect)

        self._draw_conveyor_placement_preview(screen)

    def _draw_enemy_health_on_hover(self, screen):
        """当鼠标悬停在敌人头上时显示当前血量"""
        mouse_x, mouse_y = pygame.mouse.get_pos()
        world_x, world_y = self.camera.screen_to_world(mouse_x, mouse_y)

        hovered_enemy = None
        for enemy in self.enemies:
            if enemy.rect.collidepoint(world_x, world_y):
                hovered_enemy = enemy
                break

        if hovered_enemy is not None:
            screen_x, screen_y = self.camera.world_to_screen(
                hovered_enemy.x, hovered_enemy.y
            )

            font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 18)

            health_percent = (hovered_enemy.health / hovered_enemy.max_health) * 100

            bar_width = 60
            bar_height = 6
            bg_x = screen_x - bar_width // 2
            bg_y = screen_y - 20

            pygame.draw.rect(screen, (30, 30, 30), (bg_x, bg_y, bar_width, bar_height))

            health_color = (0, 255, 0) if health_percent > 50 else (255, 255, 0) if health_percent > 25 else (255, 0, 0)
            health_width = int(bar_width * (hovered_enemy.health / hovered_enemy.max_health))
            pygame.draw.rect(screen, health_color, (bg_x, bg_y, health_width, bar_height))

            pygame.draw.rect(screen, (200, 200, 200), (bg_x, bg_y, bar_width, bar_height), 1)

            health_text = font.render(f"{hovered_enemy.health}/{hovered_enemy.max_health}", True, (255, 255, 255))
            text_x = screen_x - health_text.get_width() // 2
            text_y = bg_y - 20
            screen.blit(health_text, (text_x, text_y))

    def _draw_placement_preview(self, screen):
        """绘制建筑放置预览"""
        if self.selected_building is None:
            return

        mouse_x, mouse_y = pygame.mouse.get_pos()
        world_x, world_y = self.camera.screen_to_world(mouse_x, mouse_y)
        tile_x = int(world_x // TILE_SIZE)
        tile_y = int(world_y // TILE_SIZE)

        can_place = self.map.can_place_tower(tile_x * TILE_SIZE, tile_y * TILE_SIZE)

        cost = BUILDING_COSTS.get(self.selected_building, {})
        can_afford = self._can_afford(cost)

        if self.selected_building == "miner":
            has_ore = False
            for ore in self.ore_deposits:
                if ore.rect.collidepoint(tile_x * TILE_SIZE + TILE_SIZE // 2, tile_y * TILE_SIZE + TILE_SIZE // 2):
                    has_ore = True
                    break
            can_place = can_place and has_ore

        can_build = can_place and can_afford

        screen_x = tile_x * TILE_SIZE * self.camera.zoom + self.camera.get_offset()[0]
        screen_y = tile_y * TILE_SIZE * self.camera.zoom + self.camera.get_offset()[1]

        color = (0, 255, 0, 128) if can_build else (255, 0, 0, 128)

        preview_rect = pygame.Rect(screen_x, screen_y, TILE_SIZE * self.camera.zoom, TILE_SIZE * self.camera.zoom)
        pygame.draw.rect(screen, color, preview_rect)
        pygame.draw.rect(screen, (255, 255, 255), preview_rect, 2)

    def _draw_conveyor_placement_preview(self, screen):
        """绘制传送带放置预览"""
        if self.selected_building != "conveyor" or self.grid_manager.placing_start is None:
            return

        start_x, start_y = self.grid_manager.placing_start
        world_x = start_x * TILE_SIZE
        world_y = start_y * TILE_SIZE
        screen_x, screen_y = self.camera.world_to_screen(world_x, world_y)
        size = int(TILE_SIZE * self.camera.zoom)

        pygame.draw.rect(screen, (255, 165, 0, 180), (screen_x, screen_y, size, size))
        pygame.draw.rect(screen, (255, 255, 255), (screen_x, screen_y, size, size), 2)

        font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 14)
        text = font.render("起点", True, (255, 255, 255))
        text_rect = text.get_rect(center=(screen_x + size // 2, screen_y + size // 2))
        screen.blit(text, text_rect)


def main():
    """主函数"""
    game = Game()
    game_scene = GameScene(game)
    game.add_scene("game", game_scene)
    game.set_scene("game")
    game.run()


if __name__ == "__main__":
    main()
