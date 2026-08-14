import fs from "node:fs/promises";
import path from "node:path";
import { spawnSync } from "node:child_process";

import {
  createSlideContext,
  ensureArtifactToolWorkspace,
  importArtifactTool,
  saveBlobToFile,
} from "file:///C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/artifact_tool_utils.mjs";

process.env.HOME = process.env.HOME || "C:/Users/86177";

const ROOT = "C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2";
const DOCS = path.join(ROOT, "docs");
const WORKSPACE = path.join(ROOT, "outputs/ppt_restyle_watch_defense");
const PREVIEW_DIR = path.join(WORKSPACE, "preview_hpu_v1");
const LAYOUT_DIR = path.join(WORKSPACE, "layout_hpu_v1");
const OUTPUT_DIR = path.join(WORKSPACE, "output");
const FINAL = path.join(OUTPUT_DIR, "ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版.pptx");
const DOCS_COPY = path.join(DOCS, "ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版.pptx");
const CONTACT = path.join(PREVIEW_DIR, "contact_sheet.png");
const PYTHON = "C:/Users/86177/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";
const MAKE_CONTACT = "C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/make_contact_sheet.py";

const W = 1280;
const H = 720;
const C = {
  blue: "#38548C",
  blue2: "#2F4E86",
  deep: "#223B72",
  light: "#EDF4FA",
  paper: "#F7FBFF",
  white: "#FFFFFF",
  text: "#1E2A3A",
  sub: "#4C6078",
  line: "#C9D9EA",
  cyan: "#22A7C8",
  green: "#20A66A",
  orange: "#F2A21B",
  red: "#E35D5B",
};

const IMG = {
  sdFlow: path.join(WORKSPACE, "assets/sd_flow_cropped.png"),
  timeFlow: path.join(WORKSPACE, "assets/time_flow_cropped.png"),
  otaFlow: path.join(WORKSPACE, "assets/ota_flow_cropped.png"),
  sleepFlow: path.join(WORKSPACE, "assets/sleep_flow_cropped.png"),
  arch: path.join(DOCS, "d0ad343f-7c97-44ff-8065-99fc00d90f5a.png"),
  hwMain: "C:/Users/86177/xwechat_files/wxid_3x7h02fnhw5622_fff2/temp/RWTemp/2026-05/9e20f478899dc29eb19741386f9343c8/366668911bf289d1a52d890738393e8e.png",
  hwFpc: "C:/Users/86177/xwechat_files/wxid_3x7h02fnhw5622_fff2/temp/RWTemp/2026-05/9e20f478899dc29eb19741386f9343c8/d9a5b6f1d0d439a25796c774429745dc.png",
  powerTrend: path.join(WORKSPACE, "assets/power_trend.png"),
};

function ctxFor(artifact, slideNumber) {
  return createSlideContext(artifact, {
    slideSize: { width: W, height: H },
    slideNumber,
    workspaceDir: WORKSPACE,
    outputDir: OUTPUT_DIR,
    assetDir: path.join(WORKSPACE, "assets"),
    titleFont: "Microsoft YaHei",
    bodyFont: "Microsoft YaHei",
  });
}

function rect(ctx, slide, left, top, width, height, fill, line = "#00000000", lineWidth = 0) {
  return ctx.addShape(slide, {
    left,
    top,
    width,
    height,
    fill,
    line: ctx.line(line, line === "#00000000" ? 0 : lineWidth),
  });
}

