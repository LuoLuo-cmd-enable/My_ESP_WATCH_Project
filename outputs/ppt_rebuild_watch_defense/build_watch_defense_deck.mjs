import fs from "node:fs/promises";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

import {
  createSlideContext,
  ensureArtifactToolWorkspace,
  importArtifactTool,
  saveBlobToFile,
} from "file:///C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/artifact_tool_utils.mjs";

process.env.HOME = process.env.HOME || "C:/Users/86177";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const WORKSPACE = __dirname;
const PREVIEW_DIR = path.join(WORKSPACE, "preview");
const OUTPUT_DIR = path.join(WORKSPACE, "output");
const LAYOUT_DIR = path.join(WORKSPACE, "layout");
const FINAL = path.join(OUTPUT_DIR, "ESP32S3低功耗触控智能手表系统答辩汇报_重做版.pptx");
const DOCS_COPY = path.resolve("C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2/docs/ESP32S3低功耗触控智能手表系统答辩汇报_重做版.pptx");
const CONTACT = path.join(PREVIEW_DIR, "contact_sheet.png");
const PYTHON = "C:/Users/86177/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";
const MAKE_CONTACT = "C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/make_contact_sheet.py";

const W = 1280;
const H = 720;
const C = {
  navy: "#0A2A4A",
  blue: "#0B62A8",
  sky: "#EAF5FB",
  pale: "#F6FBFF",
  line: "#CFE3F2",
  text: "#1F2D3D",
  sub: "#5E7387",
  orange: "#F59E0B",
  green: "#18A66A",
  red: "#E25555",
  cyan: "#20A8C7",
  white: "#FFFFFF",
};

function ctxFor(artifact, slideNumber) {
  return createSlideContext(artifact, {
    slideSize: { width: W, height: H },
    slideNumber,
    workspaceDir: WORKSPACE,
    outputDir: OUTPUT_DIR,
    titleFont: "Microsoft YaHei",
    bodyFont: "Microsoft YaHei",
  });
}

function rect(ctx, slide, left, top, width, height, fill, line = "transparent") {
  return ctx.addShape(slide, {
    left,
    top,
    width,
    height,
    fill,
    line: ctx.line(line, line === "transparent" ? 0 : 1),
  });
}

function txt(ctx, slide, text, left, top, width, height, opt = {}) {
  return ctx.addText(slide, {
    text,
    left,
    top,
    width,
    height,
    fontSize: opt.size ?? 26,
    color: opt.color ?? C.text,
    bold: opt.bold ?? false,
    align: opt.align ?? "left",
    valign: opt.valign ?? "top",
    typeface: "Microsoft YaHei",
    insets: opt.insets ?? { left: 8, right: 8, top: 4, bottom: 4 },
    fill: opt.fill ?? "#00000000",
    line: opt.line ?? { style: "solid", fill: "#00000000", width: 0 },
  });
}

function card(ctx, slide, left, top, width, height, title, body, accent = C.blue) {
  rect(ctx, slide, left, top, width, height, C.white, C.line);
  rect(ctx, slide, left, top + 18, 7, 42, accent);
  txt(ctx, slide, title, left + 22, top + 18, width - 38, 36, { size: 24, color: accent, bold: true });
  txt(ctx, slide, body, left + 22, top + 66, width - 40, height - 76, { size: 18, color: C.text });
}

function bulletList(ctx, slide, items, left, top, width, lineH = 36, size = 21, color = C.text) {
  items.forEach((item, i) => {
    rect(ctx, slide, left, top + i * lineH + 9, 7, 7, C.blue);
    txt(ctx, slide, item, left + 20, top + i * lineH, width - 20, lineH + 4, { size, color });
  });
}

function chrome(ctx, slide, chapter = "", page = "") {
  rect(ctx, slide, 0, 0, W, H, C.pale);
  rect(ctx, slide, 0, 630, W, 90, C.sky);
  for (let i = 0; i < 12; i += 1) {
    rect(ctx, slide, 34 + i * 54, 650 + (i % 3) * 14, 34, 1.5, "#B7D7EA");
  }
  if (chapter) txt(ctx, slide, chapter, 42, 672, 720, 24, { size: 13, color: C.sub });
  if (page) txt(ctx, slide, page, 1194, 672, 42, 24, { size: 13, color: C.sub, align: "right" });
}

