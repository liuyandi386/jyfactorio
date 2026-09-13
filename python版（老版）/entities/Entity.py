"""实体基类

所有游戏实体的父类，提供基本的位置和渲染功能
"""

import pygame


class Entity(pygame.sprite.Sprite):
    """实体基类"""
    
    def __init__(self, x: float, y: float, width: int, height: int):
        """初始化实体
        
        Args:
            x: 初始x位置
            y: 初始y位置
            width: 实体宽度
            height: 实体高度
        """
        super().__init__()
        
        # 位置
        self.x = x
        self.y = y
        
        # 尺寸
        self.width = width
        self.height = height
        
        # 精灵图像
        self.image = pygame.Surface((width, height))
        
        # 碰撞矩形
        self.rect = self.image.get_rect()
        self.rect.x = x
        self.rect.y = y
    
    def update(self, delta_time: float) -> None:
        """更新实体状态
        
        Args:
            delta_time: 帧时间间隔
        """
        # 更新矩形位置
        self.rect.x = self.x
        self.rect.y = self.y
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染实体
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        screen_rect = camera.apply(self.rect)
        screen.blit(pygame.transform.scale(self.image, (screen_rect.width, screen_rect.height)), screen_rect)
    
    def get_center(self) -> tuple:
        """获取实体中心点坐标
        
        Returns:
            中心点坐标(x, y)
        """
        return (
            self.x + self.width // 2,
            self.y + self.height // 2
        )
    
    def distance_to(self, other) -> float:
        """计算到另一个实体的距离
        
        Args:
            other: 另一个实体
        
        Returns:
            距离值
        """
        dx = self.get_center()[0] - other.get_center()[0]
        dy = self.get_center()[1] - other.get_center()[1]
        return (dx ** 2 + dy ** 2) ** 0.5