function ellipse(ctx, slide, left, top, width, height, fill, line = "#00000000", lineWidth = 0) {
  return ctx.addShape(slide, {
    geometry: "ellipse",
    left,
    top,
    width,
    height,
    fill,
    line: ctx.line(line, line === "#00000000" ? 0 : lineWidth),
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

function pageBg(ctx, slide, section = "01") {
  rect(ctx, slide, 0, 0, W, H, C.paper);
  ellipse(ctx, slide, 790, -260, 720, 340, C.blue);
  ellipse(ctx, slide, 830, -222, 600, 250, C.paper);
  rect(ctx, slide, 0, 642, W, 78, C.light);
  rect(ctx, slide, 0, 642, W, 3, "#D6E6F2");
  text(ctx, slide, section, 1150, 36, 54, 30, { size: 18, color: C.deep, bold: true, align: "right" });
}

function coverBg(ctx, slide) {
  rect(ctx, slide, 0, 0, W, H, C.blue);
  ellipse(ctx, slide, -330, -270, 480, 480, C.white);
  ellipse(ctx, slide, -280, -220, 360, 360, C.blue);
  ellipse(ctx, slide, 890, 438, 520, 520, C.white);
  ellipse(ctx, slide, 985, 530, 300, 300, C.blue);
  rect(ctx, slide, 0, 610, W, 110, C.deep);
  for (let i = 0; i < 16; i += 1) {
    rect(ctx, slide, 60 + i * 58, 646 + (i % 3) * 15, 36, 2, "#9BB3D8");
  }
}

function header(ctx, slide, section, title, subtitle = "") {
  pageBg(ctx, slide, section);
  text(ctx, slide, section, 68, 36, 54, 30, { size: 17, color: C.deep, bold: true });
  text(ctx, slide, title, 68, 68, 840, 52, { size: 34, color: C.text, bold: true });
  rect(ctx, slide, 68, 130, 250, 4, C.blue);
  if (subtitle) text(ctx, slide, subtitle, 340, 124, 650, 28, { size: 17, color: C.sub });
}

function sectionSlide(ctx, slide, n, title, subtitle) {
  coverBg(ctx, slide);
  text(ctx, slide, n.padStart(2, "0"), 120, 92, 170, 80, { size: 64, color: C.white, bold: true });
  rect(ctx, slide, 122, 190, 360, 5, C.white);
  text(ctx, slide, title, 120, 226, 760, 74, { size: 54, color: C.white, bold: true });
  text(ctx, slide, subtitle, 124, 324, 820, 76, { size: 26, color: "#E3ECFA" });
}

function card(ctx, slide, x, y, w, h, title, body, color = C.blue) {
  rect(ctx, slide, x, y, w, h, C.white, C.line, 1.2);
  rect(ctx, slide, x, y, 10, h, color);
  text(ctx, slide, title, x + 26, y + 18, w - 52, 32, { size: 24, color, bold: true });
  text(ctx, slide, body, x + 26, y + 66, w - 52, h - 82, { size: 19, color: C.text });
}

function tag(ctx, slide, x, y, label, color = C.blue) {
  rect(ctx, slide, x, y, 136, 40, color, color, 1);
  text(ctx, slide, label, x + 10, y + 7, 116, 24, { size: 18, color: C.white, bold: true, align: "center" });
}

function bulletList(ctx, slide, items, x, y, w, size = 22, color = C.text, gap = 48) {
  items.forEach((item, i) => {
    const top = y + i * gap;
    ellipse(ctx, slide, x, top + 9, 10, 10, C.blue);
    text(ctx, slide, item, x + 22, top, w - 22, gap - 2, { size, color });
  });
}

function metricCard(ctx, slide, x, y, w, h, value, label, note, color) {
  rect(ctx, slide, x, y, w, h, C.white, C.line, 1.2);
  text(ctx, slide, value, x + 10, y + 22, w - 20, 46, { size: 31, color, bold: true, align: "center" });
  text(ctx, slide, label, x + 12, y + 78, w - 24, 30, { size: 21, color: C.text, bold: true, align: "center" });
  text(ctx, slide, note, x + 18, y + 116, w - 36, 44, { size: 17, color: C.sub, align: "center" });
}

function arrow(ctx, slide, x1, y1, x2, y2, label = "") {
  if (Math.abs(x1 - x2) >= Math.abs(y1 - y2)) {
    const left = Math.min(x1, x2);
    rect(ctx, slide, left, y1 - 2, Math.abs(x2 - x1), 4, C.blue);
    text(ctx, slide, x2 > x1 ? "→" : "←", x2 > x1 ? x2 - 22 : x2 - 4, y1 - 20, 40, 34, { size: 28, color: C.blue, bold: true, align: "center" });
    if (label) text(ctx, slide, label, left + 16, y1 - 38, Math.abs(x2 - x1) - 32, 26, { size: 17, color: C.sub, align: "center" });
  } else {
    const top = Math.min(y1, y2);
    rect(ctx, slide, x1 - 2, top, 4, Math.abs(y2 - y1), C.blue);
    text(ctx, slide, y2 > y1 ? "↓" : "↑", x1 - 18, y2 > y1 ? y2 - 28 : y2 - 10, 36, 32, { size: 26, color: C.blue, bold: true, align: "center" });
    if (label) text(ctx, slide, label, x1 + 12, top + 12, 150, 28, { size: 17, color: C.sub });
  }
}

async function imageBox(ctx, slide, imagePath, x, y, w, h, title = "") {
  rect(ctx, slide, x, y, w, h, C.white, C.line, 1.2);
  await ctx.addImage(slide, { path: imagePath, left: x + 12, top: y + 12, width: w - 24, height: h - 24, fit: "contain" });
  if (title) {
    rect(ctx, slide, x, y, w, 42, "rgba(255,255,255,0.88)");
    text(ctx, slide, title, x + 14, y + 8, w - 28, 26, { size: 19, color: C.deep, bold: true, align: "center" });
  }
}

function flowNode(ctx, slide, x, y, w, h, label, color = C.blue, fill = C.white) {
  rect(ctx, slide, x, y, w, h, fill, color, 1.5);
  text(ctx, slide, label, x + 10, y + 11, w - 20, h - 18, { size: 21, color: C.text, bold: true, align: "center", valign: "mid" });
}

function smallFlowNode(ctx, slide, x, y, w, h, label, color = C.blue) {
  rect(ctx, slide, x, y, w, h, "#F3F8FD", color, 1.3);
  text(ctx, slide, label, x + 8, y + 10, w - 16, h - 16, { size: 19, color: C.text, bold: true, align: "center", valign: "mid" });
}

function svgDataUrl(svg) {
  return `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
}

function powerTrendSvg() {
  const labels = ["100%亮度+WiFi", "10%亮度+无WiFi", "LightSleep", "DeepSleep"];
  const values = [0.233, 0.058, 0.008, 0.006];
  const lives = ["约1.9h", "约7.7h", "约56h", "约75h"];
  const width = 780;
  const height = 390;
  const left = 82;
  const right = 42;
  const top = 38;
  const bottom = 92;
  const chartW = width - left - right;
  const chartH = height - top - bottom;
  const max = 0.25;
  const pts = values.map((v, i) => {
    const x = left + (chartW / (values.length - 1)) * i;
    const y = top + chartH * (1 - v / max);
    return [x, y];
  });
  const poly = pts.map(([x, y]) => `${x},${y}`).join(" ");
  const grid = [0, 0.05, 0.10, 0.15, 0.20, 0.25].map((v) => {
    const y = top + chartH * (1 - v / max);
    return `<line x1="${left}" y1="${y}" x2="${width - right}" y2="${y}" stroke="#d8e5f1" stroke-width="1"/><text x="${left - 12}" y="${y + 5}" text-anchor="end" font-size="15" fill="#4C6078">${v.toFixed(2)}A</text>`;
  }).join("");
  const nodes = pts.map(([x, y], i) => {
    const color = i === 0 ? "#E35D5B" : i === 1 ? "#20A66A" : i === 2 ? "#38548C" : "#22A7C8";
    return `<circle cx="${x}" cy="${y}" r="7" fill="${color}"/><text x="${x}" y="${y - 16}" text-anchor="middle" font-size="18" font-weight="700" fill="${color}">${values[i].toFixed(3)}A</text><text x="${x}" y="${height - 52}" text-anchor="middle" font-size="15" fill="#1E2A3A">${labels[i]}</text><text x="${x}" y="${height - 28}" text-anchor="middle" font-size="15" fill="#4C6078">${lives[i]}</text>`;
  }).join("");
  return svgDataUrl(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
  <rect width="100%" height="100%" fill="#ffffff"/>
  ${grid}
  <line x1="${left}" y1="${top}" x2="${left}" y2="${height - bottom}" stroke="#38548C" stroke-width="2"/>
  <line x1="${left}" y1="${height - bottom}" x2="${width - right}" y2="${height - bottom}" stroke="#38548C" stroke-width="2"/>
  <polyline points="${poly}" fill="none" stroke="#2F4E86" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
  ${nodes}
  <text x="${left}" y="24" font-size="18" fill="#1E2A3A" font-weight="700">不同工作状态电流趋势</text>
  <text x="${width - right}" y="24" text-anchor="end" font-size="15" fill="#4C6078">450mAh电池理论续航估算</text>
  </svg>`);
}

async function slide01(p, ctx) {
  const slide = p.slides.add();
  coverBg(ctx, slide);
  text(ctx, slide, "HENAN POLYTECHNIC UNIVERSITY", 148, 62, 520, 30, { size: 19, color: "#DDE8FA", bold: true });
  text(ctx, slide, "基于ESP32的", 150, 146, 580, 52, { size: 42, color: C.white, bold: true });
  text(ctx, slide, "低功耗触控智能手表系统设计与实现", 148, 202, 980, 76, { size: 48, color: C.white, bold: true });
  rect(ctx, slide, 152, 304, 420, 4, "#DDE8FA");
  text(ctx, slide, "毕业设计答辩汇报", 152, 338, 360, 38, { size: 26, color: "#E8F1FF", bold: true });
  text(ctx, slide, "答辩人：吴钰锟", 156, 430, 260, 34, { size: 25, color: C.white, bold: true });
  text(ctx, slide, "指导教师：乔美英", 156, 480, 300, 34, { size: 25, color: C.white, bold: true });
  text(ctx, slide, "ESP32-S3R8 / FreeRTOS / LVGL / 低功耗 / OTA", 148, 638, 760, 30, { size: 21, color: "#EAF3FF" });
  return slide;
}

async function slide02(p, ctx) {
  const slide = p.slides.add();
  rect(ctx, slide, 0, 0, W, H, C.blue);
  ellipse(ctx, slide, 235, -260, 810, 440, C.white);
  text(ctx, slide, "目录", 570, 44, 140, 42, { size: 39, color: C.blue, bold: true, align: "center" });
  text(ctx, slide, "CONTENTS", 548, 88, 184, 24, { size: 17, color: C.blue, bold: true, align: "center" });
  const items = [
    ["1", "绪论", "背景、小标题与技术路线"],
    ["2", "总体设计", "器件选型与分层架构"],
    ["3", "硬件设计", "最小系统、显示触摸与电源"],
    ["4", "软件设计", "任务划分与核心流程"],
    ["5", "系统调试", "功能、功耗与问题分析"],
    ["6", "总结", "实现内容与后续优化"],
  ];
  items.forEach(([n, t, d], i) => {
    const col = i < 3 ? 0 : 1;
    const row = i % 3;
    const x = col === 0 ? 210 : 720;
    const y = 214 + row * 122;
    ellipse(ctx, slide, x, y, 58, 58, C.white);
    text(ctx, slide, n, x + 13, y + 8, 32, 38, { size: 30, color: C.blue, bold: true, align: "center" });
    text(ctx, slide, t, x + 82, y - 1, 220, 34, { size: 29, color: C.white, bold: true });
    text(ctx, slide, d, x + 84, y + 39, 320, 28, { size: 17, color: "#DDE8FA" });
  });
  return slide;
}

async function slide03(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "01", "绪论", "从智能手表市场背景引出本课题的设计路线");
  return slide;
}

async function slide04(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "01", "研究背景：常见智能手表路线");
  card(ctx, slide, 78, 178, 500, 312, "高端品牌路线", "苹果、华为、三星等品牌通常采用高集成SoC、成熟操作系统和完整生态服务。其优势是操作体验流畅、传感器与算法完善、佩戴体验较好；不足是研发投入、物料成本和整机售价较高，不适合低成本教学样机直接复用。", C.blue);
  card(ctx, slide, 646, 178, 560, 312, "国内常用低成本路线", "低成本手表市场常采用杰理、珠海鸿芯等定制化手表芯片。芯片厂商通常同步提供已写好的手表Demo SDK，厂家通过宏定义配置、界面素材替换、表盘资源替换和局部功能微调，就能较快形成可量产产品。该路线开发周期短、投入少、落地快，但底层架构、交互方式和产品基调容易被SDK锁定，深度定制需要重新理解厂商封装逻辑，产品也更容易趋于同质化。", C.cyan);
  rect(ctx, slide, 126, 532, 1028, 58, C.white, C.line, 1.2);
  text(ctx, slide, "引出问题", 154, 547, 120, 30, { size: 22, color: C.deep, bold: true });
  text(ctx, slide, "现成SDK适合快速量产，但难以展开更深层的二次设计；本课题转向开放MCU平台，从硬件、任务调度和UI刷新机制出发验证另一种实现路径。", 286, 542, 840, 38, { size: 21, color: C.text });
  return slide;
}

