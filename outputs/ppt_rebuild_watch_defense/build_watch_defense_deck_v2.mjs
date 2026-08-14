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
const PREVIEW_DIR = path.join(WORKSPACE, "preview_v2");
const OUTPUT_DIR = path.join(WORKSPACE, "output");
const LAYOUT_DIR = path.join(WORKSPACE, "layout_v2");
const FINAL = path.join(OUTPUT_DIR, "ESP32低功耗触控智能手表系统答辩汇报_修改版.pptx");
const DOCS_COPY = path.resolve("C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2/docs/ESP32低功耗触控智能手表系统答辩汇报_修改版.pptx");
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
  sub: "#526B82",
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

function rect(ctx, slide, left, top, width, height, fill, line = "transparent", lineWidth = 1) {
  return ctx.addShape(slide, {
    left,
    top,
    width,
    height,
    fill,
    line: ctx.line(line, line === "transparent" ? 0 : lineWidth),
  });
}

function text(ctx, slide, value, left, top, width, height, opt = {}) {
  return ctx.addText(slide, {
    text: value,
    left,
    top,
    width,
    height,
    fontSize: opt.size ?? 24,
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

function chrome(ctx, slide) {
  rect(ctx, slide, 0, 0, W, H, C.pale);
  rect(ctx, slide, 0, 632, W, 88, C.sky);
  for (let i = 0; i < 12; i += 1) {
    rect(ctx, slide, 34 + i * 54, 654 + (i % 3) * 14, 34, 1.5, "#B7D7EA");
  }
}

function header(ctx, slide, no, title) {
  text(ctx, slide, no, 58, 34, 54, 28, { size: 17, color: C.text, bold: true });
  text(ctx, slide, title, 58, 68, 900, 48, { size: 31, color: C.text, bold: true });
  rect(ctx, slide, 58, 132, 260, 4, C.blue);
}

function smallCard(ctx, slide, x, y, w, h, title, body, color = C.blue) {
  rect(ctx, slide, x, y, w, h, C.white, C.line);
  rect(ctx, slide, x, y + 16, 8, 46, color);
  text(ctx, slide, title, x + 24, y + 18, w - 44, 34, { size: 23, color, bold: true });
  text(ctx, slide, body, x + 24, y + 68, w - 42, h - 82, { size: 17, color: C.text });
}

function metricCard(ctx, slide, x, y, w, h, value, label, note, color) {
  rect(ctx, slide, x, y, w, h, C.white, C.line);
  const valueSize = value.length > 9 ? 21 : 26;
  text(ctx, slide, value, x + 12, y + 12, w - 24, 50, { size: valueSize, color, bold: true, align: "center" });
  text(ctx, slide, label, x + 12, y + 62, w - 24, 28, { size: 18, color: C.text, bold: true, align: "center" });
  text(ctx, slide, note, x + 16, y + 96, w - 32, 44, { size: 13, color: C.sub, align: "center" });
}

function flowBox(ctx, slide, x, y, w, h, title, body, color) {
  rect(ctx, slide, x, y, w, h, C.white, C.line);
  rect(ctx, slide, x, y, w, 40, color);
  text(ctx, slide, title, x + 12, y + 7, w - 24, 26, { size: 20, color: C.white, bold: true, align: "center" });
  text(ctx, slide, body, x + 16, y + 56, w - 32, h - 66, { size: 16, color: C.text, align: "center" });
}

function arrowUp(ctx, slide, x, y) {
  text(ctx, slide, "↑", x, y, 44, 40, { size: 34, color: C.blue, bold: true, align: "center" });
}

function arrowRight(ctx, slide, x, y) {
  text(ctx, slide, "→", x, y, 34, 34, { size: 28, color: C.blue, bold: true, align: "center" });
}

async function slide01(p, ctx) {
  const slide = p.slides.add();
  rect(ctx, slide, 0, 0, W, H, C.navy);
  rect(ctx, slide, 0, 512, W, 208, "#0D6F84");
  for (let i = 0; i < 22; i += 1) {
    rect(ctx, slide, 552 + i * 14, 150 + i * 13, 408, 1.3, "#2A87A5");
    rect(ctx, slide, 730 + i * 8, 192 + i * 13, 306, 1.3, "#2A87A5");
  }
  text(ctx, slide, "基于ESP32的", 70, 128, 640, 46, { size: 34, color: C.white, bold: true });
  text(ctx, slide, "低功耗触控智能手表系统设计与实现", 70, 184, 910, 62, { size: 41, color: C.white, bold: true });
  rect(ctx, slide, 70, 268, 380, 5, C.cyan);
  text(ctx, slide, "毕业设计答辩汇报", 72, 308, 320, 34, { size: 22, color: "#CBEAF5" });
  rect(ctx, slide, 70, 390, 500, 1.5, "#9DD8E8");
  text(ctx, slide, "学生姓名", 72, 414, 120, 34, { size: 22, color: C.white });
  text(ctx, slide, "吴钟锭", 214, 414, 160, 34, { size: 24, color: C.white, bold: true });
  rect(ctx, slide, 214, 452, 340, 2, "#9DD8E8");
  text(ctx, slide, "指导教师", 72, 482, 120, 34, { size: 22, color: C.white });
  text(ctx, slide, "乔美英", 214, 482, 160, 34, { size: 24, color: C.white, bold: true });
  rect(ctx, slide, 214, 520, 340, 2, "#9DD8E8");
  text(ctx, slide, "硬件驱动 · 图形交互 · 多媒体应用 · 云端协同 · 低功耗管理", 70, 632, 820, 28, { size: 18, color: "#D8F4FA" });
  return slide;
}

async function slide02(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  text(ctx, slide, "目录", 562, 42, 160, 44, { size: 32, color: C.text, bold: true, align: "center" });
  const items = [
    ["01", "绪论：研究背景与意义", "可穿戴设备发展、问题提出与课题价值"],
    ["02", "总体需求与系统架构", "功能需求、性能指标与总体实现链路"],
    ["03", "硬件设计", "主控最小系统、显示触控、RTC、存储与电源"],
    ["04", "软件设计", "分层软件结构、多任务调度与核心功能流程"],
    ["05", "系统调试与测试", "交互、多媒体、网络、OTA与功耗测试"],
  ];
  items.forEach(([n, a, b], i) => {
    const x = i % 2 === 0 ? 110 : 665;
    const y = 128 + Math.floor(i / 2) * 124;
    const w = i === 4 ? 500 : 455;
    rect(ctx, slide, x, y, w, 88, C.white, C.line);
    text(ctx, slide, n, x + 26, y + 24, 64, 36, { size: 27, color: C.blue, bold: true, align: "center" });
    text(ctx, slide, a, x + 116, y + 28, w - 142, 32, { size: 22, color: C.text, bold: true });
  });
  return slide;
}

async function slide03(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "01", "绪论：研究背景与意义");
  smallCard(ctx, slide, 86, 188, 330, 286, "研究背景", "物联网与微电子技术推动智能手表由计时工具发展为个人智能终端，可承担信息交互、健康监测、运动提醒和云端同步等任务。", C.blue);
  smallCard(ctx, slide, 476, 188, 330, 286, "现实问题", "低功耗手环算力有限，图形交互和多媒体能力不足；高端智能手表性能强但成本高、系统复杂，不利于低成本嵌入式方案落地。", C.cyan);
  smallCard(ctx, slide, 866, 188, 330, 286, "课题意义", "基于ESP32-S3R8、FreeRTOS和LVGL实现低功耗触控手表原型，验证MCU平台在图形界面、多媒体、网络协同和OTA维护方面的可行性。", C.green);
  return slide;
}

async function slide04(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "02", "总体需求：功能需求与性能指标");
  text(ctx, slide, "功能需求", 74, 166, 120, 30, { size: 22, color: C.text, bold: true });
  const req = [
    ["时间天气", "RTC守时\nNTP校时\n天气获取"],
    ["图形交互", "LVGL界面\n触控手势\n全键盘输入"],
    ["多媒体", "SD卡资源\n图片/小说\nMJPEG视频"],
    ["云端协同", "WiFi管理\nMQTT通信\nOneNET OTA"],
    ["系统维护", "本地OTA\n云端OTA\n双级休眠"],
  ];
  req.forEach(([a, b], i) => {
    const x = 72 + i * 230;
    rect(ctx, slide, x, 210, 184, 132, C.white, C.line);
    text(ctx, slide, String(i + 1).padStart(2, "0"), x + 12, 225, 42, 30, { size: 19, color: C.blue, bold: true, align: "center" });
    text(ctx, slide, a, x + 62, 225, 100, 30, { size: 20, color: C.text, bold: true, align: "center" });
    text(ctx, slide, b, x + 18, 268, 148, 56, { size: 15, color: C.sub, align: "center" });
  });
  rect(ctx, slide, 74, 376, 1110, 1.5, C.line);
  text(ctx, slide, "性能指标", 74, 400, 120, 30, { size: 22, color: C.text, bold: true });
  const metrics = [
    ["约25fps", "视频播放", "多帧缓冲与异步刷新"],
    ["≤50ms", "触控响应", "点击、滑动及时响应"],
    ["120s / 3s", "休眠触发", "无操作轻休眠 / 长按深休眠"],
    ["≥6次", "网络重连", "弱网断连后自动重试"],
    ["备份分区", "OTA保护", "写入非活动分区并校验"],
    ["0.008A\n0.006A", "休眠电流", "LightSleep / DeepSleep测试值"],
  ];
  metrics.forEach(([v, a, b], i) => {
    const x = 86 + i * 186;
    metricCard(ctx, slide, x, 430, 158, 152, v, a, b, [C.blue, C.cyan, C.orange, C.green, C.red, C.cyan][i]);
  });
  return slide;
}

async function slide05(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "02", "系统架构：硬件支撑、任务协同与显示刷新");
  const cx = 168;
  rect(ctx, slide, cx, 468, 944, 72, C.white, C.line);
  rect(ctx, slide, cx, 468, 10, 72, C.blue);
  text(ctx, slide, "硬件层", cx + 28, 486, 120, 32, { size: 24, color: C.blue, bold: true });
  text(ctx, slide, "完成LCD、触控、SD卡、Flash、RTC、WiFi、电源等底层通信，并封装驱动函数供应用层调用", cx + 170, 487, 720, 30, { size: 18, color: C.text });
  arrowUp(ctx, slide, 616, 426);
  rect(ctx, slide, cx, 346, 944, 72, C.white, C.line);
  rect(ctx, slide, cx, 346, 10, 72, C.orange);
  text(ctx, slide, "应用层", cx + 28, 364, 120, 32, { size: 24, color: C.orange, bold: true });
  text(ctx, slide, "基于FreeRTOS划分时间、天气、存储、视频、OTA、功耗等独立任务，任务之间通过异步消息队列转发状态", cx + 170, 362, 720, 36, { size: 18, color: C.text });
  arrowUp(ctx, slide, 616, 304);
  rect(ctx, slide, 320, 232, 640, 72, C.white, C.line);
  rect(ctx, slide, 320, 232, 10, 72, C.cyan);
  text(ctx, slide, "异步消息队列", 350, 247, 170, 36, { size: 24, color: C.cyan, bold: true });
  text(ctx, slide, "应用状态改变、页面跳转、进度刷新和应用间交互均统一投递到队列", 536, 249, 390, 34, { size: 17, color: C.text });
  arrowUp(ctx, slide, 616, 190);
  rect(ctx, slide, cx, 108, 944, 72, C.white, C.line);
  rect(ctx, slide, cx, 108, 10, 72, C.green);
  text(ctx, slide, "显示层", cx + 28, 126, 120, 32, { size: 24, color: C.green, bold: true });
  text(ctx, slide, "LVGL显示线程统一消费队列消息，完成页面路由、控件刷新、触控事件分发和动画状态呈现", cx + 170, 127, 720, 30, { size: 18, color: C.text });
  return slide;
}

