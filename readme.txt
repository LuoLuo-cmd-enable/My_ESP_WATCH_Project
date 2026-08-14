================================================================================
智能手表工程说明（ESP32-S3）
================================================================================

一、工程概述
--------------------------------------------------------------------------------
基于 ESP32-S3R8（QFN-56，8MB 内置 PSRAM + 模组外挂 16MB SPI Flash）的智能手表。
使用 ESP-IDF v5.4.4 + LVGL 8.3，FreeRTOS 多任务架构。

硬件要点：
  - 屏幕：ST7789 240x284（SPI，DIO 80MHz）
  - 触摸：CST816T（I2C）
  - Flash：16MB（外部 SPI），分区表见 partitions.csv
  - PSRAM：8MB（芯片内置，图片/动画缓冲用）
  - 按键：3 个 GPIO（KEY1/KEY2/KEY3），KEY3 长按 3s 深睡

二、目录结构
--------------------------------------------------------------------------------
smart_watch/
├── CMakeLists.txt            # 工程入口，定义版本 PROJECT_VER
├── partitions.csv            # 分区表（双 OTA：ota_0/ota_1 各 3MB）
├── sdkconfig                 # menuconfig 配置（不要提交 git）
├── main/                     # 应用层
│   ├── lvgl_display.c        # LVGL 主任务、消息队列、全局 UI 入口
│   ├── lv_port.c             # 显示驱动适配（40 行内部 DMA 双缓冲 + PSRAM bounce）
│   └── storage_worker.c      # SD 卡文件读取后台任务（小说/图片/视频）
└── components/
    ├── GUI/                  # GUI Guider 生成 + 定制
    │   ├── generated/        # gui_guider(切屏核心)、setup_scr_*、events_init
    │   ├── custom/           # 自定义：滑动菜单、渐变卡片按钮
    │   ├── game/             # 游戏（2048/记忆/贪吃蛇/Flappy）
    │   └── jpeg/             # JPEG 解码
    ├── bsp/                  # 板级驱动
    │   ├── st7789_driver.c   # 屏幕驱动
    │   ├── cst816t_driver.c  # 触摸驱动
    │   ├── power_sleep.c     # 浅睡/深睡电源管理
    │   ├── My_timer.c        # 按键扫描定时器（长按 3s 深睡）
    │   ├── ntp_time.c        # 网络校时
    │   └── ...               # SD/WiFi/天气/RTC 等
    ├── ota/                  # OTA 升级
    │   ├── local_ota.c       # 本地 SD 卡 OTA
    │   └── onenet_ota.c      # OneNET 云端 OTA
    ├── My_image/             # 菜单图标素材（ARGB）
    ├── lvgl/                 # LVGL 8.3 源码
    └── ...                   # 其他功能组件

三、代码架构（分层详解）
--------------------------------------------------------------------------------
本工程采用「四层架构」，从下到上：

  ┌─────────────────────────────────────────────┐
  │ ① 应用层  main/                              |
  │  lvgl_display.c / lv_port.c / storage_worker│
  ├─────────────────────────────────────────────┤
  │ ② 功能组件层  components/{ota,bsp,GUI,custom}│
  ├─────────────────────────────────────────────┤
  │ ③ 框架层  LVGL 8.3 + ESP-IDF + FreeRTOS     │
  ├─────────────────────────────────────────────┤
  │ ④ 硬件层  驱动(st7789/cst816t/SD/I2C/SPI)    │
  └─────────────────────────────────────────────┘

【3.1 应用层（main/）】
  - lvgl_display.c
      * 进程入口 app_main()：初始化外设 → 创建各任务
      * LVGL 渲染主循环 lvgl_diaplay_task（core1，优先级 6）
      * 全局 UI 结构体 lv_ui guider_ui（所有屏幕控件指针）
      * 消息队列 lvgl_msg_queue：跨任务 UI 操作统一走这里
      * 处理 WiFi/天气/OTA/小说/视频等各类 LVGL_MSG_*
  - lv_port.c
      * lv_disp_drv 适配：40 行内部 DMA 双缓冲 + PSRAM bounce
      * 决定 draw buffer 使用内部 RAM 还是 PSRAM（花屏防护）
  - storage_worker.c
      * storage_worker_init() 创建后台任务（栈 12288）
      * 命令队列 s_cmd_queue（12 深度）接收请求
      * 处理：小说列表/翻页/打开、图片列表、视频列表/打开
      * 完成后通过 lvgl_msg_send_nonblocking 发 LVGL_MSG_* 通知 UI

