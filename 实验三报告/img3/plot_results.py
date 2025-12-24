#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
调度算法实验结果可视化脚本
生成四个场景的对比图表和综合性能雷达图
"""

import matplotlib.pyplot as plt
import matplotlib
import numpy as np
from matplotlib.patches import Circle, RegularPolygon
from matplotlib.path import Path
from matplotlib.projections.polar import PolarAxes
from matplotlib.projections import register_projection
from matplotlib.spines import Spine
from matplotlib.transforms import Affine2D

# 设置中文字体
matplotlib.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS']
matplotlib.rcParams['axes.unicode_minus'] = False

# 调度器名称和颜色
schedulers = ['DEFAULT', 'RR', 'PRIORITY', 'FCFS', 'SML']
colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8']

# ==================== 场景1：车队效应测试结果 ====================
def plot_scenario1():
    """绘制场景1车队效应对比图"""
    fig, ax = plt.subplots(figsize=(12, 6))
    
    # 数据：长作业、短作业、中作业的周转时间
    data = {
        'DEFAULT': [362, 487, 583],
        'RR': [628, 146, 339],
        'PRIORITY': [430, 44, 145],
        'FCFS': [321, 33, 108],
        'SML': [627, 129, 358]
    }
    
    x = np.arange(3)
    width = 0.15
    
    for i, scheduler in enumerate(schedulers):
        offset = (i - 2) * width
        ax.bar(x + offset, data[scheduler], width, label=scheduler, color=colors[i])
    
    ax.set_xlabel('作业类型', fontsize=12)
    ax.set_ylabel('周转时间 (ticks)', fontsize=12)
    ax.set_title('场景1：车队效应测试 - 不同作业周转时间对比', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(['长作业', '短作业', '中作业'])
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig('scenario1_convoy_effect.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✓ 场景1图表已生成：scenario1_convoy_effect.png")


# ==================== 场景2：交互响应测试结果 ====================
def plot_scenario2():
    """绘制场景2交互响应对比图"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # 数据：交互型进程的就绪时间和周转时间
    ready_time = [477, 398, 72, 530, 75]
    turnaround_time = [712, 565, 353, 714, 353]
    
    x = np.arange(len(schedulers))
    
    # 子图1：就绪时间
    bars1 = ax1.bar(x, ready_time, color=colors)
    ax1.set_xlabel('调度器', fontsize=12)
    ax1.set_ylabel('就绪时间 (ticks)', fontsize=12)
    ax1.set_title('交互型进程就绪时间对比', fontsize=13, fontweight='bold')
    ax1.set_xticks(x)
    ax1.set_xticklabels(schedulers, rotation=15)
    ax1.grid(True, alpha=0.3, linestyle='--', axis='y')
    
    # 标注最优值
    min_idx = ready_time.index(min(ready_time))
    ax1.text(min_idx, ready_time[min_idx] + 20, f'{ready_time[min_idx]}', 
             ha='center', va='bottom', fontweight='bold', color='red')
    
    # 子图2：周转时间
    bars2 = ax2.bar(x, turnaround_time, color=colors)
    ax2.set_xlabel('调度器', fontsize=12)
    ax2.set_ylabel('周转时间 (ticks)', fontsize=12)
    ax2.set_title('交互型进程周转时间对比', fontsize=13, fontweight='bold')
    ax2.set_xticks(x)
    ax2.set_xticklabels(schedulers, rotation=15)
    ax2.grid(True, alpha=0.3, linestyle='--', axis='y')
    
    # 标注最优值
    min_idx = turnaround_time.index(min(turnaround_time))
    ax2.text(min_idx, turnaround_time[min_idx] + 20, f'{turnaround_time[min_idx]}', 
             ha='center', va='bottom', fontweight='bold', color='red')
    
    plt.suptitle('场景2：交互响应性测试结果', fontsize=15, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig('scenario2_interactive_response.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✓ 场景2图表已生成：scenario2_interactive_response.png")


# ==================== 场景3：优先级饥饿测试结果 ====================
def plot_scenario3():
    """绘制场景3优先级饥饿对比图"""
    fig, ax = plt.subplots(figsize=(12, 6))
    
    # 数据：高、中、低优先级的就绪时间
    data = {
        'DEFAULT': [40, 585, 830],
        'RR': [540, 537, 562],
        'PRIORITY': [1, 298, 425],
        'FCFS': [2, 546, 815],
        'SML': [1, 647, 870]
    }
    
    x = np.arange(3)
    width = 0.15
    
    for i, scheduler in enumerate(schedulers):
        offset = (i - 2) * width
        ax.bar(x + offset, data[scheduler], width, label=scheduler, color=colors[i])
    
    ax.set_xlabel('优先级组', fontsize=12)
    ax.set_ylabel('就绪时间 (ticks)', fontsize=12)
    ax.set_title('场景3：优先级饥饿测试 - 不同优先级组就绪时间对比', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(['高优先级', '中优先级', '低优先级'])
    ax.legend(loc='upper left')
    ax.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig('scenario3_priority_starvation.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✓ 场景3图表已生成：scenario3_priority_starvation.png")


# ==================== 场景4：时间片公平性测试结果 ====================
def plot_scenario4():
    """绘制场景4时间片公平性对比图"""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # 数据：运行时间方差
    variance = [1, 0, 0, 1, 1]
    
    x = np.arange(len(schedulers))
    bars = ax.bar(x, variance, color=colors)
    
    # 标注方差值
    for i, v in enumerate(variance):
        ax.text(i, v + 0.05, str(v), ha='center', va='bottom', 
                fontweight='bold', fontsize=12)
    
    ax.set_xlabel('调度器', fontsize=12)
    ax.set_ylabel('运行时间方差', fontsize=12)
    ax.set_title('场景4：时间片公平性测试 - 运行时间方差对比', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(schedulers)
    ax.set_ylim(0, 1.5)
    ax.grid(True, alpha=0.3, linestyle='--', axis='y')
    
    # 添加说明文本
    ax.text(0.5, 0.95, '方差越小表示CPU时间分配越公平', 
            transform=ax.transAxes, ha='center', va='top',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
            fontsize=11)
    
    plt.tight_layout()
    plt.savefig('scenario4_fairness.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✓ 场景4图表已生成：scenario4_fairness.png")


# ==================== 综合性能雷达图 ====================
def plot_radar_chart():
    """绘制五种调度器综合性能雷达图"""
    # 评估维度
    categories = ['短作业\n友好性', '交互\n响应性', '低优先级\n公平性', 
                  '高优先级\n响应', '时间片\n公平性']
    N = len(categories)
    
    # 性能评分：差=1, 中=2, 优=3, 最优=4
    score_map = {'差': 1, '中': 2, '优': 3, '最优': 4}
    
    data = {
        'DEFAULT': [1, 1, 1, 2, 3],   # 差, 差, 差, 中, 优
        'RR': [3, 2, 3, 1, 4],         # 优, 中, 优, 差, 最优
        'PRIORITY': [4, 4, 2, 4, 4],   # 最优, 最优, 中, 最优, 最优
        'FCFS': [2, 1, 3, 2, 3],       # 中, 差, 优, 中, 优
        'SML': [3, 4, 1, 4, 3]         # 优, 最优, 差, 最优, 优
    }
    
    # 创建雷达图
    angles = [n / float(N) * 2 * np.pi for n in range(N)]
    angles += angles[:1]
    
    fig, ax = plt.subplots(figsize=(10, 10), subplot_kw=dict(projection='polar'))
    
    for i, (scheduler, values) in enumerate(data.items()):
        values += values[:1]
        ax.plot(angles, values, 'o-', linewidth=2, label=scheduler, color=colors[i])
        ax.fill(angles, values, alpha=0.15, color=colors[i])
    
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories, fontsize=11)
    ax.set_ylim(0, 4.5)
    ax.set_yticks([1, 2, 3, 4])
    ax.set_yticklabels(['差', '中', '优', '最优'], fontsize=10)
    ax.grid(True, linestyle='--', alpha=0.7)
    
    plt.title('五种调度算法综合性能评估雷达图', fontsize=15, fontweight='bold', pad=20)
    plt.legend(loc='upper right', bbox_to_anchor=(1.3, 1.1), fontsize=11)
    
    plt.tight_layout()
    plt.savefig('overall_performance_radar.png', dpi=300, bbox_inches='tight')
    plt.close()
    print("✓ 综合性能雷达图已生成：overall_performance_radar.png")


# ==================== 主函数 ====================
def main():
    """生成所有图表"""
    print("=" * 50)
    print("开始生成调度算法实验结果图表...")
    print("=" * 50)
    
    plot_scenario1()
    plot_scenario2()
    plot_scenario3()
    plot_scenario4()
    plot_radar_chart()
    
    print("=" * 50)
    print("所有图表生成完成！")
    print("=" * 50)


if __name__ == '__main__':
    main()