function title(ctx, slide, no, main, sub = "") {
  txt(ctx, slide, no, 52, 40, 70, 24, { size: 16, color: C.text, bold: true });
  txt(ctx, slide, main, 52, 65, 650, 46, { size: 29, color: C.text, bold: true });
  if (sub) txt(ctx, slide, sub, 52, 112, 840, 28, { size: 15, color: C.sub });
  rect(ctx, slide, 52, 145, 240, 4, C.blue);
}

function step(ctx, slide, x, y, w, n, label, note, color = C.blue) {
  rect(ctx, slide, x, y, w, 70, C.white, C.line);
  rect(ctx, slide, x, y, w, 24, color);
  txt(ctx, slide, n, x, y + 2, w, 20, { size: 13, color: C.white, bold: true, align: "center" });
  txt(ctx, slide, label, x + 6, y + 29, w - 12, 23, { size: 17, color: C.text, bold: true, align: "center" });
  txt(ctx, slide, note, x + 6, y + 53, w - 12, 20, { size: 12, color: C.sub, align: "center" });
}

async function slide01(p, ctx) {
  const slide = p.slides.add();
  rect(ctx, slide, 0, 0, W, H, C.navy);
  rect(ctx, slide, 0, 510, W, 210, "#0D6F84");
  for (let i = 0; i < 22; i += 1) {
    const y = 150 + i * 14;
    rect(ctx, slide, 520 + i * 15, y, 420, 1.2, "#2A87A5");
    rect(ctx, slide, 755 + i * 8, y + 42, 300, 1.2, "#2A87A5");
  }
  txt(ctx, slide, "基于ESP32-S3R8与LVGL的", 70, 145, 720, 48, { size: 31, color: C.white, bold: true });
  txt(ctx, slide, "低功耗触控智能手表系统设计与实现", 70, 195, 870, 60, { size: 37, color: C.white, bold: true });
  rect(ctx, slide, 70, 272, 352, 5, C.cyan);
  txt(ctx, slide, "毕业设计答辩汇报", 72, 310, 300, 32, { size: 21, color: "#CBEAF5" });
  txt(ctx, slide, "硬件驱动 · 图形交互 · 多媒体应用 · 云端协同 · 低功耗管理", 70, 620, 850, 28, { size: 18, color: "#D8F4FA" });
  return slide;
}

async function slide02(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "CONTENTS", "02");
  txt(ctx, slide, "目录", 560, 38, 160, 42, { size: 30, color: C.text, bold: true, align: "center" });
  txt(ctx, slide, "CONTENTS", 583, 88, 100, 18, { size: 10, color: C.blue, align: "center" });
  const items = [
    ["01", "绪论：研究背景与意义", "可穿戴设备发展、问题提出与课题价值"],
    ["02", "总体需求与系统架构", "功能需求、性能指标与总体实现链路"],
    ["03", "硬件设计", "主控最小系统、显示触控、RTC、存储与电源"],
    ["04", "软件设计", "分层软件结构、多任务调度与核心功能流程"],
    ["05", "系统调试与测试", "交互、多媒体、网络、OTA与功耗测试"],
    ["06", "总结与展望", "研究结论、存在不足与后续优化方向"],
  ];
  items.forEach(([n, a, b], i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = 270 + col * 370;
    const y = 128 + row * 126;
    rect(ctx, slide, x, y, 330, 92, C.white, C.line);
    txt(ctx, slide, n, x + 24, y + 28, 54, 34, { size: 26, color: C.blue, bold: true, align: "center" });
    txt(ctx, slide, a, x + 96, y + 18, 210, 32, { size: 19, color: C.text, bold: true });
    txt(ctx, slide, b, x + 96, y + 54, 210, 28, { size: 12, color: C.sub });
  });
  return slide;
}

