#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电力管理器

管理所有电力网络，每tick更新电力。
负责网络拓扑构建、设备注册和电力统计。
"""

import pygame
from collections import defaultdict

from .power_network import PowerNetwork
from .power_wire import PowerWire
from .generator import PowerGenerator
from .capacitor import Capacitor
from .power_tower import PowerTower


class PowerManager:
    """电力管理器 - 全局电力系统核心

    管理所有电力网络、设备注册/注销、每tick电力计算。
    """

    def __init__(self):
        """初始化电力管理器"""
        # 所有电力网络
        self.networks = []              # PowerNetwork 列表
        self._next_network_id = 0

        # 已注册的设备（用于快速查找）
        self.registered_generators = set()    # PowerGenerator
        self.registered_capacitors = set()    # Capacitor
        self.registered_towers = {}           # tower → PowerTower 映射
        self.registered_wires = set()         # PowerWire

        # 网络统计缓存（用于 UI 显示）
        self.stats = {
            "total_generation": 0.0,
            "total_consumption": 0.0,
            "total_storage": 0.0,
            "total_capacity": 0.0,
            "network_count": 0,
            "status": "offline"
        }

    def register_generator(self, generator: PowerGenerator) -> None:
        """注册发电机到电力系统

        Args:
            generator: PowerGenerator 实例
        """
        if generator in self.registered_generators:
            return
        self.registered_generators.add(generator)
        self._rebuild_networks()

    def register_capacitor(self, capacitor: Capacitor) -> None:
        """注册电容库到电力系统

        Args:
            capacitor: Capacitor 实例
        """
        if capacitor in self.registered_capacitors:
            return
        self.registered_capacitors.add(capacitor)
        self._rebuild_networks()

    def register_tower(self, tower) -> PowerTower:
        """注册电力塔到电力系统

        Args:
            tower: 内置 Tower 实例（tower_type == "electric"）

        Returns:
            PowerTower 适配器实例
        """
        if tower in self.registered_towers:
            return self.registered_towers[tower]

        power_tower = PowerTower(tower)
        self.registered_towers[tower] = power_tower
        self._rebuild_networks()
        return power_tower

    def register_wire(self, wire: PowerWire) -> None:
        """注册电线到电力系统

        Args:
            wire: PowerWire 实例
        """
        if wire in self.registered_wires:
            return
        self.registered_wires.add(wire)
        self._rebuild_networks()

    def unregister_generator(self, generator: PowerGenerator) -> None:
        """注销发电机"""
        if generator in self.registered_generators:
            self.registered_generators.remove(generator)
            self._rebuild_networks()

    def unregister_capacitor(self, capacitor: Capacitor) -> None:
        """注销电容库"""
        if capacitor in self.registered_capacitors:
            self.registered_capacitors.remove(capacitor)
            self._rebuild_networks()

    def unregister_tower(self, tower) -> None:
        """注销电力塔"""
        if tower in self.registered_towers:
            del self.registered_towers[tower]
            self._rebuild_networks()

    def unregister_wire(self, wire: PowerWire) -> None:
        """注销电线"""
        if wire in self.registered_wires:
            self.registered_wires.remove(wire)
            self._rebuild_networks()

    def _rebuild_networks(self) -> None:
        """重建所有电力网络拓扑

        电线通过 connected_objects 连接设备。
        BFS找连通分量，每个分量对应一个 PowerNetwork。
        """
        self.networks.clear()

        all_devices = set()
        all_devices.update(self.registered_generators)
        all_devices.update(self.registered_capacitors)
        all_devices.update(self.registered_towers.values())
        all_devices.update(self.registered_wires)

        if not all_devices:
            return

        adjacency = defaultdict(set)

        for device in all_devices:
            if isinstance(device, PowerWire):
                adjacency[device]
                for obj in device.connected_objects:
                    if obj in all_devices:
                        adjacency[device].add(obj)
                        adjacency[obj].add(device)

        visited = set()

        for device in all_devices:
            if device in visited:
                continue

            component = set()
            queue = [device]
            visited.add(device)

            while queue:
                current = queue.pop(0)
                component.add(current)
                for neighbor in adjacency.get(current, []):
                    if neighbor not in visited:
                        visited.add(neighbor)
                        queue.append(neighbor)

            network = PowerNetwork(self._next_network_id)
            self._next_network_id += 1

            for dev in component:
                if isinstance(dev, PowerGenerator):
                    network.add_generator(dev)
                elif isinstance(dev, Capacitor):
                    network.add_capacitor(dev)
                elif isinstance(dev, PowerTower):
                    network.add_power_tower(dev)
                elif isinstance(dev, PowerWire):
                    network.add_wire(dev)

            self.networks.append(network)

    def update(self, delta_time: float) -> None:
        """更新所有电力网络

        Args:
            delta_time: 时间间隔（秒）
        """
        # 更新所有网络
        for network in self.networks:
            network.update(delta_time)

        # 汇总统计
        self._update_stats()

    def _update_stats(self) -> None:
        """更新全局统计信息"""
        self.stats["total_generation"] = sum(
            net.total_generation for net in self.networks
        )
        self.stats["total_consumption"] = sum(
            net.total_consumption for net in self.networks
        )
        self.stats["total_storage"] = sum(
            net.total_storage for net in self.networks
        )
        self.stats["total_capacity"] = sum(
            net.total_capacity for net in self.networks
        )
        self.stats["network_count"] = len(self.networks)

        # 确定整体状态
        if not self.networks:
            self.stats["status"] = "offline"
        elif any(net.status == "low_power" for net in self.networks):
            self.stats["status"] = "low_power"
        elif all(net.status == "offline" for net in self.networks):
            self.stats["status"] = "offline"
        else:
            self.stats["status"] = "normal"

    def get_stats(self) -> dict:
        """获取电力系统统计信息

        Returns:
            统计字典
        """
        return self.stats.copy()

    def get_network_stats(self) -> list:
        """获取各网络详情

        Returns:
            各网络统计信息列表
        """
        return [net.get_stats() for net in self.networks]

    def draw_debug(self, screen: pygame.Surface, camera) -> None:
        """绘制调试信息（连线可视化）

        Args:
            screen: 渲染目标
            camera: 摄像机
        """
        # 电线已在各自的 draw 中渲染连接线
        pass

    def clear(self) -> None:
        """清空所有注册和网络"""
        self.networks.clear()
        self.registered_generators.clear()
        self.registered_capacitors.clear()
        self.registered_towers.clear()
        self.registered_wires.clear()
        self._next_network_id = 0
        self._update_stats()
