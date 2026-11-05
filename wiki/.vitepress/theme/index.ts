import DefaultTheme from "vitepress/theme";
import SkillPrompt from "./SkillPrompt.vue";
import "@fontsource-variable/noto-sans-sc";
import "@fontsource-variable/jetbrains-mono";

import "./custom.css";
import { setupImageLightbox } from "./image-lightbox";

export default {
  ...DefaultTheme,
  enhanceApp(context) {
    DefaultTheme.enhanceApp?.(context);
    context.app.component("SkillPrompt", SkillPrompt);
    if (typeof window !== "undefined") setupImageLightbox(context.router);
  },
};