async function slide03(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "01 绪论", "03");
  title(ctx, slide, "01", "研究背景与课题意义", "从智能手表发展现状出发，明确低成本、高交互、可联网的嵌入式穿戴终端需求");
  const cards = [
    ["发展背景", "物联网与微电子技术推动可穿戴设备从计时工具转向个人智能终端，智能手表承担信息交互、健康监测、运动提醒和云端同步等任务。"],
    ["现实矛盾", "低功耗手环交互与多媒体能力有限；高端智能手表性能强但成本高、系统复杂，难以作为低成本嵌入式教学与工程方案。"],
    ["研究目标", "在ESP32-S3R8微控制器平台上，结合LVGL与FreeRTOS，实现图形界面、多媒体读取、网络协同、OTA升级和低功耗管理。"],
  ];
  cards.forEach((c, i) => card(ctx, slide, 76 + i * 392, 195, 330, 292, c[0], c[1], [C.blue, C.cyan, C.green][i]));
  return slide;
}

async function slide04(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "02 需求分析", "04");
  title(ctx, slide, "02", "功能需求与性能指标", "先明确系统要完成什么，再确定交互流畅性、通信稳定性和续航控制指标");
  const req = [
    ["时间天气", "RTC守时 / NTP校时 / 天气获取"],
    ["图形交互", "LVGL界面 / 触控手势 / 菜单切换"],
    ["多媒体", "SD卡 / 图片 / TXT / MJPEG"],
    ["云端协同", "WiFi / MQTT / OneNET OTA"],
    ["电源维护", "休眠控制 / 本地与云端升级"],
  ];
  req.forEach(([a, b], i) => {
    const x = 74 + i * 232;
    rect(ctx, slide, x, 186, 190, 108, C.white, C.line);
    txt(ctx, slide, String(i + 1).padStart(2, "0"), x + 12, 202, 50, 26, { size: 18, color: C.blue, bold: true, align: "center" });
    txt(ctx, slide, a, x + 76, 198, 92, 32, { size: 18, color: C.text, bold: true });
    txt(ctx, slide, b, x + 18, 238, 156, 36, { size: 13, color: C.sub, align: "center" });
  });
  rect(ctx, slide, 96, 362, 1088, 1.5, C.line);
  const metrics = [
    ["视频播放", "约25fps", "多帧缓冲与异步刷新"],
    ["触控响应", "≤50ms", "I2C读取与LVGL事件分发"],
    ["休眠触发", "120s / 3s", "无操作轻休眠 / 长按深休眠"],
    ["网络通信", "自动重连", "WiFi、MQTT、天气与OTA协同"],
  ];
  metrics.forEach(([a, b, c], i) => {
    const x = 120 + i * 268;
    rect(ctx, slide, x, 388, 210, 128, C.white, C.line);
    txt(ctx, slide, b, x + 12, 410, 186, 34, { size: 28, color: [C.blue, C.cyan, C.orange, C.green][i], bold: true, align: "center" });
    txt(ctx, slide, a, x + 12, 453, 186, 24, { size: 18, color: C.text, bold: true, align: "center" });
    txt(ctx, slide, c, x + 18, 482, 174, 30, { size: 12, color: C.sub, align: "center" });
  });
  return slide;
}

async function slide05(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "02 总体架构", "05");
  title(ctx, slide, "02", "总体实现链路：从输入到显示再到维护", "整机设计围绕硬件基础、应用业务、显示刷新、网络维护和能耗控制形成闭环");
  const xs = [70, 287, 504, 721, 938];
  const labels = [
    ["硬件基础", "ESP32-S3R8\nLCD/触控\nSD/RTC/电源"],
    ["驱动抽象", "SPI / I2C / GPIO\n文件系统\n外设休眠"],
    ["应用业务", "时间天气\n图片视频小说\n游戏与设置"],
    ["显示交互", "LVGL页面\n消息队列刷新\n触控事件响应"],
    ["维护闭环", "WiFi/MQTT\n本地/云端OTA\n低功耗唤醒"],
  ];
  labels.forEach(([a, b], i) => {
    rect(ctx, slide, xs[i], 226, 175, 150, C.white, C.line);
    txt(ctx, slide, a, xs[i] + 10, 246, 155, 30, { size: 23, color: [C.blue, C.cyan, C.orange, C.green, C.red][i], bold: true, align: "center" });
    txt(ctx, slide, b, xs[i] + 16, 292, 143, 68, { size: 16, color: C.text, align: "center" });
    if (i < 4) txt(ctx, slide, "→", xs[i] + 181, 275, 32, 48, { size: 36, color: C.blue, bold: true, align: "center" });
  });
  txt(ctx, slide, "论文展开顺序", 100, 446, 180, 30, { size: 22, color: C.text, bold: true });
  bulletList(ctx, slide, [
    "先完成主控、显示触控、存储、RTC和电源硬件支撑",
    "再通过FreeRTOS任务拆分解决视频、存储和网络并发问题",
    "最后用功能测试、功耗测试和OTA测试验证系统可用性",
  ], 104, 488, 950, 44, 20);
  return slide;
}

