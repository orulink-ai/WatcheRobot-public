type WikiRouter = {
  onAfterRouteChange?: (handler: () => void) => void;
};

const MIN_SCALE = 1;
const MAX_SCALE = 5;
const SCALE_STEP = 0.25;

export function setupImageLightbox(router: WikiRouter) {
  if (typeof document === "undefined" || document.documentElement.dataset.wrImageLightbox === "ready") return;
  document.documentElement.dataset.wrImageLightbox = "ready";

  const lightbox = document.createElement("div");
  lightbox.className = "wr-image-lightbox";
  lightbox.hidden = true;
  lightbox.setAttribute("role", "dialog");
  lightbox.setAttribute("aria-modal", "true");
  lightbox.setAttribute("aria-label", "图片放大预览");
  lightbox.innerHTML = `
    <button class="wr-image-lightbox__close" type="button" aria-label="关闭图片预览">×</button>
    <div class="wr-image-lightbox__viewport">
      <img class="wr-image-lightbox__image" alt="">
    </div>
    <div class="wr-image-lightbox__controls" aria-label="图片缩放控制">
      <button type="button" data-action="zoom-out" aria-label="缩小">−</button>
      <button type="button" data-action="reset" aria-label="恢复原始大小">100%</button>
      <button type="button" data-action="zoom-in" aria-label="放大">+</button>
    </div>`;
  document.body.append(lightbox);

  const preview = lightbox.querySelector<HTMLImageElement>(".wr-image-lightbox__image")!;
  const viewport = lightbox.querySelector<HTMLElement>(".wr-image-lightbox__viewport")!;
  const closeButton = lightbox.querySelector<HTMLButtonElement>(".wr-image-lightbox__close")!;
  const resetButton = lightbox.querySelector<HTMLButtonElement>('[data-action="reset"]')!;

  let scale = 1;
  let x = 0;
  let y = 0;
  let dragging = false;
  let pointerX = 0;
  let pointerY = 0;
  let previousFocus: HTMLElement | null = null;

  const render = () => {
    preview.style.transform = `translate(${x}px, ${y}px) scale(${scale})`;
    resetButton.textContent = `${Math.round(scale * 100)}%`;
    preview.classList.toggle("is-zoomed", scale > 1);
  };

  const reset = () => {
    scale = 1;
    x = 0;
    y = 0;
    render();
  };

  const setScale = (next: number) => {
    scale = Math.min(MAX_SCALE, Math.max(MIN_SCALE, next));
    if (scale === 1) {
      x = 0;
      y = 0;
    }
    render();
  };

  const close = () => {
    if (lightbox.hidden) return;
    lightbox.hidden = true;
    document.body.classList.remove("wr-image-lightbox-open");
    preview.removeAttribute("src");
    reset();
    previousFocus?.focus();
  };

  const open = (source: HTMLImageElement) => {
    previousFocus = source;
    preview.src = source.currentSrc || source.src;
    preview.alt = source.alt || "文档图片";
    lightbox.hidden = false;
    document.body.classList.add("wr-image-lightbox-open");
    reset();
    closeButton.focus();
  };

  const enhanceImages = () => {
    document.querySelectorAll<HTMLImageElement>(".vp-doc img").forEach(image => {
      image.tabIndex = 0;
      image.setAttribute("role", "button");
      image.setAttribute("aria-label", `${image.alt || "文档图片"}，点击放大`);
      image.title = "点击放大";
    });
  };

  document.addEventListener("click", event => {
    const target = event.target;
    if (target instanceof HTMLImageElement && target.matches(".vp-doc img")) {
      event.preventDefault();
      open(target);
      return;
    }
    if (target === lightbox || target === viewport || target === closeButton) close();
  });

  document.addEventListener("keydown", event => {
    const target = event.target;
    if (target instanceof HTMLImageElement && target.matches(".vp-doc img") && (event.key === "Enter" || event.key === " ")) {
      event.preventDefault();
      open(target);
      return;
    }
    if (!lightbox.hidden && event.key === "Escape") close();
  });

  lightbox.querySelector('[data-action="zoom-in"]')?.addEventListener("click", () => setScale(scale + SCALE_STEP));
  lightbox.querySelector('[data-action="zoom-out"]')?.addEventListener("click", () => setScale(scale - SCALE_STEP));
  resetButton.addEventListener("click", reset);

  viewport.addEventListener("wheel", event => {
    event.preventDefault();
    setScale(scale + (event.deltaY < 0 ? SCALE_STEP : -SCALE_STEP));
  }, { passive: false });

  preview.addEventListener("pointerdown", event => {
    if (scale === 1) return;
    dragging = true;
    pointerX = event.clientX;
    pointerY = event.clientY;
    preview.setPointerCapture(event.pointerId);
  });
  preview.addEventListener("pointermove", event => {
    if (!dragging) return;
    x += event.clientX - pointerX;
    y += event.clientY - pointerY;
    pointerX = event.clientX;
    pointerY = event.clientY;
    render();
  });
  preview.addEventListener("pointerup", event => {
    dragging = false;
    if (preview.hasPointerCapture(event.pointerId)) preview.releasePointerCapture(event.pointerId);
  });
  preview.addEventListener("pointercancel", () => { dragging = false; });

  enhanceImages();
  router.onAfterRouteChange?.(() => {
    close();
    requestAnimationFrame(enhanceImages);
  });
}