async function slide06(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "03", "硬件设计一：ESP32最小系统");
  smallCard(ctx, slide, 72, 176, 275, 270, "主控与存储", "采用ESP32-S3R8作为主控，内部集成8MB高速PSRAM，用于显示帧缓冲、多媒体缓存和任务运行；外扩16MB串行Flash保存程序、资源和固件数据。", C.blue);
  smallCard(ctx, slide, 376, 176, 275, 270, "时钟与下载", "配置40MHz晶振提供稳定主时钟；复位、启动配置和Type-C下载调试接口统一规划，便于固件烧录、调试和后期维护。", C.cyan);
  smallCard(ctx, slide, 680, 176, 275, 270, "射频与天线", "主控集成2.4GHz WiFi无线能力，电路预留陶瓷天线及射频匹配网络，保证网络校时、天气获取、MQTT通信和OTA升级的连接基础。", C.green);
  smallCard(ctx, slide, 984, 176, 224, 270, "供电与滤波", "3.3V电源域配合去耦与滤波网络，降低纹波和高频干扰，提升高速SPI、外部存储和无线通信稳定性。", C.orange);
  rect(ctx, slide, 112, 512, 1055, 62, C.white, C.line);
  text(ctx, slide, "设计重点", 138, 529, 120, 28, { size: 22, color: C.blue, bold: true });
  text(ctx, slide, "最小系统不是简单接线，而是围绕算力、存储带宽、下载调试、射频连接和电源稳定性构建整机硬件基础。", 274, 529, 780, 28, { size: 18, color: C.text });
  return slide;
}