async function slide06(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "03 硬件设计", "06");
  title(ctx, slide, "03", "硬件设计一：主控最小系统与显示触控", "硬件部分先保障算力、总线和交互输入输出，为后续图形与多媒体处理提供基础");
  card(ctx, slide, 78, 178, 346, 310, "ESP32-S3R8最小系统", "采用芯片级ESP32-S3R8，配合外部16MB Flash、40MHz晶振、复位与下载控制电路。双核240MHz处理器和片内8MB PSRAM用于图形缓存、多媒体帧数据与业务任务运行。", C.blue);
  card(ctx, slide, 468, 178, 346, 310, "LCD显示接口", "1.83英寸240×284全彩屏，ST7789P3显示驱动采用SPI接口连接主控；背光通过PWM调节，显示数据通过SPI-DMA方式搬运，降低CPU占用。", C.cyan);
  card(ctx, slide, 858, 178, 346, 310, "电容触控接口", "CST816T触控芯片通过I2C读取坐标，并利用中断引脚上报触摸事件。待机时保持低功耗扫描，触摸动作可作为系统唤醒来源。", C.green);
  return slide;
}

async function slide07(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "03 硬件设计", "07");
  title(ctx, slide, "03", "硬件设计二：时间、存储与电源路径", "围绕可穿戴设备的长期使用，补齐断电守时、大容量数据和稳定供电能力");
  const parts = [
    ["RTC时间保持", "SD3078独立RTC\nI2C通信\n断电/断网守时", C.blue],
    ["Micro SD存储", "SPI模式挂载FatFS\n保存小说、图片、视频\n支持本地OTA固件", C.cyan],
    ["外部Flash", "W25Q128 16MB\n保存程序与静态资源\n扩展非易失性空间", C.orange],
    ["电源管理", "锂电池充电\nME6217稳压3.3V\n背光与外设分级控制", C.green],
  ];
  parts.forEach(([a, b, color], i) => {
    const x = 92 + i * 285;
    rect(ctx, slide, x, 198, 230, 202, C.white, C.line);
    rect(ctx, slide, x, 198, 230, 46, color);
    txt(ctx, slide, a, x + 10, 207, 210, 28, { size: 21, color: C.white, bold: true, align: "center" });
    txt(ctx, slide, b, x + 22, 270, 186, 92, { size: 18, color: C.text, align: "center" });
  });
  txt(ctx, slide, "硬件设计重点", 108, 470, 160, 28, { size: 22, color: C.text, bold: true });
  bulletList(ctx, slide, [
    "所有高速链路均围绕供电滤波、信号完整性和接口可靠性设计",
    "显示、存储、网络与电源模块相互配合，支撑小体积手表的完整功能",
  ], 108, 512, 980, 46, 20);
  return slide;
}

async function slide08(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "04 软件设计", "08");
  title(ctx, slide, "04", "软件设计一：分层结构与任务调度", "软件采用硬件层、应用层、显示层分工，利用消息队列降低业务逻辑与UI刷新耦合");
  const layers = [
    ["硬件层", "LCD显示、触摸、SD卡、Flash、RTC、WiFi、电源与唤醒", C.blue],
    ["应用层", "时间天气、小说阅读、图片浏览、视频播放、游戏、OTA、功耗控制", C.orange],
    ["异步消息队列", "业务状态统一上报，避免多个任务直接操作UI", C.cyan],
    ["显示层", "LVGL页面路由、控件刷新、触控事件分发、动画与状态呈现", C.green],
  ];
  layers.forEach(([a, b, color], i) => {
    const y = 168 + i * 104;
    const x = i === 2 ? 322 : 180;
    const w = i === 2 ? 635 : 920;
    rect(ctx, slide, x, y, w, 72, C.white, C.line);
    rect(ctx, slide, x, y, 10, 72, color);
    txt(ctx, slide, a, x + 28, y + 18, 150, 30, { size: 25, color, bold: true });
    txt(ctx, slide, b, x + 190, y + 19, w - 220, 30, { size: 20, color: C.text });
    if (i < 3) txt(ctx, slide, "↓", 615, y + 74, 40, 28, { size: 27, color: C.blue, bold: true, align: "center" });
  });
  return slide;
}

