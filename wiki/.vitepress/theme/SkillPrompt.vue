<script setup>
import { computed, onMounted, ref } from 'vue';
const props = defineProps({ english: Boolean });
const origin = ref('https://wiki.orulink.ai');
const copied = ref(false);
onMounted(() => { origin.value = window.location.origin; });
const prompt = computed(() => props.english
  ? `Download ${origin.value}/downloads/watche-robot-app.zip, extract the complete bundle, read watche-robot-app/SKILL.md and follow its workflow to help me create a WatcheRobot Python application: [describe my idea]. Preserve existing files, run local checks and report anything not verified on hardware. Ask before controlling a robot or flashing firmware.`
  : `请下载 ${origin.value}/downloads/watche-robot-app.zip，解压完整文件，读取 watche-robot-app/SKILL.md 并按其中流程，帮我创建一个 WatcheRobot Python 应用：[填写我的需求]。保留已有文件，完成本地检查并说明未实机验证的部分；控制机器人或刷固件前先征得我同意。`);
async function copy() {
  try { await navigator.clipboard.writeText(prompt.value); copied.value = true; }
  catch { copied.value = false; } // On plain HTTP, readers can select the readonly text.
}
</script>
<template>
  <div class="skill-prompt">
    <textarea :aria-label="english ? 'Prompt for your AI agent' : '发给 AI Agent 的提示词'" :value="prompt" readonly rows="5" @focus="$event.target.select()" />
    <button type="button" @click="copy">{{ copied ? (english ? 'Copied' : '已复制') : (english ? 'Copy prompt' : '复制提示词') }}</button>
    <span role="status">{{ english ? 'If copying is unavailable, select the text above and copy it.' : '若浏览器不允许复制，点击上方文本框全选后手动复制即可。' }}</span>
  </div>
</template>
<style scoped>
.skill-prompt { margin:24px 0; padding:20px; border:1px solid var(--vp-c-divider); border-radius:3px; background:var(--vp-c-bg-soft); }
textarea { display:block; width:100%; min-height:180px; resize:vertical; color:var(--vp-c-text-1); background:transparent; font:inherit; font-size:14px; line-height:1.8; }
button { margin-top:16px; padding:9px 16px; border-radius:3px; background:var(--vp-button-brand-bg); color:var(--vp-button-brand-text); font:inherit; cursor:pointer; }
span { display:block; margin-top:10px; font-size:12px; color:var(--vp-c-text-2); }
</style>
