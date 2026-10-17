import DefaultTheme from "vitepress/theme";
import "@fontsource-variable/manrope";
import "@fontsource-variable/noto-sans-sc";
import "@fontsource-variable/jetbrains-mono";

import "./custom.css";
import { setupImageLightbox } from "./image-lightbox";

export default {
  ...DefaultTheme,
  enhanceApp(context) {
    DefaultTheme.enhanceApp?.(context);
    if (typeof window !== "undefined") setupImageLightbox(context.router);
  },
};
