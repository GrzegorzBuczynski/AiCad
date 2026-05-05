"use strict";

export function initCallMap(container, data, options = {}) {
  const host = typeof container === "string" ? document.querySelector(container) : container;
  if (!host) {
    throw new Error("initCallMap: container not found");
  }
  if (!data || !data.id) {
    throw new Error("initCallMap: data is required");
  }

  const title = options.title || "Function Call Map";
  const description = options.description || "Interactive, horizontal call tree for the core application flow. Drag to pan, use the mouse wheel to zoom, click a node to collapse or expand its branch.";

  host.innerHTML = "";
  host.classList.add("callmap");

  const style = document.createElement("style");
  style.textContent = `
    .callmap {
      position: relative;
      min-height: 100vh;
      color: #e6edf3;
      background:
        radial-gradient(1200px 600px at 10% 10%, rgba(246, 193, 119, 0.12), transparent 60%),
        radial-gradient(1000px 500px at 90% 20%, rgba(124, 198, 254, 0.14), transparent 60%),
        linear-gradient(160deg, #0f1418, #1d242b 55%, #0b0f12);
      font-family: "Space Grotesk", "IBM Plex Sans", "Segoe UI", sans-serif;
      overflow: hidden;
    }

    .callmap * {
      box-sizing: border-box;
    }

    .callmap > * {
      position: relative;
      z-index: 1;
    }

    .callmap-noise {
      position: absolute;
      inset: 0;
      pointer-events: none;
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='200' height='200' fill='none'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='.8' numOctaves='2' stitchTiles='stitch'/></filter><rect width='200' height='200' filter='url(%23n)' opacity='.05'/></svg>");
      mix-blend-mode: soft-light;
      opacity: 0.4;
      z-index: 0;
    }

    .callmap-header {
      display: grid;
      gap: 8px;
      padding: 24px 28px 10px;
    }

    .callmap-header h1 {
      font-size: 28px;
      margin: 0;
      letter-spacing: 0.5px;
    }

    .callmap-header p {
      margin: 0;
      color: #9aa4af;
      max-width: 900px;
    }

    .callmap-toolbar {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      padding: 12px 28px 16px;
    }

    .callmap-toolbar input {
      padding: 10px 12px;
      border-radius: 10px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      background: rgba(20, 26, 32, 0.85);
      color: #e6edf3;
      min-width: 240px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.35);
    }

    .callmap-toolbar button {
      padding: 10px 14px;
      border-radius: 10px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      background: rgba(20, 26, 32, 0.85);
      color: #e6edf3;
      cursor: pointer;
      transition: transform 160ms ease, background 160ms ease, border 160ms ease;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.35);
    }

    .callmap-toolbar button:hover {
      transform: translateY(-1px);
      border-color: rgba(246, 193, 119, 0.4);
      background: rgba(30, 40, 50, 0.9);
    }

    .callmap-canvas {
      position: relative;
      height: calc(100vh - 190px);
      margin: 0 20px 24px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 18px;
      overflow: hidden;
      background: rgba(10, 14, 18, 0.6);
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.35);
    }

    .callmap-svg {
      width: 100%;
      height: 100%;
      display: block;
      cursor: grab;
    }

    .callmap-svg:active {
      cursor: grabbing;
    }

    .callmap-node rect {
      fill: #111821;
      stroke: #2d3946;
      stroke-width: 1;
      rx: 10;
      ry: 10;
      transition: fill 160ms ease, stroke 160ms ease;
    }

    .callmap-node:hover rect {
      fill: #1b2430;
      stroke: rgba(246, 193, 119, 0.5);
    }

    .callmap-node text {
      fill: #e6edf3;
      font-size: 13px;
      font-family: "IBM Plex Mono", "Fira Code", "Consolas", monospace;
      pointer-events: none;
    }

    .callmap-node.highlight rect {
      stroke: #ffd166;
      box-shadow: 0 0 0 2px rgba(255, 209, 102, 0.2);
    }

    .callmap-edge {
      fill: none;
      stroke: rgba(180, 200, 220, 0.35);
      stroke-width: 1.5;
    }

    .callmap-legend {
      position: absolute;
      right: 16px;
      top: 16px;
      background: rgba(20, 26, 32, 0.85);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 12px;
      padding: 10px 12px;
      color: #9aa4af;
      font-size: 12px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.35);
    }

    .callmap-legend span {
      color: #e6edf3;
    }

    @media (max-width: 900px) {
      .callmap-canvas {
        height: 70vh;
      }

      .callmap-header, .callmap-toolbar {
        padding-left: 18px;
        padding-right: 18px;
      }
    }
  `;

  const noise = document.createElement("div");
  noise.className = "callmap-noise";
  noise.setAttribute("aria-hidden", "true");

  const header = document.createElement("header");
  header.className = "callmap-header";
  header.innerHTML = `<h1>${title}</h1><p>${description}</p>`;

  const toolbar = document.createElement("div");
  toolbar.className = "callmap-toolbar";
  toolbar.innerHTML = `
    <input type="text" class="callmap-search" placeholder="Filter nodes (type to highlight)" />
    <button type="button" class="callmap-expand">Expand all</button>
    <button type="button" class="callmap-collapse">Collapse all</button>
    <button type="button" class="callmap-fit">Fit view</button>
    <button type="button" class="callmap-reset">Reset view</button>
  `;

  const canvasWrap = document.createElement("div");
  canvasWrap.className = "callmap-canvas";
  canvasWrap.innerHTML = `
    <svg class="callmap-svg" viewBox="0 0 1200 800" role="img" aria-label="Function call map">
      <g class="callmap-viewport"></g>
    </svg>
    <div class="callmap-legend">
      <div><span>Click</span> node to toggle</div>
      <div><span>Drag</span> to pan</div>
      <div><span>Wheel</span> to zoom</div>
    </div>
  `;

  host.appendChild(style);
  host.appendChild(noise);
  host.appendChild(header);
  host.appendChild(toolbar);
  host.appendChild(canvasWrap);

  const svg = host.querySelector(".callmap-svg");
  const viewport = host.querySelector(".callmap-viewport");
  const searchInput = host.querySelector(".callmap-search");
  const expandAllButton = host.querySelector(".callmap-expand");
  const collapseAllButton = host.querySelector(".callmap-collapse");
  const fitViewButton = host.querySelector(".callmap-fit");
  const resetViewButton = host.querySelector(".callmap-reset");

  const collapsed = new Set();
  let highlightText = "";
  let scale = 1;
  let panX = 40;
  let panY = 40;

  const layoutConfig = {
    xGap: 260,
    yGap: 64,
    nodePaddingX: 16,
    nodePaddingY: 10,
    curve: 40
  };

  function walkVisible(node, fn) {
    fn(node);
    if (collapsed.has(node.id)) {
      return;
    }
    if (node.children) {
      node.children.forEach((child) => walkVisible(child, fn));
    }
  }

  function walkAll(node, fn) {
    fn(node);
    if (node.children) {
      node.children.forEach((child) => walkAll(child, fn));
    }
  }

  function visibleChildren(node) {
    if (collapsed.has(node.id)) {
      return [];
    }
    return node.children || [];
  }

  function layoutTree(root) {
    let yCursor = 0;

    function assign(node, depth) {
      const children = visibleChildren(node);
      if (!children.length) {
        node._x = depth * layoutConfig.xGap;
        node._y = yCursor;
        yCursor += layoutConfig.yGap;
        return;
      }

      const startY = yCursor;
      children.forEach((child) => assign(child, depth + 1));
      const endY = yCursor - layoutConfig.yGap;
      node._x = depth * layoutConfig.xGap;
      node._y = (startY + endY) / 2;
    }

    assign(root, 0);
  }

  function render() {
    viewport.innerHTML = "";
    layoutTree(data);

    const nodes = [];
    const edges = [];

    walkVisible(data, (node) => {
      nodes.push(node);
      const children = visibleChildren(node);
      children.forEach((child) => {
        edges.push({ from: node, to: child });
      });
    });

    edges.forEach((edge) => {
      const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
      const x1 = edge.from._x + 160;
      const y1 = edge.from._y;
      const x2 = edge.to._x - 20;
      const y2 = edge.to._y;
      const c = layoutConfig.curve;
      const d = `M ${x1} ${y1} C ${x1 + c} ${y1}, ${x2 - c} ${y2}, ${x2} ${y2}`;
      path.setAttribute("d", d);
      path.setAttribute("class", "callmap-edge");
      viewport.appendChild(path);
    });

    nodes.forEach((node) => {
      const group = document.createElementNS("http://www.w3.org/2000/svg", "g");
      group.setAttribute("class", "callmap-node");
      group.setAttribute("transform", `translate(${node._x}, ${node._y})`);
      group.dataset.id = node.id;

      const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
      const text = document.createElementNS("http://www.w3.org/2000/svg", "text");
      text.textContent = node.label;
      text.setAttribute("x", layoutConfig.nodePaddingX);
      text.setAttribute("y", 0);
      text.setAttribute("dominant-baseline", "middle");

      group.appendChild(rect);
      group.appendChild(text);
      viewport.appendChild(group);

      const textWidth = text.getComputedTextLength();
      const width = Math.max(140, textWidth + layoutConfig.nodePaddingX * 2);
      const height = 32 + layoutConfig.nodePaddingY;
      rect.setAttribute("width", width);
      rect.setAttribute("height", height);
      rect.setAttribute("y", -height / 2);

      if (highlightText && node.label.toLowerCase().includes(highlightText)) {
        group.classList.add("highlight");
      }

      group.addEventListener("click", (event) => {
        event.stopPropagation();
        if (!node.children || !node.children.length) {
          return;
        }
        if (collapsed.has(node.id)) {
          collapsed.delete(node.id);
        } else {
          collapsed.add(node.id);
        }
        render();
      });

      const title = document.createElementNS("http://www.w3.org/2000/svg", "title");
      title.textContent = node.label;
      group.appendChild(title);
    });

    applyTransform();
  }

  function applyTransform() {
    viewport.setAttribute("transform", `translate(${panX} ${panY}) scale(${scale})`);
  }

  function fitView() {
    const bbox = viewport.getBBox();
    const viewWidth = svg.clientWidth || 1200;
    const viewHeight = svg.clientHeight || 800;
    const padding = 80;
    const scaleX = (viewWidth - padding) / bbox.width;
    const scaleY = (viewHeight - padding) / bbox.height;
    scale = Math.min(scaleX, scaleY, 1.2);
    panX = (viewWidth - bbox.width * scale) / 2 - bbox.x * scale;
    panY = (viewHeight - bbox.height * scale) / 2 - bbox.y * scale;
    applyTransform();
  }

  function resetView() {
    scale = 1;
    panX = 40;
    panY = 40;
    applyTransform();
  }

  let isPanning = false;
  let lastX = 0;
  let lastY = 0;

  function handleMouseDown(event) {
    isPanning = true;
    lastX = event.clientX;
    lastY = event.clientY;
  }

  function handleMouseMove(event) {
    if (!isPanning) {
      return;
    }
    const dx = event.clientX - lastX;
    const dy = event.clientY - lastY;
    lastX = event.clientX;
    lastY = event.clientY;
    panX += dx;
    panY += dy;
    applyTransform();
  }

  function handleMouseUp() {
    isPanning = false;
  }

  function handleWheel(event) {
    event.preventDefault();
    const direction = Math.sign(event.deltaY);
    const zoom = direction > 0 ? 0.9 : 1.1;
    const rect = svg.getBoundingClientRect();
    const offsetX = event.clientX - rect.left;
    const offsetY = event.clientY - rect.top;
    const prevScale = scale;
    scale = Math.min(2.2, Math.max(0.45, scale * zoom));
    const scaleChange = scale / prevScale;
    panX = offsetX - (offsetX - panX) * scaleChange;
    panY = offsetY - (offsetY - panY) * scaleChange;
    applyTransform();
  }

  svg.addEventListener("mousedown", handleMouseDown);
  window.addEventListener("mousemove", handleMouseMove);
  window.addEventListener("mouseup", handleMouseUp);
  svg.addEventListener("wheel", handleWheel, { passive: false });

  searchInput.addEventListener("input", (event) => {
    highlightText = event.target.value.trim().toLowerCase();
    render();
  });

  expandAllButton.addEventListener("click", () => {
    collapsed.clear();
    render();
  });

  collapseAllButton.addEventListener("click", () => {
    collapsed.clear();
    walkAll(data, (node) => {
      if (node.children && node.children.length) {
        collapsed.add(node.id);
      }
    });
    collapsed.delete(data.id);
    render();
  });

  fitViewButton.addEventListener("click", fitView);
  resetViewButton.addEventListener("click", resetView);

  const handleResize = () => fitView();
  window.addEventListener("resize", handleResize);

  render();
  fitView();

  return {
    fitView,
    resetView,
    expandAll: () => {
      collapsed.clear();
      render();
    },
    collapseAll: () => {
      collapsed.clear();
      walkAll(data, (node) => {
        if (node.children && node.children.length) {
          collapsed.add(node.id);
        }
      });
      collapsed.delete(data.id);
      render();
    },
    destroy: () => {
      svg.removeEventListener("mousedown", handleMouseDown);
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
      svg.removeEventListener("wheel", handleWheel);
      window.removeEventListener("resize", handleResize);
      host.innerHTML = "";
      host.classList.remove("callmap");
    }
  };
}