async function slide05(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "01", "本课题方案：开放MCU平台下的手表原型");
  text(ctx, slide, "ESP32-S3R8 + FreeRTOS + LVGL", 104, 174, 710, 56, { size: 42, color: C.deep, bold: true });
  rect(ctx, slide, 106, 244, 468, 5, C.cyan);
  bulletList(ctx, slide, [
    "不依赖封闭手表Demo SDK，从硬件驱动、任务调度到UI刷新自行搭建",
    "用FreeRTOS拆分存储、网络、OTA、多媒体与显示任务，降低互相阻塞",
    "用LVGL和GUI Guider构建小屏交互界面，便于快速调整页面与功能",
    "保留WiFi、SD卡、RTC、OTA和低功耗接口，便于后续扩展传感器与云端功能",
  ], 112, 296, 860, 23, C.text, 56);
  metricCard(ctx, slide, 928, 174, 210, 160, "240×284", "显示分辨率", "1.83英寸触控屏", C.blue);
  metricCard(ctx, slide, 928, 356, 210, 160, "双级", "休眠策略", "轻休眠与深休眠", C.green);
  return slide;
}

async function slide06(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "01", "系统设计需求与性能指标");
  text(ctx, slide, "设计需求", 78, 166, 140, 30, { size: 25, color: C.deep, bold: true });
  const req = [
    ["基础显示", "表盘、日期、时间、壁纸切换"],
    ["触摸交互", "点击、滑动、返回、快捷面板"],
    ["本地资源", "SD卡图片、MJPEG视频、TXT小说"],
    ["联网维护", "WiFi、天气、NTP、MQTT、OTA"],
    ["低功耗", "轻休眠、深休眠、按键唤醒"],
  ];
  req.forEach(([t, b], i) => {
    const x = 74 + i * 232;
    rect(ctx, slide, x, 208, 184, 122, C.white, C.line, 1.2);
    text(ctx, slide, String(i + 1).padStart(2, "0"), x + 12, 224, 38, 26, { size: 18, color: C.blue, bold: true });
    text(ctx, slide, t, x + 50, 222, 110, 30, { size: 21, color: C.text, bold: true, align: "center" });
    text(ctx, slide, b, x + 18, 270, 148, 40, { size: 16, color: C.sub, align: "center" });
  });
  rect(ctx, slide, 78, 370, 1108, 2, C.line);
  text(ctx, slide, "性能指标", 78, 400, 140, 30, { size: 25, color: C.deep, bold: true });
  const metrics = [
    ["≤50ms", "触控响应", "点击与滑动及时反馈", C.blue],
    ["约25fps", "视频播放", "普通MJPEG连续播放", C.orange],
    ["120s", "轻休眠触发", "无操作自动进入", C.green],
    ["约3s", "深休眠触发", "长按确认键进入", C.cyan],
    ["0.008A", "轻休眠电流", "关闭背光与部分外设", C.blue],
    ["0.006A", "深休眠电流", "仅保留必要唤醒条件", C.green],
  ];
  metrics.forEach(([v, label, note, color], i) => {
    metricCard(ctx, slide, 86 + i * 186, 430, 158, 150, v, label, note, color);
  });
  return slide;
}

