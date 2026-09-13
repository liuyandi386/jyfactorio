#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""组件模块

包含游戏中所有实体使用的组件类：
- InventoryComponent: 库存组件
- PositionComponent: 位置组件
- PowerComponent: 电力组件
- ProductionComponent: 生产组件
"""


class InventoryComponent:
    """库存组件"""
    
    def __init__(self, max_slots: int = 10, max_stack_size: int = 50):
        """初始化库存组件
        
        Args:
            max_slots: 最大槽位数
            max_stack_size: 每槽位最大堆叠数
        """
        self.max_slots = max_slots
        self.max_stack_size = max_stack_size
        self.items = {}
        self.total_items = 0
    
    def add_item(self, item_name: str, amount: int = 1) -> int:
        """添加物品"""
        if item_name not in self.items:
            self.items[item_name] = 0
        
        can_add = min(amount, self.max_stack_size * self.max_slots - self.total_items)
        self.items[item_name] += can_add
        self.total_items += can_add
        
        return can_add
    
    def remove_item(self, item_name: str, amount: int = 1) -> int:
        """移除物品"""
        if item_name not in self.items:
            return 0
        
        can_remove = min(amount, self.items[item_name])
        self.items[item_name] -= can_remove
        self.total_items -= can_remove
        
        if self.items[item_name] <= 0:
            del self.items[item_name]
        
        return can_remove
    
    def get_item_count(self, item_name: str) -> int:
        """获取指定物品数量"""
        return self.items.get(item_name, 0)
    
    def has_item(self, item_name: str, amount: int = 1) -> bool:
        """检查是否有足够的物品"""
        return self.items.get(item_name, 0) >= amount
    
    def is_full(self) -> bool:
        """检查库存是否已满"""
        return self.total_items >= self.max_stack_size * self.max_slots
    
    def get_space(self) -> int:
        """获取剩余空间"""
        return self.max_stack_size * self.max_slots - self.total_items
    
    def clear(self) -> None:
        """清空库存"""
        self.items.clear()
        self.total_items = 0
    
    def get_all_items(self) -> dict:
        """获取所有物品"""
        return self.items.copy()


class PositionComponent:
    """位置组件"""
    
    def __init__(self, x: float = 0.0, y: float = 0.0):
        """初始化位置组件"""
        self.x = x
        self.y = y
    
    def get_center(self, width: int, height: int) -> tuple:
        """获取中心点坐标"""
        return (
            self.x + width // 2,
            self.y + height // 2
        )
    
    def distance_to(self, other, width: int = 0, height: int = 0) -> float:
        """计算到另一个位置的距离"""
        dx = self.get_center(width, height)[0] - other.get_center(width, height)[0]
        dy = self.get_center(width, height)[1] - other.get_center(width, height)[1]
        return (dx ** 2 + dy ** 2) ** 0.5


class PowerComponent:
    """电力组件"""
    
    def __init__(self, max_power: float = 0.0, power_generation: float = 0.0, power_consumption: float = 0.0):
        """初始化电力组件"""
        self.current_power = 0.0
        self.max_power = max_power
        self.power_generation = power_generation
        self.power_consumption = power_consumption
        self.is_powered = False
        self.power_grid_id = None
        self.connection_radius = 0.0
        self.connected_buildings = []
    
    def generate_power(self, delta_time: float) -> float:
        """产生电力"""
        if self.power_generation <= 0:
            return 0.0
        
        power_generated = self.power_generation * delta_time
        self.current_power = min(self.current_power + power_generated, self.max_power)
        
        return power_generated
    
    def consume_power(self, delta_time: float) -> bool:
        """消耗电力"""
        if self.power_consumption <= 0:
            self.is_powered = True
            return True
        
        needed = self.power_consumption * delta_time
        
        if self.current_power >= needed:
            self.current_power -= needed
            self.is_powered = True
            return True
        else:
            self.is_powered = False
            return False
    
    def set_powered(self, powered: bool) -> None:
        """设置供电状态"""
        self.is_powered = powered
    
    def get_power_percentage(self) -> float:
        """获取电力百分比"""
        if self.max_power <= 0:
            return 1.0
        return self.current_power / self.max_power
    
    def add_power(self, amount: float) -> None:
        """添加电力"""
        self.current_power = min(self.current_power + amount, self.max_power)
    
    def can_operate(self) -> bool:
        """检查是否可以运行"""
        if self.power_consumption <= 0:
            return True
        return self.is_powered


class ProductionComponent:
    """生产组件"""
    
    def __init__(self, production_time: float = 1.0, input_items: dict = None, output_items: dict = None):
        """初始化生产组件"""
        self.production_time = production_time
        self.progress = 0.0
        self.is_producing = False
        self.input_items = input_items or {}
        self.output_items = output_items or {}
        self.efficiency = 1.0
        self.auto_output = True
        self.last_production_time = 0.0
    
    def update(self, delta_time: float, has_power: bool = True, inventory=None) -> dict:
        """更新生产状态"""
        if not has_power:
            self.is_producing = False
            return {}
        
        if inventory and self.input_items:
            for item_name, amount in self.input_items.items():
                if inventory.get_item_count(item_name) < amount:
                    self.is_producing = False
                    return {}
        
        self.is_producing = True
        self.progress += (delta_time / self.production_time) * self.efficiency
        
        if self.progress >= 1.0:
            self.progress = 0.0
            
            if inventory and self.input_items:
                for item_name, amount in self.input_items.items():
                    inventory.remove_item(item_name, amount)
            
            return self.output_items.copy()
        
        return {}
    
    def get_progress_percentage(self) -> float:
        """获取生产进度百分比"""
        return min(self.progress, 1.0)
    
    def can_produce(self, inventory=None) -> bool:
        """检查是否可以生产"""
        if not self.input_items:
            return True
        
        if inventory is None:
            return False
        
        for item_name, amount in self.input_items.items():
            if inventory.get_item_count(item_name) < amount:
                return False
        
        return True
    
    def reset(self) -> None:
        """重置生产进度"""
        self.progress = 0.0
        self.is_producing = False
    
    def set_efficiency(self, efficiency: float) -> None:
        """设置生产效率"""
        self.efficiency = max(0.0, efficiency)