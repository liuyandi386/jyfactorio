#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
虚空动力传送带测试程序
复刻 Minecraft 机械动力模组传送带逻辑，搭配自定义虚空动力机制

核心功能：
1. 两点式放置传送带（起点-终点自动填充）
2. 虚空动力机制（无外部能源，永久运行）
3. 物品运输规则（禁止电力类物品）
4. 阻塞逻辑和多段衔接
5. 完整测试用例覆盖
"""

import sys
import os

# 使用标准库实现简单GUI（避免依赖第三方库）
try:
    import tkinter as tk
    from tkinter import messagebox, simpledialog
    HAS_TKINTER = True
except ImportError:
    HAS_TKINTER = False

# 配置常量
GRID_WIDTH = 15      # 网格宽度
GRID_HEIGHT = 10     # 网格高度
CELL_SIZE = 40       # 每个格子大小（像素）
MAX_BELT_LENGTH = 10 # 单条传送带最大长度
BUCKET_CAPACITY = 5  # 储物桶容量上限

# 格子类型
CELL_EMPTY = 0
CELL_CONVEYOR = 1
CELL_OBSTACLE = 2
CELL_START = 3
CELL_END = 4
CELL_BUCKET = 5      # 储物桶

# 方向定义
DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

# 方向符号
DIR_SYMBOLS = ['UP', 'RT', 'DN', 'LT']

# 物品类型
ITEM_TYPES = [
    ('iron_ore', '铁矿石', 'Fe', True),
    ('copper_ore', '铜矿石', 'Cu', True),
    ('coal', '煤矿', 'Co', True),
    ('iron_ingot', '铁锭', 'FeI', True),
    ('copper_ingot', '铜锭', 'CuI', True),
    ('gear', '齿轮', 'G', True),
    ('circuit', '电路板', 'C', True),
    ('ammo', '弹药', 'A', True),
    ('electricity', '电力', 'EL', False),  # 禁止运输
    ('energy_cell', '能量单元', 'EC', False),  # 禁止运输
]


class ConveyorGrid:
    """传送带网格系统"""
    
    def __init__(self, width, height):
        self.width = width
        self.height = height
        
        # 网格数据：(cell_type, direction, item)
        self.grid = [[{'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None} 
                      for _ in range(height)] for _ in range(width)]
        
        # 放置状态
        self.placing_start = None  # (x, y) or None
        self.selected_cells = []   # 预览选中的格子
        
        # 运行状态
        # 记录上一步结束时传送带起点的占用状态，用于储物桶输出判断
        self.last_frame_start_occupied = {}
        self.is_running = False
        self.log = []
        
    def get_cell(self, x, y):
        """获取指定格子的数据"""
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.grid[x][y]
        return None
    
    def is_valid_position(self, x, y):
        """检查位置是否在网格范围内"""
        return 0 <= x < self.width and 0 <= y < self.height
    
    def can_place_conveyor(self, x1, y1, x2, y2):
        """检查是否可以放置传送带
        
        Args:
            x1, y1: 起点坐标
            x2, y2: 终点坐标
        
        Returns:
            (bool, message): 是否可以放置及原因
        """
        # 检查是否是同一点
        if x1 == x2 and y1 == y2:
            return False, "起点和终点不能相同"
        
        # 检查是否在同一条水平线或垂直线上
        if x1 != x2 and y1 != y2:
            return False, "仅支持水平或垂直直线放置，斜向无效"
        
        # 计算路径长度
        length = abs(x2 - x1) + abs(y2 - y1) + 1
        if length > MAX_BELT_LENGTH:
            return False, f"传送带长度({length})超过最大限制({MAX_BELT_LENGTH})"
        
        # 确定方向
        if x1 == x2:
            # 垂直方向
            direction = DIR_DOWN if y2 > y1 else DIR_UP
        else:
            # 水平方向
            direction = DIR_RIGHT if x2 > x1 else DIR_LEFT
        
        # 检查路径上的所有格子
        cells = self._get_line_cells(x1, y1, x2, y2)
        
        for i, (x, y) in enumerate(cells):
            cell = self.grid[x][y]
            
            # 起点位置：允许连接到已有传送带的终点（方向匹配时）
            if i == 0:
                if cell['type'] == CELL_END:
                    # 检查方向是否匹配（已有终点的方向应与新起点方向一致）
                    if cell['direction'] != direction:
                        return False, f"位置 ({x},{y}) 方向不匹配，无法衔接"
                    # 允许衔接，继续检查其他位置
                    continue
                elif cell['type'] != CELL_EMPTY:
                    return False, f"位置 ({x},{y}) 已被占用"
            
            # 终点位置：允许连接到已有传送带的起点（方向匹配时）
            elif i == len(cells) - 1:
                if cell['type'] == CELL_START:
                    # 检查方向是否匹配
                    if cell['direction'] != direction:
                        return False, f"位置 ({x},{y}) 方向不匹配，无法衔接"
                    # 允许衔接，继续检查其他位置
                    continue
                elif cell['type'] != CELL_EMPTY:
                    return False, f"位置 ({x},{y}) 已被占用"
            
            # 中间位置：必须是空的
            else:
                if cell['type'] != CELL_EMPTY:
                    return False, f"位置 ({x},{y}) 已被占用"
        
        return True, f"可以放置，长度: {length}, 方向: {DIR_SYMBOLS[direction]}"
    
    def _get_line_cells(self, x1, y1, x2, y2):
        """获取两点之间的所有格子（包括起点和终点）"""
        cells = []
        
        if x1 == x2:
            # 垂直线
            min_y = min(y1, y2)
            max_y = max(y1, y2)
            for y in range(min_y, max_y + 1):
                cells.append((x1, y))
        else:
            # 水平线
            min_x = min(x1, x2)
            max_x = max(x1, x2)
            for x in range(min_x, max_x + 1):
                cells.append((x, y1))
        
        return cells
    
    def place_conveyor(self, x1, y1, x2, y2):
        """放置传送带
        
        Args:
            x1, y1: 起点坐标
            x2, y2: 终点坐标
        
        Returns:
            (bool, message): 是否成功及原因
        """
        # 验证放置条件
        can_place, msg = self.can_place_conveyor(x1, y1, x2, y2)
        if not can_place:
            return False, msg
        
        # 确定方向
        if x1 == x2:
            direction = DIR_DOWN if y2 > y1 else DIR_UP
        else:
            direction = DIR_RIGHT if x2 > x1 else DIR_LEFT
        
        # 获取路径上的所有格子
        cells = self._get_line_cells(x1, y1, x2, y2)
        
        # 检查起点是否衔接已有传送带
        start_cell = self.grid[cells[0][0]][cells[0][1]]
        start_is_connection = start_cell['type'] == CELL_END
        
        # 检查终点是否衔接已有传送带
        end_cell = self.grid[cells[-1][0]][cells[-1][1]]
        end_is_connection = end_cell['type'] == CELL_START
        
        # 设置起点（如果不是衔接点）
        if not start_is_connection:
            sx, sy = cells[0]
            self.grid[sx][sy] = {'type': CELL_START, 'direction': direction, 'item': None}
        else:
            # 将已有终点转换为中间段
            sx, sy = cells[0]
            self.grid[sx][sy] = {'type': CELL_CONVEYOR, 'direction': direction, 'item': start_cell['item']}
        
        # 设置中间格子
        for i in range(1, len(cells) - 1):
            x, y = cells[i]
            self.grid[x][y] = {'type': CELL_CONVEYOR, 'direction': direction, 'item': None}
        
        # 设置终点（如果不是衔接点）
        if not end_is_connection:
            ex, ey = cells[-1]
            self.grid[ex][ey] = {'type': CELL_END, 'direction': direction, 'item': None}
        else:
            # 将已有起点转换为中间段
            ex, ey = cells[-1]
            self.grid[ex][ey] = {'type': CELL_CONVEYOR, 'direction': direction, 'item': end_cell['item']}
        
        self.add_log(f"传送带放置成功: ({x1},{y1}) → ({x2},{y2}), 方向: {DIR_SYMBOLS[direction]}, 长度: {len(cells)}")
        return True, f"传送带放置成功"
    
    def add_obstacle(self, x, y):
        """添加障碍物"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_EMPTY:
            return False, "该位置已被占用"
        
        self.grid[x][y] = {'type': CELL_OBSTACLE, 'direction': DIR_RIGHT, 'item': None}
        self.add_log(f"障碍物放置: ({x},{y})")
        return True, "障碍物放置成功"
    
    def remove_obstacle(self, x, y):
        """移除障碍物"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_OBSTACLE:
            return False, "该位置不是障碍物"
        
        self.grid[x][y] = {'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None}
        self.add_log(f"障碍物移除: ({x},{y})")
        return True, "障碍物移除成功"
    
    def add_item(self, x, y, item_type):
        """在指定位置添加物品
        
        Args:
            x, y: 位置坐标
            item_type: 物品类型（来自ITEM_TYPES）
        
        Returns:
            (bool, message): 是否成功及原因
        """
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        # 检查物品是否允许运输
        item_info = next((i for i in ITEM_TYPES if i[0] == item_type), None)
        if item_info is None:
            return False, "未知物品类型"
        
        if not item_info[3]:
            return False, f"禁止放置能源类物品: {item_info[1]}"
        
        cell = self.grid[x][y]
        
        # 只能在传送带上放置物品
        if cell['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END]:
            return False, "只能在传送带上放置物品"
        
        # 检查是否已有物品
        if cell['item'] is not None:
            return False, "该位置已有物品"
        
        cell['item'] = item_type
        self.add_log(f"物品放置: {item_info[1]} at ({x},{y})")
        return True, f"物品放置成功: {item_info[1]}"
    
    def remove_item(self, x, y):
        """移除指定位置的物品"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['item'] is None:
            return False, "该位置没有物品"
        
        item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
        item_name = item_info[1] if item_info else "未知物品"
        
        cell['item'] = None
        self.add_log(f"物品移除: {item_name} at ({x},{y})")
        return True, f"物品移除成功"
    
    def place_bucket(self, x, y):
        """放置储物桶
        
        Args:
            x, y: 位置坐标
        
        Returns:
            (bool, message): 是否成功及原因
        """
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_EMPTY:
            return False, "该位置已被占用"
        
        # 创建储物桶，包含物品列表
        self.grid[x][y] = {
            'type': CELL_BUCKET,
            'direction': DIR_RIGHT,
            'item': None,
            'items': []  # 储物桶内的物品列表（FIFO）
        }
        self.add_log(f"储物桶放置: ({x},{y})")
        return True, "储物桶放置成功"
    
    def remove_bucket(self, x, y):
        """移除储物桶"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_BUCKET:
            return False, "该位置不是储物桶"
        
        # 获取桶内物品数量
        item_count = len(cell.get('items', []))
        self.grid[x][y] = {'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None}
        self.add_log(f"储物桶移除: ({x},{y})，内含 {item_count} 个物品")
        return True, f"储物桶移除成功，丢弃 {item_count} 个物品"
    
    def add_item_to_bucket(self, x, y, item_type):
        """向储物桶添加物品
        
        Args:
            x, y: 储物桶位置
            item_type: 物品类型
        
        Returns:
            (bool, message): 是否成功及原因
        """
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_BUCKET:
            return False, "该位置不是储物桶"
        
        # 检查物品是否允许存放
        item_info = next((i for i in ITEM_TYPES if i[0] == item_type), None)
        if item_info is None:
            return False, "未知物品类型"
        
        if not item_info[3]:
            return False, f"禁止存放能源类物品: {item_info[1]}"
        
        # 检查是否已满
        items = cell.get('items', [])
        if len(items) >= BUCKET_CAPACITY:
            return False, f"储物桶已满（容量: {BUCKET_CAPACITY}）"
        
        # 添加物品
        items.append(item_type)
        cell['items'] = items
        self.add_log(f"物品存入储物桶: {item_info[1]} at ({x},{y}), 当前数量: {len(items)}/{BUCKET_CAPACITY}")
        return True, f"物品存入成功: {item_info[1]}"
    
    def get_bucket_items(self, x, y):
        """获取储物桶内物品信息"""
        if not self.is_valid_position(x, y):
            return None
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_BUCKET:
            return None
        
        return cell.get('items', [])
    
    def bucket_is_full(self, x, y):
        """检查储物桶是否已满"""
        items = self.get_bucket_items(x, y)
        if items is None:
            return False
        return len(items) >= BUCKET_CAPACITY
    
    def bucket_has_items(self, x, y):
        """检查储物桶是否有物品"""
        items = self.get_bucket_items(x, y)
        return items is not None and len(items) > 0
    
    def _update_conveyor_to_bucket(self):
        """更新传送带末端到储物桶的传输"""
        for x in range(self.width):
            for y in range(self.height):
                cell = self.grid[x][y]
                
                # 处理所有传送带类型（包括末端）
                if cell['type'] not in [CELL_END, CELL_CONVEYOR, CELL_START] or cell['item'] is None:
                    continue
                
                # 计算传送带输出方向
                dx, dy = 0, 0
                if cell['direction'] == DIR_UP:
                    dy = -1
                elif cell['direction'] == DIR_DOWN:
                    dy = 1
                elif cell['direction'] == DIR_LEFT:
                    dx = -1
                elif cell['direction'] == DIR_RIGHT:
                    dx = 1
                
                # 检查输出位置
                output_x, output_y = x + dx, y + dy
                if not self.is_valid_position(output_x, output_y):
                    continue
                
                output_cell = self.grid[output_x][output_y]
                
                # 检查是否是储物桶
                if output_cell['type'] == CELL_BUCKET:
                    items = output_cell.get('items', [])
                    if len(items) < BUCKET_CAPACITY:
                        # 将物品放入储物桶
                        items.append(cell['item'])
                        output_cell['items'] = items
                        item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
                        item_name = item_info[1] if item_info else "未知物品"
                        self.add_log(f"物品从传送带进入储物桶: {item_name}")
                        cell['item'] = None  # 清空传送带上的物品
    
    def _update_bucket_to_conveyor(self, start_cell_occupied=None):
        """更新储物桶到传送带起点的输出"""
        for x in range(self.width):
            for y in range(self.height):
                cell = self.grid[x][y]
                
                # 只处理储物桶
                if cell['type'] != CELL_BUCKET:
                    continue
                
                items = cell.get('items', [])
                if len(items) == 0:
                    continue
                
                # 检查四个方向是否有传送带起点
                for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                    neighbor_x, neighbor_y = x + dx, y + dy
                    if not self.is_valid_position(neighbor_x, neighbor_y):
                        continue
                    
                    neighbor_cell = self.grid[neighbor_x][neighbor_y]
                    
                    # 检查是否是传送带起点且方向朝向储物桶
                    if neighbor_cell['type'] == CELL_START:
                        # 确定传送带是否朝向储物桶
                        conv_dir = neighbor_cell['direction']
                        # 传送带方向应该与储物桶到传送带的方向相同
                        # 即物品可以从储物桶进入传送带并沿传送带方向移动
                        if ((conv_dir == DIR_LEFT and dx == -1) or   # 传送带向左，储物桶在右边
                            (conv_dir == DIR_RIGHT and dx == 1) or    # 传送带向右，储物桶在左边
                            (conv_dir == DIR_UP and dy == -1) or      # 传送带上，储物桶在下边
                            (conv_dir == DIR_DOWN and dy == 1)):      # 传送带向下，储物桶在上边
                            
                            # 检查传送带起点是否为空
                            # 如果提供了上一帧的状态，使用它；否则使用当前状态
                            if start_cell_occupied is not None:
                                was_occupied = start_cell_occupied.get((neighbor_x, neighbor_y))
                                if was_occupied is None:
                                    # 如果上一帧没有记录，使用当前状态
                                    was_occupied = neighbor_cell['item'] is not None
                            else:
                                was_occupied = neighbor_cell['item'] is not None
                            
                            if not was_occupied:
                                # 将储物桶第一个物品放入传送带
                                item_type = items.pop(0)
                                cell['items'] = items
                                neighbor_cell['item'] = item_type
                                item_info = next((i for i in ITEM_TYPES if i[0] == item_type), None)
                                item_name = item_info[1] if item_info else "未知物品"
                                self.add_log(f"物品从储物桶输出到传送带: {item_name}")
                                break  # 每次只输出一个物品
    
    def clear_grid(self):
        """清空整个网格"""
        self.grid = [[{'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None} 
                      for _ in range(self.height)] for _ in range(self.width)]
        self.placing_start = None
        self.selected_cells = []
        self.add_log("网格已清空")
    
    def delete_conveyor(self, x, y):
        """删除指定位置所在的整条传送带"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END]:
            return False, "该位置不是传送带"
        
        # 找到整条传送带
        direction = cell['direction']
        belt_cells = []
        
        # 找到起点
        cx, cy = x, y
        while True:
            prev_x, prev_y = cx, cy
            if direction == DIR_UP:
                cy += 1
            elif direction == DIR_DOWN:
                cy -= 1
            elif direction == DIR_LEFT:
                cx += 1
            elif direction == DIR_RIGHT:
                cx -= 1
            
            if not self.is_valid_position(cx, cy) or self.grid[cx][cy]['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END]:
                start_x, start_y = prev_x, prev_y
                break
        
        # 收集所有传送带格子
        cx, cy = start_x, start_y
        while True:
            if self.is_valid_position(cx, cy) and self.grid[cx][cy]['type'] in [CELL_START, CELL_CONVEYOR, CELL_END]:
                belt_cells.append((cx, cy))
                
                if direction == DIR_UP:
                    cy -= 1
                elif direction == DIR_DOWN:
                    cy += 1
                elif direction == DIR_LEFT:
                    cx -= 1
                elif direction == DIR_RIGHT:
                    cx += 1
            else:
                break
        
        # 删除所有传送带格子
        for cx, cy in belt_cells:
            self.grid[cx][cy] = {'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None}
        
        self.add_log(f"传送带删除成功，共 {len(belt_cells)} 格")
        return True, f"传送带删除成功"
    
    def step(self):
        """单步运行"""
        changed = False
        
        # 记录传送带起点在本帧开始时的占用状态
        current_start_occupied = {}
        for x in range(self.width):
            for y in range(self.height):
                cell = self.grid[x][y]
                if cell['type'] == CELL_START:
                    current_start_occupied[(x, y)] = cell['item'] is not None
        
        # 使用上一帧结束时的起点状态来判断储物桶是否应该输出
        # 这样如果上一步结束时起点被占用，储物桶就会暂停输出
        previous_start_occupied = self.last_frame_start_occupied
        
        # 创建物品移动计划（避免同时修改导致问题）
        moves = []
        
        for x in range(self.width):
            for y in range(self.height):
                cell = self.grid[x][y]
                if cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END] and cell['item'] is not None:
                    # 计算下一格位置
                    dx, dy = 0, 0
                    if cell['direction'] == DIR_UP:
                        dy = -1
                    elif cell['direction'] == DIR_DOWN:
                        dy = 1
                    elif cell['direction'] == DIR_LEFT:
                        dx = -1
                    elif cell['direction'] == DIR_RIGHT:
                        dx = 1
                    
                    next_x, next_y = x + dx, y + dy
                    
                    # 检查下一格是否有效
                    if not self.is_valid_position(next_x, next_y):
                        # 超出边界，物品消失（或可选择其他处理）
                        moves.append((x, y, None, None))
                        self.add_log(f"物品 {cell['item']} 从传送带末端离开")
                        changed = True
                        continue
                    
                    next_cell = self.grid[next_x][next_y]
                    
                    # 检查下一格是否可以接收物品
                    if next_cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END] and next_cell['item'] is None:
                        # 可以移动
                        moves.append((x, y, next_x, next_y))
                        changed = True
                    elif next_cell['type'] == CELL_OBSTACLE or next_cell['item'] is not None:
                        # 阻塞
                        self.add_log(f"物品 {cell['item']} 在 ({x},{y}) 阻塞")
                    elif next_cell['type'] == CELL_BUCKET:
                        # 检查储物桶是否已满
                        if len(next_cell.get('items', [])) < BUCKET_CAPACITY:
                            # 可以放入储物桶（在第二步处理）
                            pass
                        else:
                            # 储物桶已满，阻塞
                            self.add_log(f"物品 {cell['item']} 在 ({x},{y}) 阻塞（储物桶已满）")
        
        # 执行移动
        for from_x, from_y, to_x, to_y in moves:
            cell = self.grid[from_x][from_y]
            item = cell['item']
            cell['item'] = None
            
            if to_x is not None and to_y is not None:
                self.grid[to_x][to_y]['item'] = item
        
        # 第二步：传送带输出到储物桶
        self._update_conveyor_to_bucket()
        
        # 第三步：储物桶输出到传送带（在传送带动之后执行）
        # 使用上一帧结束时的起点状态判断，这样如果上一步结束时起点被占用，储物桶就会暂停输出
        self._update_bucket_to_conveyor(previous_start_occupied)
        
        # 更新上一帧的起点状态记录
        self.last_frame_start_occupied = current_start_occupied
        
        return changed
    
    def add_log(self, message):
        """添加日志"""
        self.log.append(message)
        if len(self.log) > 50:
            self.log.pop(0)
    
    def get_status_report(self):
        """获取状态报告"""
        report = []
        conveyor_count = 0
        item_count = 0
        obstacle_count = 0
        
        for x in range(self.width):
            for y in range(self.height):
                cell = self.grid[x][y]
                if cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END]:
                    conveyor_count += 1
                    if cell['item'] is not None:
                        item_count += 1
                elif cell['type'] == CELL_OBSTACLE:
                    obstacle_count += 1
        
        report.append(f"网格状态: {conveyor_count} 传送带格, {item_count} 物品, {obstacle_count} 障碍物")
        
        return report


