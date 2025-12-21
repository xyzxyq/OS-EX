#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
调度算法性能对比可视化脚本
生成各调度算法的性能指标对比图
"""

import matplotlib.pyplot as plt
import numpy as np

# 尝试设置中文字体，如果失败则使用默认字体
try:
    import matplotlib
    matplotlib.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'DejaVu Sans']
    matplotlib.rcParams['axes.unicode_minus'] = False
except:
    pass

# 测试数据
schedulers = ['PRIORITY', 'FCFS', 'RR', 'SML']

# 每个调度算法的详细数据 (PID, Ready, Run, Sleep, Turnaround)
data = {
    'PRIORITY': {
        'pids': [4, 5, 6, 7, 8, 9],
        'ready': [1, 12, 23, 35, 35, 36],
        'run': [12, 11, 12, 0, 0, 0],
        'sleep': [0, 0, 0, 250, 250, 250],
        'turnaround': [13, 23, 35, 285, 285, 286],
        'avg': {'ready': 23, 'run': 5, 'sleep': 125, 'turnaround': 154}
    },
    'FCFS': {
        'pids': [10, 11, 12, 13, 14, 15],
        'ready': [1, 12, 24, 36, 36, 37],
        'run': [12, 12, 12, 0, 0, 0],
        'sleep': [0, 0, 0, 250, 250, 250],
        'turnaround': [13, 24, 36, 286, 286, 287],
        'avg': {'ready': 24, 'run': 6, 'sleep': 125, 'turnaround': 155}
    },
    'RR': {
        'pids': [16, 17, 18, 19, 20, 21],
        'ready': [23, 25, 23, 25, 25, 25],
        'run': [11, 12, 13, 0, 1, 1],
        'sleep': [0, 0, 0, 234, 234, 234],
        'turnaround': [34, 37, 36, 259, 260, 260],
        'avg': {'ready': 24, 'run': 6, 'sleep': 117, 'turnaround': 147}
    },
    'SML': {
        'pids': [22, 23, 24, 25, 26, 27],
        'ready': [1, 13, 24, 25, 25, 36],
        'run': [12, 12, 12, 0, 0, 0],
        'sleep': [0, 0, 0, 250, 250, 245],
        'turnaround': [13, 25, 36, 275, 275, 281],
        'avg': {'ready': 20, 'run': 6, 'sleep': 124, 'turnaround': 150}
    }
}

def plot_average_comparison():
    """绘制平均指标对比图"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('调度算法性能对比', fontsize=20, fontweight='bold')
    
    metrics = ['ready', 'run', 'sleep', 'turnaround']
    titles = ['平均就绪时间 (Ready Time)', '平均运行时间 (Run Time)', 
              '平均休眠时间 (Sleep Time)', '平均周转时间 (Turnaround Time)']
    colors = ['#3498db', '#e74c3c', '#2ecc71', '#9b59b6']
    
    for idx, (metric, title, color) in enumerate(zip(metrics, titles, colors)):
        ax = axes[idx // 2, idx % 2]
        values = [data[s]['avg'][metric] for s in schedulers]
        bars = ax.bar(schedulers, values, color=color, edgecolor='black', linewidth=1.2)
        ax.set_title(title, fontsize=14, fontweight='bold')
        ax.set_ylabel('时间 (ticks)', fontsize=12)
        ax.set_xlabel('调度算法', fontsize=12)
        ax.tick_params(axis='both', labelsize=11)
        
        # 添加数值标签
        for bar, val in zip(bars, values):
            ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1, 
                   str(val), ha='center', va='bottom', fontsize=13, fontweight='bold')
        
        ax.set_ylim(0, max(values) * 1.15)
        ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('scheduler_comparison_avg.png', dpi=150, bbox_inches='tight')
    print("已保存: scheduler_comparison_avg.png")
    plt.show()

