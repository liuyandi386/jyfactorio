#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""工业电力系统测试程序 - 电线9宫格方向配置

独立测试工具，演示基于电线的电力系统：
  - 电线是电力传输核心，有4个独立方向面（上/右/下/左）
  - 每面可单独设为 INPUT / TRANSFER / OUTPUT
  - 建筑（发电机/电容库/电力塔）通过相邻电线接入电网
  - 点击电线弹出9宫格面配置界面

用法：
  cd game
  python power_test.py

操作：
  1. 左键点击按钮选择放置对象
  2. 左键点击网格放置
  3. 左键点击电线 → 打开9宫格面配置
  4. 在面配置界面点击方向：INPUT→TRANSFER→OUTPUT→NONE→INPUT
  5. 右键点击移除
  6. Space 键 开始/暂停电力模拟
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import pygame

# ========== 配置常量 ==========
GRID_WIDTH = 12
GRID_HEIGHT = 8
CELL_SIZE = 48
PANEL_WIDTH = 220

SCREEN_WIDTH = GRID_WIDTH * CELL_SIZE + PANEL_WIDTH
SCREEN_HEIGHT = GRID_HEIGHT * CELL_SIZE + 80

# ========== 颜色主题 ==========
BG_COLOR = (30, 30, 35)
GRID_LINE = (50, 50, 55)
PANEL_BG = (40, 42, 48)
PANEL_ACCENT = (0, 150, 255)
BTN_COLOR = (66, 66, 75)
BTN_HOVER = (90, 90, 100)
BTN_SELECTED = (0, 120, 220)
TEXT_WHITE = (230, 230, 240)
TEXT_GREEN = (0, 220, 100)
TEXT_RED = (255, 80, 80)
TEXT_YELLOW = (255, 220, 50)

# ========== 单元格类型 ==========
CELL_EMPTY = 0
CELL_GENERATOR = 1
CELL_CAPACITOR = 2
CELL_WIRE = 3
CELL_TOWER = 4

# ========== 面配置常量 ==========
FACE_NONE = 0
FACE_INPUT = 1
FACE_TRANSFER = 2
FACE_OUTPUT = 3

DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

DIR_CHARS = {0: "↑", 1: "→", 2: "↓", 3: "←"}
DIR_OFFSETS = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}
DIR_OPPOSITE = {0: 2, 1: 3, 2: 0, 3: 1}

FACE_LABELS = {FACE_NONE: "NONE", FACE_INPUT: "INP", FACE_TRANSFER: "TRS", FACE_OUTPUT: "OUT"}
FACE_COLOR_MAP = {
    FACE_NONE: (80, 80, 85),
    FACE_INPUT: (0, 140, 255),
    FACE_TRANSFER: (180, 180, 40),
    FACE_OUTPUT: (255, 140, 0)
}

# 电线默认面配置（全是TRANSFER，可任意连接）
DEFAULT_WIRE_FACES = [FACE_TRANSFER, FACE_TRANSFER, FACE_TRANSFER, FACE_TRANSFER]


