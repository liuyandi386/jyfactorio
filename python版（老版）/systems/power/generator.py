#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""燃煤发电机（电力系统实体）

消耗煤炭，产生 EU/t 电力。（测试程序中为无限燃料）
放置在电网上，通过电线向其他设备供电。
"""

import pygame
from entities.Entity import Entity
from components import PositionComponent, InventoryComponent
from settings import TILE_SIZE


class PowerGenerator(Entity):
    """燃煤发电机 - 消耗煤炭产生 EU/t 电力

    属性:
        fuel: 燃料物品名称
        fuel_time: 剩余燃料燃烧时间（秒）
        output_eut: 输出电力（EU/t）
        burn_efficiency: 燃烧效率
        is_running: 是否运行中
    """

    DEFAULT_OUTPUT_EUT = 32
    COAL_BURN_TIME = 5.0

    def __init__(self, x: float, y: float):
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        self.position = PositionComponent(x, y)
        self.inventory = InventoryComponent(max_slots=2, max_stack_size=64)

        self.fuel = "coal"
        self.fuel_time = 0.0
        self.output_eut = self.DEFAULT_OUTPUT_EUT
        self.is_running = False
        self.burn_efficiency = 1.0
        self.infinite_fuel = True  # 测试模式：无限燃料

        self.image = None
        self._init_image()

    def _init_image(self) -> None:
        size = TILE_SIZE
        surf = pygame.Surface((size, size), pygame.SRCALPHA)
        pygame.draw.rect(surf, (50, 50, 55), (2, 2, size - 4, size - 4), border_radius=3)
        pygame.draw.rect(surf, (70, 70, 75), (4, 4, size - 8, size - 8), border_radius=2)
        for i in range(3):
            y = 6 + i * 5
            pygame.draw.rect(surf, (30, 30, 35), (6, y, size - 12, 3))
            pygame.draw.rect(surf, (80, 80, 85), (6, y + 1, size - 12, 1))
        center = size // 2
        pygame.draw.rect(surf, (60, 60, 65), (center + 4, 1, 6, 10))
        pygame.draw.rect(surf, (80, 80, 85), (center + 5, 2, 4, 8))
        pygame.draw.polygon(surf, (255, 215, 0), [
            (center - 4, center + 4), (center - 1, center + 4),
            (center - 2, center + 9), (center + 1, center + 9),
            (center, center + 13), (center - 3, center + 9)
        ])
        self.image = surf

    def add_fuel(self, item_name: str, amount: int = 1) -> int:
        if item_name != self.fuel:
            return 0
        return self.inventory.add_item(item_name, amount)

    def has_fuel(self) -> bool:
        if self.infinite_fuel:
            return True
        return self.inventory.get_item_count(self.fuel) > 0 or self.fuel_time > 0

    def get_eut_output(self) -> float:
        if not self.is_running:
            return 0.0
        return self.output_eut * self.burn_efficiency

    def update(self, delta_time: float) -> None:
        super().update(delta_time)
        if self.infinite_fuel:
            self.is_running = True
            return
        if self.fuel_time > 0:
            self.fuel_time -= delta_time
            self.is_running = True
            if self.fuel_time <= 0:
                self.fuel_time = 0.0
                self.is_running = False
        if self.fuel_time <= 0 and self.inventory.get_item_count(self.fuel) > 0:
            consumed = self.inventory.remove_item(self.fuel, 1)
            if consumed > 0:
                self.fuel_time = self.COAL_BURN_TIME
                self.is_running = True

    def draw(self, screen: pygame.Surface, camera) -> None:
        super().draw(screen, camera)
        if self.is_running:
            screen_x, screen_y = camera.world_to_screen(self.x + self.width // 2, self.y + 6)
            size = max(2, int(3 * camera.zoom))
            import math
            alpha = int(128 + 127 * abs(math.sin(pygame.time.get_ticks() * 0.005)))
            pygame.draw.circle(screen, (0, alpha, 0), (int(screen_x), int(screen_y)), size)
