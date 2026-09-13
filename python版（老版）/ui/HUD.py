#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""HUD类

负责游戏界面的显示，包括金币、波次、生命值等信息
"""

from settings import COLORS, SCREEN_WIDTH, SCREEN_HEIGHT, INITIAL_GOLD

import pygame
from settings import COLORS, SCREEN_WIDTH, SCREEN_HEIGHT


class HUD:
    """游戏界面显示类"""
    
    def __init__(self):
        """初始化HUD"""
        # 字体 - 使用 Windows 默认宋体
        try:
            self.font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 36)
            self.small_font = pygame.font.Font('C:\\Windows\\Fonts\\simsun.ttc', 24)
        except:
            self.font = pygame.font.Font(None, 36)
            self.small_font = pygame.font.Font(None, 24)
        
        # 游戏数据
        self.gold = INITIAL_GOLD             
        self.wave = 1
        self.lives = 20
        self.selected_tower = None
        
        # 消息提示
        self.message = ""
        self.message_timer = 0
        
        # 塔类型列表
        self.tower_types = [
            {"type": "basic", "name": "基础塔", "cost": 100},
            {"type": "rapid", "name": "速射塔", "cost": 150},
            {"type": "sniper", "name": "狙击塔", "cost": 200}
        ]
    
    def set_message(self, message: str) -> None:
        """设置消息提示
        
        Args:
            message: 消息内容
        """
        self.message = message
        self.message_timer = 2.0  # 显示2秒
    
    def set_gold(self, gold: int) -> None:
        """设置金币数量
        
        Args:
            gold: 金币数量
        """
        self.gold = gold
    
    def set_wave(self, wave: int) -> None:
        """设置当前波次
        
        Args:
            wave: 波次编号
        """
        self.wave = wave
    
    def set_lives(self, lives: int) -> None:
        """设置生命值
        
        Args:
            lives: 生命值
        """
        self.lives = lives
    
    def set_selected_tower(self, tower_type: str) -> None:
        """设置选中的塔类型
        
        Args:
            tower_type: 塔类型
        """
        self.selected_tower = tower_type
    
    def draw(self, screen: pygame.Surface) -> None:
        """渲染HUD
        
        Args:
            screen: 渲染目标表面
        """
        # 绘制背景面板
        panel_height = 60
        panel_rect = pygame.Rect(0, SCREEN_HEIGHT - panel_height, SCREEN_WIDTH, panel_height)
        pygame.draw.rect(screen, (30, 30, 30), panel_rect)
        pygame.draw.rect(screen, (50, 50, 50), panel_rect, 2)
        
        # 绘制金币
        gold_text = self.font.render(f"金币: {self.gold}", True, COLORS["gold_color"])
        screen.blit(gold_text, (20, SCREEN_HEIGHT - panel_height + 10))
        
        # 绘制波次
        wave_text = self.font.render(f"波次: {self.wave}", True, COLORS["text_color"])
        screen.blit(wave_text, (200, SCREEN_HEIGHT - panel_height + 10))
        
        # 绘制生命值
        lives_text = self.font.render(f"生命: {self.lives}", True, (255, 0, 0))
        screen.blit(lives_text, (350, SCREEN_HEIGHT - panel_height + 10))
        
        # 绘制塔选择面板
        self._draw_tower_panel(screen)
    
    def _draw_tower_panel(self, screen: pygame.Surface) -> None:
        """绘制塔选择面板
        
        Args:
            screen: 渲染目标表面
        """
        start_x = SCREEN_WIDTH - 450
        y = SCREEN_HEIGHT - 55
        
        for i, tower_info in enumerate(self.tower_types):
            # 按钮宽度和高度
            button_width = 120
            button_height = 40
            button_x = start_x + i * (button_width + 20)
            
            # 判断是否可以购买
            can_afford = self.gold >= tower_info["cost"]
            
            # 判断是否被选中
            is_selected = self.selected_tower == tower_info["type"]
            
            # 设置按钮颜色
            if is_selected:
                color = (0, 255, 0)
            elif can_afford:
                color = (100, 100, 100)
            else:
                color = (50, 50, 50)
            
            # 绘制按钮背景
            pygame.draw.rect(screen, color, (button_x, y, button_width, button_height))
            pygame.draw.rect(screen, (255, 255, 255), (button_x, y, button_width, button_height), 2)
            
            # 绘制塔名称
            name_text = self.small_font.render(tower_info["name"], True, COLORS["text_color"])
            name_rect = name_text.get_rect(center=(button_x + button_width // 2, y + 15))
            screen.blit(name_text, name_rect)
            
            # 绘制价格
            cost_text = self.small_font.render(f"${tower_info['cost']}", True, COLORS["gold_color"])
            cost_rect = cost_text.get_rect(center=(button_x + button_width // 2, y + 32))
            screen.blit(cost_text, cost_rect)
        
        # 绘制快捷键提示
        hint_y = SCREEN_HEIGHT - 20
        hint_text = self.small_font.render("快捷键: WASD移动 | 1-2塔 | 3-7物流 | 8-0电力 | Tab切换 | T测试 | Space暂停", True, (100, 100, 100))
        screen.blit(hint_text, (10, hint_y))
        
        # 绘制消息提示
        if self.message_timer > 0:
            message_text = self.small_font.render(self.message, True, (255, 255, 0))
            message_rect = message_text.get_rect(center=(SCREEN_WIDTH // 2, SCREEN_HEIGHT - 80))
            screen.blit(message_text, message_rect)
    
    def update(self, delta_time: float) -> None:
        """更新消息计时器"""
        if self.message_timer > 0:
            self.message_timer -= delta_time
            if self.message_timer <= 0:
                self.message = ""