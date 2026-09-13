"""敌人类

负责敌人的逻辑，包括自动寻路和血量管理
"""

import pygame
from entities.Entity import Entity
from settings import ENEMY_HEALTH, ENEMY_SPEED, ENEMY_REWARD, TILE_SIZE, COLORS, PATH_POINTS
from sprites import get_sprite


class Enemy(Entity):
    """敌人类"""
    
    def __init__(self, enemy_type: str = "basic"):
        """初始化敌人
        
        Args:
            enemy_type: 敌人类型（basic/fast/tank）
        """
        # 从路径起点开始
        start_x, start_y = PATH_POINTS[0]
        super().__init__(start_x * TILE_SIZE, start_y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
        
        # 敌人类型
        self.enemy_type = enemy_type
        
        # 最大血量
        self.max_health = ENEMY_HEALTH.get(enemy_type, 100)
        
        # 当前血量
        self.health = self.max_health
        
        # 移动速度
        self.speed = ENEMY_SPEED.get(enemy_type, 1.5)
        
        # 击杀奖励
        self.reward = ENEMY_REWARD.get(enemy_type, 20)
        
        # 路径索引
        self.path_index = 0
        
        # 目标位置
        self.target_x, self.target_y = self._get_next_target()
        
        # 是否到达终点
        self.reached_end = False
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化敌人外观"""
        sprite_name = f'enemy_{self.enemy_type}'
        self.image = get_sprite(sprite_name)
    
    def _get_next_target(self) -> tuple:
        """获取下一个路径点
        
        Returns:
            下一个目标位置(x, y)（像素坐标）
        """
        if self.path_index < len(PATH_POINTS) - 1:
            self.path_index += 1
            target_tile_x, target_tile_y = PATH_POINTS[self.path_index]
            return (target_tile_x * TILE_SIZE + TILE_SIZE // 2,
                    target_tile_y * TILE_SIZE + TILE_SIZE // 2)
        else:
            return self.get_center()
    
    def take_damage(self, amount: int) -> bool:
        """受到伤害
        
        Args:
            amount: 伤害量
        
        Returns:
            是否死亡
        """
        self.health -= amount
        if self.health <= 0:
            self.health = 0
            return True
        return False
    
    def update(self, delta_time: float) -> None:
        """更新敌人状态
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        
        # 计算到目标的方向
        dx = self.target_x - self.get_center()[0]
        dy = self.target_y - self.get_center()[1]
        distance = (dx ** 2 + dy ** 2) ** 0.5
        
        if distance < 5:
            # 到达当前目标点
            if self.path_index >= len(PATH_POINTS) - 1:
                # 到达终点
                self.reached_end = True
            else:
                # 获取下一个目标
                self.target_x, self.target_y = self._get_next_target()
        else:
            # 向目标移动
            move_speed = self.speed * TILE_SIZE * delta_time
            self.x += (dx / distance) * move_speed
            self.y += (dy / distance) * move_speed
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染敌人
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        super().draw(screen, camera)
        
        # 渲染血条
        if self.health < self.max_health:
            screen_x, screen_y = camera.world_to_screen(self.x, self.y - 8)
            bar_width = self.width * camera.zoom
            bar_height = 4 * camera.zoom
            
            # 背景
            pygame.draw.rect(screen, (0, 0, 0), (screen_x, screen_y, bar_width, bar_height))
            
            # 血量
            health_ratio = self.health / self.max_health
            health_color = (0, 255, 0) if health_ratio > 0.5 else (255, 255, 0) if health_ratio > 0.25 else (255, 0, 0)
            pygame.draw.rect(screen, health_color, (screen_x, screen_y, bar_width * health_ratio, bar_height))