class TestRunner:
    """测试用例运行器"""
    
    def __init__(self, grid):
        self.grid = grid
    
    def run_all_tests(self):
        """运行所有测试用例"""
        tests = [
            ("四向传送带放置测试", self.test_directions),
            ("物品运输测试", self.test_item_transport),
            ("电力物品拦截测试", self.test_electricity_block),
            ("障碍物放置失败测试", self.test_obstacle_block),
            ("物品阻塞测试", self.test_item_blocking),
            ("多段衔接测试", self.test_conveyor_connection),
            ("长度限制测试", self.test_length_limit),
            ("储物桶基本操作测试", self.test_bucket_basic),
            ("传送带→储物桶测试", self.test_conveyor_to_bucket),
            ("储物桶→传送带测试", self.test_bucket_to_conveyor),
        ]
        
        results = []
        for name, test_func in tests:
            print(f"\n=== 测试: {name} ===")
            try:
                result = test_func()
                results.append((name, result))
                print(f"结果: {'通过' if result else '失败'}")
            except Exception as e:
                results.append((name, False))
                print(f"结果: 异常 - {e}")
        
        # 汇总
        passed = sum(1 for _, r in results if r)
        total = len(results)
        print(f"\n=== 测试汇总 ===")
        print(f"通过: {passed}/{total}")
        
        return passed == total
    
    def test_directions(self):
        """测试四向传送带放置"""
        self.grid.clear_grid()
        
        # 向右
        success, _ = self.grid.place_conveyor(0, 4, 5, 4)
        if not success:
            print("向右放置失败")
            return False
        
        # 向左
        success, _ = self.grid.place_conveyor(10, 5, 7, 5)
        if not success:
            print("向左放置失败")
            return False
        
        # 向下
        success, _ = self.grid.place_conveyor(12, 0, 12, 4)
        if not success:
            print("向下放置失败")
            return False
        
        # 向上
        success, _ = self.grid.place_conveyor(13, 6, 13, 2)
        if not success:
            print("向上放置失败")
            return False
        
        print("四向放置全部成功")
        return True
    
    def test_item_transport(self):
        """测试物品正常运输"""
        self.grid.clear_grid()
        
        # 放置传送带
        success, _ = self.grid.place_conveyor(0, 5, 6, 5)
        if not success:
            return False
        
        # 添加物品
        success, _ = self.grid.add_item(0, 5, 'iron_ore')
        if not success:
            return False
        
        # 运行几步
        for _ in range(7):
            self.grid.step()
        
        # 检查物品是否到达终点
        cell = self.grid.get_cell(6, 5)
        if cell['item'] != 'iron_ore':
            print("物品未到达终点")
            return False
        
        print("物品运输测试通过")
        return True
    
    def test_electricity_block(self):
        """测试电力物品拦截"""
        self.grid.clear_grid()
        
        # 放置传送带
        success, _ = self.grid.place_conveyor(0, 5, 3, 5)
        if not success:
            return False
        
        # 尝试放置电力物品
        success, msg = self.grid.add_item(0, 5, 'electricity')
        if success:
            print("电力物品未被拦截")
            return False
        
        print(f"电力物品拦截测试通过: {msg}")
        return True
    
    def test_obstacle_block(self):
        """测试障碍物导致放置失败"""
        self.grid.clear_grid()
        
        # 先放置障碍物
        success, _ = self.grid.add_obstacle(2, 5)
        if not success:
            return False
        
        # 尝试跨越障碍物放置传送带
        success, msg = self.grid.place_conveyor(0, 5, 4, 5)
        if success:
            print("障碍物未阻止传送带放置")
            return False
        
        print(f"障碍物阻止放置测试通过: {msg}")
        return True
    
    def test_item_blocking(self):
        """测试物品阻塞"""
        self.grid.clear_grid()
        
        # 放置传送带
        success, _ = self.grid.place_conveyor(0, 5, 4, 5)
        if not success:
            return False
        
        # 在终点放置障碍物
        success, _ = self.grid.add_obstacle(5, 5)
        if not success:
            return False
        
        # 添加多个物品
        success, _ = self.grid.add_item(0, 5, 'iron_ore')
        success, _ = self.grid.add_item(1, 5, 'copper_ore')
        
        # 运行几步
        for _ in range(5):
            self.grid.step()
        
        # 检查是否有物品阻塞
        blocked = False
        for x in range(5):
            cell = self.grid.get_cell(x, 5)
            if cell['item'] is not None:
                blocked = True
                break
        
        if not blocked:
            print("物品未阻塞")
            return False
        
        print("物品阻塞测试通过")
        return True
    
    def test_conveyor_connection(self):
        """测试多段传送带衔接"""
        self.grid.clear_grid()
        
        # 放置两段衔接的传送带
        success, _ = self.grid.place_conveyor(0, 5, 3, 5)
        if not success:
            return False
        
        success, _ = self.grid.place_conveyor(3, 5, 6, 5)
        if not success:
            return False
        
        # 添加物品
        success, _ = self.grid.add_item(0, 5, 'iron_ore')
        if not success:
            return False
        
        # 运行足够步数
        for _ in range(10):
            self.grid.step()
        
        # 检查物品是否穿过衔接点到达终点
        cell = self.grid.get_cell(6, 5)
        if cell['item'] != 'iron_ore':
            print("物品未通过衔接点")
            return False
        
        print("多段衔接测试通过")
        return True
    
    def test_length_limit(self):
        """测试长度限制"""
        self.grid.clear_grid()
        
        # 尝试放置超长传送带
        success, msg = self.grid.place_conveyor(0, 5, 12, 5)  # 13格，超过MAX_BELT_LENGTH=10
        if success:
            print("超长传送带未被拦截")
            return False
        
        print(f"长度限制测试通过: {msg}")
        return True
    
    def test_bucket_basic(self):
        """测试储物桶基本操作：放置、移除、手动物品投放、桶满拦截、违禁品拦截"""
        self.grid.clear_grid()
        
        # 1. 放置储物桶
        success, _ = self.grid.place_bucket(5, 5)
        if not success:
            print("储物桶放置失败")
            return False
        
        # 2. 手动存入物品
        success, _ = self.grid.add_item_to_bucket(5, 5, 'iron_ore')
        if not success:
            print("物品存入失败")
            return False
        
        # 3. 违禁品拦截测试
        success, msg = self.grid.add_item_to_bucket(5, 5, 'electricity')
        if success:
            print("违禁品未被拦截")
            return False
        
        # 4. 填充到满容量
        for _ in range(4):  # 已有1个，还需4个
            success, _ = self.grid.add_item_to_bucket(5, 5, 'copper_ore')
            if not success:
                print("物品填充失败")
                return False
        
        # 5. 桶满拦截测试
        success, msg = self.grid.add_item_to_bucket(5, 5, 'coal')
        if success:
            print("桶满未被拦截")
            return False
        
        # 6. 移除储物桶
        success, _ = self.grid.remove_bucket(5, 5)
        if not success:
            print("储物桶移除失败")
            return False
        
        print("储物桶基本操作测试通过")
        return True
    
    def test_conveyor_to_bucket(self):
        """测试传送带末端对接储物桶：物品自动入桶、桶满导致阻塞"""
        self.grid.clear_grid()
        
        # 放置传送带（向右）和储物桶（在传送带终点右侧）
        success, _ = self.grid.place_conveyor(0, 5, 2, 5)  # 终点在(2,5)
        if not success:
            return False
        
        success, _ = self.grid.place_bucket(3, 5)  # 储物桶在(3,5)
        if not success:
            return False
        
        # 在传送带起点添加物品
        success, _ = self.grid.add_item(0, 5, 'iron_ore')
        if not success:
            return False
        
        # 运行直到物品到达终点并进入储物桶
        for _ in range(10):
            self.grid.step()
        
        # 检查储物桶是否收到物品
        items = self.grid.get_bucket_items(3, 5)
        if items is None or len(items) == 0:
            print("物品未进入储物桶")
            return False
        
        # 填满储物桶（手动添加）
        for _ in range(BUCKET_CAPACITY - len(items)):
            self.grid.add_item_to_bucket(3, 5, 'copper_ore')
        
        # 检查储物桶是否已满
        if not self.grid.bucket_is_full(3, 5):
            print("储物桶未满")
            return False
        
        # 在传送带起点添加物品，应该阻塞
        success, _ = self.grid.add_item(0, 5, 'coal')
        if not success:
            return False
        
        # 运行几步让物品堆积
        for _ in range(10):
            self.grid.step()
        
        # 检查传送带上是否有阻塞的物品
        blocked = False
        for x in range(3):  # 传送带范围是0-2
            cell = self.grid.get_cell(x, 5)
            if cell['item'] is not None:
                blocked = True
                break
        
        if not blocked:
            print("桶满未导致传送带阻塞")
            return False
        
        print("传送带末端对接储物桶测试通过")
        return True
    
    def test_bucket_to_conveyor(self):
        """测试储物桶对接传送带起点：桶内物品自动输出、传送带首格占用时暂停输出"""
        self.grid.clear_grid()
        
        # 放置储物桶在(1,5)，传送带起点在(2,5)，方向向右
        # 这样储物桶右边相邻的就是传送带起点
        success, _ = self.grid.place_bucket(1, 5)  # 储物桶在左侧
        if not success:
            return False
        
        # 放置向右的传送带，起点在(2,5)，终点在(5,5)
        success, _ = self.grid.place_conveyor(2, 5, 5, 5)
        if not success:
            return False
        
        # 向储物桶添加物品
        for _ in range(3):
            success, _ = self.grid.add_item_to_bucket(1, 5, 'iron_ore')
            if not success:
                return False
        
        # 在传送带起点放置物品（先占用，防止储物桶输出）
        start_cell = self.grid.get_cell(2, 5)
        start_cell['item'] = 'copper_ore'  # 占用起点
        
        # 运行几步
        for _ in range(3):
            self.grid.step()
        
        # 检查储物桶是否还有3个物品（应该暂停输出）
        items = self.grid.get_bucket_items(1, 5)
        if len(items) != 3:
            print("储物桶在传送带占用时未暂停输出")
            return False
        
        # 移除起点物品，允许输出
        start_cell['item'] = None
        
        # 运行一步
        self.grid.step()
        
        # 检查传送带上是否有物品
        found_item = False
        for x in range(2, 6):
            cell = self.grid.get_cell(x, 5)
            if cell['item'] == 'iron_ore':
                found_item = True
                break
        
        if not found_item:
            print("储物桶未输出物品到传送带")
            return False
        
        print("储物桶对接传送带起点测试通过")
        return True