async function slide07(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "02", "总体设计", "器件选型、功能边界与系统分层架构");
  return slide;
}

async function slide08(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "02", "总体方案：核心器件与功能边界");
  const blocks = [
    ["主控平台", "ESP32-S3R8\n双核240MHz\n8MB PSRAM", C.blue],
    ["显示交互", "1.83英寸触控屏\nST7789P3显示\nCST816T触摸", C.cyan],
    ["存储时间", "W25Q128 Flash\nMicro SD卡\nSD3078 RTC", C.green],
    ["软件支撑", "FreeRTOS任务\nLVGL界面\nGUI Guider辅助", C.orange],
    ["联网维护", "WiFi连接\n天气/NTP/MQTT\n本地与云端OTA", C.red],
  ];
  blocks.forEach(([t, b, color], i) => {
    const x = 70 + i * 238;
    rect(ctx, slide, x, 184, 196, 248, C.white, C.line, 1.2);
    rect(ctx, slide, x, 184, 196, 52, color, color, 1);
    text(ctx, slide, t, x + 12, 198, 172, 28, { size: 22, color: C.white, bold: true, align: "center" });
    text(ctx, slide, b, x + 16, 272, 164, 120, { size: 22, color: C.text, bold: true, align: "center" });
  });
  rect(ctx, slide, 116, 506, 1040, 56, C.white, C.line, 1.2);
  text(ctx, slide, "总体边界", 142, 520, 130, 30, { size: 22, color: C.deep, bold: true });
  text(ctx, slide, "硬件提供显示、触摸、存储、时钟、网络和电源能力；软件完成任务调度、界面刷新、数据读取、网络协同与低功耗控制。", 282, 518, 820, 34, { size: 21, color: C.text });
  return slide;
}

