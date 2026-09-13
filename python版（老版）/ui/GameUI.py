#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""游戏UI (工业扁平化浅色风格)

采用浅色主题，工业风格扁平设计
"""

import pygame
from settings import TILE_SIZE, COLORS, SCREEN_WIDTH, SCREEN_HEIGHT, INITIAL_GOLD

# 从 GridManager 导入常量
from maps.GridManager import (
    GRID_WIDTH, GRID_HEIGHT, BUCKET_CAPACITY,
    CELL_EMPTY, CELL_CONVEYOR, CELL_OBSTACLE, CELL_START, CELL_END, CELL_BUCKET,
    DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT,
    ITEM_TYPES
)

# ========== 工业扁平化浅色主题 ==========
UI_BG = (245, 245, 245)           # 浅灰白主背景
UI_PANEL = (230, 230, 235)        # 面板背景
UI_PANEL_LIGHT = (240, 240, 245)  # 浅色面板
UI_BORDER = (180, 180, 190)       # 边框色
UI_BORDER_LIGHT = (200, 200, 210) # 浅边框

UI_ACCENT = (70, 130, 180)        # Steel Blue 强调色
UI_ACCENT_GREEN = (60, 150, 100)  # 工业绿
UI_ACCENT_ORANGE = (200, 120, 50) # 工业橙
UI_ACCENT_RED = (180, 60, 60)     # 工业红

UI_TEXT = (50, 50, 60)            # 深色文字
UI_TEXT_DIM = (120, 120, 130)     # 灰色文字
UI_TEXT_LIGHT = (240, 240, 250)   # 浅色文字

# 方向符号（8方向）
DIR_SYMBOLS = {
    0: '↑', 1: '↗', 2: '→', 3: '↘',
    4: '↓', 5: '↙', 6: '←', 7: '↖'
}
DIR_NAMES = {
    0: '上', 1: '右上', 2: '右', 3: '右下',
    4: '下', 5: '左下', 6: '左', 7: '左上'
}

# 支持8方向的建筑
BUILDINGS_8_DIRECTION = {"basic_tower", "rapid_tower", "sniper_tower", "electric_tower"}
# 支持4方向的建筑
BUILDINGS_4_DIRECTION = {"miner", "ammo_factory", "bucket", "conveyor", "power_generator", "capacitor"}

# 建筑名称
BUILDING_NAMES = {
    "basic_tower": "基础塔",
    "electric_tower": "电力塔",
    "rapid_tower": "速射塔",
    "sniper_tower": "狙击塔",
    "miner": "采矿机",
    "conveyor": "传送带",
    "bucket": "储物桶",
    "ammo_factory": "弹药",
    "generator": "发电机",
    "power_pole": "电线杆",
    "power_generator": "燃煤发电机",
    "capacitor": "电容库",
    "power_wire": "电力线缆"
}


class GameUI:
    """游戏UI主类 (工业扁平化浅色风格)"""

    def __init__(self, game_scene):
        """初始化UI

        Args:
            game_scene: 游戏场景引用
        """
        self.game_scene = game_scene

        # 字体 - 使用 Windows 默认宋体 (simsun.ttc)
        try:
            self.font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 13)
            self.font_small = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 11)
            self.font_title = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 14)
            self.font_large = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 18)
            self.font_bold = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 16)
        except:
            # 回退到默认字体
            self.font = pygame.font.Font(None, 13)
            self.font_small = pygame.font.Font(None, 11)
            self.font_title = pygame.font.Font(None, 14)
            self.font_large = pygame.font.Font(None, 18)
            self.font_bold = pygame.font.Font(None, 16)

        # 资源栏位置 (顶部)
        self.resource_bar_height = 50

        # 右侧面板 (建筑选择)
        self.side_panel_width = 200
        self.side_panel_x = SCREEN_WIDTH - self.side_panel_width

        # 按钮配置
        self.buttons = {}
        self._init_buttons()

        # 游戏状态
        self.gold = INITIAL_GOLD
        self.lives = 10
        self.wave = 1

        # 建筑选择
        self.selected_building = None

        # 弹窗提示
        self.toast_message = ""
        self.toast_timer = 0
        self.toast_duration = 3.0

        # 方向选择悬浮窗
        self.direction_popup_active = False
        self.direction_popup_building = None  # 等待选择方向的建筑类型
        self.direction_popup_position = None  # 悬浮窗位置（屏幕坐标）
        self.direction_popup_tile_pos = None  # 放置的瓦片位置
        self.direction_popup_buttons = {}  # 方向按钮

        # 悬浮窗（机器信息）
        self.hover_tooltip_active = False
        self.hover_tooltip_info = None  # {"type": "miner/ammo_factory/generator/bucket", "data": {...}}

        # 倒计时
        self.countdown_time = 0
        self.countdown_active = False

    def _init_buttons(self):
        """初始化按钮"""
        # 右侧建筑按钮
        btn_x = self.side_panel_x + 10
        btn_y = 60
        btn_width = 85
        btn_height = 32
        spacing = 5

        buildings = [
            ("1", "basic_tower", "基础塔"),
            ("2", "electric_tower", "电力塔"),
            ("3", "miner", "采矿机"),
            ("4", "conveyor", "传送带"),
            ("5", "bucket", "储物桶"),
            ("6", "ammo_factory", "弹药"),
            ("0", "power_generator", "燃煤发电机"),
            ("-", "capacitor", "电容库"),
            ("=", "power_wire", "电力线缆"),
            ("\\", "splitter", "分流器"),
        ]

        for i, (key, building, name) in enumerate(buildings):
            row = i // 2
            col = i % 2
            x = btn_x + col * (btn_width + spacing)
            y = btn_y + row * (btn_height + spacing)

            self.buttons[f"building_{building}"] = {
                "rect": pygame.Rect(x, y, btn_width, btn_height),
                "label": f"[{key}] {name}",
                "building": building,
                "active": False
            }

        # 保存/加载按钮
        save_load_y = btn_y + (len(buildings) // 2 + 1) * (btn_height + spacing) + 20
        
        self.buttons["save_button"] = {
            "rect": pygame.Rect(btn_x, save_load_y, btn_width, btn_height),
            "label": "[F5] 保存",
            "action": "save",
            "active": False
        }
        
        self.buttons["load_button"] = {
            "rect": pygame.Rect(btn_x + btn_width + spacing, save_load_y, btn_width, btn_height),
            "label": "[F9] 加载",
            "action": "load",
            "active": False
        }

    def handle_event(self, event):
        """处理事件

        Args:
            event: pygame事件

        Returns:
            是否处理了事件
        """
        # 如果悬浮窗激活，优先处理悬浮窗事件
        if self.direction_popup_active:
            handled, direction, building, tile_pos = self.handle_direction_popup_event(event)
            if handled:
                # 通知游戏场景处理放置
                if hasattr(self.game_scene, 'place_building_with_direction'):
                    self.game_scene.place_building_with_direction(building, tile_pos, direction)
                return True
            return False

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            return self._handle_click(event.pos)

        return False

    def _handle_click(self, pos):
        """处理鼠标点击"""
        # 检查按钮点击
        for key, btn in self.buttons.items():
            if btn["rect"].collidepoint(pos):
                self._handle_button_click(key)
                return True
        return False

    def _handle_button_click(self, key):
        """处理按钮点击"""
        if key.startswith("building_"):
            building = key.split("_", 1)[1]
            self.select_building(building)
            self.game_scene.selected_building = building
        elif key == "save_button":
            if hasattr(self.game_scene, 'save_game'):
                self.game_scene.save_game()
        elif key == "load_button":
            if hasattr(self.game_scene, 'load_game'):
                self.game_scene.load_game()

    def select_building(self, building):
        """选择建筑"""
        # 重置所有建筑按钮状态
        for key, btn in self.buttons.items():
            if key.startswith("building_"):
                btn["active"] = False

        # 设置选中的建筑按钮
        btn_key = f"building_{building}"
        if btn_key in self.buttons:
            self.buttons[btn_key]["active"] = True

        self.selected_building = building

    def get_building_name(self, building):
        """获取建筑中文名"""
        return BUILDING_NAMES.get(building, building)

    def set_gold(self, amount):
        """设置金币"""
        self.gold = amount

    def add_gold(self, amount):
        """增加金币"""
        self.gold += amount

    def set_lives(self, amount):
        """设置生命值"""
        self.lives = amount

    def set_wave(self, wave):
        """设置波次"""
        self.wave = wave

    def show_toast(self, message):
        """显示弹窗提示

        Args:
            message: 提示消息
        """
        self.toast_message = message
        self.toast_timer = self.toast_duration

    def show_direction_popup(self, building: str, tile_pos: tuple, screen_pos: tuple):
        """显示方向选择悬浮窗

        Args:
            building: 建筑类型
            tile_pos: 瓦片位置 (tile_x, tile_y)
            screen_pos: 屏幕位置 (x, y)
        """
        self.direction_popup_active = True
        self.direction_popup_building = building
        self.direction_popup_tile_pos = tile_pos
        self.direction_popup_position = screen_pos

        # 初始化方向按钮
        btn_size = 50
        spacing = 8
        center_x = screen_pos[0]
        center_y = screen_pos[1]

        self.direction_popup_buttons = {}

        # 判断是8方向还是4方向
        if building in BUILDINGS_8_DIRECTION:
            # 8方向布局（圆形布局）
            import math
            radius = btn_size + spacing
            
            for dir_idx in range(8):
                angle = math.radians(dir_idx * 45 - 90)
                x = center_x - btn_size // 2 + int(radius * math.cos(angle))
                y = center_y - btn_size // 2 + int(radius * math.sin(angle))
                rect = pygame.Rect(x, y, btn_size, btn_size)
                self.direction_popup_buttons[dir_idx] = rect
        else:
            # 4方向布局（十字形）
            # 上
            up_rect = pygame.Rect(center_x - btn_size // 2, center_y - btn_size - spacing - btn_size, btn_size, btn_size)
            self.direction_popup_buttons[DIR_UP] = up_rect

            # 下
            down_rect = pygame.Rect(center_x - btn_size // 2, center_y + spacing + btn_size, btn_size, btn_size)
            self.direction_popup_buttons[DIR_DOWN] = down_rect

            # 左
            left_rect = pygame.Rect(center_x - btn_size - spacing - btn_size, center_y - btn_size // 2, btn_size, btn_size)
            self.direction_popup_buttons[DIR_LEFT] = left_rect

            # 右
            right_rect = pygame.Rect(center_x + spacing + btn_size, center_y - btn_size // 2, btn_size, btn_size)
            self.direction_popup_buttons[DIR_RIGHT] = right_rect

    def hide_direction_popup(self):
        """隐藏方向选择悬浮窗"""
        self.direction_popup_active = False
        self.direction_popup_building = None
        self.direction_popup_tile_pos = None
        self.direction_popup_position = None
        self.direction_popup_buttons = {}

    def show_hover_tooltip(self, info: dict):
        """显示悬浮窗信息

        Args:
            info: 悬浮窗信息字典
                {
                    "type": "miner/ammo_factory/generator/bucket",
                    "data": {...}  # 各类型特有的数据
                }
        """
        self.hover_tooltip_active = True
        self.hover_tooltip_info = info

    def hide_hover_tooltip(self):
        """隐藏悬浮窗"""
        self.hover_tooltip_active = False
        self.hover_tooltip_info = None

    def _draw_hover_tooltip(self, screen):
        """绘制悬浮窗"""
        if not self.hover_tooltip_active or not self.hover_tooltip_info:
            return

        info = self.hover_tooltip_info
        tooltip_type = info.get("type")

        # 获取鼠标位置
        mouse_x, mouse_y = pygame.mouse.get_pos()

        # 悬浮窗大小 - 基础塔需要更大的窗口
        popup_width = 160
        if tooltip_type == "tower":
            popup_height = 140  # 塔悬浮窗调大
        else:
            popup_height = 100

        # 位置在鼠标下方
        popup_x = mouse_x + 15
        popup_y = mouse_y + 15

        # 确保不超出屏幕
        if popup_x + popup_width > screen.get_width():
            popup_x = mouse_x - popup_width - 15
        if popup_y + popup_height > screen.get_height():
            popup_y = mouse_y - popup_height - 15

        # 背景
        bg_rect = pygame.Rect(popup_x, popup_y, popup_width, popup_height)
        pygame.draw.rect(screen, UI_PANEL, bg_rect)
        pygame.draw.rect(screen, UI_ACCENT, bg_rect, 2)

        # 标题
        title = ""
        if tooltip_type == "miner":
            title = "采矿机"
        elif tooltip_type == "ammo_factory":
            title = "弹药制造机"
        elif tooltip_type == "generator":
            title = "发电机"
        elif tooltip_type == "bucket":
            title = "储物桶"
        elif tooltip_type == "tower":
            data = info.get("data", {})
            tower_types = {
                "basic": "基础塔",
                "rapid": "快速塔",
                "sniper": "狙击塔",
                "electric": "电塔"
            }
            title = tower_types.get(data.get("tower_type"), "塔")
        else:
            title = str(tooltip_type)

        title_text = self.font_title.render(title, True, UI_TEXT)
        screen.blit(title_text, (popup_x + 10, popup_y + 8))

        # 内容
        content_y = popup_y + 35
        if tooltip_type == "miner":
            data = info.get("data", {})
            ore_type = data.get("ore_type", "未知")
            progress = data.get("progress", 0)
            is_producing = data.get("is_producing", False)
            status = "工作中" if is_producing else "待机"
            content_lines = [
                f"矿石: {ore_type}",
                f"状态: {status}",
                f"进度: {progress*100:.0f}%"
            ]
        elif tooltip_type == "ammo_factory":
            data = info.get("data", {})
            iron = data.get("iron_ore", 0)
            copper = data.get("copper_ore", 0)
            ammo = data.get("ammo", 0)
            recipe = "2铁 + 1铜 → 1弹药"
            content_lines = [
                f"配方: {recipe}",
                f"库存: 铁{iron} 铜{copper}",
                f"弹药: {ammo}"
            ]
        elif tooltip_type == "generator":
            data = info.get("data", {})
            fuel = data.get("fuel", 0)
            power = data.get("power", 0)
            content_lines = [
                f"燃料: {fuel}",
                f"电力: {power:.1f} EU/s"
            ]
        elif tooltip_type == "power_generator":
            data = info.get("data", {})
            fuel = data.get("fuel", 0)
            output = data.get("output", 0)
            running = data.get("running", False)
            content_lines = [
                f"燃料(煤): {fuel}",
                f"输出: {output:.0f} EU/t",
                f"状态: {'运行中' if running else '待燃料'}"
            ]
        elif tooltip_type == "capacitor":
            data = info.get("data", {})
            energy = data.get("energy", 0)
            capacity = data.get("capacity", 0)
            content_lines = [
                f"储能: {energy:.0f}/{capacity:.0f} EU",
                f"充电: {energy/capacity*100:.0f}%" if capacity > 0 else "空"
            ]
        elif tooltip_type == "power_wire":
            data = info.get("data", {})
            mode = data.get("mode", "未知")
            connected = data.get("connected", 0)
            content_lines = [
                f"模式: {mode}",
                f"连接数: {connected}"
            ]
        elif tooltip_type == "splitter":
            data = info.get("data", {})
            faces = data.get("faces", "未知")
            queued = data.get("queued", 0)
            content_lines = [
                f"面状态: {faces}",
                f"队列: {queued}"
            ]
        elif tooltip_type == "bucket":
            data = info.get("data", {})
            if data:
                content_lines = [f"{k}: {v}" for k, v in data.items()]
            else:
                content_lines = ["空桶"]
        elif tooltip_type == "tower":
            data = info.get("data", {})
            tower_type = data.get("tower_type", "basic")
            damage = data.get("damage", 0)
            fire_rate = data.get("fire_rate", 0)
            range_val = data.get("range", 0)
            is_electric = data.get("is_electric", False)
            
            content_lines = [
                f"伤害: {damage}",
                f"射速: {fire_rate:.1f}/s",
                f"射程: {range_val}"
            ]
            
            if is_electric:
                power = data.get("power", 0)
                max_power = data.get("max_power", 100)
                has_power = data.get("has_power", False)
                power_status = "供电中" if has_power else "断电"
                content_lines.append(f"电力: {power}/{max_power}")
                content_lines.append(f"状态: {power_status}")
            else:
                ammo = data.get("ammo", 0)
                has_ammo = data.get("has_ammo", False)
                ammo_status = "有弹药" if has_ammo else "无弹药"
                content_lines.append(f"弹药: {ammo}")
                content_lines.append(f"状态: {ammo_status}")
        else:
            content_lines = []

        for i, line in enumerate(content_lines):
            if content_y + i * 18 > popup_y + popup_height - 10:
                break
            text = self.font.render(line, True, UI_TEXT)
            screen.blit(text, (popup_x + 10, content_y + i * 18))

    def handle_direction_popup_event(self, event) -> tuple:
        """处理方向悬浮窗事件

        Args:
            event: pygame事件

        Returns:
            (handled, direction, building, tile_pos) - 是否处理、选择的方向、建筑类型、瓦片位置
        """
        if not self.direction_popup_active:
            return False, None, None, None

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            for direction, rect in self.direction_popup_buttons.items():
                if rect.collidepoint(event.pos):
                    building = self.direction_popup_building
                    tile_pos = self.direction_popup_tile_pos
                    self.hide_direction_popup()
                    return True, direction, building, tile_pos

        return False, None, None, None

    def update(self, delta_time):
        """更新UI状态"""
        # 更新弹窗计时
        if self.toast_timer > 0:
            self.toast_timer -= delta_time

    def draw(self, screen):
        """渲染UI"""
        # 绘制顶部资源栏
        self._draw_resource_bar(screen)

        # 绘制右侧建筑面板
        self._draw_side_panel(screen)

        # 绘制弹窗提示
        if self.toast_timer > 0:
            self._draw_toast(screen)

        # 绘制方向选择悬浮窗
        if self.direction_popup_active:
            self._draw_direction_popup(screen)

        # 绘制悬浮窗（机器信息）
        if self.hover_tooltip_active:
            self._draw_hover_tooltip(screen)

    def _draw_direction_popup(self, screen):
        """绘制方向选择悬浮窗"""
        if not self.direction_popup_position:
            return

        center_x, center_y = self.direction_popup_position

        # 根据按钮数量计算背景大小
        btn_count = len(self.direction_popup_buttons)
        if btn_count == 8:
            popup_width = 200
            popup_height = 200
        else:
            popup_width = 180
            popup_height = 180

        # 背景
        bg_rect = pygame.Rect(
            center_x - popup_width // 2,
            center_y - popup_height // 2,
            popup_width,
            popup_height
        )
        pygame.draw.rect(screen, UI_PANEL, bg_rect)
        pygame.draw.rect(screen, UI_ACCENT, bg_rect, 3)

        # 标题
        title_text = self.font_title.render("选择方向", True, UI_TEXT)
        title_rect = title_text.get_rect(center=(center_x, center_y - popup_height // 2 + 15))
        screen.blit(title_text, title_rect)

        # 绘制方向按钮
        for direction, rect in self.direction_popup_buttons.items():
            pygame.draw.rect(screen, UI_ACCENT, rect)
            pygame.draw.rect(screen, UI_BORDER_LIGHT, rect, 1)

            # 区分4方向和8方向的符号显示
            if btn_count == 4:
                # 4方向符号映射
                dir_symbols_4 = {0: '↑', 1: '→', 2: '↓', 3: '←'}
                symbol = dir_symbols_4.get(direction, '?')
            else:
                symbol = DIR_SYMBOLS.get(direction, '?')
            text = self.font_bold.render(symbol, True, UI_TEXT_LIGHT)
            text_rect = text.get_rect(center=rect.center)
            screen.blit(text, text_rect)

    def _draw_resource_bar(self, screen):
        """绘制顶部资源栏"""
        # 资源栏背景
        pygame.draw.rect(screen, UI_PANEL, (0, 0, SCREEN_WIDTH - self.side_panel_width, self.resource_bar_height))
        pygame.draw.line(screen, UI_BORDER, (0, self.resource_bar_height), (SCREEN_WIDTH - self.side_panel_width, self.resource_bar_height), 2)

        # 资源图标和数值
        inventory = self.game_scene.player_inventory
        resources = [
            ("iron_ore", "铁矿石", inventory.get("iron_ore", 0), (140, 90, 70)),
            ("copper_ore", "铜矿石", inventory.get("copper_ore", 0), (180, 100, 50)),
            ("coal", "煤矿", inventory.get("coal", 0), (50, 50, 50)),
            ("ammo", "弹药", inventory.get("ammo", 0), (200, 180, 50)),
        ]

        x = 20
        y = 15
        for item_id, name, count, color in resources:
            # 资源色块
            pygame.draw.rect(screen, color, (x, y, 16, 16))
            pygame.draw.rect(screen, UI_BORDER, (x, y, 16, 16), 1)

            # 资源名称和数量
            text = self.font.render(f"{name}: {count}", True, UI_TEXT)
            screen.blit(text, (x + 22, y + 1))
            x += 110

        # 金币
        gold_x = 480
        pygame.draw.rect(screen, (220, 180, 50), (gold_x, y, 16, 16))
        pygame.draw.rect(screen, UI_BORDER, (gold_x, y, 16, 16), 1)
        gold_text = self.font_large.render(f"{self.gold}", True, UI_TEXT)
        screen.blit(gold_text, (gold_x + 22, y - 2))

        # 波次信息 (资源右边)
        wave_x = gold_x + 120
        wave_color = UI_ACCENT_RED if self.lives <= 3 else UI_ACCENT
        wave_text = self.font_large.render(f"第 {self.wave} 波", True, wave_color)
        screen.blit(wave_text, (wave_x, y - 2))

        # 生命值
        lives_x = wave_x + 100
        lives_color = UI_ACCENT_RED if self.lives <= 3 else UI_ACCENT_GREEN
        lives_text = self.font_large.render(f"生命: {self.lives}", True, lives_color)
        screen.blit(lives_text, (lives_x, y - 2))

        # 倒计时 (右上角)
        if self.countdown_active:
            mins = int(self.countdown_time) // 60
            secs = int(self.countdown_time) % 60
            countdown_text = self.font_large.render(f"倒计时: {mins:02d}:{secs:02d}", True, UI_ACCENT_ORANGE)
            countdown_x = SCREEN_WIDTH - self.side_panel_width - 150
            screen.blit(countdown_text, (countdown_x, y - 2))
        else:
            countdown_text = self.font_large.render("等待开始", True, UI_TEXT_DIM)
            countdown_x = SCREEN_WIDTH - self.side_panel_width - 100
            screen.blit(countdown_text, (countdown_x, y - 2))

    def _draw_side_panel(self, screen):
        """绘制右侧建筑面板"""
        panel_x = self.side_panel_x
        panel_height = SCREEN_HEIGHT

        # 面板背景
        pygame.draw.rect(screen, UI_PANEL, (panel_x, 0, self.side_panel_width, panel_height))

        # 标题
        title_rect = pygame.Rect(panel_x, 0, self.side_panel_width, 40)
        pygame.draw.rect(screen, UI_ACCENT, title_rect)
        title_text = self.font_title.render("建筑列表", True, UI_TEXT_LIGHT)
        title_rect_text = title_text.get_rect(center=(panel_x + self.side_panel_width // 2, 20))
        screen.blit(title_text, title_rect_text)

        # 绘制建筑按钮
        for key, btn in self.buttons.items():
            if not key.startswith("building_"):
                continue

            color = UI_ACCENT if btn.get("active") else UI_PANEL_LIGHT
            text_color = UI_TEXT_LIGHT if btn.get("active") else UI_TEXT

            pygame.draw.rect(screen, color, btn["rect"])
            pygame.draw.rect(screen, UI_BORDER_LIGHT, btn["rect"], 1)

            text = self.font_small.render(btn["label"], True, text_color)
            text_rect = text.get_rect(center=btn["rect"].center)
            screen.blit(text, text_rect)

        # 工业电力系统面板
        self._draw_power_panel(screen)

    def _draw_power_panel(self, screen):
        """绘制工业电力系统信息面板"""
        if not hasattr(self.game_scene, 'power_manager'):
            return

        pm = self.game_scene.power_manager
        stats = pm.get_stats()

        panel_x = self.side_panel_x
        # 面板位置：在按钮下方
        y = 400  # 建筑按钮总高度约为 ~360px

        # 分隔线
        pygame.draw.line(screen, UI_ACCENT,
                        (panel_x + 10, y), (panel_x + self.side_panel_width - 10, y), 2)

        # 标题
        title = self.font_small.render("⚡ 电力网络", True, (0, 200, 255))
        screen.blit(title, (panel_x + 10, y + 8))

        y += 30

        # 网络状态
        status = stats.get("status", "offline")
        status_colors = {
            "normal": (0, 255, 100),
            "low_power": (255, 200, 50),
            "offline": (255, 80, 80)
        }
        status_color = status_colors.get(status, (150, 150, 150))
        status_cn = {"normal": "正常", "low_power": "缺电", "offline": "断电"}

        status_text = self.font_small.render(
            f"状态: {status_cn.get(status, status)}",
            True, status_color
        )
        screen.blit(status_text, (panel_x + 10, y))
        y += 20

        # 统计信息
        lines = [
            (f"发电: {stats.get('total_generation', 0):.0f} EU/t",
             (0, 255, 100)),
            (f"消耗: {stats.get('total_consumption', 0):.0f} EU/t",
             (255, 200, 50)),
            (f"储能: {stats.get('total_storage', 0):.0f} / "
             f"{stats.get('total_capacity', 0):.0f} EU",
             (0, 180, 255) if stats.get('total_storage', 0) > 0 else (150, 150, 150)),
            (f"网络数: {stats.get('network_count', 0)}",
             (180, 180, 190)),
        ]

        for text, color in lines:
            surf = self.font_small.render(text, True, color)
            screen.blit(surf, (panel_x + 10, y))
            y += 18

        # 显示各网络详情
        networks = pm.get_network_stats()
        for net in networks:
            net_y = y + 8
            gen = net.get("generation", 0)
            con = net.get("consumption", 0)
            if gen > 0 or con > 0:
                net_text = (f"网{net.get('network_id', '?')}: "
                           f"发{gen:.0f} 耗{con:.0f}EU/t")
                surf = self.font_small.render(net_text, True, (140, 140, 150))
                screen.blit(surf, (panel_x + 10, net_y))
                y = net_y + 16

    def _draw_toast(self, screen):
        """绘制弹窗提示"""
        # 计算透明度 (最后0.5秒淡出)
        alpha = 255
        if self.toast_timer < 0.5:
            alpha = int(255 * (self.toast_timer / 0.5))

        # 弹窗大小
        toast_width = 300
        toast_height = 60
        toast_x = (SCREEN_WIDTH - self.side_panel_width - toast_width) // 2
        toast_y = 80

        # 半透明背景
        toast_surface = pygame.Surface((toast_width, toast_height), pygame.SRCALPHA)
        toast_surface.fill((0, 0, 0, int(180 * alpha / 255)))
        screen.blit(toast_surface, (toast_x, toast_y))

        # 边框
        pygame.draw.rect(screen, UI_ACCENT_RED, (toast_x, toast_y, toast_width, toast_height), 2)

        # 提示文字
        text = self.font_large.render(self.toast_message, True, (255, 255, 255))
        text_rect = text.get_rect(center=(toast_x + toast_width // 2, toast_y + toast_height // 2))
        screen.blit(text, text_rect)