async function slide07(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "03", "硬件设计二：显示触控、时间存储与电源");
  const blocks = [
    ["显示触控屏", "1.83英寸240×284一体化模组\nST7789P3负责LCD显示\nCST816T负责电容触摸检测\nSPI-DMA刷新 + I2C中断触控", C.blue],
    ["RTC与SD存储", "SD3078独立RTC断电守时\nMicro SD通过SPI挂载FatFS\n保存图片、视频、小说和本地OTA固件", C.cyan],
    ["电源与休眠", "锂电池充电模块提供安全充电\nME6217输出稳定3.3V\n背光、WiFi和外设按需启停", C.green],
  ];
  blocks.forEach(([a, b, color], i) => {
    const x = 90 + i * 390;
    rect(ctx, slide, x, 178, 320, 260, C.white, C.line);
    rect(ctx, slide, x, 178, 320, 46, color);
    text(ctx, slide, a, x + 12, 187, 296, 28, { size: 22, color: C.white, bold: true, align: "center" });
    text(ctx, slide, b, x + 28, 254, 264, 120, { size: 18, color: C.text, align: "center" });
  });
  rect(ctx, slide, 126, 498, 1028, 56, C.white, C.line);
  text(ctx, slide, "硬件链路", 148, 512, 120, 28, { size: 22, color: C.blue, bold: true });
  text(ctx, slide, "主控最小系统提供算力和通信接口，显示触控负责交互入口，RTC与SD卡保障时间和资源，电源模块支撑低功耗运行。", 284, 512, 782, 28, { size: 18, color: C.text });
  return slide;
}