class PowerTestGrid:
    """电力测试网格"""

    def __init__(self, w, h):
        self.width = w
        self.height = h
        self.grid = [[CELL_EMPTY for _ in range(h)] for _ in range(w)]

        # 电线面配置: (x, y) → [UP, RIGHT, DOWN, LEFT]
        self.wire_faces = {}

        # 设备状态
        self.generator_running = {}
        self.capacitor_energy = {}
        self.tower_powered = {}

    def place(self, x, y, cell_type):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return False
        if self.grid[x][y] != CELL_EMPTY:
            return False

        self.grid[x][y] = cell_type
        pos = (x, y)

        if cell_type == CELL_WIRE:
            self.wire_faces[pos] = list(DEFAULT_WIRE_FACES)
        elif cell_type == CELL_GENERATOR:
            self.generator_running[pos] = True
        elif cell_type == CELL_CAPACITOR:
            self.capacitor_energy[pos] = 0.0
        elif cell_type == CELL_TOWER:
            self.tower_powered[pos] = False
        return True

    def remove(self, x, y):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        ct = self.grid[x][y]
        pos = (x, y)
        self.grid[x][y] = CELL_EMPTY
        if ct == CELL_WIRE:
            self.wire_faces.pop(pos, None)
        elif ct == CELL_GENERATOR:
            self.generator_running.pop(pos, None)
        elif ct == CELL_CAPACITOR:
            self.capacitor_energy.pop(pos, None)
        elif ct == CELL_TOWER:
            self.tower_powered.pop(pos, None)

    def cycle_wire_face(self, x, y, direction):
        """切换电线指定方向面模式: NONE→INPUT→TRANSFER→OUTPUT→NONE"""
        pos = (x, y)
        if pos not in self.wire_faces:
            return
        self.wire_faces[pos][direction] = (self.wire_faces[pos][direction] + 1) % 4

    def get_wire_face(self, x, y, direction):
        pos = (x, y)
        if pos not in self.wire_faces:
            return FACE_NONE
        return self.wire_faces[pos][direction]

    def is_wire(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.grid[x][y] == CELL_WIRE
        return False

    def is_device(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.grid[x][y] in (CELL_GENERATOR, CELL_CAPACITOR, CELL_TOWER)
        return False

    def get_connected_devices(self):
        """通过电线构建电力网络拓扑

        电线是传输核心：
        - 电线与相邻电线（4方向）连通
        - 建筑与相邻电线连通
        - 面配置影响电力流向（OUTPUT→INPUT匹配）

        Returns:
            list of dict: 每个独立网络
        """
        all_nodes = set()
        device_type = {}

        for x in range(self.width):
            for y in range(self.height):
                ct = self.grid[x][y]
                if ct == CELL_EMPTY:
                    continue
                pos = (x, y)
                all_nodes.add(pos)
                if ct == CELL_GENERATOR:
                    device_type[pos] = 'generator'
                elif ct == CELL_CAPACITOR:
                    device_type[pos] = 'capacitor'
                elif ct == CELL_TOWER:
                    device_type[pos] = 'tower'
                elif ct == CELL_WIRE:
                    device_type[pos] = 'wire'

        if not all_nodes:
            return []

        # 构建邻接关系
        adjacency = {pos: set() for pos in all_nodes}

        for pos in all_nodes:
            x, y = pos
            ct = self.grid[x][y]
            for d in range(4):
                dx, dy = DIR_OFFSETS[d]
                nx, ny = x + dx, y + dy
                npos = (nx, ny)
                if npos not in all_nodes:
                    continue
                nct = self.grid[nx][ny]
                # 电线 ↔ 电线：直接连通（面配置不影响连通性，只影响电力流向）
                # 电线 ↔ 建筑：连通
                # 建筑 ↔ 建筑：不直接连通
                if ct == CELL_WIRE or nct == CELL_WIRE:
                    adjacency[pos].add(npos)
                    adjacency[npos].add(pos)

        # BFS 找连通分量
        visited = set()
        networks = []

        for start in all_nodes:
            if start in visited:
                continue

            component = set()
            queue = [start]
            visited.add(start)

            while queue:
                cur = queue.pop(0)
                component.add(cur)
                for nb in adjacency.get(cur, []):
                    if nb not in visited:
                        visited.add(nb)
                        queue.append(nb)

            gens, caps, twrs, wrs = [], [], [], []
            for pos in component:
                dt = device_type.get(pos)
                if dt == 'generator':
                    gens.append(pos)
                elif dt == 'capacitor':
                    caps.append(pos)
                elif dt == 'tower':
                    twrs.append(pos)
                elif dt == 'wire':
                    wrs.append(pos)

            if gens or caps or twrs:
                networks.append({
                    'generators': gens,
                    'capacitors': caps,
                    'towers': twrs,
                    'wires': wrs,
                    'adjacency': adjacency
                })

        return networks


class PowerTestApp:
    GENERATOR_OUTPUT = 32
    TOWER_CONSUMPTION = 8
    CAPACITOR_CAPACITY = 50000
    CAPACITOR_MAX_IO = 64

    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption("电力系统测试 - 电线9宫格方向配置")
        self.clock = pygame.time.Clock()

        try:
            self.font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 13)
            self.font_small = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 10)
            self.font_bold = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 14)
        except Exception:
            self.font = pygame.font.Font(None, 14)
            self.font_small = pygame.font.Font(None, 10)
            self.font_bold = pygame.font.Font(None, 14)

        self.grid = PowerTestGrid(GRID_WIDTH, GRID_HEIGHT)
        self.running = False
        self.selected_type = None
        self.mouse_pos = (0, 0)

        # 面配置编辑（仅电线）
        self.face_edit_target = None
        self.face_edit_buttons = {}

        self.buttons = []
        self._init_buttons()

        self.network_status = "offline"
        self.network_stats = []
        self.total_generation = 0.0
        self.total_consumption = 0.0
        self.total_storage = 0.0

    def _init_buttons(self):
        btn_x = SCREEN_WIDTH - PANEL_WIDTH + 10
        btn_y = 50
        btn_w = PANEL_WIDTH - 40
        btn_h = 36
        gap = 6

        items = [
            ("燃煤发电机 32EU/t", CELL_GENERATOR, (50, 55, 50)),
            ("电容库", CELL_CAPACITOR, (40, 45, 55)),
            ("电线", CELL_WIRE, (80, 80, 85)),
            ("电力塔 8EU/t", CELL_TOWER, (100, 50, 200)),
        ]

        for i, (label, ct, color) in enumerate(items):
            rect = pygame.Rect(btn_x, btn_y + i * (btn_h + gap), btn_w, btn_h)
            self.buttons.append({
                "rect": rect, "label": label,
                "cell_type": ct, "color": color, "selected": False
            })

    def run(self):
        running = True
        while running:
            dt = self.clock.tick(60) / 1000.0
            if self.running:
                dt = min(dt, 0.1)

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.MOUSEMOTION:
                    self.mouse_pos = event.pos
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    self._handle_mouse(event)
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_SPACE:
                        self.running = not self.running
                    elif event.key == pygame.K_r:
                        self.grid = PowerTestGrid(GRID_WIDTH, GRID_HEIGHT)
                        self.running = False
                        self.selected_type = None
                        self.face_edit_target = None
                        for btn in self.buttons:
                            btn["selected"] = False
                    elif event.key == pygame.K_ESCAPE:
                        self.face_edit_target = None

            if self.running:
                self._update_power(dt)

            self._draw()
        pygame.quit()

    def _handle_mouse(self, event):
        pos = event.pos

        # 面配置编辑模式（仅电线）
        if self.face_edit_target is not None:
            for d, rect in self.face_edit_buttons.items():
                if rect.collidepoint(pos):
                    fx, fy = self.face_edit_target
                    self.grid.cycle_wire_face(fx, fy, d)
                    return
            gx, gy = self.face_edit_target
            center_rect = pygame.Rect(gx * CELL_SIZE, gy * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            if not center_rect.inflate(CELL_SIZE * 3, CELL_SIZE * 3).collidepoint(pos):
                self.face_edit_target = None
                return
            return

        # 按钮点击
        for btn in self.buttons:
            if btn["rect"].collidepoint(pos):
                for b in self.buttons:
                    b["selected"] = False
                btn["selected"] = True
                self.selected_type = btn["cell_type"]
                return

        # 网格点击
        grid_x = pos[0] // CELL_SIZE
        grid_y = pos[1] // CELL_SIZE
        if 0 <= grid_x < GRID_WIDTH and 0 <= grid_y < GRID_HEIGHT:
            ct = self.grid.grid[grid_x][grid_y]
            if event.button == 1:  # 左键
                if ct == CELL_WIRE:
                    # 点击电线 → 打开9宫格（无论是否选中工具）
                    self.face_edit_target = (grid_x, grid_y)
                elif self.selected_type is not None:
                    # 空格子上放置建筑
                    self.grid.place(grid_x, grid_y, self.selected_type)
                elif ct != CELL_EMPTY:
                    # 点击建筑也可以看（但不编辑面）
                    pass
            elif event.button == 3:  # 右键移除
                self.grid.remove(grid_x, grid_y)
                if self.face_edit_target == (grid_x, grid_y):
                    self.face_edit_target = None

    def _update_power(self, dt):
        networks = self.grid.get_connected_devices()

        total_gen = 0.0
        total_con = 0.0
        total_sto = 0.0
        overall_status = "offline"
        self.network_stats = []

        for net in networks:
            gens = net['generators']
            caps = net['capacitors']
            towers = net['towers']
            adjacency = net['adjacency']

            # BFS从发电机出发，穿过电线，找到所有能收到电的设备
            # 规则：电力从发电机流向相邻电线 → 电线间按面配置(OUTPUT→INPUT)传输 → 到达建筑
            powered_nodes = set()
            queue = []

            # 发电机启动BFS
            for gx, gy in gens:
                powered_nodes.add((gx, gy))
                # 发电机向4方向相邻电线输出
                for d in range(4):
                    dx, dy = DIR_OFFSETS[d]
                    nx, ny = gx + dx, gy + dy
                    npos = (nx, ny)
                    if npos in adjacency.get((gx, gy), set()):
                        if self.grid.grid[nx][ny] == CELL_WIRE:
                            # 检查电线面的匹配
                            wire_face = self.grid.get_wire_face(nx, ny, DIR_OPPOSITE[d])
                            if wire_face in (FACE_INPUT, FACE_TRANSFER):
                                powered_nodes.add(npos)
                                queue.append(npos)
                        else:
                            # 直接相邻的建筑
                            powered_nodes.add(npos)
                            queue.append(npos)

            while queue:
                cx, cy = queue.pop(0)
                ct = self.grid.grid[cx][cy]
                for d in range(4):
                    dx, dy = DIR_OFFSETS[d]
                    nx, ny = cx + dx, cy + dy
                    npos = (nx, ny)
                    if npos in powered_nodes:
                        continue
                    if npos not in adjacency.get((cx, cy), set()):
                        continue

                    nct = self.grid.grid[nx][ny]
                    if ct == CELL_WIRE:
                        # 从电线到相邻节点
                        my_face = self.grid.get_wire_face(cx, cy, d)
                        if nct == CELL_WIRE:
                            # 电线→电线：需要对面是INPUT/TRANSFER
                            their_face = self.grid.get_wire_face(nx, ny, DIR_OPPOSITE[d])
                            if my_face in (FACE_OUTPUT, FACE_TRANSFER) and their_face in (FACE_INPUT, FACE_TRANSFER):
                                powered_nodes.add(npos)
                                queue.append(npos)
                        else:
                            # 电线→建筑：电线是OUTPUT/TRANSFER即可
                            if my_face in (FACE_OUTPUT, FACE_TRANSFER):
                                powered_nodes.add(npos)
                                queue.append(npos)
                    else:
                        # 从建筑到相邻电线（建筑是发电机/电容库有电）
                        if nct == CELL_WIRE:
                            wire_face = self.grid.get_wire_face(nx, ny, DIR_OPPOSITE[d])
                            if wire_face in (FACE_INPUT, FACE_TRANSFER):
                                powered_nodes.add(npos)
                                queue.append(npos)

            # 电容库可以有电后向相邻电线供电
            for cap in caps:
                energy = self.grid.capacitor_energy.get(cap, 0.0)
                if energy > 0 and cap in powered_nodes:
                    cx, cy = cap
                    for d in range(4):
                        dx, dy = DIR_OFFSETS[d]
                        nx, ny = cx + dx, cy + dy
                        npos = (nx, ny)
                        if npos in powered_nodes:
                            continue
                        if npos not in adjacency.get(cap, set()):
                            continue
                        nct = self.grid.grid[nx][ny]
                        if nct == CELL_WIRE:
                            wire_face = self.grid.get_wire_face(nx, ny, DIR_OPPOSITE[d])
                            if wire_face in (FACE_INPUT, FACE_TRANSFER):
                                powered_nodes.add(npos)
                                queue.append(npos)
                                while queue:
                                    cx2, cy2 = queue.pop(0)
                                    ct2 = self.grid.grid[cx2][cy2]
                                    for d2 in range(4):
                                        dx2, dy2 = DIR_OFFSETS[d2]
                                        nx2, ny2 = cx2 + dx2, cy2 + dy2
                                        npos2 = (nx2, ny2)
                                        if npos2 in powered_nodes:
                                            continue
                                        if npos2 not in adjacency.get((cx2, cy2), set()):
                                            continue
                                        nct2 = self.grid.grid[nx2][ny2]
                                        if ct2 == CELL_WIRE:
                                            my_f = self.grid.get_wire_face(cx2, cy2, d2)
                                            if nct2 == CELL_WIRE:
                                                their_f = self.grid.get_wire_face(nx2, ny2, DIR_OPPOSITE[d2])
                                                if my_f in (FACE_OUTPUT, FACE_TRANSFER) and their_f in (FACE_INPUT, FACE_TRANSFER):
                                                    powered_nodes.add(npos2)
                                                    queue.append(npos2)
                                            else:
                                                if my_f in (FACE_OUTPUT, FACE_TRANSFER):
                                                    powered_nodes.add(npos2)
                                                    queue.append(npos2)

            effective_gens = [g for g in gens if g in powered_nodes]
            effective_caps = [c for c in caps if c in powered_nodes]
            effective_towers = [t for t in towers if t in powered_nodes]

            if not effective_gens and not effective_caps:
                net_status = "offline"
                for tx, ty in towers:
                    self.grid.tower_powered[(tx, ty)] = False
                net_gen = 0.0
                net_con = 0.0
                net_sto = 0.0
            else:
                net_gen = len(effective_gens) * self.GENERATOR_OUTPUT
                net_con = len(effective_towers) * self.TOWER_CONSUMPTION

                generation = net_gen
                consumption = net_con

                if generation >= consumption:
                    surplus = generation - consumption
                    if effective_caps:
                        per_cap = surplus / len(effective_caps)
                        for cx, cy in effective_caps:
                            energy = self.grid.capacitor_energy.get((cx, cy), 0.0)
                            space = self.CAPACITOR_CAPACITY - energy
                            self.grid.capacitor_energy[(cx, cy)] = energy + min(per_cap, space, self.CAPACITOR_MAX_IO)
                    for tx, ty in effective_towers:
                        self.grid.tower_powered[(tx, ty)] = True
                    net_status = "normal"
                else:
                    deficit = consumption - generation
                    discharged = 0.0
                    if effective_caps:
                        per_cap = deficit / len(effective_caps)
                        for cx, cy in effective_caps:
                            energy = self.grid.capacitor_energy.get((cx, cy), 0.0)
                            d = min(per_cap, energy, self.CAPACITOR_MAX_IO)
                            self.grid.capacitor_energy[(cx, cy)] = energy - d
                            discharged += d

                    effective_gen = generation + discharged
                    if effective_gen >= consumption:
                        for tx, ty in effective_towers:
                            self.grid.tower_powered[(tx, ty)] = True
                        net_status = "normal"
                    else:
                        ratio = effective_gen / consumption if consumption > 0 else 0.0
                        for tx, ty in effective_towers:
                            self.grid.tower_powered[(tx, ty)] = ratio >= 1.0
                        net_status = "low_power"

                if not gens:
                    stored = sum(self.grid.capacitor_energy.get(c, 0.0) for c in effective_caps)
                    if stored <= 0:
                        net_status = "offline"
                        for tx, ty in effective_towers:
                            self.grid.tower_powered[(tx, ty)] = False

                net_sto = sum(self.grid.capacitor_energy.get((cx, cy), 0.0) for cx, cy in effective_caps)

            for tx, ty in towers:
                if (tx, ty) not in powered_nodes:
                    self.grid.tower_powered[(tx, ty)] = False

            self.network_stats.append({
                'generation': net_gen,
                'consumption': net_con,
                'storage': net_sto,
                'status': net_status,
                'generators': len(gens),
                'capacitors': len(caps),
                'towers': len(towers),
                'wires': len(net.get('wires', []))
            })

            total_gen += net_gen
            total_con += net_con
            total_sto += net_sto

            if net_status == "normal" and overall_status != "low_power":
                overall_status = "normal"
            elif net_status == "low_power":
                overall_status = "low_power"

        if not networks:
            overall_status = "offline"

        self.total_generation = total_gen
        self.total_consumption = total_con
        self.total_storage = total_sto
        self.network_status = overall_status

    def _build_face_edit_buttons(self, gx, gy):
        cx = gx * CELL_SIZE + CELL_SIZE // 2
        cy = gy * CELL_SIZE + CELL_SIZE // 2
        btn_size = 28
        spacing = CELL_SIZE

        self.face_edit_buttons = {
            DIR_UP: pygame.Rect(cx - btn_size // 2, cy - spacing - btn_size // 2, btn_size, btn_size),
            DIR_RIGHT: pygame.Rect(cx + spacing - btn_size // 2, cy - btn_size // 2, btn_size, btn_size),
            DIR_DOWN: pygame.Rect(cx - btn_size // 2, cy + spacing - btn_size // 2, btn_size, btn_size),
            DIR_LEFT: pygame.Rect(cx - spacing - btn_size // 2, cy - btn_size // 2, btn_size, btn_size),
        }

    def _draw(self):
        self.screen.fill(BG_COLOR)

        # 绘制网格
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                rect = pygame.Rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE)
                ct = self.grid.grid[x][y]
                pos = (x, y)
                cx = x * CELL_SIZE + CELL_SIZE // 2
                cy = y * CELL_SIZE + CELL_SIZE // 2

                if ct == CELL_EMPTY:
                    pygame.draw.rect(self.screen, (40, 42, 48), rect)
                elif ct == CELL_GENERATOR:
                    color = (55, 85, 55) if self.grid.generator_running.get(pos, False) else (50, 55, 50)
                    pygame.draw.rect(self.screen, color, rect)
                    pygame.draw.polygon(self.screen, (255, 215, 0), [
                        (cx - 4, cy - 2), (cx - 1, cy - 2),
                        (cx - 2, cy + 2), (cx + 1, cy + 2),
                        (cx, cy + 6), (cx - 3, cy + 2)
                    ])
                    self._draw_label(f"发电机\n{self.GENERATOR_OUTPUT}EU/t", x, y, TEXT_GREEN)
                elif ct == CELL_CAPACITOR:
                    pygame.draw.rect(self.screen, (40, 45, 55), rect)
                    energy = self.grid.capacitor_energy.get(pos, 0.0)
                    bar_h = 6
                    bar_y = (y + 1) * CELL_SIZE - bar_h - 2
                    pygame.draw.rect(self.screen, (30, 30, 30),
                                   (x * CELL_SIZE + 2, bar_y, CELL_SIZE - 4, bar_h))
                    ratio = energy / self.CAPACITOR_CAPACITY if self.CAPACITOR_CAPACITY else 0
                    ec = (0, 150, 255) if ratio > 0.3 else (200, 50, 50)
                    pygame.draw.rect(self.screen, ec,
                                   (x * CELL_SIZE + 2, bar_y, int((CELL_SIZE - 4) * ratio), bar_h))
                    self._draw_label(f"电容\n{int(energy)}", x, y)
                elif ct == CELL_WIRE:
                    faces = self.grid.wire_faces.get(pos, DEFAULT_WIRE_FACES)
                    # 底色
                    pygame.draw.rect(self.screen, (60, 60, 65), rect)
                    # 4方向面指示
                    self._draw_wire_face_indicators(x, y, pos, faces, rect)
                    # 中心点
                    pygame.draw.circle(self.screen, (200, 200, 200), (cx, cy), 3)
                elif ct == CELL_TOWER:
                    powered = self.grid.tower_powered.get(pos, False)
                    color = (100, 50, 200) if powered else (80, 40, 40)
                    pygame.draw.rect(self.screen, color, rect)
                    self._draw_label(
                        f"电力塔\n{self.TOWER_CONSUMPTION}EU/t\n{'ON' if powered else 'OFF'}",
                        x, y, TEXT_GREEN if powered else TEXT_RED
                    )
                    lc = (0, 255, 0) if powered else (255, 0, 0)
                    pygame.draw.circle(self.screen, lc, (cx, cy + 4), 4)

                pygame.draw.rect(self.screen, GRID_LINE, rect, 1)

        # 面配置编辑覆盖层（仅电线）
        if self.face_edit_target is not None:
            self._draw_face_editor()

        # 绘制连接线
        self._draw_connections()

        # UI面板
        self._draw_ui()

        pygame.display.flip()

    def _draw_wire_face_indicators(self, gx, gy, pos, faces, rect):
        """在电线上绘制4个方向面的颜色指示"""
        half = CELL_SIZE // 2
        third = CELL_SIZE // 3
        indicator_size = max(3, CELL_SIZE // 10)

        face_positions = {
            DIR_UP: (gx * CELL_SIZE + half, gy * CELL_SIZE + third),
            DIR_RIGHT: (gx * CELL_SIZE + CELL_SIZE - third, gy * CELL_SIZE + half),
            DIR_DOWN: (gx * CELL_SIZE + half, gy * CELL_SIZE + CELL_SIZE - third),
            DIR_LEFT: (gx * CELL_SIZE + third, gy * CELL_SIZE + half),
        }

        for d in range(4):
            mode = faces[d]
            color = FACE_COLOR_MAP.get(mode, (80, 80, 85))
            fx, fy = face_positions[d]
            pygame.draw.rect(self.screen, color,
                           (fx - indicator_size, fy - indicator_size,
                            indicator_size * 2, indicator_size * 2),
                           border_radius=1)

    def _draw_face_editor(self):
        """绘制电线的9宫格方向编辑界面"""
        gx, gy = self.face_edit_target
        cx = gx * CELL_SIZE + CELL_SIZE // 2
        cy = gy * CELL_SIZE + CELL_SIZE // 2

        if self.grid.grid[gx][gy] != CELL_WIRE:
            self.face_edit_target = None
            return

        self._build_face_edit_buttons(gx, gy)
        faces = self.grid.wire_faces.get((gx, gy), DEFAULT_WIRE_FACES)

        # 半透明覆盖
        overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 120))
        self.screen.blit(overlay, (0, 0))

        # 中心电线高亮
        center_rect = pygame.Rect(gx * CELL_SIZE, gy * CELL_SIZE, CELL_SIZE, CELL_SIZE)
        hl_surf = pygame.Surface((CELL_SIZE, CELL_SIZE), pygame.SRCALPHA)
        hl_surf.fill((255, 255, 0, 80))
        self.screen.blit(hl_surf, center_rect)
        pygame.draw.rect(self.screen, (255, 255, 0), center_rect, 3)

        # 4个方向按钮
        for d in range(4):
            rect = self.face_edit_buttons[d]
            mode = faces[d]
            color = FACE_COLOR_MAP.get(mode, (80, 80, 85))
            hover = rect.collidepoint(self.mouse_pos)

            pygame.draw.rect(self.screen, color, rect, border_radius=4)
            if hover:
                pygame.draw.rect(self.screen, (255, 255, 255), rect, 3, border_radius=4)
            else:
                pygame.draw.rect(self.screen, (180, 180, 180), rect, 1, border_radius=4)

            arrow = DIR_CHARS[d]
            arrow_surf = self.font_bold.render(arrow, True, TEXT_WHITE)
            ar = arrow_surf.get_rect(center=rect.center)
            self.screen.blit(arrow_surf, ar)

            label = FACE_LABELS[mode]
            label_surf = self.font_small.render(label, True, TEXT_WHITE)
            lr = label_surf.get_rect(center=(rect.centerx, rect.bottom + 10))
            self.screen.blit(label_surf, lr)

        # 提示
        tip = self.font.render(
            "编辑电线方向 - 点击切换 INP/TRS/OUT/NONE",
            True, TEXT_YELLOW
        )
        tip_rect = tip.get_rect(center=(cx, gy * CELL_SIZE - 30))
        self.screen.blit(tip, tip_rect)
        tip2 = self.font_small.render("[ESC]或点击外部关闭", True, (180, 180, 180))
        tip2_rect = tip2.get_rect(center=(cx, (gy + 1) * CELL_SIZE + 50))
        self.screen.blit(tip2, tip2_rect)

    def _draw_connections(self):
        """绘制电线连接线"""
        networks = self.grid.get_connected_devices()
        drawn = set()

        for net in networks:
            adjacency = net['adjacency']
            for n1 in adjacency:
                x1, y1 = n1
                for n2 in adjacency[n1]:
                    x2, y2 = n2
                    key = (min(n1, n2), max(n1, n2))
                    if key in drawn:
                        continue
                    drawn.add(key)

                    scx = x1 * CELL_SIZE + CELL_SIZE // 2
                    scy = y1 * CELL_SIZE + CELL_SIZE // 2
                    dcx = x2 * CELL_SIZE + CELL_SIZE // 2
                    dcy = y2 * CELL_SIZE + CELL_SIZE // 2

                    ct1 = self.grid.grid[x1][y1]
                    ct2 = self.grid.grid[x2][y2]

                    if ct1 == CELL_WIRE and ct2 == CELL_WIRE:
                        color = (80, 80, 90)
                    elif ct1 == CELL_WIRE or ct2 == CELL_WIRE:
                        color = (100, 100, 110)
                    elif ct1 == CELL_GENERATOR or ct2 == CELL_GENERATOR:
                        color = (80, 180, 80)
                    elif ct1 == CELL_TOWER or ct2 == CELL_TOWER:
                        powered = self.grid.tower_powered.get(
                            (x1, y1) if ct1 == CELL_TOWER else (x2, y2), False
                        )
                        color = (0, 200, 0) if powered else (150, 100, 100)
                    else:
                        color = (120, 120, 130)

                    line_w = max(1, CELL_SIZE // 16)
                    pygame.draw.line(self.screen, color, (scx, scy), (dcx, dcy), line_w)

    def _draw_ui(self):
        # 底部状态栏
        status_y = GRID_HEIGHT * CELL_SIZE
        pygame.draw.rect(self.screen, (20, 22, 28), (0, status_y, SCREEN_WIDTH, 80))
        pygame.draw.line(self.screen, PANEL_ACCENT, (0, status_y), (SCREEN_WIDTH, status_y), 2)

        status_cn = {"normal": "正常", "low_power": "缺电", "offline": "断电"}
        status_text = f"状态: {status_cn.get(self.network_status, self.network_status)}  |  "
        status_text += f"发电: {int(self.total_generation)} EU/t  |  "
        status_text += f"消耗: {int(self.total_consumption)} EU/t  |  "
        status_text += f"储能: {int(self.total_storage)} EU  |  "
        status_text += f"{'运行中' if self.running else '已暂停'}"
        ps = self.font_bold.render(status_text, True, TEXT_WHITE)
        self.screen.blit(ps, (10, status_y + 10))

        hints = [
            "[Space]启动/暂停 [R]重置 [左键电线]9宫格 [右键]移除 [ESC]关闭",
            "发电机(测试无限燃料) → 电线 → 电容/电力塔"
        ]
        for i, hint in enumerate(hints):
            hs = self.font_small.render(hint, True, (150, 150, 155))
            self.screen.blit(hs, (10, status_y + 34 + i * 18))

        # 右侧面板
        panel_x = SCREEN_WIDTH - PANEL_WIDTH
        pygame.draw.rect(self.screen, PANEL_BG, (panel_x, 0, PANEL_WIDTH, SCREEN_HEIGHT))
        pygame.draw.line(self.screen, (60, 60, 65), (panel_x, 0), (panel_x, SCREEN_HEIGHT), 2)

        title = self.font_bold.render("电力系统测试", True, PANEL_ACCENT)
        self.screen.blit(title, (panel_x + 10, 10))

        # 按钮
        for btn in self.buttons:
            rect = btn["rect"]
            color = BTN_SELECTED if btn["selected"] else btn["color"]
            if rect.collidepoint(self.mouse_pos) and not btn["selected"]:
                color = BTN_HOVER
            pygame.draw.rect(self.screen, color, rect, border_radius=4)
            pygame.draw.rect(self.screen, (80, 80, 90), rect, 1, border_radius=4)
            label = self.font_small.render(btn["label"], True, TEXT_WHITE)
            lr = label.get_rect(center=rect.center)
            self.screen.blit(label, lr)

        # 网络统计
        y = 240
        nt = self.font_bold.render("网络状态", True, (200, 200, 200))
        self.screen.blit(nt, (panel_x + 10, y))
        y += 22
        for i, net in enumerate(self.network_stats):
            lines = [
                f"网{i}: 发{net['generation']:.0f} 耗{net['consumption']:.0f}EU/t",
                f"   G:{net['generators']} C:{net['capacitors']} T:{net['towers']} W:{net.get('wires',0)}",
                f"   储能: {int(net['storage'])} EU",
            ]
            sc = {"normal": TEXT_GREEN, "low_power": TEXT_YELLOW, "offline": TEXT_RED}
            for line in lines:
                ls = self.font_small.render(line, True, (160, 160, 170))
                self.screen.blit(ls, (panel_x + 10, y))
                y += 14
            ss = self.font_small.render(f"   {status_cn.get(net['status'], '?')}", True, sc.get(net['status'], TEXT_WHITE))
            self.screen.blit(ss, (panel_x + 10, y))
            y += 16

        # 图例
        legend_y = SCREEN_HEIGHT - 190
        ls = self.font_small.render("电线方向图例:", True, (200, 200, 200))
        self.screen.blit(ls, (panel_x + 10, legend_y))
        legends = [
            ("■ 蓝色 INP = 输入", (0, 140, 255)),
            ("■ 黄色 TRS = 传输", (180, 180, 40)),
            ("■ 橙色 OUT = 输出", (255, 140, 0)),
            ("■ 灰色 NONE = 无", (80, 80, 85)),
            ("", (255, 255, 255)),
            ("OUT→IN 电力流向", (0, 220, 100)),
            ("点击电线编辑面配置", (160, 160, 170)),
        ]
        for i, (text, color) in enumerate(legends):
            if not text:
                continue
            ls2 = self.font_small.render(text, True, color)
            self.screen.blit(ls2, (panel_x + 10, legend_y + 16 + i * 15))

        if self.selected_type is not None:
            hint = self.font.render("左键网格放置", True, TEXT_YELLOW)
            self.screen.blit(hint, (panel_x + 10, SCREEN_HEIGHT - 60))
        else:
            hint = self.font.render("左键电线=9宫格", True, (150, 150, 155))
            self.screen.blit(hint, (panel_x + 10, SCREEN_HEIGHT - 60))

    def _draw_label(self, text, gx, gy, color=TEXT_WHITE):
        cx = gx * CELL_SIZE + CELL_SIZE // 2
        cy = gy * CELL_SIZE + CELL_SIZE // 2
        lines = text.split('\n')
        for i, line in enumerate(lines):
            surf = self.font_small.render(line, True, color)
            sr = surf.get_rect(center=(cx, cy + (i - len(lines) / 2 + 0.5) * 14))
            self.screen.blit(surf, sr)


def main():
    app = PowerTestApp()
    app.run()


if __name__ == "__main__":
    main()
