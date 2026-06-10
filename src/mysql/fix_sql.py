#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复 MySQL SQL 文件中的 PostgreSQL 特定语法
"""

import re
import sys

def fix_mysql_sql(mysql_file):
    """
    修复 MySQL SQL 文件中的 PostgreSQL 特定语法
    """
    
    print(f"[开始] 读取 MySQL 文件: {mysql_file}")
    
    try:
        with open(mysql_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"[错误] 无法读取文件: {e}")
        return False
    
    print(f"[成功] 文件大小: {len(content) / 1024 / 1024:.2f} MB")
    
    print("[开始] 应用修复规则...")
    
    # 1. 移除 USING btree 语法
    print("  [修复] 移除 USING btree 语法...")
    content = re.sub(r' USING btree', '', content, flags=re.IGNORECASE)
    
    # 2. 移除 COLLATE pg_catalog.default
    print("  [修复] 移除 COLLATE pg_catalog.default...")
    content = re.sub(r' COLLATE `pg_catalog`\.`default`', '', content)
    content = re.sub(r' COLLATE "pg_catalog"\."default"', '', content)
    
    # 3. 移除 text_ops 和其他 PostgreSQL 操作符类
    print("  [修复] 移除 PostgreSQL 操作符类...")
    content = re.sub(r' `pg_catalog`\.`\w+_ops`', '', content)
    content = re.sub(r' "pg_catalog"\."(\w+_ops)"', '', content)
    
    # 4. 移除 ASC NULLS LAST / DESC NULLS FIRST
    print("  [修复] 移除 PostgreSQL 排序语法...")
    content = re.sub(r' ASC NULLS LAST', ' ASC', content, flags=re.IGNORECASE)
    content = re.sub(r' DESC NULLS FIRST', ' DESC', content, flags=re.IGNORECASE)
    content = re.sub(r' NULLS LAST', '', content, flags=re.IGNORECASE)
    content = re.sub(r' NULLS FIRST', '', content, flags=re.IGNORECASE)
    
    # 5. 修复 CREATE INDEX 语法 - 移除多行的括号
    print("  [修复] 修复 CREATE INDEX 语法...")
    # 将多行的 CREATE INDEX 转换为单行
    content = re.sub(
        r'CREATE INDEX `([^`]+)` ON `([^`]+)` \(\s*`([^`]+)`[^\)]*\)',
        r'CREATE INDEX `\1` ON `\2` (`\3`)',
        content,
        flags=re.DOTALL
    )
    
    # 6. 移除 PRIMARY KEY 中的多余括号和 PostgreSQL 语法
    print("  [修复] 修复 PRIMARY KEY 语法...")
    content = re.sub(
        r'ALTER TABLE `([^`]+)` ADD CONSTRAINT `([^`]+)` PRIMARY KEY \(\s*`([^`]+)`\s*\)',
        r'ALTER TABLE `\1` ADD CONSTRAINT `\2` PRIMARY KEY (`\3`)',
        content
    )
    
    # 7. 移除空行和多余的空格
    print("  [修复] 清理空行和多余空格...")
    lines = content.split('\n')
    cleaned_lines = []
    for line in lines:
        # 移除行尾空格
        line = line.rstrip()
        # 保留非空行
        if line or (cleaned_lines and cleaned_lines[-1]):  # 保留单个空行
            cleaned_lines.append(line)
    content = '\n'.join(cleaned_lines)
    
    print("[成功] 修复规则应用完成")
    
    # 写入文件
    print(f"[开始] 写入修复后的文件: {mysql_file}")
    try:
        with open(mysql_file, 'w', encoding='utf-8') as f:
            f.write(content)
    except Exception as e:
        print(f"[错误] 无法写入文件: {e}")
        return False
    
    print("[成功] 文件已保存")
    print(f"[统计] 最终文件大小: {len(content) / 1024 / 1024:.2f} MB")
    
    return True


def main():
    """主函数"""
    mysql_file = r"g:\back\recovered\ruoyi-cpp\src\mysql\mysql.sql"
    
    print("=" * 60)
    print("MySQL SQL 文件修复工具")
    print("修复 PostgreSQL 特定语法")
    print("=" * 60)
    print()
    
    success = fix_mysql_sql(mysql_file)
    
    print()
    if success:
        print("[完成] ✅ 修复成功！")
        print(f"输出文件: {mysql_file}")
        return 0
    else:
        print("[完成] ❌ 修复失败！")
        return 1


if __name__ == '__main__':
    sys.exit(main())