async function slide09(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "04 软件设计", "09");
  title(ctx, slide, "04", "软件设计二：核心功能流程", "按系统运行时的数据流，将多媒体、时间同步、OTA和低功耗逻辑串联起来");
  const modules = [
    ["SD多媒体", "文件扫描 → 路径解析 → 分块读取 → 图片/视频/小说刷新", C.blue],
    ["时间同步", "RTC读时 → NTP校准 → 写入RTC → 表盘同步显示", C.cyan],
    ["OTA升级", "本地SD卡或OneNET通知 → 写入备份分区 → 校验 → 重启或点击跳转", C.orange],
    ["低功耗", "空闲检测/长按确认键 → 关闭外设 → 设置唤醒源 → 进入休眠", C.green],
  ];
  modules.forEach(([a, b, color], i) => {
    const x = 94 + (i % 2) * 548;
    const y = 178 + Math.floor(i / 2) * 182;
    rect(ctx, slide, x, y, 480, 126, C.white, C.line);
    rect(ctx, slide, x, y, 480, 38, color);
    txt(ctx, slide, a, x + 18, y + 7, 160, 24, { size: 21, color: C.white, bold: true });
    txt(ctx, slide, b, x + 28, y + 58, 424, 46, { size: 19, color: C.text, align: "center" });
  });
  return slide;
}

async function slide10(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "05 系统调试", "10");
  title(ctx, slide, "05", "系统调试一：主要功能测试结果", "把论文中的功能表转为答辩可读的测试列表，只保留结论和关键现象");
  const rows = [
    ["手势交互", "表盘、主菜单、快捷面板切换正常；快速滑动时偶有轻微撕裂"],
    ["图片与壁纸", "支持SD卡图片浏览和壁纸更换；大图快速翻页会受SD卡读取与解码影响"],
    ["视频播放", "MJPEG播放可连续显示，平均约25fps；高负载下允许少量掉帧"],
    ["小说阅读", "TXT分屏读取并支持断点续读，打开文件可回到上次阅读位置"],
    ["网络与OTA", "WiFi连接、天气、NTP、OneNET通信、本地/云端OTA均完成验证"],
  ];
  rows.forEach(([a, b], i) => {
    const y = 168 + i * 78;
    rect(ctx, slide, 100, y, 1080, 56, i % 2 ? C.pale : C.white, C.line);
    txt(ctx, slide, a, 126, y + 13, 150, 26, { size: 20, color: C.blue, bold: true });
    txt(ctx, slide, b, 290, y + 13, 835, 26, { size: 18, color: C.text });
  });
  return slide;
}

async function slide11(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "05 系统调试", "11");
  title(ctx, slide, "05", "系统调试二：功耗与续航测试", "亮屏电流受亮度和WiFi影响明显，双级休眠能显著降低待机电流");
  const data = [
    ["100%亮度+WiFi", 0.233, "约1.9h", C.red],
    ["10%亮度+无WiFi", 0.058, "约7.7h", C.green],
    ["LightSleep", 0.008, "约56h", C.blue],
    ["DeepSleep", 0.006, "约75h", C.cyan],
  ];
  data.forEach(([label, v, life, color], i) => {
    const y = 194 + i * 78;
    txt(ctx, slide, label, 122, y + 4, 190, 30, { size: 18, color: C.text, bold: true });
    rect(ctx, slide, 340, y + 10, 610, 22, "#E4EEF6", "#E4EEF6");
    rect(ctx, slide, 340, y + 10, 610 * (v / 0.233), 22, color, color);
    txt(ctx, slide, `${v.toFixed(3)}A`, 970, y + 4, 90, 30, { size: 18, color, bold: true });
    txt(ctx, slide, life, 1080, y + 4, 90, 30, { size: 18, color: C.sub, align: "right" });
  });
  rect(ctx, slide, 120, 530, 1030, 58, C.white, C.line);
  txt(ctx, slide, "结论", 142, 546, 70, 24, { size: 20, color: C.blue, bold: true });
  txt(ctx, slide, "亮度调节、WiFi按需启停、LightSleep短待机和DeepSleep长待机共同构成能耗管理方案，兼顾响应速度与续航。", 224, 540, 860, 44, { size: 17, color: C.text });
  return slide;
}