def plot_process_details():
    """绘制各进程详细数据对比图"""
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('各调度算法进程执行详情', fontsize=20, fontweight='bold')
    
    for idx, scheduler in enumerate(schedulers):
        ax = axes[idx // 2, idx % 2]
        d = data[scheduler]
        x = np.arange(len(d['pids']))
        width = 0.2
        
        # 绘制堆叠条形图
        bars1 = ax.bar(x - width, d['ready'], width, label='就绪时间', color='#3498db')
        bars2 = ax.bar(x, d['run'], width, label='运行时间', color='#e74c3c')
        bars3 = ax.bar(x + width, d['sleep'], width, label='休眠时间', color='#2ecc71')
        
        ax.set_title(f'{scheduler} 调度算法', fontsize=14, fontweight='bold')
        ax.set_xlabel('进程类型', fontsize=12)
        ax.set_ylabel('时间 (ticks)', fontsize=12)
        ax.tick_params(axis='both', labelsize=11)
        ax.set_xticks(x)
        process_labels = ['CPU-1', 'CPU-2', 'CPU-3', 'IO-1', 'IO-2', 'IO-3']
        ax.set_xticklabels(process_labels)
        ax.legend(loc='upper right', fontsize=11)
        ax.grid(axis='y', alpha=0.3)
        
        # 添加周转时间标注
        for i, t in enumerate(d['turnaround']):
            ax.annotate(f'T={t}', (i, max(d['ready'][i], d['run'][i], d['sleep'][i]) + 5),
                       ha='center', fontsize=10, color='gray')
    
    plt.tight_layout()
    plt.savefig('scheduler_process_details.png', dpi=150, bbox_inches='tight')
    print("已保存: scheduler_process_details.png")
    plt.show()

def plot_turnaround_comparison():
    """绘制周转时间对比图"""
    fig, ax = plt.subplots(figsize=(12, 6))
    
    x = np.arange(6)
    width = 0.2
    colors = ['#3498db', '#e74c3c', '#2ecc71', '#9b59b6']
    process_labels = ['CPU-1\n(高优先级)', 'CPU-2', 'CPU-3', 'IO-1\n(低优先级)', 'IO-2', 'IO-3']
    
    for i, (scheduler, color) in enumerate(zip(schedulers, colors)):
        offset = (i - 1.5) * width
        bars = ax.bar(x + offset, data[scheduler]['turnaround'], width, 
                     label=scheduler, color=color, edgecolor='black', linewidth=0.5)
    
    ax.set_title('各调度算法进程周转时间对比', fontsize=16, fontweight='bold')
    ax.set_xlabel('进程', fontsize=13)
    ax.set_ylabel('周转时间 (ticks)', fontsize=13)
    ax.tick_params(axis='both', labelsize=12)
    ax.set_xticks(x)
    ax.set_xticklabels(process_labels)
    ax.legend(title='调度算法', loc='upper left', fontsize=11, title_fontsize=12)
    ax.grid(axis='y', alpha=0.3)
    
    # 添加分隔线区分CPU密集型和IO密集型
    ax.axvline(x=2.5, color='gray', linestyle='--', alpha=0.7)
    ax.text(1, ax.get_ylim()[1]*0.95, 'CPU密集型', ha='center', fontsize=12, style='italic')
    ax.text(4, ax.get_ylim()[1]*0.95, 'IO密集型', ha='center', fontsize=12, style='italic')
    
    plt.tight_layout()
    plt.savefig('scheduler_turnaround_comparison.png', dpi=150, bbox_inches='tight')
    print("已保存: scheduler_turnaround_comparison.png")
    plt.show()

def plot_summary_radar():
    """绘制雷达图对比各算法综合性能"""
    categories = ['响应速度\n(就绪时间↓)', '执行效率\n(运行时间↑)', 
                  '休眠控制\n(休眠时间↓)', '总体效率\n(周转时间↓)']
    
    # 归一化分数 (就绪时间和周转时间取反，越小越好)
    scores = {}
    for scheduler in schedulers:
        avg = data[scheduler]['avg']
        scores[scheduler] = [
            100 - (avg['ready'] / 30 * 100),  # 就绪时间越小越好
            avg['run'] / 10 * 100,             # 运行时间
            100 - (avg['sleep'] / 150 * 100), # 休眠时间越小越好
            100 - (avg['turnaround'] / 200 * 100)  # 周转时间越小越好
        ]
    
    fig, ax = plt.subplots(figsize=(10, 8), subplot_kw=dict(polar=True))
    
    angles = np.linspace(0, 2 * np.pi, len(categories), endpoint=False).tolist()
    angles += angles[:1]  # 闭合
    
    colors = ['#3498db', '#e74c3c', '#2ecc71', '#9b59b6']
    
    for scheduler, color in zip(schedulers, colors):
        values = scores[scheduler] + scores[scheduler][:1]  # 闭合
        ax.plot(angles, values, 'o-', linewidth=2, label=scheduler, color=color)
        ax.fill(angles, values, alpha=0.15, color=color)
    
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories, fontsize=12)
    ax.set_title('调度算法综合性能雷达图', fontsize=16, fontweight='bold', pad=20)
    ax.tick_params(axis='y', labelsize=11)
    ax.legend(loc='upper right', bbox_to_anchor=(1.3, 1.0), fontsize=11)
    ax.set_ylim(0, 100)
    
    plt.tight_layout()
    plt.savefig('scheduler_radar_chart.png', dpi=150, bbox_inches='tight')
    print("已保存: scheduler_radar_chart.png")
    plt.show()

def main():
    print("=" * 50)
    print("    调度算法性能对比可视化")
    print("=" * 50)
    
    print("\n1. 生成平均指标对比图...")
    plot_average_comparison()
    
    print("\n2. 生成进程详细数据图...")
    plot_process_details()
    
    print("\n3. 生成周转时间对比图...")
    plot_turnaround_comparison()
    
    print("\n4. 生成综合性能雷达图...")
    plot_summary_radar()
    
    print("\n" + "=" * 50)
    print("所有图表生成完成！")
    print("=" * 50)

if __name__ == "__main__":
    main()