【3.2 功能组件层（components/）】
  A. GUI 组件（components/GUI/）
     - generated/gui_guider.c：切屏核心
         * setup_ui()：启动时初始化删除标志 + 创建时钟屏
         * ui_load_scr_animation()：标准切屏（120ms 防抖 + setup_scr 重建）
         * init_scr_del_flag()：所有屏 *_del 标志初始为 true
     - generated/gui_guider.h：lv_ui 结构体（每屏控件指针 + 删除标志）
     - generated/setup_scr_*.c：每屏的控件构建 + 样式（GUI Guider 生成）
     - generated/events_init*.c：每屏的事件绑定（手势/点击/键盘）
     - custom/custom.c：自定义扩展
         * create_swipeable_menu()：横向滑动菜单
         * ui_gradient_btn_create()：渐变卡片按钮（各列表共用）
     - generated/ui_transition.c：缩放切屏动画（截图+PSRAM+Canvas）
  B. 板级驱动（components/bsp/）
     - st7789_driver.c / cst816t_driver.c：屏幕/触摸
     - power_sleep.c：浅睡/深睡状态机
     - My_timer.c：按键扫描（硬件定时器 1ms 中断，20ms 采样）
     - ntp_time.c / rtc_time_service.c：时间
  C. OTA 组件（components/ota/）：见第五章

【3.3 界面与切屏机制（重点）】
  GUI 界面全部由 GUI Guider 生成，遵循统一模式：
    setup_scr_xxx(ui)  →  创建屏对象 → events_init_xxx(ui)  →  绑定事件

  切屏调用链（以"时钟→菜单"为例）：
    手势事件（events_init.c clock_screen_event_handler）
      → ui_load_scr_with_zoom()（自定义缩放动画）
        → 截旧屏 → Canvas 缩小 → ui_load_scr_animation()（原始切屏）
          → setup_scr_menu_screen() 重建菜单屏
            → events_init_menu_screen() → create_swipeable_menu()

  关键设计：
    - ui_load_scr_animation() 有 120ms 防抖，防止快速连点重复切屏
    - 每屏 *_del 标志控制是否重建（auto_del=true 时切走即删除）
    - setup_scr 开头会清空对应指针，避免悬空判断
    - ui_transition_is_busy() 防动画重入（手势冒泡/连点）

四、任务与通信
--------------------------------------------------------------------------------
【4.1 任务表】
| 任务名             | 栈    | 优先级 | 职责                          |
|--------------------|-------|--------|-------------------------------|
| lvgl_diaplay_task  | 8192  | 6      | LVGL 渲染主循环 + 消息队列消费 |
| power_sleep_task   | 4096  | 3      | 120s 空闲浅睡 / 长按深睡判断   |
| storage_worker     | 12288 | 5      | SD 卡读写后台任务              |
| local_ota_task     | 8192  | 5      | 本地固件烧录后台任务           |
| onenet_ota_task    | 8192  | 2      | 云端固件下载后台任务           |
| quick_wifi_task    | 3072  | 4      | WiFi 开关执行（避免阻塞 LVGL） |
| sntp_*_task        | 4096  | 4/5    | 网络校时任务                   |