async function slide12(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide, "06 总结与展望", "12");
  title(ctx, slide, "06", "总结与未来展望", "本课题完成了从硬件搭建、软件实现到系统测试的完整闭环");
  card(ctx, slide, 92, 178, 500, 296, "研究总结", "完成基于ESP32-S3R8的低功耗触控智能手表原型，实现表盘显示、触摸交互、SD卡多媒体、小说阅读、小游戏、网络校时、天气获取、OneNET通信、本地与云端OTA升级以及双级休眠管理。", C.blue);
  const future = [
    ["性能优化", "继续优化SPI刷新、MJPEG解码和SD卡读取调度，减少高负载下卡顿与撕裂。"],
    ["功耗优化", "细化外设电源门控与网络唤醒策略，提高实际穿戴场景下的续航。"],
    ["功能拓展", "增加传感器、健康监测、移动端同步和更完善的云端数据管理能力。"],
  ];
  future.forEach(([a, b], i) => {
    const y = 178 + i * 98;
    rect(ctx, slide, 660, y, 470, 70, C.white, C.line);
    rect(ctx, slide, 660, y, 8, 70, [C.orange, C.green, C.cyan][i]);
    txt(ctx, slide, a, 684, y + 13, 110, 25, { size: 21, color: C.text, bold: true });
    txt(ctx, slide, b, 804, y + 12, 290, 36, { size: 15, color: C.sub });
  });
  return slide;
}

async function build() {
  await fs.mkdir(PREVIEW_DIR, { recursive: true });
  await fs.mkdir(OUTPUT_DIR, { recursive: true });
  await fs.mkdir(LAYOUT_DIR, { recursive: true });
  await ensureArtifactToolWorkspace(WORKSPACE);
  const artifact = await importArtifactTool(WORKSPACE);
  const { Presentation, PresentationFile } = artifact;
  const presentation = Presentation.create({ slideSize: { width: W, height: H } });
  const slides = [slide01, slide02, slide03, slide04, slide05, slide06, slide07, slide08, slide09, slide10, slide11, slide12];
  for (let i = 0; i < slides.length; i += 1) {
    await slides[i](presentation, ctxFor(artifact, i + 1));
  }
  const previews = [];
  for (let i = 0; i < presentation.slides.count; i += 1) {
    const slide = presentation.slides.getItem(i);
    const out = path.join(PREVIEW_DIR, `slide-${String(i + 1).padStart(2, "0")}.png`);
    const blob = await presentation.export({ slide, format: "png", scale: 1 });
    await saveBlobToFile(blob, out);
    previews.push(out);
    const layoutBlob = await presentation.export({ slide, format: "layout" });
    await fs.writeFile(path.join(LAYOUT_DIR, `slide-${String(i + 1).padStart(2, "0")}.layout.json`), await layoutBlob.text(), "utf8");
  }
  const result = spawnSync(PYTHON, [MAKE_CONTACT, "--output", CONTACT, ...previews], { encoding: "utf8" });
  if (result.status !== 0) {
    throw new Error(`contact sheet failed\n${result.stdout}\n${result.stderr}`);
  }
  const pptx = await PresentationFile.exportPptx(presentation);
  await pptx.save(FINAL);
  await fs.copyFile(FINAL, DOCS_COPY);
  console.log(JSON.stringify({ FINAL, DOCS_COPY, CONTACT, slideCount: presentation.slides.count }, null, 2));
}

build().catch((error) => {
  console.error(error.stack || error.message || String(error));
  process.exit(1);
});