async function slide08b(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "02", "总体架构：硬件支撑、任务协同与显示刷新");
  await imageBox(ctx, slide, IMG.arch, 52, 154, 850, 456);
  text(ctx, slide, "说明", 940, 172, 120, 32, { size: 29, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "硬件层封装显示、触摸、存储、时钟、网络和电源能力",
    "应用层按功能拆分为系统服务、OTA、多媒体、网络和存储任务",
    "应用层状态变化统一写入异步消息队列",
    "显示层消费队列消息，完成页面刷新、事件分发和屏幕输出",
  ], 944, 228, 258, 19, C.text, 72);
  return slide;
}

async function slide09(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "03", "硬件设计", "围绕最小系统、显示触摸、存储时间和电源路径展开");
  return slide;
}

async function slide10(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "03", "最小系统：ESP32-S3R8芯片级设计");
  const items = [
    ["ESP32-S3R8", "双核32位处理器，最高240MHz；片内合封8MB PSRAM，用于图形缓存、多媒体缓冲和任务运行空间。"],
    ["W25Q128 Flash", "外扩16MB串行Flash，用于程序固件、图像资源、配置参数和升级备份等非易失性存储。"],
    ["40MHz晶振", "为主控提供稳定系统时钟，同时配合复位、启动配置和下载控制电路完成可靠启动。"],
    ["射频与天线", "2.4GHz陶瓷天线与匹配网络用于WiFi链路，布线和阻抗控制影响无线稳定性。"],
  ];
  items.forEach(([t, b], i) => {
    const x = i % 2 === 0 ? 88 : 678;
    const y = 184 + Math.floor(i / 2) * 176;
    card(ctx, slide, x, y, 510, 128, t, b, [C.blue, C.green, C.cyan, C.orange][i]);
  });
  rect(ctx, slide, 150, 560, 980, 42, C.white, C.line, 1.2);
  text(ctx, slide, "设计重点：芯片级布板提高空间灵活性，同时需要重点保证电源完整性、时钟稳定性、射频匹配和高速总线信号质量。", 176, 568, 930, 26, { size: 20, color: C.text, align: "center" });
  return slide;
}

