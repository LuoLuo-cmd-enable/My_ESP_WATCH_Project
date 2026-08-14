import fs from "node:fs/promises";
import path from "node:path";
import JSZip from "jszip";

const ROOT = "C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2";
const PPTX = path.join(ROOT, "docs/ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版.pptx");
const OUT = path.join(ROOT, "docs/ESP32低功耗触控智能手表系统答辩汇报_仅方案页修改版.pptx");
const BACKUP = path.join(ROOT, "docs/ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版_slide4改前备份.pptx");

function esc(s) {
  return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function mutateShape(xml, id, fn) {
  return xml.replace(/<p:sp>[\s\S]*?<\/p:sp>/g, (block) => {
    if (!block.includes(`<p:cNvPr id="${id}"`)) return block;
    return fn(block);
  });
}

function getShape(xml, id) {
  for (const match of xml.matchAll(/<p:sp>[\s\S]*?<\/p:sp>/g)) {
    if (match[0].includes(`<p:cNvPr id="${id}"`)) return match[0];
  }
  return "";
}

function setText(xml, id, text) {
  return mutateShape(xml, id, (block) => {
    let done = false;
    return block.replace(/<a:t>[\s\S]*?<\/a:t>/g, () => {
      if (!done) {
        done = true;
        return `<a:t>${esc(text)}</a:t>`;
      }
      return "<a:t></a:t>";
    });
  });
}

function setGeom(xml, id, x, y, cx, cy) {
  return mutateShape(xml, id, (block) =>
    block
      .replace(/<a:off x="-?\d+" y="-?\d+"\/>/, `<a:off x="${x}" y="${y}"/>`)
      .replace(/<a:ext cx="\d+" cy="\d+"\/>/, `<a:ext cx="${cx}" cy="${cy}"/>`)
  );
}

function setFont(xml, id, sz) {
  return mutateShape(xml, id, (block) => block.replace(/<a:rPr\b([^>]*)\bsz="\d+"/g, `<a:rPr$1 sz="${sz}"`));
}

function removeShape(xml, id) {
  return xml.replace(/<p:sp>[\s\S]*?<\/p:sp>/g, (block) => (block.includes(`<p:cNvPr id="${id}"`) ? "" : block));
}

function addBeforeSpTreeEnd(xml, shapeXml) {
  return xml.replace("</p:spTree>", `${shapeXml}</p:spTree>`);
}

async function main() {
  try {
    await fs.access(BACKUP);
  } catch {
    await fs.copyFile(PPTX, BACKUP);
  }

  const zip = await JSZip.loadAsync(await fs.readFile(PPTX));
  let xml = await zip.file("ppt/slides/slide4.xml").async("string");

  // Remove previously inserted 5th bullet if the script is rerun.
  xml = removeShape(xml, 29);
  xml = removeShape(xml, 30);

  xml = setText(xml, 8, "本课题方案：开放 MCU 平台下的手表原型");
  xml = setGeom(xml, 8, 658586, 647700, 9200000, 495300);

  xml = setText(xml, 10, "ESP32-S3R8 + ESP-IDF + FreeRTOS + LVGL");
  xml = setGeom(xml, 10, 990600, 1550000, 10200000, 533400);
  xml = setFont(xml, 10, 2850);
  xml = setGeom(xml, 11, 1009650, 2200000, 5200000, 47625);

  const bullets = [
    "趋势基础：MCU 集成度和性能提升，开放 MCU 已能承担图形界面、联网通信、本地存储与多任务调度。",
    "LVGL 生态：控件丰富、适配范围扩大，芯片厂商和屏幕器件厂商持续兼容；GUI Guider 等工具降低小屏 UI 开发难度。",
    "ESP-IDF 支撑：提供 WiFi、文件系统、NTP、MQTT、OTA 和外设驱动组件，减少底层驱动重复编写。",
    "FreeRTOS 调度：将显示、触摸、SD 读取、网络、OTA 和低功耗拆为独立任务，便于功能添加、调试和优化。",
    "方案价值：相比封闭手表 Demo SDK，可自主设计硬件接口、任务调度、UI 页面和业务逻辑，更适合课题验证与功能拓展。",
  ];
  const textIds = [13, 15, 17, 19, 30];
  const dotIds = [12, 14, 16, 18, 29];
  const dotY = [2580000, 3190000, 3800000, 4410000, 5020000];
  const textY = dotY.map((v) => v - 85725);

  // Create fifth bullet by duplicating the fourth bullet's dot and text.
  const dotTemplate = getShape(xml, 18)
    .replace(/<p:cNvPr id="18" name="[^"]*"/, '<p:cNvPr id="29" name="椭圆 28"')
    .replace(/<a16:creationId([^>]*?) id="\{[^"]+\}"\/>/, '<a16:creationId$1 id="{A28C97A1-1B86-4A9D-B01C-1B4B8A41C529}"/>');
  const textTemplate = getShape(xml, 19)
    .replace(/<p:cNvPr id="19" name="[^"]*"/, '<p:cNvPr id="30" name="矩形 29"')
    .replace(/<a16:creationId([^>]*?) id="\{[^"]+\}"\/>/, '<a16:creationId$1 id="{F861A1D4-7C9D-4E45-9D55-8B1BE33B207B}"/>');
  xml = addBeforeSpTreeEnd(xml, dotTemplate + textTemplate);

  for (let i = 0; i < bullets.length; i += 1) {
    xml = setGeom(xml, dotIds[i], 1066800, dotY[i], 95250, 95250);
    xml = setGeom(xml, textIds[i], 1276350, textY[i], 9600000, 560000);
    xml = setText(xml, textIds[i], bullets[i]);
    xml = setFont(xml, textIds[i], 1850);
  }

  zip.file("ppt/slides/slide4.xml", xml);
  const buf = await zip.generateAsync({ type: "nodebuffer", compression: "DEFLATE" });
  let savedTo = PPTX;
  try {
    await fs.writeFile(PPTX, buf);
  } catch (error) {
    if (error?.code !== "EBUSY") throw error;
    savedTo = OUT;
    await fs.writeFile(OUT, buf);
  }
  console.log(JSON.stringify({ pptx: savedTo, backup: BACKUP }, null, 2));
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
