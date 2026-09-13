#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""场景基类

所有场景的父类，提供场景的基本生命周期管理
"""

import pygame


class Scene:
    """场景基类"""
    
    def __init__(self, game):
        """初始化场景
        
        Args:
            game: 游戏主对象引用
        """
        self.game = game
        self.entities = pygame.sprite.Group()
    
    def on_enter(self) -> None:
        """进入场景时调用"""
        pass
    
    def on_exit(self) -> None:
        """退出场景时调用"""
        pass
    
    def handle_event(self, event: pygame.event.Event) -> None:
        """处理事件
        
        Args:
            event: pygame事件对象
        """
        pass
    
    def update(self, delta_time: float) -> None:
        """更新场景状态
        
        Args:
            delta_time: 帧时间间隔
        """
        self.entities.update(delta_time)
    
    def render(self, screen: pygame.Surface) -> None:
        """渲染场景
        
        Args:
            screen: 渲染目标表面
        """
        self.entities.draw(screen)