async function slide11(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "03", "显示触摸、时间与存储接口设计");
  card(ctx, slide, 78, 178, 350, 310, "显示触摸模组", "1.83英寸240×284一体化模组；显示端采用ST7789P3，通过SPI与DMA搬运提高刷新效率；触摸端采用CST816T，通过I2C与中断实现坐标读取和唤醒。", C.blue);
  card(ctx, slide, 466, 178, 350, 310, "时间与存储", "SD3078独立RTC承担断网守时、时间同步和事件记录；Micro SD卡作为图片、视频、小说和本地OTA固件的大容量外部存储。", C.green);
  card(ctx, slide, 854, 178, 350, 310, "下载调试接口", "Type-C接口统一承担供电、固件下载和调试通信；复位与启动配置电路配合完成程序烧录和异常恢复。", C.cyan);
  rect(ctx, slide, 136, 544, 1008, 46, C.white, C.line, 1.2);
  text(ctx, slide, "硬件链路：SPI负责高吞吐显示和存储，I2C负责触摸与RTC等低速外设，GPIO负责复位、背光、卡检测和休眠唤醒。", 160, 554, 960, 25, { size: 20, color: C.text, align: "center" });
  return slide;
}

async function slide12(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "03", "电源路径：充电、稳压与休眠唤醒");
  metricCard(ctx, slide, 100, 180, 240, 160, "3.0-4.2V", "锂电池输入", "单节锂电池随电量变化", C.blue);
  metricCard(ctx, slide, 380, 180, 240, 160, "3.3V", "系统供电", "ME6217低压差稳压输出", C.green);
  metricCard(ctx, slide, 660, 180, 240, 160, "5V", "Type-C充电", "充电状态硬件指示", C.orange);
  metricCard(ctx, slide, 940, 180, 240, 160, "GPIO7", "深休眠唤醒", "实体按键防误触唤醒", C.cyan);
  bulletList(ctx, slide, [
    "输入与输出端配置滤波和去耦电容，降低纹波与瞬态干扰",
    "充电芯片提供过流、过压和温度保护，提高电池安全性",
    "LightSleep保留快速唤醒能力，DeepSleep关闭更多资源以降低待机功耗",
  ], 150, 410, 930, 23, C.text, 56);
  return slide;
}

async function slide12b(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "03", "硬件实物：主板器件与屏幕连接");
  await imageBox(ctx, slide, IMG.hwMain, 74, 154, 520, 412, "主板正面与主要器件");
  await imageBox(ctx, slide, IMG.hwFpc, 664, 154, 520, 412, "触摸屏FPC连接结构");
  rect(ctx, slide, 124, 584, 1032, 48, C.white, C.line, 1.2);
  text(ctx, slide, "实物调试重点：确认Type-C供电与下载、Micro SD卡座、SD3078 RTC、W25Q128 Flash、ESP32-S3R8主控以及屏幕FPC连接可靠。", 140, 592, 1000, 30, { size: 18, color: C.text, align: "center" });
  return slide;
}

async function slide13(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "04", "软件设计", "用FreeRTOS划分任务，用消息队列连接业务与显示");
  return slide;
}

async function slide14(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "04", "软件架构：从底层驱动到界面刷新");
  const layers = [
    ["硬件层", "ST7789、CST816T、SD卡、Flash、RTC、WiFi、电源\n通过SPI/I2C/GPIO等驱动封装为上层接口", C.blue],
    ["应用层", "时间、天气、存储、多媒体、OTA、游戏、功耗控制\n由FreeRTOS拆分为相互独立的任务", C.orange],
    ["异步消息队列", "业务状态、页面请求、进度信息、任务间协作\n统一进入队列，避免多任务直接操作UI", C.cyan],
    ["显示层", "LVGL显示线程消费队列消息\n完成页面刷新、事件分发、控件重绘和屏幕输出", C.green],
  ];
  layers.forEach(([t, b, color], i) => {
    const y = 172 + i * 104;
    rect(ctx, slide, 126, y, 1028, 76, C.white, C.line, 1.2);
    rect(ctx, slide, 126, y, 12, 76, color);
    text(ctx, slide, t, 160, y + 22, 185, 30, { size: 25, color, bold: true });
    text(ctx, slide, b, 360, y + 12, 730, 48, { size: 20, color: C.text, align: "center" });
    if (i < layers.length - 1) arrow(ctx, slide, 640, y + 76, 640, y + 104);
  });
  return slide;
}