async function slide08(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "04", "软件设计一：自底向上的分层结构");
  const layers = [
    ["硬件层", "驱动LCD、触摸、SD卡、Flash、RTC、WiFi和电源模块\n封装SPI、I2C、GPIO、文件系统和休眠唤醒接口", C.blue],
    ["应用层", "FreeRTOS拆分为时间、天气、存储、视频、OTA、功耗等任务\n各任务彼此独立，状态变化和任务间交互通过异步消息队列转发", C.orange],
    ["异步消息队列", "统一承接业务状态、页面请求、进度信息和应用间控制消息", C.cyan],
    ["显示层", "LVGL显示线程读取队列消息，完成页面刷新、控件更新和事件分发", C.green],
  ];
  layers.forEach(([a, b, color], i) => {
    const y = 456 - i * 108;
    const x = i === 2 ? 288 : 160;
    const w = i === 2 ? 704 : 960;
    rect(ctx, slide, x, y, w, 78, C.white, C.line);
    rect(ctx, slide, x, y, 10, 78, color);
    text(ctx, slide, a, x + 26, y + 20, 160, 34, { size: 25, color, bold: true });
    text(ctx, slide, b, x + 210, y + 15, w - 240, 48, { size: 18, color: C.text });
    if (i < 3) rect(ctx, slide, 638, y - 30, 3, 28, C.blue, C.blue);
  });
  return slide;
}

