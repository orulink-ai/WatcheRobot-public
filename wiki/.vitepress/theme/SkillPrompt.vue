<script setup>
import { computed, onMounted, ref } from 'vue';
const props = defineProps({ english: Boolean });
const origin = ref('');
const requirement = ref('');
const status = ref('');
const samples = computed(() => props.english ? [
  ['Touch interaction', 'When I touch the robot’s back, turn its light blue for one second. Ignore repeated touches while busy; release my resources on exit.', 'Touch → blue light for one second.'],
  ['Web photo button', 'Create a local web page with a photo button. Each click captures a real photo and shows it on the page. Disable the button during capture and show errors clearly.', 'Click → a real photo appears.'],
  ['Timer', 'After I start the app, wait 60 seconds, flash the light three times, then exit. Allow cancellation while waiting and release my resources.', 'Wait → three flashes → exit.'],
  ['Voice assistant', 'Help me build a voice app using my own ASR, LLM and TTS services. First ask for providers and required configuration; keep secrets out of source code. Do not connect to a robot before confirmation.', 'After configuration and hardware testing → a spoken reply.'],
] : [
  ['触摸互动', '触摸机器人背部后，灯光变蓝一秒。处理期间忽略重复触摸，退出时释放本应用资源。', '触摸 → 蓝灯亮一秒。'],
  ['网页拍照', '创建一个本机网页，点击按钮拍摄真实照片并显示在页面上。拍摄中禁用按钮，失败时显示具体错误。', '点击按钮 → 页面显示真实照片。'],
  ['定时提醒', '启动后等待60秒，灯光闪烁三次，然后退出。等待时可以取消，退出时释放本应用资源。', '等待 → 闪灯三次 → 退出。'],
  ['语音助手', '帮我创建接入自选ASR、LLM、TTS服务的语音应用。先询问服务商及配置要求，凭据不写进源码，确认前不连接机器人。', '完成配置并实机验收后 → 听到语音回复。'],
]);
onMounted(() => { origin.value = window.location.origin; });
const prompt = computed(() => !origin.value || !requirement.value.trim() ? '' : props.english
  ? `Download ${origin.value}/downloads/watche-robot-app.zip, extract the complete bundle and read watche-robot-app/SKILL.md. Create a WatcheRobot Python app with this requirement: ${requirement.value.trim()} Preserve existing files, run local checks and report anything not verified on hardware. Ask before controlling a robot or flashing firmware.`
  : `请下载 ${origin.value}/downloads/watche-robot-app.zip，解压完整文件并读取 watche-robot-app/SKILL.md。按 Skill 流程创建 WatcheRobot Python 应用，需求是：${requirement.value.trim()} 保留已有文件，完成本地检查并说明未实机验证的部分；控制机器人或刷固件前先征得我同意。`);
function select(sample) { requirement.value = sample[1]; status.value = ''; }
async function copy() {
  try { await navigator.clipboard.writeText(prompt.value); status.value = props.english ? 'Copied' : '已复制'; }
  catch { status.value = props.english ? 'Select the generated text and copy it manually.' : '浏览器未允许自动复制，请点击生成内容全选后手动复制。'; }
}
</script>
<template>
  <div class="skill-prompt">
    <div class="examples">
      <button v-for="sample in samples" :key="sample[0]" type="button" @click="select(sample)">{{ sample[0] }}<small>{{ sample[2] }}</small></button>
    </div>
    <label for="skill-requirement">{{ english ? 'Your idea (edit a template or write your own)' : '你的需求（选择模板后可修改，也可以自己写）' }}</label>
    <textarea id="skill-requirement" v-model="requirement" rows="3" :placeholder="english ? 'What should trigger the app, and what should happen?' : '什么触发？机器人做什么？怎样算成功？'" @input="status = ''" />
    <template v-if="prompt">
      <p class="destination">{{ english ? 'Download origin: ' : '本次使用的下载站点：' }}{{ origin }}</p>
      <textarea class="generated" :aria-label="english ? 'Generated agent prompt' : '生成的 Agent 提示词'" :value="prompt" readonly rows="5" @focus="$event.target.select()" />
    </template>
    <button class="copy" type="button" :disabled="!prompt" @click="copy">{{ english ? 'Copy and send to your agent' : '复制并发给 Agent' }}</button>
    <span role="status">{{ status || (english ? 'Choose a template or enter your requirement first.' : '先选择模板或填写需求，再复制。') }}</span>
  </div>
</template>
<style scoped>
.skill-prompt { margin:24px 0; padding:20px; border:1px solid var(--vp-c-divider); border-radius:3px; background:var(--vp-c-bg-soft); }
.examples { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:10px; margin-bottom:22px; }
.examples button { border:1px solid var(--vp-c-divider); padding:12px; text-align:left; color:var(--vp-c-text-1); background:var(--vp-c-bg); }
small { display:block; margin-top:5px; color:var(--vp-c-text-2); font-size:12px; line-height:1.6; }
label { display:block; margin-bottom:10px; }
textarea { display:block; width:100%; min-height:110px; resize:vertical; padding:12px; border:1px solid var(--vp-c-divider); color:var(--vp-c-text-1); background:var(--vp-c-bg); font:inherit; font-size:14px; line-height:1.8; }
.generated { min-height:180px; }
.destination { font-size:12px; overflow-wrap:anywhere; color:var(--vp-c-text-2); }
button { border-radius:3px; font:inherit; cursor:pointer; }
.copy { margin-top:16px; padding:9px 16px; background:var(--vp-button-brand-bg); color:var(--vp-button-brand-text); }
button:disabled { opacity:.5; cursor:not-allowed; }
span { display:block; margin-top:10px; font-size:12px; color:var(--vp-c-text-2); }
@media(max-width:640px) { .examples { grid-template-columns:1fr; } }
</style>