async function slide15(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "04", "SD卡与多媒体流程");
  await imageBox(ctx, slide, IMG.sdFlow, 54, 150, 820, 466);
  text(ctx, slide, "讲解重点", 922, 174, 180, 32, { size: 28, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "启动阶段初始化SD接口并挂载FatFS",
    "扫描/sdcard资源，建立图片、视频、小说文件列表缓存",
    "图片分块读取解码，减少内存占用",
    "视频帧和小说翻页通过消息通知显示层刷新",
  ], 926, 232, 276, 20, C.text, 64);
  return slide;
}

async function slide16(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "04", "时间同步流程");
  await imageBox(ctx, slide, IMG.timeFlow, 54, 150, 820, 466);
  text(ctx, slide, "三条时间来源", 922, 174, 220, 34, { size: 28, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "上电读取RTC校准系统时钟",
    "联网后SNTP校时并写回RTC",
    "无网依靠RTC守时并刷新缓存",
    "手动改时同步写入系统与RTC",
  ], 926, 236, 276, 20, C.text, 62);
  return slide;
}

async function slide17(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "04", "OTA升级流程");
  await imageBox(ctx, slide, IMG.otaFlow, 54, 150, 820, 466);
  text(ctx, slide, "升级策略", 922, 174, 200, 34, { size: 28, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "支持云端OTA与本地SD卡OTA两条路径",
    "读取当前运行分区，写入备份分区并进行校验",
    "升级完成后重启或点击跳转按钮进入新固件",
    "若新固件启动异常，则回滚到上一可用分区",
  ], 926, 236, 276, 19, C.text, 64);
  return slide;
}

async function slide18(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "04", "低功耗流程");
  await imageBox(ctx, slide, IMG.sleepFlow, 54, 150, 820, 466);
  text(ctx, slide, "双级休眠", 922, 174, 200, 34, { size: 28, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "120秒无操作：进入轻休眠",
    "长按确认键3秒：进入深休眠",
    "轻休眠快醒，深休眠防误触",
  ], 926, 244, 276, 20, C.text, 86);
  return slide;
}

async function slide19(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "05", "系统调试", "从硬件连通、功能验证到功耗评估");
  return slide;
}

async function slide20(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "05", "调试过程：先连通，再验证功能闭环");
  const steps = [
    ["硬件检查", "焊接、电源电压、Type-C下载接口、复位启动"],
    ["外设连通", "LCD显示、CST816T触摸、SD卡挂载、RTC读写"],
    ["业务验证", "表盘切换、图片壁纸、MJPEG播放、TXT断点续读"],
    ["联网维护", "WiFi连接、天气/NTP、MQTT通信、本地与云端OTA"],
  ];
  steps.forEach(([t, b], i) => {
    const x = 82 + i * 292;
    ellipse(ctx, slide, x + 88, 184, 72, 72, [C.blue, C.cyan, C.green, C.orange][i]);
    text(ctx, slide, String(i + 1), x + 104, 194, 40, 42, { size: 34, color: C.white, bold: true, align: "center" });
    rect(ctx, slide, x, 286, 238, 150, C.white, C.line, 1.2);
    text(ctx, slide, t, x + 18, 310, 202, 30, { size: 25, color: C.deep, bold: true, align: "center" });
    text(ctx, slide, b, x + 20, 358, 198, 54, { size: 19, color: C.text, align: "center" });
    if (i < 3) arrow(ctx, slide, x + 236, 220, x + 292, 220);
  });
  rect(ctx, slide, 126, 522, 1028, 52, C.white, C.line, 1.2);
  text(ctx, slide, "测试结论", 154, 536, 128, 28, { size: 22, color: C.deep, bold: true });
  text(ctx, slide, "系统能够完成主要交互与业务流程，高负载下的卡顿、掉帧和画面撕裂是后续优化重点。", 294, 534, 820, 30, { size: 22, color: C.text });
  return slide;
}

async function slide21(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "05", "功能测试结果：核心业务基本闭环");
  const rows = [
    ["手势交互", 92, C.green, "表盘、主菜单、快捷面板切换正常"],
    ["图片壁纸", 84, C.cyan, "支持SD卡图片浏览与壁纸更换"],
    ["视频播放", 82, C.orange, "MJPEG可连续播放，高负载下偶发掉帧"],
    ["小说阅读", 94, C.green, "TXT分页读取，支持断点续读"],
    ["网络OTA", 90, C.blue, "WiFi、天气、NTP、OTA流程验证通过"],
  ];
  rows.forEach(([name, score, color, note], i) => {
    const y = 172 + i * 76;
    text(ctx, slide, name, 118, y + 8, 130, 30, { size: 22, color: C.deep, bold: true });
    rect(ctx, slide, 278, y + 14, 520, 24, "#DAE8F4", "#DAE8F4", 1);
    rect(ctx, slide, 278, y + 14, 520 * score / 100, 24, color, color, 1);
    text(ctx, slide, `${score}%`, 824, y + 6, 68, 32, { size: 22, color, bold: true });
    text(ctx, slide, note, 928, y + 3, 270, 42, { size: 18, color: C.text });
  });
  return slide;
}