def print_help():
    """打印帮助信息"""
    help_text = """
========================================
    虚空动力传送带测试程序 v1.0
========================================

操作说明：
  鼠标操作：
    左键点击两次 = 放置传送带（起点到终点）
    Shift+左键 = 添加障碍物
    Ctrl+左键 = 移除障碍物/物品/传送带

  快捷键：
    P = 放置物品（选择物品类型）
    R = 移除物品
    D = 删除传送带
    S = 单步运行
    A = 自动连续运行（再次按A停止）
    C = 清空网格
    T = 运行测试用例
    Q = 退出程序

  方向符号：
    UP = 向上    DN = 向下
    LT = 向左    RT = 向右

  物品符号：
    Fe = 铁矿石  Cu = 铜矿石  Co = 煤矿
    FeI = 铁锭   CuI = 铜锭   G = 齿轮
    C = 电路板   A = 弹药     EL = 电力(禁止)

  网格符号：
    . = 空格子   # = 障碍物
    S = 传送带起点    E = 传送带终点
    RT DN LT UP = 传送带方向

========================================
"""
    print(help_text)


def main():
    """主程序入口"""
    print_help()
    
    if HAS_TKINTER:
        # 使用Tkinter GUI版本
        run_gui_version()
    else:
        # 使用控制台版本
        run_console_version()


