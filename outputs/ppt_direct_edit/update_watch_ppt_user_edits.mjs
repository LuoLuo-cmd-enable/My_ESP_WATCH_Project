import fs from "node:fs/promises";
import path from "node:path";
import JSZip from "jszip";

const ROOT = "C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2";
const PPTX = path.join(ROOT, "docs/ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版.pptx");
const BACKUP = path.join(ROOT, "docs/ESP32低功耗触控智能手表系统答辩汇报_六部分重构修改版_改前备份.pptx");

const img = (name) => path.join(ROOT, "docs", name);

function esc(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function mutateShape(xml, id, fn) {
  return xml.replace(/<p:sp>[\s\S]*?<\/p:sp>/g, (block) => {
    if (!block.includes(`<p:cNvPr id="${id}"`)) return block;
    return fn(block);
  });
}

function setShapeText(xml, id, text) {
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

function setShapeGeom(xml, id, x, y, cx, cy) {
  return mutateShape(xml, id, (block) => {
    let changedOff = false;
    let changedExt = false;
    return block
      .replace(/<a:off x="-?\d+" y="-?\d+"\/>/, () => {
        changedOff = true;
        return `<a:off x="${x}" y="${y}"/>`;
      })
      .replace(/<a:ext cx="\d+" cy="\d+"\/>/, () => {
        changedExt = true;
        return `<a:ext cx="${cx}" cy="${cy}"/>`;
      });
  });
}

function removeShapes(xml, ids) {
  const idSet = new Set(ids.map(String));
  return xml.replace(/<p:sp>[\s\S]*?<\/p:sp>/g, (block) => {
    const id = block.match(/<p:cNvPr id="(\d+)"/)?.[1];
    return idSet.has(id) ? "" : block;
  });
}

async function main() {
  try {
    await fs.access(BACKUP);
  } catch {
    await fs.copyFile(PPTX, BACKUP);
  }

  const zip = await JSZip.loadAsync(await fs.readFile(PPTX));

  // Slide 6: design requirements and performance indicators.
  let s6 = await zip.file("ppt/slides/slide6.xml").async("string");
  s6 = setShapeGeom(s6, 57, 0, 0, 12192000, 6858000);
  s6 = setShapeText(s6, 13, "显示交互");
  s6 = setShapeText(s6, 14, "表盘/菜单/键盘/游戏");
  s6 = setShapeText(s6, 17, "多媒体");
  s6 = setShapeText(s6, 18, "图片/视频/小说");
  s6 = setShapeText(s6, 21, "联网维护");
  s6 = setShapeText(s6, 22, "WiFi/MQTT/OTA");
  s6 = setShapeText(s6, 25, "时间天气");
  s6 = setShapeText(s6, 26, "NTP/RTC/天气");
  s6 = setShapeText(s6, 29, "低功耗");
  s6 = setShapeText(s6, 30, "轻休眠/深休眠");
  s6 = setShapeText(s6, 43, "轻休眠触发");
  s6 = setShapeText(s6, 44, "120秒无操作");
  s6 = setShapeText(s6, 47, "深休眠触发");
  s6 = setShapeText(s6, 48, "长按确认键");
  s6 = setShapeText(s6, 51, "轻休眠电流");
  s6 = setShapeText(s6, 52, "关背光/外设");
  s6 = setShapeText(s6, 55, "深休眠电流");
  s6 = setShapeText(s6, 56, "保留按键唤醒");
  zip.file("ppt/slides/slide6.xml", s6);

  // Slide 9: remove debug-interface content; split time and storage.
  let s9 = await zip.file("ppt/slides/slide9.xml").async("string");
  s9 = setShapeText(s9, 16, "SD卡存储");
  s9 = setShapeText(
    s9,
    17,
    "SD卡用于图片、MJPEG视频、TXT小说和本地OTA固件；挂载FatFS后支持文件扫描、分块读取和断点续读。"
  );
  s9 = setShapeText(s9, 20, "时间管理");
  s9 = setShapeText(
    s9,
    21,
    "系统上电读取SD3078 RTC并同步到C语言time库；联网后NTP校时并写回RTC，断网或休眠恢复仍保持时间连续。"
  );
  s9 = setShapeText(
    s9,
    23,
    "硬件链路：SPI负责LCD显示和SD卡存储，I2C负责触摸与SD3078 RTC，GPIO负责背光、卡检测和休眠唤醒。"
  );
  zip.file("ppt/slides/slide9.xml", s9);

  // Slides 11-14: replace detailed flowcharts with the previous simplified diagrams.
  zip.file("ppt/media/image4.png", await fs.readFile(img("storage_multimedia_layers_largefont.png")));
  zip.file("ppt/media/image5.png", await fs.readFile(img("time_sync_layers_largefont.png")));
  zip.file("ppt/media/image6.png", await fs.readFile(img("ota_layers_largefont.png")));
  zip.file("ppt/media/image7.png", await fs.readFile(img("sleep_state_layers_largefont.png")));

  // Slide 17: remove RTC backup-power item; keep three improvement directions and expand them.
  let s17 = await zip.file("ppt/slides/slide17.xml").async("string");
  s17 = setShapeText(s17, 12, "显示同步与存储带宽");
  s17 = setShapeText(
    s17,
    13,
    "高负载下仍会出现轻微撕裂、卡顿或掉帧；后续可引出LCD TE同步信号，并将SD卡接口由SPI升级为4-bit SDMMC，提高图片切换和视频读取速度。"
  );
  s17 = setShapeText(s17, 16, "硬件电源与低功耗");
  s17 = setShapeText(
    s17,
    17,
    "当前休眠电流仍处于毫安级；后续可加入负载开关或PMOS电源门控，在深休眠时切断屏幕、SD卡等非必要外设供电，只保留按键唤醒链路。"
  );
  s17 = setShapeText(s17, 20, "结构小型化");
  s17 = setShapeText(
    s17,
    21,
    "样机采用3D打印外壳，屏幕FPC与主板连接仍占用较多空间；后续可优化PCB外形、FPC座位置和堆叠结构，降低整机厚度并提升佩戴舒适度。"
  );
  // Expand the third card across the lower row and remove the old fourth card.
  s17 = setShapeGeom(s17, 18, 819150, 3352800, 10477500, 1257300);
  s17 = setShapeGeom(s17, 19, 819150, 3352800, 95250, 1257300);
  s17 = setShapeGeom(s17, 20, 1066800, 3524250, 9715500, 304800);
  s17 = setShapeGeom(s17, 21, 1066800, 3981450, 9715500, 476250);
  s17 = removeShapes(s17, [22, 23, 24, 25]);
  s17 = setShapeText(s17, 26, "后续目标：更平滑显示、更低待机功耗、更小整机体积。");
  zip.file("ppt/slides/slide17.xml", s17);

  // Slide 18: concise final conclusion with clearer engineering takeaways.
  let s18 = await zip.file("ppt/slides/slide18.xml").async("string");
  s18 = setShapeText(s18, 26, "完成ESP32-S3R8低功耗触控智能手表样机，实现显示触摸、SD存储、RTC守时、WiFi联网和双级休眠等硬件闭环");
  s18 = setShapeText(s18, 28, "软件采用FreeRTOS任务划分与异步消息队列，完成表盘、天气、NTP、图片/视频/小说、小游戏和OTA等功能");
  s18 = setShapeText(s18, 30, "测试表明常规触摸操作、页面切换、电子书断点续读、网络连接与固件维护功能均可稳定运行");
  s18 = setShapeText(s18, 32, "高分辨率图片翻页和MJPEG播放仍受显示同步、总线带宽与微控制器算力限制，是后续优化重点");
  s18 = setShapeText(
    s18,
    34,
    "后续将围绕TE同步刷新、SDMMC存储带宽、外设电源门控和结构小型化继续改进，使系统更接近可长期佩戴的工程样机。"
  );
  zip.file("ppt/slides/slide18.xml", s18);

  const buf = await zip.generateAsync({ type: "nodebuffer", compression: "DEFLATE" });
  await fs.writeFile(PPTX, buf);
  console.log(JSON.stringify({ pptx: PPTX, backup: BACKUP }, null, 2));
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