async function slide22(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "05", "功耗测试：工作状态电流趋势");
  rect(ctx, slide, 76, 158, 820, 430, C.white, C.line, 1.2);
  await ctx.addImage(slide, { path: IMG.powerTrend, left: 96, top: 178, width: 780, height: 390, fit: "contain" });
  text(ctx, slide, "测试结论", 922, 176, 160, 34, { size: 28, color: C.deep, bold: true });
  bulletList(ctx, slide, [
    "满亮联网时电流最高，理论续航约1.9小时",
    "低亮度并关闭联网后，亮屏续航约7.7小时",
    "轻休眠和深休眠进入毫安级电流，适合待机存放",
  ], 926, 236, 288, 21, C.text, 86);
  return slide;
}

async function slide23(p, ctx) {
  const slide = p.slides.add();
  header(ctx, slide, "05", "不足分析与后续改进");
  const items = [
    ["显示同步与总线带宽", "高负载下仍会出现轻微撕裂、卡顿或掉帧；后续可引出LCD TE同步信号，并将SD卡由SPI升级为4-bit SDMMC。", C.orange],
    ["硬件电源与低功耗", "LightSleep与DeepSleep仍为毫安级电流；后续可增加负载开关或PMOS电源门控，深睡时切断非必要外设供电。", C.green],
    ["RTC备用电源", "当前RTC仍依赖主电池；后续可加入法拉电容或小容量备用电池，主电池耗尽后仍能保持基本走时。", C.cyan],
    ["结构小型化", "3D打印外壳和FPC连接占用空间较大；后续可优化PCB与FPC座位置，降低整机厚度并提升佩戴体验。", C.red],
  ];
  items.forEach(([t, b, color], i) => {
    const x = i % 2 === 0 ? 86 : 666;
    const y = 176 + Math.floor(i / 2) * 176;
    card(ctx, slide, x, y, 520, 132, t, b, color);
  });
  text(ctx, slide, "后续目标：更平滑显示、更低待机功耗、更小整机体积。", 262, 568, 760, 34, { size: 25, color: C.deep, bold: true, align: "center" });
  return slide;
}

async function slide24(p, ctx) {
  const slide = p.slides.add();
  sectionSlide(ctx, slide, "06", "总结", "完成原型验证，并明确后续工程化优化方向");
  return slide;
}

async function slide25(p, ctx) {
  const slide = p.slides.add();
  coverBg(ctx, slide);
  text(ctx, slide, "总结", 158, 84, 220, 60, { size: 54, color: C.white, bold: true });
  rect(ctx, slide, 162, 160, 300, 5, C.white);
  const left = [
    "完成ESP32-S3R8低功耗触控智能手表样机设计",
    "实现表盘、触摸交互、多媒体、小说、小游戏等本地功能",
    "实现WiFi、天气、NTP、MQTT、本地/云端OTA等联网维护能力",
    "通过FreeRTOS任务划分与异步消息队列完成业务和显示解耦",
  ];
  left.forEach((item, i) => {
    ellipse(ctx, slide, 118, 235 + i * 66, 12, 12, C.white);
    text(ctx, slide, item, 148, 222 + i * 66, 790, 38, { size: 25, color: C.white });
  });
  rect(ctx, slide, 120, 526, 820, 1.8, "#AAC0E2");
  text(ctx, slide, "后续将围绕显示同步、存储带宽、外设断电和结构小型化继续优化，使系统更接近可长期佩戴的工程样机。", 120, 550, 760, 58, { size: 24, color: "#E8F1FF" });
  text(ctx, slide, "谢谢各位老师批评指正", 760, 636, 380, 36, { size: 30, color: C.white, bold: true, align: "right" });
  return slide;
}

async function build() {
  await fs.rm(PREVIEW_DIR, { recursive: true, force: true });
  await fs.rm(LAYOUT_DIR, { recursive: true, force: true });
  await fs.mkdir(PREVIEW_DIR, { recursive: true });
  await fs.mkdir(OUTPUT_DIR, { recursive: true });
  await fs.mkdir(LAYOUT_DIR, { recursive: true });
  await ensureArtifactToolWorkspace(WORKSPACE);
  const artifact = await importArtifactTool(WORKSPACE);
  const { Presentation, PresentationFile } = artifact;
  const presentation = Presentation.create({ slideSize: { width: W, height: H } });
  const slideFns = [
    slide01, slide02, slide03, slide04, slide05,
    slide06, slide07, slide08, slide08b, slide09,
    slide10, slide11, slide12, slide12b, slide13, slide14, slide15,
    slide16, slide17, slide18, slide19, slide20,
    slide21, slide22, slide23, slide24, slide25,
  ];
  for (let i = 0; i < slideFns.length; i += 1) {
    await slideFns[i](presentation, ctxFor(artifact, i + 1));
  }

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
