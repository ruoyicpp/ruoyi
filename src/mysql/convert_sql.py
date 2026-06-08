#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PostgreSQL SQL 转 MySQL SQL 转换脚本
将 postgresql.sql 中的所有 INSERT 语句转换为 MySQL 格式
"""

import re
import sys

def convert_postgresql_to_mysql(pg_file, mysql_file):
    """
    转换 PostgreSQL SQL 为 MySQL SQL
    
    转换规则：
    1. "public"."table" → `table`
    2. "column" → `column`
    3. 't' → 1 (布尔真)
    4. 'f' → 0 (布尔假)
    5. timestamp(6) → DATETIME
    6. int8 → BIGINT
    7. int4 → INT
    8. 移除 COLLATE 子句
    9. 移除 ::regclass 类型转换
    """
    
    print(f"[开始] 读取 PostgreSQL 文件: {pg_file}")
    
    try:
        with open(pg_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"[错误] 无法读取文件: {e}")
        return False
    
    print(f"[成功] 文件大小: {len(content) / 1024 / 1024:.2f} MB")
    
    # 提取 INSERT 语句部分
    insert_start = content.find("-- Records of")
    if insert_start == -1:
        print("[警告] 未找到 INSERT 语句")
        return False
    
    insert_content = content[insert_start:]
    print(f"[成功] 提取 INSERT 语句部分: {len(insert_content) / 1024:.2f} KB")
    
    # 转换规则
    print("[转换] 应用转换规则...")
    
    # 1. 转换表和列名称
    insert_content = re.sub(r'"public"\.', '', insert_content)
    insert_content = insert_content.replace('"', '`')
    
    # 2. 转换布尔值
    insert_content = insert_content.replace("'t'", "1")
    insert_content = insert_content.replace("'f'", "0")
    
    # 3. 转换 COLLATE 子句（移除）
    insert_content = re.sub(r' COLLATE "pg_catalog"\."default"', '', insert_content)
    
    # 4. 转换类型转换（移除）
    insert_content = re.sub(r'::regclass', '', insert_content)
    insert_content = re.sub(r'::character varying', '', insert_content)
    insert_content = re.sub(r'::\w+', '', insert_content)
    
    # 5. 转换 nextval() 函数
    insert_content = re.sub(r"nextval\('([^']+)'\)", r'NULL', insert_content)
    
    print("[成功] 转换规则应用完成")
    
    # 读取现有的 MySQL 文件
    print(f"[开始] 读取现有 MySQL 文件: {mysql_file}")
    try:
        with open(mysql_file, 'r', encoding='utf-8') as f:
            mysql_content = f.read()
    except Exception as e:
        print(f"[错误] 无法读取 MySQL 文件: {e}")
        return False
    
    # 追加数据部分
    print("[开始] 追加数据部分...")
    
    data_section = "\n\n-- ============================================================\n"
    data_section += "-- 数据记录\n"
    data_section += "-- ============================================================\n\n"
    data_section += insert_content
    
    # 写入 MySQL 文件
    print(f"[开始] 写入 MySQL 文件: {mysql_file}")
    try:
        with open(mysql_file, 'w', encoding='utf-8') as f:
            f.write(mysql_content)
            f.write(data_section)
    except Exception as e:
        print(f"[错误] 无法写入文件: {e}")
        return False
    
    print("[成功] MySQL 文件已生成")
    
    # 统计信息
    insert_count = insert_content.count("INSERT INTO")
    print(f"\n[统计]")
    print(f"  - INSERT 语句数: {insert_count}")
    print(f"  - 文件大小: {len(mysql_content + data_section) / 1024 / 1024:.2f} MB")
    
    return True


def main():
    """主函数"""
    pg_file = r"g:\back\recovered\ruoyi-cpp\src\mysql\postgresql.sql"
    mysql_file = r"g:\back\recovered\ruoyi-cpp\src\mysql\mysql.sql"
    
    print("=" * 60)
    print("PostgreSQL SQL 转 MySQL SQL 转换工具")
    print("=" * 60)
    print()
    
    success = convert_postgresql_to_mysql(pg_file, mysql_file)
    
    print()
    if success:
        print("[完成] ✅ 转换成功！")
        print(f"输出文件: {mysql_file}")
        return 0
    else:
        print("[完成] ❌ 转换失败！")
        return 1


if __name__ == '__main__':
    sys.exit(main())
