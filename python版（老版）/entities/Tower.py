#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""塔类

Factorio风格的防御塔
- 基础塔(basic)、快速塔(rapid)、狙击塔(sniper)：消耗弹药攻击
- 电力塔(electric)：消耗电力攻击
"""

import pygame
import math
from entities.Entity import Entity
from entities.Bullet import Bullet
from components import PositionComponent, InventoryComponent, PowerComponent, ProductionComponent
from settings import TOWER_RANGE, TOWER_DAMAGE, TOWER_FIRE_RATE, TOWER_BULLET_SPEED, TILE_SIZE, COLORS, PATH_POINTS
from sprites import get_sprite


class Tower(Entity):
    """防御塔类"""
    
    # 攻击模式枚举
    TARGET_MODE_HIGHEST_HEALTH = 1  # 优先攻击血量最高的敌人
    TARGET_MODE_NEAREST_TO_END = 2  # 优先攻击离终点最近的敌人
    
    def __init__(self, x: float, y: float, tower_type: str = "basic"):
        """初始化塔
        
        Args:
            x: 初始x位置
            y: 初始y位置
            tower_type: 塔类型（basic/rapid/sniper/electric）
        """
        super().__init__(x, y, TILE_SIZE, TILE_SIZE)
        
        # 位置组件
        self.position = PositionComponent(x, y)
        
        # 塔类型
        self.tower_type = tower_type
        
        # 判断是否为电力塔
        self.is_electric = tower_type == "electric"
        
        # 攻击范围（圆形半径）
        self.range = TOWER_RANGE.get(tower_type, 150)
        
        # 攻击力
        self.damage = TOWER_DAMAGE.get(tower_type, 20)
        
        # 子弹速度
        self.bullet_speed = TOWER_BULLET_SPEED.get(tower_type, 10.0)
        
        # 攻击速度（每秒发射次数）
        self.fire_rate = TOWER_FIRE_RATE.get(tower_type, 1.0)
        
        # 攻击冷却时间
        self.cooldown = 0
        
        # 目标敌人
        self.target = None
        
        # 所有敌人引用
        self.enemies = None
        
        # 子弹组引用
        self.bullets = None
        
        # 攻击模式（默认使用离终点最近模式）
        self.target_mode = Tower.TARGET_MODE_NEAREST_TO_END
        
        # 炮管方向（0-上, 1-右上, 2-右, 3-右下, 4-下, 5-左下, 6-左, 7-左上）
        self.direction = 0
        
        # 电力组件
        if self.is_electric:
            # 电力塔需要大量电力
            self.power = PowerComponent(
                max_power=100,
                power_generation=0,
                power_consumption=20  # 每秒消耗20电力
            )
            # 电力塔必须连接电网才能工作
            self.power.is_powered = False
        else:
            # 弹药塔不需要外部电力
            self.power = PowerComponent(
                max_power=50,
                power_generation=0,
                power_consumption=0
            )
            self.power.is_powered = True
        
        # 是否连接到电网
        self.connected_to_grid = False
        
        # 库存组件（弹药塔存储弹药）
        if not self.is_electric:
            self.inventory = InventoryComponent(max_slots=3, max_stack_size=20)
        else:
            self.inventory = None
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化塔的外观"""
        # 根据类型使用对应贴图
        sprite_map = {
            "basic": "basic_tower",
            "rapid": "rapid_tower",
            "sniper": "sniper_tower",
            "electric": "electric_tower"
        }
        sprite_name = sprite_map.get(self.tower_type, "basic_tower")
        self.image = get_sprite(sprite_name, direction=self.direction)
    
    def rotate_direction(self) -> None:
        """旋转炮管方向（8个方向循环）"""
        self.direction = (self.direction + 1) % 8
        self._init_image()
    
    def set_enemies(self, enemies) -> None:
        """设置敌人组引用"""
        self.enemies = enemies
    
    def set_bullets(self, bullets) -> None:
        """设置子弹组引用"""
        self.bullets = bullets
    
    def add_ammo(self, amount: int = 1) -> int:
        """添加弹药（仅对非电力塔有效）"""
        if self.inventory:
            return self.inventory.add_item("ammo", amount)
        return 0
    
    def has_ammo(self) -> bool:
        """检查是否有弹药（仅对非电力塔有效）"""
        if self.inventory:
            return self.inventory.get_item_count("ammo") > 0
        return False
    
    def has_power(self) -> bool:
        """检查是否有足够电力（仅对电力塔有效）"""
        if self.is_electric:
            return self.power.is_powered and self.power.current_power >= 10
        return True
    
    def find_target(self) -> None:
        """寻找范围内的敌人（根据攻击模式选择目标）"""
        if self.enemies is None:
            self.target = None
            return
        
        # 获取塔的中心位置
        tower_center = self.get_center()
        
        # 获取范围内的所有敌人（圆形检测）
        enemies_in_range = []
        for enemy in self.enemies:
            enemy_center = enemy.get_center()
            distance = math.sqrt(
                (enemy_center[0] - tower_center[0]) ** 2 +
                (enemy_center[1] - tower_center[1]) ** 2
            )
            
            if distance <= self.range:
                enemies_in_range.append((enemy, distance))
        
        if not enemies_in_range:
            self.target = None
            return
        
        # 根据攻击模式选择目标
        if self.target_mode == Tower.TARGET_MODE_HIGHEST_HEALTH:
            best_enemy = None
            highest_health = -1
            for enemy, dist in enemies_in_range:
                if enemy.health > highest_health:
                    highest_health = enemy.health
                    best_enemy = enemy
            self.target = best_enemy
            
        elif self.target_mode == Tower.TARGET_MODE_NEAREST_TO_END:
            best_enemy = None
            nearest_to_end = float('inf')
            
            end_point = PATH_POINTS[-1]
            end_x = end_point[0] * TILE_SIZE + TILE_SIZE // 2
            end_y = end_point[1] * TILE_SIZE + TILE_SIZE // 2
            
            for enemy, dist in enemies_in_range:
                enemy_center = enemy.get_center()
                distance_to_end = math.sqrt(
                    (enemy_center[0] - end_x) ** 2 +
                    (enemy_center[1] - end_y) ** 2
                )
                score = distance_to_end - enemy.path_index * TILE_SIZE
                
                if score < nearest_to_end:
                    nearest_to_end = score
                    best_enemy = enemy
            
            self.target = best_enemy
        
        else:
            self.target = min(enemies_in_range, key=lambda x: x[1])[0]
    
    def set_target_mode(self, mode: int) -> None:
        """设置攻击模式"""
        if mode in [Tower.TARGET_MODE_HIGHEST_HEALTH, Tower.TARGET_MODE_NEAREST_TO_END]:
            self.target_mode = mode
    
    def attack(self) -> None:
        """攻击目标敌人"""
        if self.target is None or self.bullets is None:
            return
        
        # 检查攻击条件
        if self.is_electric:
            # 电力塔：检查电力
            if not self.has_power():
                return
            # 消耗电力
            self.power.consume_power(10)
        else:
            # 弹药塔：检查弹药
            if not self.has_ammo():
                return
            # 消耗弹药
            self.inventory.remove_item("ammo", 1)
        
        # 创建子弹
        bullet = Bullet(
            self.get_center()[0],
            self.get_center()[1],
            self.target,
            self.damage,
            self.bullet_speed,
            self.tower_type
        )
        self.bullets.add(bullet)
        
        # 重置冷却
        self.cooldown = 1.0 / self.fire_rate
    
    def update(self, delta_time: float) -> None:
        """更新塔状态"""
        super().update(delta_time)
        
        # 更新冷却
        if self.cooldown > 0:
            self.cooldown -= delta_time
        
        # 寻找目标
        self.find_target()
        
        # 如果有目标且冷却完成，进行攻击
        if self.target is not None and self.cooldown <= 0:
            self.attack()
    
    def draw(self, screen: pygame.Surface, camera, show_range: bool = False) -> None:
        """渲染塔"""
        super().draw(screen, camera)
        
        # 绘制攻击范围（圆形）
        if show_range:
            tower_center = self.get_center()
            screen_x, screen_y = camera.world_to_screen(tower_center[0], tower_center[1])
            range_radius = int(self.range * camera.zoom)
            
            range_surface = pygame.Surface((range_radius * 2, range_radius * 2), pygame.SRCALPHA)
            pygame.draw.circle(range_surface, (100, 100, 255, 50), (range_radius, range_radius), range_radius)
            pygame.draw.circle(range_surface, (100, 100, 255, 150), (range_radius, range_radius), range_radius, 2)
            screen.blit(range_surface, (screen_x - range_radius, screen_y - range_radius))
        
        # 绘制电力状态指示
        if not self.power.is_powered:
            screen_x, screen_y = camera.world_to_screen(self.x, self.y - 8)
            size = int(4 * camera.zoom)
            pygame.draw.circle(screen, (255, 0, 0), (int(screen_x + size), int(screen_y)), size)
        
        # 绘制弹药指示（仅非电力塔）
        if not self.is_electric and self.has_ammo():
            screen_x, screen_y = camera.world_to_screen(self.x + self.width - 10, self.y + 2)
            size = int(3 * camera.zoom)
            ammo_count = self.inventory.get_item_count("ammo")
            for i in range(min(3, ammo_count // 5)):
                pygame.draw.rect(screen, (255, 255, 0), (
                    int(screen_x), int(screen_y + i * size * 1.5), size, size
                ))
        
        # 电力塔绘制电力指示
        if self.is_electric:
            screen_x, screen_y = camera.world_to_screen(self.x + self.width - 10, self.y + 2)
            size = int(3 * camera.zoom)
            # 显示电力条
            power_ratio = self.power.current_power / self.power.max_power
            pygame.draw.rect(screen, (30, 30, 30), (int(screen_x), int(screen_y), size * 3, size))
            pygame.draw.rect(screen, (0, 150, 255), (int(screen_x), int(screen_y), size * 3 * power_ratio, size))