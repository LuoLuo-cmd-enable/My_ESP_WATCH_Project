import fs from "node:fs/promises";
import path from "node:path";
import { spawnSync } from "node:child_process";

import {
  ensureArtifactToolWorkspace,
  importArtifactTool,
  saveBlobToFile,
} from "file:///C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/artifact_tool_utils.mjs";

process.env.HOME = process.env.HOME || "C:/Users/86177";

const WORKSPACE = "C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2/outputs/ppt_restyle_watch_defense";
const PPTX = "C:/Users/86177/Desktop/ESP32_chukong/chu_kong_git/lvgl_display_test_2/docs/ESP32低功耗触控智能手表系统答辩汇报_最终修改版.pptx";
const PREVIEW_DIR = path.join(WORKSPACE, "current_preview");
const CONTACT = path.join(PREVIEW_DIR, "contact_sheet.png");
const PYTHON = "C:/Users/86177/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe";
const MAKE_CONTACT = "C:/Users/86177/.codex/plugins/cache/openai-primary-runtime/presentations/26.521.10419/skills/presentations/scripts/make_contact_sheet.py";

function slidesFromPresentation(presentation) {
  if (Array.isArray(presentation.slides?.items)) return presentation.slides.items;
  if (Number.isInteger(presentation.slides?.count) && typeof presentation.slides.getItem === "function") {
    return Array.from({ length: presentation.slides.count }, (_, index) => presentation.slides.getItem(index));
  }
  throw new Error("Could not enumerate imported presentation slides.");
}

await ensureArtifactToolWorkspace(WORKSPACE);
const { FileBlob, PresentationFile } = await importArtifactTool(WORKSPACE);
await fs.rm(PREVIEW_DIR, { recursive: true, force: true });
await fs.mkdir(PREVIEW_DIR, { recursive: true });

const presentation = await PresentationFile.importPptx(await FileBlob.load(PPTX));
const slides = slidesFromPresentation(presentation);
const previewPaths = [];
for (let i = 0; i < slides.length; i += 1) {
  const n = String(i + 1).padStart(2, "0");
  const out = path.join(PREVIEW_DIR, `slide-${n}.png`);
  const png = await presentation.export({ slide: slides[i], format: "png", scale: 1 });
  await saveBlobToFile(png, out);
  previewPaths.push(out);
}

const contact = spawnSync(PYTHON, [MAKE_CONTACT, "--output", CONTACT, ...previewPaths], { encoding: "utf8" });
if (contact.status !== 0) {
  throw new Error(`Contact sheet failed\n${contact.stdout}\n${contact.stderr}`);
}

console.log(CONTACT);