async function slide09(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "04", "软件设计二：核心应用流程");
  const modules = [
    ["多媒体资源", "SD卡扫描\n路径解析\n分块读取\n图片/视频/小说显示", C.blue],
    ["时间同步", "RTC读时\nSNTP校准\n写入SD3078\n表盘同步显示", C.cyan],
    ["OTA升级", "本地或云端输入\n写入备份分区\n固件校验\n重启或点击跳转", C.orange],
    ["低功耗管理", "空闲检测/长按按键\n关闭背光与外设\n设置唤醒源\n进入休眠", C.green],
  ];
  modules.forEach(([a, b, color], i) => {
    const x = 78 + i * 300;
    flowBox(ctx, slide, x, 188, 238, 278, a, b, color);
  });
  text(ctx, slide, "队列作用：业务任务只负责产生状态和数据，显示层只负责消费消息并刷新界面，从而降低多任务并发下的UI阻塞风险。", 124, 522, 1030, 38, { size: 20, color: C.text, align: "center" });
  return slide;
}

async function slide10(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "05", "系统调试一：功能测试结果");
  const rows = [
    ["手势交互", 92, "表盘、主菜单、快捷面板切换正常；快速滑动时偶有轻微撕裂"],
    ["图片壁纸", 84, "支持SD卡图片浏览和壁纸更换；大图快速翻页存在读取延迟"],
    ["视频播放", 82, "MJPEG可连续播放，平均约25fps；高负载下会少量掉帧"],
    ["小说阅读", 94, "TXT分屏读取稳定，支持断点续读和阅读位置保存"],
    ["网络OTA", 90, "WiFi、天气、NTP、OneNET通信、本地与云端OTA完成验证"],
  ];
  rows.forEach(([name, score, note], i) => {
    const y = 166 + i * 74;
    text(ctx, slide, name, 105, y + 8, 120, 28, { size: 20, color: C.blue, bold: true });
    rect(ctx, slide, 250, y + 14, 470, 20, "#DDEBF5", "#DDEBF5");
    rect(ctx, slide, 250, y + 14, 470 * score / 100, 20, [C.green, C.cyan, C.orange, C.green, C.blue][i], [C.green, C.cyan, C.orange, C.green, C.blue][i]);
    text(ctx, slide, `${score}%`, 738, y + 5, 70, 30, { size: 20, color: C.text, bold: true });
    text(ctx, slide, note, 830, y + 4, 330, 36, { size: 16, color: C.text });
  });
  rect(ctx, slide, 104, 562, 1048, 48, C.white, C.line);
  text(ctx, slide, "测试结论", 126, 574, 120, 26, { size: 20, color: C.blue, bold: true });
  text(ctx, slide, "核心功能均能运行，主要瓶颈集中在SD卡大文件读取、MJPEG解码刷新和快速手势下的局部显示撕裂。", 260, 574, 770, 26, { size: 18, color: C.text });
  return slide;
}

