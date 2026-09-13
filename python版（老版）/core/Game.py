#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""游戏主循环类

负责管理游戏的主循环、事件处理和场景切换
"""

import pygame
import sys
from settings import SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE, FPS
from core.Scene import Scene


class Game:
    """游戏主类"""
    
    def __init__(self):
        # 初始化pygame
        pygame.init()
        
        # 创建游戏窗口
        self.screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
        pygame.display.set_caption(SCREEN_TITLE)
        
        # 创建时钟对象
        self.clock = pygame.time.Clock()
        
        # 当前活动场景
        self.current_scene = None
        
        # 游戏是否运行
        self.running = True
        
        # 场景字典
        self.scenes = {}
    
    def add_scene(self, name: str, scene: Scene) -> None:
        """添加场景到场景字典
        
        Args:
            name: 场景名称
            scene: 场景对象
        """
        self.scenes[name] = scene
    
    def set_scene(self, name: str) -> None:
        """切换到指定场景
        
        Args:
            name: 场景名称
        """
        if name in self.scenes:
            self.current_scene = self.scenes[name]
            # 初始化场景
            self.current_scene.on_enter()
    
    def handle_events(self) -> None:
        """处理游戏事件"""
        for event in pygame.event.get():
            # 退出事件
            if event.type == pygame.QUIT:
                self.running = False
            
            # 传递事件给当前场景
            if self.current_scene:
                self.current_scene.handle_event(event)
    
    def update(self, delta_time: float) -> None:
        """更新游戏状态
        
        Args:
            delta_time: 帧时间间隔
        """
        if self.current_scene:
            self.current_scene.update(delta_time)
    
    def render(self) -> None:
        """渲染游戏画面"""
        # 清空屏幕
        self.screen.fill((0, 0, 0))
        
        # 渲染当前场景
        if self.current_scene:
            self.current_scene.render(self.screen)
        
        # 更新显示
        pygame.display.flip()
    
    def run(self) -> None:
        """游戏主循环"""
        while self.running:
            # 计算帧时间
            delta_time = self.clock.tick(FPS) / 1000.0
            
            # 处理事件
            self.handle_events()
            
            # 更新游戏状态
            self.update(delta_time)
            
            # 渲染画面
            self.render()
        
        # 退出游戏
        pygame.quit()
        sys.exit()