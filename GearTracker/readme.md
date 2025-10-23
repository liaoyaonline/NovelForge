
# GearTracker - 库存管理系统

## 项目概述
GearTracker 是一个基于C++和MySQL的库存管理系统，提供命令行和Web界面两种操作方式。系统支持物品管理、库存操作、日志记录等功能，适合仓库管理、物品追踪等场景使用。

## 功能特点
- **物品管理**
  - 添加新物品到物品列表
  - 查看物品详细信息
  - 修改物品属性
- **库存管理**
  - 添加物品到库存
  - 查看库存物品（分页显示）
  - 更新库存数量及位置
  - 删除库存物品
- **操作日志**
  - 记录所有关键操作（添加、修改、删除）
  - 支持分页查看操作记录
  - 可按类型、物品名、备注搜索
- **配置管理**
  - 数据库连接配置（主机、端口、凭据）
  - 应用配置（日志级别、分页设置）
  - 配置热重载
- **Web界面**
  - 响应式库存列表展示
  - 实时操作日志面板
  - 数据库连接状态监控

## 文件结构
```
.
├── build/                 # 构建目录
├── CMakeLists.txt         # CMake构建文件
├── config/
│   └── config.ini         # 配置文件
├── include/               # 头文件
│   ├── Config.h           # 配置管理
│   ├── Database.h         # 数据库操作
│   ├── httplib.h          # HTTP服务器库
│   └── WebServer.h        # Web服务器
├── src/                   # 源文件
│   ├── Config.cpp         # 配置实现
│   ├── Database.cpp       # 数据库实现
│   ├── main.cpp           # 主程序入口
│   └── WebServer.cpp      # Web服务器实现
└── web/                   # Web前端
    ├── css/
    │   └── styles.css     # 样式表
    ├── index.html         # 主页面
    └── js/
        └── main.js        # 前端逻辑
```

## 依赖项
- **编译依赖**
  - CMake (>= 3.10)
  - C++17 兼容编译器
  - MySQL Connector/C++
  - JSON库 (jsoncpp)
- **运行依赖**
  - MySQL服务器
  - 系统库：libssl, libcrypto, pthread

## 编译指南

### 前提条件
```bash
sudo apt update
sudo apt install -y cmake g++ libmysqlcppconn-dev libssl-dev libjsoncpp-dev
```

### 编译步骤
1. 创建构建目录：
   ```bash
   mkdir build && cd build
   ```

2. 配置项目：
   ```bash
   cmake ..
   ```

3. 编译项目：
   ```bash
   make
   ```

4. 编译完成后，可执行文件 `geartracker` 位于 `build` 目录下

### 调试模式
```bash
make run_debug  # 使用GDB调试运行
```

## 配置与运行

### 配置文件
`config/config.ini` 示例：
```ini
[database]
host = 192.168.1.7
port = 3306
username = vm_liaoya
password = 123
database = geartracker

[application]
log_level = info
page_size = 10
log_file = geartracker.log
```

### 运行程序
```bash
./geartracker
```

### 访问Web界面
程序启动后，访问：  
http://localhost:8080

## 使用说明

### 命令行界面
```
==== 仓库管理系统 ====
1. 添加新物品到物品列表
2. 添加物品到库存
3. 显示库存物品
4. 显示操作记录
5. 配置管理
6. 退出
```

### Web界面功能
- **库存管理**
  - 分页查看库存物品
  - 搜索物品名称/位置
  - 编辑库存数量/位置
  - 删除库存项目
- **操作日志**
  - 按操作类型筛选
  - 时间排序
  - 关键词搜索
- **状态监控**
  - 实时显示数据库连接状态

## 注意事项
1. 首次运行会自动创建默认配置文件
2. 确保MySQL服务器已启动且配置正确
3. Web界面需要现代浏览器支持
4. 修改配置后可通过"重新加载配置"选项生效

## 项目结构说明
| 文件 | 功能描述 |
|------|----------|
| `Config.h/cpp` | 配置文件解析与管理 |
| `Database.h/cpp` | MySQL数据库操作封装 |
| `WebServer.h/cpp` | HTTP服务器实现 |
| `main.cpp` | 程序入口和主循环 |
| `index.html` | Web界面主框架 |
| `styles.css` | Web界面样式 |
| `main.js` | 前端交互逻辑 |
| `CMakeLists.txt` | 构建系统配置文件 |

## 开发与贡献
欢迎贡献代码，提交issue或改进建议。请确保：
1. 遵循现有代码风格
2. 提交前通过测试
3. 更新相关文档