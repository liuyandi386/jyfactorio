#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Author : JYGAME
# Copyright : JYGAME

"""存档系统

提供游戏存档和读档功能
"""

import json
import os
import time
from typing import Dict, Any, Optional
from settings import TILE_SIZE


SAVE_DIR = "saves"
SAVE_FILE = "save.json"


def get_save_path() -> str:
    """获取存档文件路径"""
    save_path = os.path.join(os.path.dirname(__file__), SAVE_DIR)
    if not os.path.exists(save_path):
        os.makedirs(save_path)
    return os.path.join(save_path, SAVE_FILE)


def save_game(scene) -> bool:
    """保存游戏
    
    Args:
        scene: GameScene实例
    
    Returns:
        是否保存成功
    """
    try:
        save_data = {
            "version": "1.0",
            "timestamp": time.time(),
            "game_state": {
                "game_over": scene.game_over,
                "paused": scene.paused,
                "current_wave": scene.current_wave,
                "lives": scene.game_ui.lives,
                "gold": scene.game_ui.gold
            },
            "player_inventory": scene.player_inventory,
            "buildings": {
                "towers": [],
                "miners": [],
                "conveyors": [],
                "buckets": [],
                "generators": [],
                "power_poles": [],
                "ammo_factories": [],
                "power_generators": [],
                "power_capacitors": [],
                "power_wires": [],
                "splitters": []
            },
            "enemies": [],  # 添加敌人列表
            "ore_deposits": []
        }
        
        # 保存塔
        for tower in scene.towers:
            tower_data = {
                "x": tower.x,
                "y": tower.y,
                "tower_type": tower.tower_type,
                "target_mode": tower.target_mode,
                "ammo": tower.inventory.get_item_count("ammo") if tower.inventory else 0
            }
            save_data["buildings"]["towers"].append(tower_data)
        
        # 保存采矿机
        for miner in scene.miners:
            # 保存采矿机inventory中的物品
            inventory_items = {}
            if hasattr(miner, 'inventory') and miner.inventory:
                for item_name in ["iron_ore", "copper_ore", "coal"]:
                    count = miner.inventory.get_item_count(item_name)
                    if count > 0:
                        inventory_items[item_name] = count
            
            miner_data = {
                "x": miner.x,
                "y": miner.y,
                "target_ore_type": miner.target_ore.ore_type if miner.target_ore else None,
                "output_direction": miner.output_direction,
                "inventory": inventory_items
            }
            save_data["buildings"]["miners"].append(miner_data)
        
        # 保存传送带
        for conveyor in scene.conveyors:
            conveyor_data = {
                "x": conveyor.x,
                "y": conveyor.y,
                "direction": conveyor.direction,
                "item": conveyor.item if conveyor.item else None  # 保存传送带上的物品
            }
            save_data["buildings"]["conveyors"].append(conveyor_data)
        
        # 保存储物桶
        for bucket in scene.buckets:
            bucket_data = {
                "x": bucket.x,
                "y": bucket.y,
                "output_direction": bucket.output_direction,
                "items": bucket.items.copy()
            }
            save_data["buildings"]["buckets"].append(bucket_data)
        
        # 保存发电机
        for generator in scene.generators:
            generator_data = {
                "x": generator.x,
                "y": generator.y,
                "coal": generator.inventory.get_item_count("coal") if generator.inventory else 0
            }
            save_data["buildings"]["generators"].append(generator_data)
        
        # 保存电力杆
        for pole in scene.power_poles:
            pole_data = {
                "x": pole.x,
                "y": pole.y
            }
            save_data["buildings"]["power_poles"].append(pole_data)
        
        # 保存弹药制造机
        for factory in scene.ammo_factories:
            factory_data = {
                "x": factory.x,
                "y": factory.y,
                "output_direction": factory.output_direction,
                "iron_ore": factory.inventory.get_item_count("iron_ore"),
                "copper_ore": factory.inventory.get_item_count("copper_ore"),
                "ammo": factory.inventory.get_item_count("ammo")
            }
            save_data["buildings"]["ammo_factories"].append(factory_data)
        
        # 保存燃煤发电机（工业电力系统）
        for pg in scene.power_generators:
            pg_data = {
                "x": pg.x,
                "y": pg.y,
                "fuel_time": pg.fuel_time,
                "coal": pg.inventory.get_item_count("coal")
            }
            save_data["buildings"]["power_generators"].append(pg_data)
        
        # 保存电容库
        for cap in scene.power_capacitors:
            cap_data = {
                "x": cap.x,
                "y": cap.y,
                "energy": cap.energy
            }
            save_data["buildings"]["power_capacitors"].append(cap_data)
        
        # 保存电力线缆
        for wire in scene.power_wires:
            wire_data = {
                "x": wire.x,
                "y": wire.y,
                "faces": list(wire.faces)
            }
            save_data["buildings"]["power_wires"].append(wire_data)
        
        # 保存物品分流器
        for sp in scene.splitters:
            sp_data = {
                "x": sp.x,
                "y": sp.y,
                "faces": list(sp.faces),
                "queue": list(sp.item_queue)
            }
            save_data["buildings"]["splitters"].append(sp_data)
        
        # 保存矿石资源
        for ore in scene.ore_deposits:
            ore_data = {
                "x": ore.x,
                "y": ore.y,
                "ore_type": ore.ore_type,
                "amount": ore.inventory.get_item_count(ore.ore_type) if ore.inventory else ore.amount
            }
            save_data["ore_deposits"].append(ore_data)
        
        # 保存敌人
        for enemy in scene.enemies:
            enemy_data = {
                "x": enemy.x,
                "y": enemy.y,
                "enemy_type": enemy.enemy_type,
                "health": enemy.health,
                "max_health": enemy.max_health,
                "path_index": enemy.path_index,
                "target_x": enemy.target_x,
                "target_y": enemy.target_y
            }
            save_data["enemies"].append(enemy_data)
        
        # 写入文件
        with open(get_save_path(), 'w', encoding='utf-8') as f:
            json.dump(save_data, f, indent=2, ensure_ascii=False)
        
        return True
    
    except Exception as e:
        print(f"保存失败: {e}")
        return False


