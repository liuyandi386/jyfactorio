#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""电力网络计算模块

基于BFS + 电线面方向配置的电力路由：
  - 发电机 → 电线（OUTPUT/TRANSFER面接收）→ 电容库/电力塔
  - 电容库 → 电线（INPUT面抽取）→ 电力塔
  - 电力严格按电线面方向流动
"""

from collections import defaultdict
from .power_wire import PowerWire, FACE_NONE, FACE_INPUT, FACE_TRANSFER, FACE_OUTPUT
from .generator import PowerGenerator
from .capacitor import Capacitor
from .power_tower import PowerTower


class PowerNetwork:
    """电力网络 - BFS路由 + 面方向匹配"""

    # 4方向偏移
    DIRS = [(0, -1), (1, 0), (0, 1), (-1, 0)]   # UP, RIGHT, DOWN, LEFT
    OPPOSITE = {0: 2, 1: 3, 2: 0, 3: 1}

    def __init__(self, network_id: int = 0):
        self.network_id = network_id
        self.generators = []          # PowerGenerator
        self.capacitors = []          # Capacitor
        self.power_towers = []        # PowerTower
        self.wires = []               # PowerWire

        self.total_generation = 0.0
        self.total_consumption = 0.0
        self.total_storage = 0.0
        self.total_capacity = 0.0
        self.status = "normal"
        self.power_deficit = 0.0

    def add_generator(self, generator) -> None:
        if generator not in self.generators:
            self.generators.append(generator)

    def add_capacitor(self, capacitor) -> None:
        if capacitor not in self.capacitors:
            self.capacitors.append(capacitor)

    def add_power_tower(self, tower) -> None:
        if tower not in self.power_towers:
            self.power_towers.append(tower)

    def add_wire(self, wire) -> None:
        if wire not in self.wires:
            self.wires.append(wire)

    def remove_generator(self, generator) -> None:
        if generator in self.generators:
            self.generators.remove(generator)

    def remove_capacitor(self, capacitor) -> None:
        if capacitor in self.capacitors:
            self.capacitors.remove(capacitor)

    def remove_power_tower(self, tower) -> None:
        if tower in self.power_towers:
            self.power_towers.remove(tower)

    def remove_wire(self, wire) -> None:
        if wire in self.wires:
            self.wires.remove(wire)

    def update(self, delta_time: float) -> None:
        """每tick BFS路由电力"""
        self.total_generation = 0.0
        self.total_consumption = 0.0
        self.total_storage = 0.0
        self.total_capacity = 0.0

        # ====== 构建 tile→设备 映射 ======
        obj_at = {}   # (tx, ty) → device
        TILE_SIZE_REF = None

        for gen in self.generators:
            tx, ty = int(gen.x // 48), int(gen.y // 48)
            obj_at[(tx, ty)] = gen
            if gen.is_running and gen.has_fuel():
                self.total_generation += gen.get_eut_output()
        for cap in self.capacitors:
            tx, ty = int(cap.x // 48), int(cap.y // 48)
            obj_at[(tx, ty)] = cap
            self.total_storage += cap.energy
            self.total_capacity += cap.capacity
        for t in self.power_towers:
            tx, ty = int(t.x // 48), int(t.y // 48)
            obj_at[(tx, ty)] = t
            if hasattr(t, 'power_required'):
                self.total_consumption += t.power_required
        for w in self.wires:
            tx, ty = int(w.x // 48), int(w.y // 48)
            obj_at[(tx, ty)] = w

        # ====== BFS：从发电机出发，沿电线面方向传播电力 ======
        powered = set()     # {(tx, ty), ...} 已通电的tile

        for gen in self.generators:
            if gen.is_running and gen.has_fuel():
                gx, gy = int(gen.x // 48), int(gen.y // 48)
                powered.add((gx, gy))
                self._propagate_from_device(gx, gy, obj_at, powered)

        # ====== 电容库作为二级电源：有电的电容可向相邻INPUT面电线输出 ======
        for cap in self.capacitors:
            if cap.energy > 0:
                cx, cy = int(cap.x // 48), int(cap.y // 48)
                if (cx, cy) in powered:
                    self._propagate_from_device(cx, cy, obj_at, powered)

        # ====== 分配电力 ======
        powered_towers = []
        unpowered_towers = []

        for t in self.power_towers:
            tx, ty = int(t.x // 48), int(t.y // 48)
            if (tx, ty) in powered:
                powered_towers.append(t)
            else:
                unpowered_towers.append(t)

        # 计算可用电力
        available = self.total_generation
        for cap in self.capacitors:
            cx, cy = int(cap.x // 48), int(cap.y // 48)
            if (cx, cy) in powered and cap.energy > 0:
                available += cap.max_output_eut

        needed = sum(self._tower_consumption(t) for t in powered_towers)

        # 电容库充放电
        surplus = available - needed
        if surplus > 0:
            # 多余电力存入电容库
            self._charge_bfs(surplus, obj_at, powered)
        elif surplus < 0:
            # 不足时从电容库放电
            deficit = -surplus
            discharged = self._discharge_bfs(deficit, obj_at, powered)
            surplus += discharged

        # 供电状态
        if surplus >= 0:
            for t in powered_towers:
                self._set_powered(t, True)
            self.status = "normal" if not unpowered_towers else "normal"
            self.power_deficit = 0.0
        else:
            ratio = (available + max(0, surplus + needed)) / needed if needed > 0 else 0
            for t in powered_towers:
                self._set_powered(t, ratio >= 1.0)
            if unpowered_towers and not powered_towers:
                self.status = "offline"
                self.power_deficit = needed
            else:
                self.status = "low_power"
                self.power_deficit = -surplus

        for t in unpowered_towers:
            self._set_powered(t, False)

        if not self.generators and self.total_storage <= 0:
            self.status = "offline"
            for t in self.power_towers:
                self._set_powered(t, False)

    def _propagate_from_device(self, sx, sy, obj_at, powered):
        """从设备(sx,sy)出发，通过电线面方向BFS传播电力

        传播规则：
          - 发电机 → 任何方向都输出
          - 电线 → 电线：当前面OUTPUT/TRANSFER + 对面INPUT/TRANSFER = 连通
          - 电线 → 电容库/电力塔：电线面OUTPUT/TRANSFER = 连通
          - 电容库 → 电线：电线面INPUT = 从电容抽电
        """
        queue = [(sx, sy)]
        visited = set(powered)

        while queue:
            cx, cy = queue.pop(0)
            cur_obj = obj_at.get((cx, cy))

            for d, (dx, dy) in enumerate(self.DIRS):
                nx, ny = cx + dx, cy + dy
                npos = (nx, ny)
                if npos in visited:
                    continue
                nobj = obj_at.get(npos)
                if nobj is None:
                    continue

                can_reach = False

                if isinstance(cur_obj, PowerWire) and isinstance(nobj, PowerWire):
                    # 电线 → 电线：面方向匹配
                    my_face = cur_obj.get_face(d)
                    their_face = nobj.get_face(self.OPPOSITE[d])
                    if my_face in (FACE_OUTPUT, FACE_TRANSFER) and their_face in (FACE_INPUT, FACE_TRANSFER):
                        can_reach = True

                elif isinstance(cur_obj, PowerWire):
                    # 电线 → 设备（电容库/电力塔）：电线面为OUTPUT/TRANSFER即可
                    my_face = cur_obj.get_face(d)
                    if my_face in (FACE_OUTPUT, FACE_TRANSFER):
                        if isinstance(nobj, (Capacitor, PowerTower)):
                            can_reach = True

                elif isinstance(nobj, PowerWire):
                    # 设备（发电机/电容库）→ 电线：电线面为INPUT/TRANSFER即可
                    their_face = nobj.get_face(self.OPPOSITE[d])
                    if isinstance(cur_obj, PowerGenerator):
                        # 发电机输出：电线面INPUT/TRANSFER即可
                        if their_face in (FACE_INPUT, FACE_TRANSFER):
                            can_reach = True
                    elif isinstance(cur_obj, Capacitor):
                        # 电容库输出：电线面INPUT/TRANSFER（从电容抽电）
                        if their_face in (FACE_INPUT, FACE_TRANSFER):
                            can_reach = True

                if can_reach:
                    powered.add(npos)
                    visited.add(npos)
                    queue.append(npos)

    def _charge_bfs(self, surplus, obj_at, powered):
        """将多余电力充入已通电区域中的电容库"""
        if surplus <= 0:
            return
        recepient_caps = []
        for cap in self.capacitors:
            cx, cy = int(cap.x // 48), int(cap.y // 48)
            if (cx, cy) in powered and not cap.is_full():
                # 检查是否有电线OUTPUT/TRANSFER面朝向电容（可充电）
                can_charge = False
                for d, (dx, dy) in enumerate(self.DIRS):
                    nx, ny = cx + dx, cy + dy
                    nwire = obj_at.get((nx, ny))
                    if isinstance(nwire, PowerWire):
                        if nwire.get_face(self.OPPOSITE[d]) in (FACE_OUTPUT, FACE_TRANSFER):
                            can_charge = True
                            break
                # 也检查是否直接相邻发电机
                if not can_charge:
                    for d, (dx, dy) in enumerate(self.DIRS):
                        nx, ny = cx + dx, cy + dy
                        ngen = obj_at.get((nx, ny))
                        if ngen in self.generators:
                            can_charge = True
                            break
                if can_charge:
                    recepient_caps.append(cap)
        if not recepient_caps:
            return
        per_cap = surplus / len(recepient_caps)
        for cap in recepient_caps:
            cap.charge(per_cap)

    def _discharge_bfs(self, deficit, obj_at, powered):
        """从已通电的电容库放电补充不足"""
        if deficit <= 0:
            return 0.0
        source_caps = []
        for cap in self.capacitors:
            cx, cy = int(cap.x // 48), int(cap.y // 48)
            if (cx, cy) in powered and cap.energy > 0:
                # 检查是否有电线INPUT面朝向电容（可抽电）
                can_discharge = False
                for d, (dx, dy) in enumerate(self.DIRS):
                    nx, ny = cx + dx, cy + dy
                    nwire = obj_at.get((nx, ny))
                    if isinstance(nwire, PowerWire):
                        if nwire.get_face(self.OPPOSITE[d]) in (FACE_INPUT, FACE_TRANSFER):
                            can_discharge = True
                            break
                if can_discharge:
                    source_caps.append(cap)
        if not source_caps:
            return 0.0
        total = 0.0
        per_cap = deficit / len(source_caps)
        for cap in source_caps:
            total += cap.discharge(per_cap)
        return total

    def _tower_consumption(self, tower) -> float:
        if hasattr(tower, 'power_required'):
            return tower.power_required
        if hasattr(tower, 'power') and hasattr(tower, 'is_electric') and tower.is_electric:
            return tower.power.power_consumption
        return 0.0

    def _set_powered(self, tower, state: bool):
        if hasattr(tower, 'set_powered'):
            tower.set_powered(state)
        elif hasattr(tower, 'power'):
            tower.power.set_powered(state)
            if hasattr(tower, 'is_electric') and tower.is_electric:
                tower.power.is_powered = state

    def get_stats(self) -> dict:
        return {
            "network_id": self.network_id,
            "generation": self.total_generation,
            "consumption": self.total_consumption,
            "storage": self.total_storage,
            "capacity": self.total_capacity,
            "status": self.status,
            "deficit": self.power_deficit,
            "generators": len(self.generators),
            "capacitors": len(self.capacitors),
            "towers": len(self.power_towers),
            "wires": len(self.wires)
        }

    def is_connected_to(self, obj) -> bool:
        return (
            obj in self.generators or
            obj in self.capacitors or
            obj in self.power_towers or
            obj in self.wires
        )

    def clear(self) -> None:
        self.generators.clear()
        self.capacitors.clear()
        self.power_towers.clear()
        self.wires.clear()