def run_console_version():
    """控制台版本"""
    grid = ConveyorGrid(GRID_WIDTH, GRID_HEIGHT)
    
    while True:
        # 打印网格
        print("\n" + "=" * (GRID_WIDTH * 3 + 4))
        for y in range(GRID_HEIGHT):
            line = "| "
            for x in range(GRID_WIDTH):
                cell = grid.grid[x][y]
                
                # 显示格子类型
                if cell['type'] == CELL_EMPTY:
                    char = '.'
                elif cell['type'] == CELL_OBSTACLE:
                    char = '#'
                elif cell['type'] == CELL_START:
                    char = 'S'
                elif cell['type'] == CELL_END:
                    char = 'E'
                elif cell['type'] == CELL_CONVEYOR:
                    char = DIR_SYMBOLS[cell['direction']]
                elif cell['type'] == CELL_BUCKET:
                    # 显示储物桶状态
                    items = cell.get('items', [])
                    count = len(items)
                    if count == 0:
                        char = 'B'  # 空桶
                    elif count >= BUCKET_CAPACITY:
                        char = 'F'  # 满桶
                    else:
                        char = str(count)  # 显示物品数量
                else:
                    char = '?'
                
                # 显示物品（仅传送带上的物品）
                if cell['item'] is not None and cell['type'] != CELL_BUCKET:
                    item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
                    item_char = item_info[2] if item_info else '?'
                    char = item_char
                
                line += f"{char} "
            line += "|"
            print(line)
        print("=" * (GRID_WIDTH * 3 + 4))
        
        # 显示日志
        if grid.log:
            print("\n最近日志:")
            for log in grid.log[-3:]:
                print(f"  {log}")
        
        # 获取输入
        cmd = input("\n输入命令 (h=帮助): ").strip().lower()
        
        if cmd == 'h':
            print_help()
        elif cmd == 'p':
            # 放置物品
            print("\n可选物品:")
            for i, item in enumerate(ITEM_TYPES):
                status = "允许" if item[3] else "禁止"
                print(f"  {i+1}. {item[1]} ({item[2]}) - {status}")
            
            try:
                idx = int(input("选择物品编号: ")) - 1
                x = int(input("X坐标: "))
                y = int(input("Y坐标: "))
                
                if 0 <= idx < len(ITEM_TYPES):
                    item_type = ITEM_TYPES[idx][0]
                    success, msg = grid.add_item(x, y, item_type)
                    print(msg)
                else:
                    print("无效的物品编号")
            except ValueError:
                print("输入无效")
        elif cmd == 'o':
            # 添加障碍物
            try:
                x = int(input("X坐标: "))
                y = int(input("Y坐标: "))
                success, msg = grid.add_obstacle(x, y)
                print(msg)
            except ValueError:
                print("输入无效")
        elif cmd == 's':
            # 单步运行
            grid.step()
            print("单步运行完成")
        elif cmd == 'a':
            # 自动运行
            print("自动运行中... (按 Ctrl+C 停止)")
            try:
                step = 0
                while True:
                    grid.step()
                    step += 1
                    if step % 5 == 0:
                        print(f"已运行 {step} 步")
                    import time
                    time.sleep(0.5)
            except KeyboardInterrupt:
                print(f"\n自动运行停止，共运行 {step} 步")
        elif cmd == 'c':
            # 清空
            grid.clear_grid()
            print("网格已清空")
        elif cmd == 'd':
            # 删除传送带
            try:
                x = int(input("点击传送带上任意点的X坐标: "))
                y = int(input("Y坐标: "))
                success, msg = grid.delete_conveyor(x, y)
                print(msg)
            except ValueError:
                print("输入无效")
        elif cmd == 't':
            # 运行测试
            print("\n运行测试用例...")
            runner = TestRunner(grid)
            runner.run_all_tests()
        elif cmd == 'q':
            # 退出
            print("退出程序")
            break
        else:
            print(f"未知命令: {cmd}")


