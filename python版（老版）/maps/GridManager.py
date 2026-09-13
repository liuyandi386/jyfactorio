#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""网格系统管理器

将测试版的ConveyorGrid集成到主游戏中，保持行为一致性。
提供网格渲染、放置、运行逻辑。
"""

import pygame
from settings import TILE_SIZE, COLORS

# 网格配置
GRID_WIDTH = 50
GRID_HEIGHT = 50
BUCKET_CAPACITY = 5

# 格子类型
CELL_EMPTY = 0
CELL_CONVEYOR = 1
CELL_OBSTACLE = 2
CELL_START = 3
CELL_END = 4
CELL_BUCKET = 5

# 方向定义
DIR_UP = 0
DIR_RIGHT = 1
DIR_DOWN = 2
DIR_LEFT = 3

# 方向符号
DIR_SYMBOLS = ['↑', '→', '↓', '←']

# 方向颜色
DIR_COLORS = {
    DIR_UP: (76, 175, 80),    # 绿色
    DIR_RIGHT: (233, 30, 99), # 粉色
    DIR_DOWN: (33, 150, 243), # 蓝色
    DIR_LEFT: (255, 152, 0)   # 橙色
}

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
    ('electricity', '电力', 'EL', False),
    ('energy_cell', '能量单元', 'EC', False),
]


class GridManager:
    """网格管理器"""
    
    def __init__(self, game_scene):
        """初始化网格管理器
        
        Args:
            game_scene: 游戏场景引用
        """
        self.game_scene = game_scene
        
        # 网格数据
        self.grid = [[{'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None} 
                      for _ in range(GRID_HEIGHT)] for _ in range(GRID_WIDTH)]
        
        # 放置状态
        self.placing_start = None
        self.selected_cells = []
        
        # 运行状态
        self.last_frame_start_occupied = {}
        self.log = []
        
    def is_valid_position(self, x, y):
        """检查位置是否有效"""
        return 0 <= x < GRID_WIDTH and 0 <= y < GRID_HEIGHT
    
    def reset(self):
        """重置网格状态"""
        self.grid = [[{'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None} 
                      for _ in range(GRID_HEIGHT)] for _ in range(GRID_WIDTH)]
        self.placing_start = None
        self.selected_cells = []
        self.last_frame_start_occupied = {}
        self.log = []
    
    def place_conveyor(self, start_x, start_y, end_x, end_y, direction=None):
        """放置传送带（两点式）"""
        if not self.is_valid_position(start_x, start_y) or not self.is_valid_position(end_x, end_y):
            return False, "位置无效"
        
        # 计算路径
        path = self._calculate_path(start_x, start_y, end_x, end_y)
        if not path:
            return False, "无法生成路径"
        
        # 检查路径上是否有障碍物
        for x, y in path:
            if self.grid[x][y]['type'] == CELL_OBSTACLE:
                return False, "路径上有障碍物"
        
        # 确定方向（如果未指定）
        if direction is None:
            if start_x == end_x:
                direction = DIR_DOWN if start_y < end_y else DIR_UP
            else:
                direction = DIR_RIGHT if start_x < end_x else DIR_LEFT
        
        # 放置传送带
        for i, (x, y) in enumerate(path):
            if i == 0:
                cell_type = CELL_START
            elif i == len(path) - 1:
                cell_type = CELL_END
            else:
                cell_type = CELL_CONVEYOR
            
            self.grid[x][y] = {
                'type': cell_type,
                'direction': direction,
                'item': None
            }
        
        msg = f"放置传送带: ({start_x},{start_y}) -> ({end_x},{end_y})"
        self.add_log(msg)
        return True, msg
    
    def _calculate_path(self, start_x, start_y, end_x, end_y):
        """计算两点之间的直线路径"""
        path = []
        
        if start_x == end_x:
            # 垂直路径
            min_y = min(start_y, end_y)
            max_y = max(start_y, end_y)
            for y in range(min_y, max_y + 1):
                path.append((start_x, y))
        elif start_y == end_y:
            # 水平路径
            min_x = min(start_x, end_x)
            max_x = max(start_x, end_x)
            for x in range(min_x, max_x + 1):
                path.append((x, start_y))
        else:
            # 斜线路径（简化为水平+垂直）
            # 先水平后垂直
            min_x = min(start_x, end_x)
            max_x = max(start_x, end_x)
            for x in range(min_x, max_x + 1):
                path.append((x, start_y))
            
            min_y = min(start_y, end_y)
            max_y = max(start_y, end_y)
            for y in range(min_y + 1, max_y + 1):
                path.append((end_x, y))
        
        return path
    
    def place_conveyor_single(self, x, y, direction=DIR_DOWN):
        """放置单个传送带
        
        Args:
            x: 瓦片X坐标
            y: 瓦片Y坐标
            direction: 方向（默认向下）
        
        Returns:
            (成功标志, 消息)
        """
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        if self.grid[x][y]['type'] != CELL_EMPTY:
            return False, "该位置已被占用"
        
        self.grid[x][y] = {
            'type': CELL_CONVEYOR,
            'direction': direction,
            'item': None
        }
        
        msg = f"放置传送带: ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def place_bucket(self, x, y):
        """放置储物桶"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        if self.grid[x][y]['type'] != CELL_EMPTY:
            return False, "该位置已被占用"
        
        self.grid[x][y] = {
            'type': CELL_BUCKET,
            'direction': DIR_RIGHT,
            'item': None,
            'items': []  # 储物桶内的物品列表（FIFO）
        }
        
        msg = f"储物桶放置: ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def add_item(self, x, y, item_name):
        """在指定位置添加物品"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        
        if cell['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END]:
            return False, "只能在传送带上放置物品"
        
        if cell['item'] is not None:
            return False, "该位置已有物品"
        
        # 检查是否是违禁品
        item_info = next((i for i in ITEM_TYPES if i[0] == item_name), None)
        if item_info and not item_info[3]:
            return False, f"禁止放置违禁品: {item_info[1]}"
        
        cell['item'] = item_name
        msg = f"放置物品: {item_name} at ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def add_item_to_bucket(self, x, y, item_name):
        """向储物桶添加物品"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] != CELL_BUCKET:
            return False, "该位置不是储物桶"
        
        items = cell.get('items', [])
        if len(items) >= BUCKET_CAPACITY:
            return False, "储物桶已满"
        
        # 检查是否是违禁品
        item_info = next((i for i in ITEM_TYPES if i[0] == item_name), None)
        if item_info and not item_info[3]:
            return False, f"禁止放置违禁品: {item_info[1]}"
        
        items.append(item_name)
        cell['items'] = items
        msg = f"物品放入储物桶: {item_name} ({len(items)}/{BUCKET_CAPACITY})"
        self.add_log(msg)
        return True, msg
    
    def remove_bucket(self, x, y):
        """移除储物桶"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        if self.grid[x][y]['type'] != CELL_BUCKET:
            return False, "该位置没有储物桶"
        
        self.grid[x][y] = {'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None}
        msg = f"移除储物桶: ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def delete_conveyor(self, x, y):
        """删除传送带"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END]:
            return False, "该位置没有传送带"
        
        self.grid[x][y] = {'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None}
        msg = f"删除传送带: ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def remove_item(self, x, y):
        """移除物品"""
        if not self.is_valid_position(x, y):
            return False, "位置无效"
        
        cell = self.grid[x][y]
        if cell['item'] is None:
            return False, "该位置没有物品"
        
        cell['item'] = None
        msg = f"移除物品: ({x},{y})"
        self.add_log(msg)
        return True, msg
    
    def step(self):
        """单步运行"""
        changed = False
        
        # 记录当前帧开始时传送带起点的占用状态
        self.last_frame_start_occupied = {}
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                cell = self.grid[x][y]
                if cell['type'] == CELL_START:
                    self.last_frame_start_occupied[(x, y)] = cell['item'] is not None
        
        # 创建物品移动计划
        moves = []
        
        # 从终点开始逆向处理，避免同时修改导致问题
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                cell = self.grid[x][y]
                
                # 只处理有物品的传送带格子
                if cell['type'] not in [CELL_START, CELL_CONVEYOR, CELL_END] or cell['item'] is None:
                    continue
                
                # 计算下一个位置
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
                
                # 检查下一个位置是否有效
                if not self.is_valid_position(next_x, next_y):
                    # 物品离开网格
                    self.add_log(f"物品离开网格: {cell['item']}")
                    cell['item'] = None
                    changed = True
                    continue
                
                next_cell = self.grid[next_x][next_y]
                
                # 检查下一个位置是否是传送带或终点
                if next_cell['type'] not in [CELL_CONVEYOR, CELL_END]:
                    # 前方不是传送带，物品留在原地（阻塞）
                    continue
                
                # 检查下一个位置是否有物品
                if next_cell['item'] is not None:
                    # 前方有物品，阻塞
                    continue
                
                # 记录移动
                moves.append((x, y, next_x, next_y))
        
        # 执行移动
        for from_x, from_y, to_x, to_y in moves:
            self.grid[to_x][to_y]['item'] = self.grid[from_x][from_y]['item']
            self.grid[from_x][from_y]['item'] = None
            changed = True
        
        # 第二步：传送带输出到储物桶
        self._update_conveyor_to_bucket()
        
        # 第三步：储物桶输出到传送带（使用上一帧状态判断）
        self._update_bucket_to_conveyor(self.last_frame_start_occupied)
        
        return changed
    
    def _update_conveyor_to_bucket(self):
        """更新传送带末端到储物桶的传输"""
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                cell = self.grid[x][y]
                
                # 只处理传送带终点且有物品
                if cell['type'] != CELL_END or cell['item'] is None:
                    continue
                
                # 计算输出方向
                dx, dy = 0, 0
                if cell['direction'] == DIR_UP:
                    dy = -1
                elif cell['direction'] == DIR_DOWN:
                    dy = 1
                elif cell['direction'] == DIR_LEFT:
                    dx = -1
                elif cell['direction'] == DIR_RIGHT:
                    dx = 1
                
                # 检查输出位置是否有储物桶
                output_x, output_y = x + dx, y + dy
                if not self.is_valid_position(output_x, output_y):
                    continue
                
                output_cell = self.grid[output_x][output_y]
                if output_cell['type'] == CELL_BUCKET:
                    items = output_cell.get('items', [])
                    if len(items) < BUCKET_CAPACITY:
                        # 将物品放入储物桶
                        items.append(cell['item'])
                        output_cell['items'] = items
                        item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
                        item_name = item_info[1] if item_info else "未知物品"
                        self.add_log(f"物品从传送带进入储物桶: {item_name}")
                        cell['item'] = None
    
    def _update_bucket_to_conveyor(self, last_frame_start_occupied):
        """更新储物桶到传送带的输出"""
        for x in range(GRID_WIDTH):
            for y in range(GRID_HEIGHT):
                cell = self.grid[x][y]
                
                if cell['type'] != CELL_BUCKET:
                    continue
                
                items = cell.get('items', [])
                if len(items) == 0:
                    continue
                
                # 检查四个方向是否有传送带起点
                for dx, dy in [(0, -1), (1, 0), (0, 1), (-1, 0)]:
                    neighbor_x, neighbor_y = x + dx, y + dy
                    
                    if not self.is_valid_position(neighbor_x, neighbor_y):
                        continue
                    
                    neighbor_cell = self.grid[neighbor_x][neighbor_y]
                    
                    # 检查是否是传送带起点
                    if neighbor_cell['type'] == CELL_START:
                        # 检查方向是否匹配（传送带朝向储物桶）
                        if ((dx == 0 and dy == -1 and neighbor_cell['direction'] == DIR_DOWN) or
                            (dx == 1 and dy == 0 and neighbor_cell['direction'] == DIR_LEFT) or
                            (dx == 0 and dy == 1 and neighbor_cell['direction'] == DIR_UP) or
                            (dx == -1 and dy == 0 and neighbor_cell['direction'] == DIR_RIGHT)):
                            
                            # 使用上一帧结束时的状态判断
                            was_occupied = last_frame_start_occupied.get((neighbor_x, neighbor_y), False)
                            
                            if not was_occupied and neighbor_cell['item'] is None:
                                # 输出物品到传送带
                                item = items.pop(0)
                                cell['items'] = items
                                neighbor_cell['item'] = item
                                item_info = next((i for i in ITEM_TYPES if i[0] == item), None)
                                item_name = item_info[1] if item_info else "未知物品"
                                self.add_log(f"储物桶输出物品: {item_name}")
                                break  # 每次只输出一个
    
    def add_log(self, message):
        """添加日志"""
        self.log.append(message)
        if len(self.log) > 50:
            self.log.pop(0)
    
    def clear_grid(self):
        """清空网格"""
        self.grid = [[{'type': CELL_EMPTY, 'direction': DIR_RIGHT, 'item': None} 
                      for _ in range(GRID_HEIGHT)] for _ in range(GRID_WIDTH)]
        self.placing_start = None
        self.selected_cells = []
        self.add_log("网格已清空")
    
    def draw(self, screen, camera):
        """渲染网格（只绘制有内容的格子）"""
        # 获取摄像机视野内的格子范围
        offset_x, offset_y = camera.get_offset()
        zoom = camera.zoom
        
        start_x = max(0, int(offset_x // TILE_SIZE) - 1)
        start_y = max(0, int(offset_y // TILE_SIZE) - 1)
        end_x = min(GRID_WIDTH, int((offset_x + screen.get_width()) // TILE_SIZE) + 1)
        end_y = min(GRID_HEIGHT, int((offset_y + screen.get_height()) // TILE_SIZE) + 1)
        
        for x in range(start_x, end_x):
            for y in range(start_y, end_y):
                cell = self.grid[x][y]
                
                # 只绘制非空的格子
                if cell['type'] == CELL_EMPTY:
                    continue

                # 传送带由 ConveyorBelt 实体绘制，这里跳过
                if cell['type'] in [CELL_START, CELL_CONVEYOR, CELL_END]:
                    continue

                # 计算屏幕坐标
                world_x = x * TILE_SIZE
                world_y = y * TILE_SIZE
                screen_x, screen_y = camera.world_to_screen(world_x, world_y)
                width = int(TILE_SIZE * zoom)
                height = int(TILE_SIZE * zoom)

                # 绘制格子背景
                if cell['type'] == CELL_OBSTACLE:
                    color = (85, 85, 85)  # 灰色
                elif cell['type'] == CELL_BUCKET:
                    items = cell.get('items', [])
                    count = len(items)
                    if count == 0:
                        color = (33, 150, 243)  # 蓝色 - 空桶
                    elif count >= BUCKET_CAPACITY:
                        color = (255, 87, 34)  # 橙色 - 满桶
                    else:
                        color = (103, 58, 183)  # 深紫色 - 有物品
                else:
                    color = (255, 255, 255)  # 白色
                
                pygame.draw.rect(screen, color, (screen_x, screen_y, width, height))
                pygame.draw.rect(screen, (100, 100, 100), (screen_x, screen_y, width, height), 1)

                # 绘制储物桶状态
                if cell['type'] == CELL_BUCKET:
                    items = cell.get('items', [])
                    count = len(items)
                    
                    font = pygame.font.SysFont('Arial', max(8, int(10 * zoom)))
                    bucket_text = font.render('桶', True, (255, 255, 255))
                    text_rect = bucket_text.get_rect(center=(screen_x + width//2, screen_y + height//2 - 4))
                    screen.blit(bucket_text, text_rect)
                    
                    small_font = pygame.font.SysFont('Arial', max(6, int(8 * zoom)))
                    count_text = small_font.render(f"{count}/{BUCKET_CAPACITY}", True, (255, 255, 255))
                    count_rect = count_text.get_rect(center=(screen_x + width//2, screen_y + height - 6))
                    screen.blit(count_text, count_rect)
                
                # 绘制物品
                if cell['item'] is not None:
                    item_info = next((i for i in ITEM_TYPES if i[0] == cell['item']), None)
                    item_color = (255, 215, 0) if item_info and item_info[3] else (255, 68, 68)
                    
                    small_font = pygame.font.SysFont('Arial', max(6, int(10 * zoom)))
                    item_text = small_font.render(item_info[2] if item_info else '?', True, item_color)
                    item_rect = item_text.get_rect(center=(screen_x + width//2, screen_y + height - 8))
                    screen.blit(item_text, item_rect)
