#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
贴图整理脚本

功能：
1. 创建目录结构
2. 旋转塔贴图（8方向）
3. 旋转机器贴图（4方向）
4. 复制现有贴图到对应目录
5. 传送带命名规则：_1为正式用，_2为备用
"""

import os
import shutil
import pygame
import math

# 初始化pygame
pygame.init()
pygame.display.set_mode((1, 1), pygame.HIDDEN)

# 贴图尺寸
TILE_SIZE = 32

# 颜色定义
METAL_BASE = (100, 100, 100)
METAL_DARK = (60, 60, 60)
METAL_LIGHT = (140, 140, 140)
COPPER_BASE = (184, 115, 51)
COPPER_DARK = (140, 85, 35)
COPPER_LIGHT = (210, 140, 70)
STEEL_BASE = (120, 140, 160)
STEEL_DARK = (80, 100, 120)
STEEL_LIGHT = (160, 180, 200)
WARNING_ORANGE = (255, 152, 0)
ELECTRIC_BLUE = (60, 120, 200)
ELECTRIC_CYAN = (0, 200, 255)
ELECTRIC_GLOW = (0, 180, 220)
WARNING_YELLOW = (255, 200, 0)

# 源目录
SOURCE_DIR = "assets/sprites"
# 目标目录
BASE_DIR = "assets/sprites"

# 目录结构
DIRECTORIES = [
    "towers",
    "machines",
    "containers",
    "conveyors",
    "terrain",
    "ores",
    "enemies",
    "items"
]

# 塔类贴图（需要8方向）
TOWERS = [
    "tower_basic.png",
    "tower_rapid.png", 
    "tower_sniper.png",
    "tower_electric.png"
]

# 机器类贴图（需要4方向）
MACHINES = [
    ("machine_miner.png", "miner"),
    ("machine_ammo_factory.png", "ammo_factory"),
    ("machine_generator.png", "generator"),
    ("machine_power_pole.png", "power_pole")
]

# 储物桶（需要4方向）
CONTAINERS = [
    ("container_bucket.png", "bucket")
]

# 传送带（已有贴图，直接复制）
CONVEYORS = [
    "conveyor_dir0_1.png",
    "conveyor_dir0_2.png",
    "conveyor_dir1_1.png",
    "conveyor_dir1_2.png",
    "conveyor_dir2_1.png",
    "conveyor_dir2_2.png",
    "conveyor_dir3_1.png",
    "conveyor_dir3_2.png"
]

# 地面贴图
TERRAIN = [
    "terrain_grass.png",
    "terrain_path.png"
]

# 矿石贴图
ORES = [
    "ore_iron.png",
    "ore_copper.png",
    "ore_coal.png"
]

# 敌人贴图
ENEMIES = [
    "enemy_basic.png",
    "enemy_fast.png",
    "enemy_tank.png"
]

# 物品贴图
ITEMS = [
    "item_ammo.png"
]

# 方向名称映射（8方向）
DIR_NAMES_8 = {
    0: "up",
    1: "up_right",
    2: "right",
    3: "down_right",
    4: "down",
    5: "down_left",
    6: "left",
    7: "up_left"
}

# 方向名称映射（4方向）
DIR_NAMES_4 = {
    0: "up",
    1: "right",
    2: "down",
    3: "left"
}


def _create_surface():
    """创建透明表面"""
    surf = pygame.Surface((TILE_SIZE, TILE_SIZE), pygame.SRCALPHA)
    surf.fill((0, 0, 0, 0))
    return surf


def _draw_circular_base(surf, color):
    """绘制圆形底座"""
    center = TILE_SIZE // 2
    pygame.draw.circle(surf, color, (center, center), 12)
    pygame.draw.circle(surf, METAL_LIGHT, (center, center), 10)
    pygame.draw.circle(surf, METAL_DARK, (center, center), 6)


def generate_rapid_tower_image():
    """生成速射塔贴图"""
    surf = _create_surface()
    center = TILE_SIZE // 2
    
    _draw_circular_base(surf, COPPER_BASE)
    
    pygame.draw.circle(surf, COPPER_DARK, (center, center), 7)
    pygame.draw.circle(surf, COPPER_LIGHT, (center, center), 5)
    
    barrel_length = center - 6
    for i in range(3):
        offset_angle = (i - 1) * 20
        angle = offset_angle
        rad = math.radians(angle)
        end_x = center + int(barrel_length * math.sin(rad))
        end_y = center - int(barrel_length * math.cos(rad))
        
        barrel_color = COPPER_BASE if i % 2 == 0 else COPPER_LIGHT
        pygame.draw.line(surf, barrel_color, (center, center), (end_x, end_y), 3)
    
    pygame.draw.circle(surf, WARNING_ORANGE, (center, 3), 2)
    
    return surf


def generate_bucket_image():
    """生成储物桶贴图"""
    surf = _create_surface()
    center = TILE_SIZE // 2
    size = TILE_SIZE
    
    points = [
        (center-8, 4),
        (center+8, 4),
        (center+10, size-4),
        (center-10, size-4)
    ]
    pygame.draw.polygon(surf, (100, 149, 237), points)
    pygame.draw.polygon(surf, (70, 130, 180), points, 2)
    
    pygame.draw.ellipse(surf, (120, 170, 255), (center-8, 2, 16, 6))
    pygame.draw.ellipse(surf, (80, 130, 200), (center-8, 2, 16, 6), 1)
    
    pygame.draw.polygon(surf, (80, 120, 200), [
        (center-6, 8), (center+6, 8),
        (center+8, size-6), (center-8, size-6)
    ])
    
    pygame.draw.rect(surf, (60, 100, 160), (2, center-4, 3, 8), border_radius=1)
    pygame.draw.rect(surf, (60, 100, 160), (size-5, center-4, 3, 8), border_radius=1)
    
    arrow_color = (255, 152, 0)
    arrow_size = 6
    pygame.draw.polygon(surf, arrow_color, [
        (center, size-4),
        (center-arrow_size, size-8),
        (center+arrow_size, size-8)
    ])
    
    return surf


def create_directories():
    """创建目录结构"""
    for dir_name in DIRECTORIES:
        full_path = os.path.join(BASE_DIR, dir_name)
        if not os.path.exists(full_path):
            os.makedirs(full_path)
            print(f"创建目录: {full_path}")


def load_image(filepath):
    """加载图片"""
    if os.path.exists(filepath):
        return pygame.image.load(filepath).convert_alpha()
    return None


def rotate_and_save(image, angle_degrees, output_path):
    """旋转图片并保存"""
    rotated = pygame.transform.rotate(image, -angle_degrees)
    pygame.image.save(rotated, output_path)
    print(f"  保存: {output_path}")


def process_towers():
    """处理塔类贴图（8方向）"""
    target_dir = os.path.join(BASE_DIR, "towers")
    
    for tower_file in TOWERS:
        source_path = os.path.join(SOURCE_DIR, tower_file)
        image = load_image(source_path)
        
        if image is None:
            print(f"警告: 未找到塔贴图 {tower_file}")
            continue
        
        tower_name = tower_file.replace(".png", "")
        
        print(f"\n处理塔: {tower_name}")
        for dir_idx in range(8):
            angle = dir_idx * 45
            dir_name = DIR_NAMES_8[dir_idx]
            output_filename = f"{tower_name}_{dir_name}.png"
            output_path = os.path.join(target_dir, output_filename)
            rotate_and_save(image, angle, output_path)


def process_machines():
    """处理机器类贴图（4方向）"""
    target_dir = os.path.join(BASE_DIR, "machines")
    
    for machine_file, machine_name in MACHINES:
        source_path = os.path.join(SOURCE_DIR, machine_file)
        image = load_image(source_path)
        
        if image is None:
            print(f"警告: 未找到机器贴图 {machine_file}")
            continue
        
        print(f"\n处理机器: {machine_name}")
        if machine_name == "power_pole":
            # 电线杆不需要方向
            output_filename = f"machine_{machine_name}.png"
            output_path = os.path.join(target_dir, output_filename)
            pygame.image.save(image, output_path)
            print(f"  保存: {output_path}")
        else:
            # 采矿机、弹药制造机、发电机需要4方向
            for dir_idx in range(4):
                angle = dir_idx * 90
                dir_name = DIR_NAMES_4[dir_idx]
                output_filename = f"machine_{machine_name}_{dir_name}.png"
                output_path = os.path.join(target_dir, output_filename)
                rotate_and_save(image, angle, output_path)


def process_containers():
    """处理储物桶贴图（4方向）"""
    target_dir = os.path.join(BASE_DIR, "containers")
    
    for container_file, container_name in CONTAINERS:
        source_path = os.path.join(SOURCE_DIR, container_file)
        image = load_image(source_path)
        
        if image is None:
            print(f"警告: 未找到储物桶贴图 {container_file}")
            continue
        
        print(f"\n处理储物桶: {container_name}")
        for dir_idx in range(4):
            angle = dir_idx * 90
            dir_name = DIR_NAMES_4[dir_idx]
            output_filename = f"container_{container_name}_{dir_name}.png"
            output_path = os.path.join(target_dir, output_filename)
            rotate_and_save(image, angle, output_path)


def process_conveyors():
    """处理传送带贴图（已有贴图，直接复制）"""
    target_dir = os.path.join(BASE_DIR, "conveyors")
    
    print("\n处理传送带:")
    for conveyor_file in CONVEYORS:
        source_path = os.path.join(SOURCE_DIR, conveyor_file)
        if os.path.exists(source_path):
            output_path = os.path.join(target_dir, conveyor_file)
            shutil.copy2(source_path, output_path)
            print(f"  复制: {conveyor_file}")
        else:
            print(f"  警告: 未找到 {conveyor_file}")


def copy_files(file_list, target_dir_name):
    """复制文件到目标目录"""
    target_dir = os.path.join(BASE_DIR, target_dir_name)
    
    print(f"\n处理{target_dir_name}:")
    for filename in file_list:
        source_path = os.path.join(SOURCE_DIR, filename)
        if os.path.exists(source_path):
            output_path = os.path.join(target_dir, filename)
            shutil.copy2(source_path, output_path)
            print(f"  复制: {filename}")
        else:
            print(f"  警告: 未找到 {filename}")


def main():
    """主函数"""
    print("=" * 60)
    print("贴图整理脚本")
    print("=" * 60)
    
    # 1. 创建目录结构
    print("\n1. 创建目录结构")
    create_directories()
    
    # 2. 处理塔类贴图（8方向）
    print("\n2. 处理塔类贴图（8方向）")
    process_towers()
    
    # 3. 处理机器类贴图（4方向）
    print("\n3. 处理机器类贴图（4方向）")
    process_machines()
    
    # 4. 处理储物桶贴图（4方向）
    print("\n4. 处理储物桶贴图（4方向）")
    process_containers()
    
    # 5. 处理传送带贴图
    print("\n5. 处理传送带贴图")
    process_conveyors()
    
    # 6. 处理地面贴图
    print("\n6. 处理地面贴图")
    copy_files(TERRAIN, "terrain")
    
    # 7. 处理矿石贴图
    print("\n7. 处理矿石贴图")
    copy_files(ORES, "ores")
    
    # 8. 处理敌人贴图
    print("\n8. 处理敌人贴图")
    copy_files(ENEMIES, "enemies")
    
    # 9. 处理物品贴图
    print("\n9. 处理物品贴图")
    copy_files(ITEMS, "items")
    
    print("\n" + "=" * 60)
    print("贴图整理完成!")
    print("=" * 60)
    
    pygame.quit()


if __name__ == "__main__":
    main()