def run_gui_version():
    """Tkinter GUI版本"""
    root = tk.Tk()
    root.title("虚空动力传送带测试程序")
    
    grid = ConveyorGrid(GRID_WIDTH, GRID_HEIGHT)
    
    # 创建画布
    canvas = tk.Canvas(root, width=GRID_WIDTH * CELL_SIZE, height=GRID_HEIGHT * CELL_SIZE, bg='white')
    canvas.pack(side=tk.LEFT, padx=10, pady=10)
    
    # 创建日志框
    log_frame = tk.Frame(root)
    log_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)
    
    log_text = tk.Text(log_frame, width=40, height=20, wrap=tk.WORD)
    log_text.pack(expand=True, fill=tk.BOTH)
    
    scrollbar = tk.Scrollbar(log_frame, command=log_text.yview)
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    log_text.config(yscrollcommand=scrollbar.set)
    
    # 创建状态标签
    status_label = tk.Label(root, text="状态: 就绪", bg='lightgray')
    status_label.pack(side=tk.BOTTOM, fill=tk.X)
    
    # 更新日志显示
    def update_log():
        log_text.delete(1.0, tk.END)
        for msg in grid.log[-20:]:
            log_text.insert(tk.END, msg + "\n")
        log_text.see(tk.END)
    
    # 绘制网格
    def draw_grid():
        canvas.delete("all")
        
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                px = x * CELL_SIZE
                py = y * CELL_SIZE
                
                cell = grid.grid[x][y]
                
                # 绘制格子背景
                if cell['type'] == CELL_EMPTY:
                    color = '#f0f0f0'
                elif cell['type'] == CELL_OBSTACLE:
                    color = '#444444'
                elif cell['type'] == CELL_START:
                    color = '#4CAF50'  # 绿色
                elif cell['type'] == CELL_END:
                    color = '#f44336'  # 红色
                elif cell['type'] == CELL_CONVEYOR:
                    color = '#9C27B0'  # 紫色
                elif cell['type'] == CELL_BUCKET:
                    # 储物桶根据状态显示不同颜色
                    items = cell.get('items', [])
                    count = len(items)
                    if count == 0:
                        color = '#2196F3'  # 蓝色 - 空桶
                    elif count >= BUCKET_CAPACITY:
                        color = '#FF5722'  # 橙色 - 满桶
                    else:
                        color = '#673AB7'  # 深紫色 - 有物品
                else:
                    color = '#ffffff'
                
                canvas.create_rectangle(px, py, px + CELL_SIZE, py + CELL_SIZE, fill=color, outline='#cccccc')
                
                # 绘制方向符号
                if cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END]:
                    canvas.create_text(px + CELL_SIZE//2, py + CELL_SIZE//2, 
                                      text=DIR_SYMBOLS[cell['direction']], 
                                      font=('Arial', 14, 'bold'), fill='white')
                
                # 绘制储物桶状态
                if cell['type'] == CELL_BUCKET:
                    items = cell.get('items', [])
                    count = len(items)
                    # 绘制桶图标
                    canvas.create_text(px + CELL_SIZE//2, py + CELL_SIZE//2 - 5, 
                                      text='桶', font=('Arial', 12), fill='white')
                    # 绘制物品数量
                    canvas.create_text(px + CELL_SIZE//2, py + CELL_SIZE - 10, 
                                      text=f"{count}/{BUCKET_CAPACITY}", font=('Arial', 8), fill='white')
                
                # 绘制物品（仅传送带上的物品）
                if cell['item'] is not None and cell['type'] != CELL_BUCKET:
                    item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
                    item_color = '#FFD700' if item_info[3] else '#FF4444'
                    canvas.create_text(px + CELL_SIZE//2, py + CELL_SIZE - 10, 
                                      text=item_info[2], font=('Arial', 10), fill=item_color)
        
        # 绘制预览选中的格子
        for x, y in grid.selected_cells:
            px = x * CELL_SIZE
            py = y * CELL_SIZE
            canvas.create_rectangle(px, py, px + CELL_SIZE, py + CELL_SIZE, 
                                    outline='#FF9800', width=3, dash=(5, 5))
    
    # 鼠标点击处理
    def on_click(event):
        x = event.x // CELL_SIZE
        y = event.y // CELL_SIZE
        
        if not grid.is_valid_position(x, y):
            return
        
        if event.state & 0x0001:  # Shift键
            # 添加障碍物
            success, msg = grid.add_obstacle(x, y)
            status_label.config(text=f"状态: {msg}")
            grid.add_log(msg)
            update_log()
            draw_grid()
        elif event.state & 0x0004:  # Ctrl键
            # 移除
            cell = grid.grid[x][y]
            if cell['type'] == CELL_OBSTACLE:
                success, msg = grid.remove_obstacle(x, y)
            elif cell['item'] is not None:
                success, msg = grid.remove_item(x, y)
            elif cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END]:
                success, msg = grid.delete_conveyor(x, y)
            else:
                msg = "该位置无内容可移除"
            
            status_label.config(text=f"状态: {msg}")
            grid.add_log(msg)
            update_log()
            draw_grid()
        else:
            # 放置传送带（两点式）
            if grid.placing_start is None:
                # 第一步：选择起点
                grid.placing_start = (x, y)
                grid.selected_cells = [(x, y)]
                status_label.config(text=f"状态: 选择起点 ({x},{y})，请点击终点")
                draw_grid()
            else:
                # 第二步：选择终点
                start_x, start_y = grid.placing_start
                
                # 检查是否点击同一格取消
                if start_x == x and start_y == y:
                    grid.placing_start = None
                    grid.selected_cells = []
                    status_label.config(text="状态: 取消选择")
                    draw_grid()
                    return
                
                # 尝试放置
                success, msg = grid.place_conveyor(start_x, start_y, x, y)
                status_label.config(text=f"状态: {msg}")
                grid.add_log(msg)
                update_log()
                
                # 重置状态
                grid.placing_start = None
                grid.selected_cells = []
                draw_grid()
    
    # 单步运行
    def step_run():
        grid.step()
        update_log()
        draw_grid()
    
    # 自动运行
    auto_running = False
    auto_timer = None
    
    def toggle_auto_run():
        nonlocal auto_running, auto_timer
        
        if auto_running:
            root.after_cancel(auto_timer)
            auto_running = False
            status_label.config(text="状态: 自动运行已停止")
        else:
            auto_running = True
            status_label.config(text="状态: 自动运行中")
            run_auto_step()
    
    def run_auto_step():
        nonlocal auto_timer
        
        if auto_running:
            grid.step()
            update_log()
            draw_grid()
            auto_timer = root.after(200, run_auto_step)
    
    # 添加物品对话框
    def add_item_dialog():
        # 第一步：选择物品
        item_window = tk.Toplevel(root)
        item_window.title("选择物品")
        
        selected_item = [None]
        
        def select_item(item_type):
            selected_item[0] = item_type
            item_window.destroy()
        
        for i, item in enumerate(ITEM_TYPES):
            status = "允许" if item[3] else "禁止"
            bg_color = '#e8f5e9' if item[3] else '#ffebee'
            btn = tk.Button(item_window, text=f"{item[1]} ({item[2]}) - {status}", 
                           command=lambda t=item[0]: select_item(t), bg=bg_color, width=30)
            btn.pack(pady=2)
        
        item_window.wait_window()
        
        if selected_item[0] is None:
            return
        
        # 第二步：选择投放位置类型
        target_type = [None]
        
        target_window = tk.Toplevel(root)
        target_window.title("选择投放位置")
        
        def select_target(t):
            target_type[0] = t
            target_window.destroy()
        
        tk.Button(target_window, text="放入传送带", command=lambda: select_target('conveyor'), width=20).pack(pady=5)
        tk.Button(target_window, text="放入储物桶", command=lambda: select_target('bucket'), width=20).pack(pady=5)
        
        target_window.wait_window()
        
        if target_type[0] is None:
            return
        
        # 第三步：选择具体位置
        def add_at_position(event):
            x = event.x // CELL_SIZE
            y = event.y // CELL_SIZE
            
            if target_type[0] == 'conveyor':
                success, msg = grid.add_item(x, y, selected_item[0])
            else:
                success, msg = grid.add_item_to_bucket(x, y, selected_item[0])
            
            status_label.config(text=f"状态: {msg}")
            grid.add_log(msg)
            update_log()
            draw_grid()
            canvas.unbind('<Button-1>')
        
        target_name = "传送带" if target_type[0] == 'conveyor' else "储物桶"
        status_label.config(text=f"状态: 请点击{target_name}放置物品")
        canvas.bind('<Button-1>', add_at_position)
    
    # 放置储物桶
    def place_bucket_dialog():
        def add_bucket_at_position(event):
            x = event.x // CELL_SIZE
            y = event.y // CELL_SIZE
            success, msg = grid.place_bucket(x, y)
            status_label.config(text=f"状态: {msg}")
            grid.add_log(msg)
            update_log()
            draw_grid()
            canvas.unbind('<Button-1>')
        
        status_label.config(text="状态: 请点击空格子放置储物桶")
        canvas.bind('<Button-1>', add_bucket_at_position)
    
    # 清空网格
    def clear_all():
        if messagebox.askyesno("确认", "确定清空所有内容？"):
            grid.clear_grid()
            status_label.config(text="状态: 网格已清空")
            update_log()
            draw_grid()
    
    # 运行测试
    def run_tests():
        runner = TestRunner(grid)
        result = runner.run_all_tests()
        msg = "测试全部通过" if result else "部分测试失败"
        status_label.config(text=f"状态: {msg}")
        grid.add_log(msg)
        update_log()
        draw_grid()
    
    # 创建工具栏
    toolbar = tk.Frame(root)
    toolbar.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=5)
    
    tk.Label(toolbar, text="操作: 点击两格放置传送带 | Shift+点击移除 | 放置物品/储物桶后点击目标").pack(side=tk.LEFT, padx=5)
    tk.Button(toolbar, text="放置物品", command=add_item_dialog).pack(side=tk.LEFT, padx=2)
    tk.Button(toolbar, text="放置储物桶", command=place_bucket_dialog).pack(side=tk.LEFT, padx=2)
    tk.Button(toolbar, text="单步运行", command=step_run).pack(side=tk.LEFT, padx=2)
    tk.Button(toolbar, text="自动运行", command=toggle_auto_run).pack(side=tk.LEFT, padx=2)
    tk.Button(toolbar, text="清空", command=clear_all).pack(side=tk.LEFT, padx=2)
    tk.Button(toolbar, text="运行测试", command=run_tests).pack(side=tk.LEFT, padx=2)
    
    # 绑定事件
    canvas.bind('<Button-1>', on_click)
    
    # 初始绘制
    draw_grid()
    
    root.mainloop()


if __name__ == '__main__':
    main()