【4.2 跨任务通信（LVGL 消息队列）】
  规则：非 LVGL 线程禁止直接操作 UI，一律发消息到 lvgl_msg_queue，
       由 LVGL 主循环 lvgl_process_msg_queue() 统一消费。

  示例（打开小说）：
    UI 点击 → storage_worker 命令队列 → SD 读文件
    → lvgl_msg_send(LVGL_MSG_NOVEL_OPEN_READY)
    → LVGL 主循环收到 → ui_load_scr_animation 切到阅读屏 → 显示内容

  消息分类：
    - 数据就绪：NOVEL_LIST_READY / VIDEO_LIST_READY / NOVEL_PAGE_READY
    - 状态刷新：OTA_STATUS / OTA_PROGRESS / WEATHER_STATUS / NTP_SYNC_STATUS
    - 操作请求：NOVEL_OPEN_REQ / VIDEO_OPEN_REQ / OTA 相关

五、界面与交互设计
--------------------------------------------------------------------------------
1. 时钟屏（主界面）
   - 上滑 → 滑动菜单；下滑 → 快捷面板；右滑 → 弧形菜单；左滑 → 关闭弧形菜单
   - 点击时间可循环切换 6 种颜色主题

2. 滑动菜单（custom.c）
   - 横向滑动 + 缩放（zoom 300→200）+ 淡出效果
   - 5 个入口：小说 / 图片 / 视频 / 设置 / 游戏
   - 点击菜单项用缩放切屏动画

3. 列表页（小说/图片/视频/OTA 通用）
   - 统一渐变卡片按钮（ui_gradient_btn_create）
   - 8 组渐变色循环、圆角、点击变色；文件名过长省略号

4. 快捷面板：WiFi / NTP / 亮度滑条 / 天气
5. 设置页：时钟设置、WiFi 设置、OTA 升级

六、OTA 升级设计（双分区 A/B）
--------------------------------------------------------------------------------
分区表（partitions.csv）：
  nvs 16KB / otadata 8KB / phy_init 4KB / ota_0 3MB / ota_1 3MB

流程：
  1. 本地 OTA：SD 卡 /sdcard/firmware/*.bin → 写备用分区 → 点 Jump → 切 boot 分区 → 重启
  2. 云端 OTA：OneNET 平台下发 → HTTP 下载写备用分区 → 点跳转 → 重启
  3. 安全机制：
     - 只写备用分区，不碰当前运行分区（烧坏不影响系统）
     - 新固件启动后 PENDING_VERIFY 验证：正常转正，崩溃自动回滚
  4. 版本号：CMakeLists.txt 的 PROJECT_VER 编译进固件，切换分区后可见

七、低功耗设计
--------------------------------------------------------------------------------
- 空闲 120 秒 → 浅睡（light sleep）：CPU 暂停、RAM 保留，唤醒秒回
- KEY3 长按 3 秒 → 深睡（deep sleep）：RAM 掉电，唤醒 = 冷启动
- 浅睡唤醒源：KEY3 / 触摸；深睡唤醒源：KEY3（EXT0）
- 深睡 IO 保持 + 隔离，最大程度省电
- 唤醒后防误触：深睡唤醒有按键守护（防止一唤醒又深睡）

八、显示性能优化要点
--------------------------------------------------------------------------------
- 40 行内部 DMA 双缓冲 + PSRAM bounce 兜底（解决 PSRAM 直发花屏）
- 滑动淡出只对图片/文字设置不透明度，不触发 layer 渲染（避免 draw buffer 不足白框）
- 列表卡片去掉阴影、不用循环滚动文字（软渲染开销大，会卡顿）
- 缩放切屏动画用 PSRAM 截图缓冲（2 x 240x284），防重入（ui_transition_is_busy）

九、常见开发操作
--------------------------------------------------------------------------------
1. 构建：ESP-IDF Build（或 idf.py build）
2. 烧录：ESP-IDF Flash（或 idf.py flash -p COM10）
3. 查看日志：ESP-IDF Monitor
4. 配置：idf.py menuconfig
5. 固件版本：改 CMakeLists.txt 的 PROJECT_VER

十、Git 注意事项
--------------------------------------------------------------------------------
- .gitignore 已忽略 build/、sdkconfig 等编译产物，直接 git add . 不会误提交
- 提交习惯：git add . && git commit -m "说明"

================================================================================





