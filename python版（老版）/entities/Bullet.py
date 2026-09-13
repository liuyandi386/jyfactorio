"""子弹类

负责子弹的飞行和碰撞检测
"""

import pygame
from entities.Entity import Entity
from settings import COLORS, TOWER_BULLET_SPEED


class Bullet(Entity):
    """子弹类"""
    
    def __init__(self, x: float, y: float, target, damage: int, speed: float = 10.0, tower_type: str = "basic"):
        """初始化子弹
        
        Args:
            x: 初始x位置
            y: 初始y位置
            target: 目标敌人
            damage: 伤害值
            speed: 子弹速度
            tower_type: 发射子弹的塔类型
        """
        super().__init__(x, y, 8, 8)
        
        # 初始位置（用于平滑轨迹计算）
        self.start_x = x
        self.start_y = y
        
        # 目标敌人
        self.target = target
        
        # 伤害值
        self.damage = damage
        
        # 子弹速度（根据塔类型不同）
        self.speed = TOWER_BULLET_SPEED.get(tower_type, 10.0)
        
        # 塔类型（影响外观）
        self.tower_type = tower_type
        
        # 是否击中目标
        self.hit = False
        
        # 飞行时间（用于平滑插值）
        self.flight_time = 0.0
        
        # 预计到达时间
        self.arrival_time = 0.0
        
        # 初始化外观
        self._init_image()
    
    def _init_image(self) -> None:
        """初始化子弹外观"""
        # 创建子弹图像（根据塔类型设置颜色）
        self.image.fill((0, 0, 0, 0))
        self.image.set_colorkey((0, 0, 0))
        
        center = self.width // 2
        
        # 根据塔类型设置颜色
        colors = {
            "basic": (255, 255, 0),      # 黄色
            "rapid": (0, 255, 255),      # 青色
            "sniper": (255, 0, 255)      # 紫色
        }
        color = colors.get(self.tower_type, COLORS["bullet_color"])
        
        # 绘制圆形子弹
        pygame.draw.circle(self.image, color, (center, center), center)
        
        # 添加光晕效果
        pygame.draw.circle(self.image, (255, 255, 255), (center, center), center // 2)
    
    def update(self, delta_time: float) -> None:
        """更新子弹状态（带平滑轨迹）
        
        Args:
            delta_time: 帧时间间隔
        """
        super().update(delta_time)
        
        if self.hit or self.target is None:
            self.kill()
            return
        
        # 检查目标是否仍然存在
        if not self.target.alive():
            self.kill()
            return
        
        # 计算到目标的方向和距离
        target_center = self.target.get_center()
        dx = target_center[0] - self.get_center()[0]
        dy = target_center[1] - self.get_center()[1]
        distance = (dx ** 2 + dy ** 2) ** 0.5
        
        if distance < 12:
            # 击中目标
            self.target.take_damage(self.damage)
            self.hit = True
            self.kill()
        else:
            # 使用平滑移动（带缓动效果）
            self.flight_time += delta_time
            
            # 计算移动速度（带加速效果）
            move_speed = self.speed * 32 * delta_time
            
            # 使用缓动函数（ease-out）使轨迹更平滑
            t = min(self.flight_time * 2, 1)  # 时间因子
            ease_factor = 1 - (1 - t) ** 3  # ease-out cubic
            
            # 向目标移动
            self.x += (dx / distance) * move_speed * ease_factor
            self.y += (dy / distance) * move_speed * ease_factor
    
    def draw(self, screen: pygame.Surface, camera) -> None:
        """渲染子弹（带拖尾效果）
        
        Args:
            screen: 渲染目标表面
            camera: 摄像机对象
        """
        # 绘制拖尾效果
        trail_length = 3
        for i in range(trail_length):
            alpha = 255 - (i * 85)
            if alpha <= 0:
                break
            
            # 计算拖尾位置（向后偏移）
            dx = self.x - self.start_x
            dy = self.y - self.start_y
            trail_x = self.x - (dx * i * 0.15)
            trail_y = self.y - (dy * i * 0.15)
            
            # 转换到屏幕坐标
            screen_x, screen_y = camera.world_to_screen(trail_x, trail_y)
            size = int((self.width - i * 2) * camera.zoom)
            
            if size > 0:
                # 根据塔类型设置颜色
                colors = {
                    "basic": (255, 255, 0),
                    "rapid": (0, 255, 255),
                    "sniper": (255, 0, 255)
                }
                color = colors.get(self.tower_type, (255, 255, 0))
                
                # 绘制半透明拖尾
                trail_color = (color[0], color[1], color[2], alpha)
                pygame.draw.circle(screen, trail_color, (int(screen_x), int(screen_y)), size // 2)
        
        # 绘制子弹本体
        super().draw(screen, camera)