def load_game(scene) -> bool:
    """加载游戏
    
    Args:
        scene: GameScene实例
    
    Returns:
        是否加载成功
    """
    save_path = get_save_path()
    if not os.path.exists(save_path):
        return False
    
    try:
        with open(save_path, 'r', encoding='utf-8') as f:
            save_data = json.load(f)
        
        # 清空现有实体
        scene.towers.empty()
        scene.miners.empty()
        scene.conveyors.empty()
        scene.buckets.empty()
        scene.generators.empty()
        scene.power_poles.empty()
        scene.ammo_factories.empty()
        scene.power_generators.empty()
        scene.power_capacitors.empty()
        scene.power_wires.empty()
        scene.splitters.empty()
        scene.ore_deposits.empty()
        scene.enemies.empty()
        scene.bullets.empty()
        
        # 重置网格
        scene.grid_manager.reset()
        
        # 恢复游戏状态
        game_state = save_data.get("game_state", {})
        scene.game_over = game_state.get("game_over", False)
        scene.paused = game_state.get("paused", False)
        scene.current_wave = game_state.get("current_wave", 1)
        
        # 恢复UI状态
        if hasattr(scene.game_ui, 'lives'):
            scene.game_ui.lives = game_state.get("lives", 20)
        if hasattr(scene.game_ui, 'gold'):
            scene.game_ui.gold = game_state.get("gold", 100)
        
        # 恢复玩家资源
        scene.player_inventory = save_data.get("player_inventory", {
            "iron_ore": 999999,
            "copper_ore": 999999,
            "coal": 999999,
            "ammo": 999999
        })
        
        # 先恢复矿石资源（采矿机依赖矿石）
        for ore_data in save_data.get("ore_deposits", []):
            from entities.OreDeposit import OreDeposit
            ore = OreDeposit(ore_data["x"], ore_data["y"], ore_data["ore_type"])
            # OreDeposit使用inventory存储数量
            if hasattr(ore, 'inventory'):
                ore.inventory.items[ore_data["ore_type"]] = ore_data.get("amount", 999999)
            scene.ore_deposits.add(ore)
        
        # 恢复建筑
        buildings = save_data.get("buildings", {})
        
        # 恢复塔
        for tower_data in buildings.get("towers", []):
            tower_type_full = tower_data["tower_type"] + "_tower"  # 转换为完整类型名
            tower = scene._place_building(
                tower_data["x"], tower_data["y"], 
                tower_type_full
            )
            if tower and hasattr(tower, 'inventory') and tower.inventory is not None:
                tower.inventory.add_item("ammo", tower_data.get("ammo", 0))
        
        # 恢复采矿机（需要先找到对应类型的矿石）
        for miner_data in buildings.get("miners", []):
            target_ore_type = miner_data.get("target_ore_type")
            miner_x = miner_data["x"]
            miner_y = miner_data["y"]
            
            # 找到对应位置和类型的矿石
            target_ore = None
            for ore in scene.ore_deposits:
                if ore.ore_type == target_ore_type and ore.rect.collidepoint(miner_x + TILE_SIZE // 2, miner_y + TILE_SIZE // 2):
                    target_ore = ore
                    break
            
            if target_ore:
                # 直接创建采矿机，绕过 _place_building 的矿石检测
                from entities.Miner import Miner
                miner = Miner(miner_x, miner_y, miner_data.get("output_direction", 2))
                miner.set_target_ore(target_ore)
                scene.miners.add(miner)
                scene.power_grid.add_consumer(miner)
                
                # 恢复采矿机inventory中的物品
                inventory_items = miner_data.get("inventory", {})
                for item_name, count in inventory_items.items():
                    miner.inventory.add_item(item_name, count)
        
        # 恢复传送带
        for conveyor_data in buildings.get("conveyors", []):
            scene._place_building(
                conveyor_data["x"], conveyor_data["y"], 
                "conveyor", conveyor_data.get("direction", 0)
            )
            # 恢复传送带上的物品（需要找到对应的传送带）
            item_data = conveyor_data.get("item")
            if item_data:
                for conveyor in scene.conveyors:
                    if abs(conveyor.x - conveyor_data["x"]) < 1 and abs(conveyor.y - conveyor_data["y"]) < 1:
                        conveyor.item = item_data.copy()
                        break
        
        # 恢复储物桶
        for bucket_data in buildings.get("buckets", []):
            bucket = scene._place_building(
                bucket_data["x"], bucket_data["y"], 
                "bucket", bucket_data.get("output_direction", 2)
            )
            if bucket and hasattr(bucket, 'items'):
                bucket.items = bucket_data.get("items", []).copy()
        
        # 恢复发电机
        for generator_data in buildings.get("generators", []):
            generator = scene._place_building(
                generator_data["x"], generator_data["y"], 
                "generator"
            )
            if generator and hasattr(generator, 'inventory') and generator.inventory is not None:
                generator.inventory.add_item("coal", generator_data.get("coal", 0))
        
        # 恢复电力杆
        for pole_data in buildings.get("power_poles", []):
            scene._place_building(pole_data["x"], pole_data["y"], "power_pole")
        
        # 恢复弹药制造机
        for factory_data in buildings.get("ammo_factories", []):
            factory = scene._place_building(
                factory_data["x"], factory_data["y"], 
                "ammo_factory", factory_data.get("output_direction", 2)
            )
            if factory and hasattr(factory, 'inventory') and factory.inventory is not None:
                factory.inventory.add_item("iron_ore", factory_data.get("iron_ore", 0))
                factory.inventory.add_item("copper_ore", factory_data.get("copper_ore", 0))
                factory.inventory.add_item("ammo", factory_data.get("ammo", 0))
        
        # 恢复燃煤发电机（工业电力系统）
        for pg_data in buildings.get("power_generators", []):
            pg = scene._place_building(pg_data["x"], pg_data["y"], "power_generator")
            if pg and hasattr(pg, 'inventory') and pg.inventory is not None:
                pg.inventory.add_item("coal", pg_data.get("coal", 0))
            if pg and hasattr(pg, 'fuel_time') and pg.fuel_time is not None:
                pg.fuel_time = pg_data.get("fuel_time", 0.0)
        
        # 恢复电容库
        for cap_data in buildings.get("power_capacitors", []):
            cap = scene._place_building(cap_data["x"], cap_data["y"], "capacitor")
            if cap and hasattr(cap, 'energy'):
                cap.energy = cap_data.get("energy", 0.0)
        
        # 恢复电力线缆
        for wire_data in buildings.get("power_wires", []):
            wire = scene._place_building(wire_data["x"], wire_data["y"], "power_wire")
            if wire and hasattr(wire, 'faces'):
                saved_faces = wire_data.get("faces")
                if saved_faces and len(saved_faces) == 4:
                    wire.faces = list(saved_faces)
                    wire.refresh_appearance()
        
        # 恢复物品分流器
        for sp_data in buildings.get("splitters", []):
            sp = scene._place_building(sp_data["x"], sp_data["y"], "splitter")
            if sp and hasattr(sp, 'faces'):
                saved_faces = sp_data.get("faces")
                if saved_faces and len(saved_faces) == 4:
                    sp.faces = list(saved_faces)
                    sp.refresh_appearance()
                saved_queue = sp_data.get("queue", [])
                for item in saved_queue:
                    sp.add_item(item)
        
        # 恢复敌人
        for enemy_data in save_data.get("enemies", []):
            from entities.Enemy import Enemy
            enemy = Enemy(enemy_data.get("enemy_type", "basic"))
            # 设置敌人位置和状态
            enemy.x = enemy_data.get("x", enemy.x)
            enemy.y = enemy_data.get("y", enemy.y)
            enemy.health = enemy_data.get("health", enemy.max_health)
            enemy.path_index = enemy_data.get("path_index", 0)
            enemy.target_x = enemy_data.get("target_x", enemy.target_x)
            enemy.target_y = enemy_data.get("target_y", enemy.target_y)
            enemy.rect.x = enemy.x
            enemy.rect.y = enemy.y
            scene.enemies.add(enemy)
        
        # 刷新电线外观
        if hasattr(scene, "_rebuild_power_wire_appearances"):
            scene._rebuild_power_wire_appearances()
        if hasattr(scene, "power_manager"):
            scene.power_manager._rebuild_networks()
        
        return True
    
    except Exception as e:
        print(f"加载失败: {e}")
        return False


def has_save() -> bool:
    """检查是否有存档"""
    return os.path.exists(get_save_path())