async function slide11(p, ctx) {
  const slide = p.slides.add();
  chrome(ctx, slide);
  header(ctx, slide, "05", "系统调试二：功耗测试与待改进方向");
  const current = [
    ["100%亮度+WiFi", 0.233, C.red],
    ["10%亮度+无WiFi", 0.058, C.green],
    ["LightSleep", 0.008, C.blue],
    ["DeepSleep", 0.006, C.cyan],
  ];
  text(ctx, slide, "电流测试", 90, 166, 140, 30, { size: 22, color: C.text, bold: true });
  current.forEach(([label, v, color], i) => {
    const y = 208 + i * 54;
    text(ctx, slide, label, 96, y, 170, 28, { size: 17, color: C.text, bold: true });
    rect(ctx, slide, 286, y + 6, 396, 18, "#DDEBF5", "#DDEBF5");
    rect(ctx, slide, 286, y + 6, 396 * v / 0.233, 18, color, color);
    text(ctx, slide, `${v.toFixed(3)}A`, 704, y, 86, 28, { size: 17, color, bold: true });
  });
  text(ctx, slide, "理论续航", 90, 444, 140, 30, { size: 22, color: C.text, bold: true });
  const points = [
    [185, 560, "1.9h"],
    [330, 512, "7.7h"],
    [475, 468, "56h"],
    [620, 444, "75h"],
  ];
  rect(ctx, slide, 128, 560, 560, 1.5, C.line);
  rect(ctx, slide, 128, 436, 1.5, 124, C.line);
  for (let i = 0; i < points.length - 1; i += 1) {
    const [x1, y1] = points[i];
    const [x2, y2] = points[i + 1];
    const midX = Math.min(x1, x2);
    const midY = Math.min(y1, y2);
    rect(ctx, slide, midX, midY, Math.abs(x2 - x1), 3, C.blue, C.blue);
  }
  points.forEach(([x, y, t]) => {
    rect(ctx, slide, x - 5, y - 5, 10, 10, C.blue, C.blue);
    text(ctx, slide, t, x - 24, y - 34, 60, 24, { size: 15, color: C.text, align: "center" });
  });
  const todo = [
    ["不足", "高负载下仍会出现轻微卡顿、掉帧或画面撕裂；大图和视频受SD卡读取速度影响明显。"],
    ["改进一", "优化SPI刷新节奏、MJPEG帧缓冲和SD卡异步读取策略，提高多媒体连续性。"],
    ["改进二", "细化外设电源门控、WiFi按需唤醒和休眠恢复流程，进一步提高实际续航。"],
    ["改进三", "后续可增加健康传感器、移动端同步和云端数据管理能力，提高产品完整度。"],
  ];
  todo.forEach(([a, b], i) => {
    const y = 178 + i * 98;
    rect(ctx, slide, 820, y, 350, 70, C.white, C.line);
    rect(ctx, slide, 820, y, 8, 70, [C.red, C.orange, C.green, C.cyan][i]);
    text(ctx, slide, a, 844, y + 13, 88, 28, { size: 21, color: C.text, bold: true });
    text(ctx, slide, b, 938, y + 10, 208, 42, { size: 15, color: C.sub });
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
  const slides = [slide01, slide02, slide03, slide04, slide05, slide06, slide07, slide08, slide09, slide10, slide11];
  for (let i = 0; i < slides.length; i += 1) await slides[i](presentation, ctxFor(artifact, i + 1));
  const previews = [];
  for (let i = 0; i < presentation.slides.count; i += 1) {
    const slide = presentation.slides.getItem(i);
    const num = String(i + 1).padStart(2, "0");
    const preview = path.join(PREVIEW_DIR, `slide-${num}.png`);
    const png = await presentation.export({ slide, format: "png", scale: 1 });
    await saveBlobToFile(png, preview);
    previews.push(preview);
    const layout = await presentation.export({ slide, format: "layout" });
    await fs.writeFile(path.join(LAYOUT_DIR, `slide-${num}.layout.json`), await layout.text(), "utf8");
  }
  const contact = spawnSync(PYTHON, [MAKE_CONTACT, "--output", CONTACT, ...previews], { encoding: "utf8" });
  if (contact.status !== 0) throw new Error(`contact sheet failed\n${contact.stdout}\n${contact.stderr}`);
  const pptx = await PresentationFile.exportPptx(presentation);
  await pptx.save(FINAL);
  await fs.copyFile(FINAL, DOCS_COPY);
  console.log(JSON.stringify({ FINAL, DOCS_COPY, CONTACT, slideCount: presentation.slides.count }, null, 2));
}

build().catch((error) => {
  console.error(error.stack || error.message || String(error));
  process.exit(1);
});
