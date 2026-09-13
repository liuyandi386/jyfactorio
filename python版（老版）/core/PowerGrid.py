#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电网管理器

管理整个游戏的电力网络
"""

from entities.Generator import Generator
from entities.PowerPole import PowerPole


class PowerGrid:
    """电网管理器"""
    
    def __init__(self):
        """初始化电网管理器"""
        # 发电机列表
        self.generators = []
        
        # 电线杆列表
        self.poles = []
        
        # 用电建筑列表
        self.consumers = []
        
        # 总发电量
        self.total_generation = 0.0
        
        # 总耗电量
        self.total_consumption = 0.0
        
        # 电力盈余/赤字
        self.power_balance = 0.0
    
    def add_generator(self, generator: Generator) -> None:
        """添加发电机
        
        Args:
            generator: 发电机实体
        """
        if generator not in self.generators:
            self.generators.append(generator)
    
    def add_pole(self, pole: PowerPole) -> None:
        """添加电线杆
        
        Args:
            pole: 电线杆实体
        """
        if pole not in self.poles:
            self.poles.append(pole)
            
            # 自动连接到范围内的其他电线杆
            for other_pole in self.poles:
                if other_pole != pole:
                    pole.connect_to_pole(other_pole)
                    other_pole.connect_to_pole(pole)
    
    def add_consumer(self, consumer) -> None:
        """添加用电建筑
        
        Args:
            consumer: 用电建筑实体
        """
        if consumer not in self.consumers:
            self.consumers.append(consumer)
            
            # 尝试连接到最近的电线杆
            self._connect_consumer(consumer)
    
    def _connect_consumer(self, consumer) -> bool:
        """连接建筑到电网
        
        Args:
            consumer: 用电建筑
        
        Returns:
            是否成功连接
        """
        closest_pole = None
        closest_distance = float('inf')
        
        for pole in self.poles:
            if hasattr(consumer, 'position'):
                distance = pole.position.distance_to(
                    consumer.position, 
                    pole.width, 
                    pole.height
                )
            else:
                dx = pole.x - consumer.x
                dy = pole.y - consumer.y
                distance = (dx ** 2 + dy ** 2) ** 0.5
            
            if distance < closest_distance:
                closest_distance = distance
                closest_pole = pole
        
        if closest_pole and closest_distance <= closest_pole.connection_radius:
            return closest_pole.connect_building(consumer)
        
        return False
    
    def remove_generator(self, generator: Generator) -> None:
        """移除发电机
        
        Args:
            generator: 发电机实体
        """
        if generator in self.generators:
            self.generators.remove(generator)
    
    def remove_pole(self, pole: PowerPole) -> None:
        """移除电线杆
        
        Args:
            pole: 电线杆实体
        """
        if pole in self.poles:
            # 断开所有连接
            for other_pole in pole.connected_poles:
                if pole in other_pole.connected_poles:
                    other_pole.connected_poles.remove(pole)
            
            for building in pole.connected_buildings:
                building.power.power_grid_id = None
                building.power.is_powered = False
            
            self.poles.remove(pole)
    
    def remove_consumer(self, consumer) -> None:
        """移除用电建筑
        
        Args:
            consumer: 用电建筑实体
        """
        if consumer in self.consumers:
            self.consumers.remove(consumer)
    
    def update(self, delta_time: float) -> None:
        """更新电网状态
        
        Args:
            delta_time: 时间间隔
        """
        # 计算总发电量
        self.total_generation = 0.0
        for generator in self.generators:
            if generator.has_fuel():
                self.total_generation += generator.power.power_generation
        
        # 计算总耗电量
        self.total_consumption = 0.0
        for consumer in self.consumers:
            if hasattr(consumer, 'power'):
                self.total_consumption += consumer.power.power_consumption
        
        # 电力平衡
        self.power_balance = self.total_generation - self.total_consumption
        
        # 分配电力给用电建筑
        if self.total_generation >= self.total_consumption:
            # 电力充足，所有建筑供电
            for consumer in self.consumers:
                if hasattr(consumer, 'power'):
                    consumer.power.set_powered(True)
        else:
            # 电力不足，按比例供电（简化处理：优先供电给部分建筑）
            if self.total_consumption > 0:
                supply_ratio = self.total_generation / self.total_consumption
            else:
                supply_ratio = 0.0
            
            for consumer in self.consumers:
                if hasattr(consumer, 'power'):
                    consumer.power.set_powered(supply_ratio >= 1.0 or supply_ratio > 0.5)
    
    def get_stats(self) -> dict:
        """获取电网统计信息
        
        Returns:
            统计信息字典
        """
        return {
            "generation": self.total_generation,
            "consumption": self.total_consumption,
            "balance": self.power_balance,
            "generators": len(self.generators),
            "poles": len(self.poles),
            "consumers": len(self.consumers)
        }