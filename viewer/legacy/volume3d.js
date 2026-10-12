// Small software 3D renderer: actual voxel fields, projected curves and slices.
// No external browser runtime is required by the offline viewer.

const Volume3D = (() => {
  const camera = { yaw: 0.7, pitch: -0.45, zoom: 1, panX: 0, panY: 0 },
    cache = new WeakMap();

  let drag = null,
    redraw = () => {},
    enabled = () => false;

  function scalar(dataset, frame, field) {
    const m = dataset.metadata,
      n = m.nx * m.ny * m.nz;
    const offset = m.fields.indexOf(field) * n;
    if (offset >= 0) return frame.values.subarray(offset, offset + n);
    if (!["__speed__", "__vorticity__"].includes(field))
      throw Error("Componente 3D ausente: " + field);
    const u = scalar(dataset, frame, "velocity_x"),
      v = scalar(dataset, frame, "velocity_y"),
      w = scalar(dataset, frame, "velocity_z"),
      result = new Float64Array(n);
    if (field === "__speed__") {
      for (let i = 0; i < n; i++) result[i] = Math.hypot(u[i], v[i], w[i]);
      return result;
    }
    const sample = (a, x, y, z) =>
      a[
        (((z + m.nz) % m.nz) * m.ny + ((y + m.ny) % m.ny)) * m.nx +
          ((x + m.nx) % m.nx)
      ];
    for (let z = 0; z < m.nz; z++)
      for (let y = 0; y < m.ny; y++)
        for (let x = 0; x < m.nx; x++) {
          const wx =
            (sample(w, x, y + 1, z) -
              sample(w, x, y - 1, z) -
              sample(v, x, y, z + 1) +
              sample(v, x, y, z - 1)) /
            2;
          const wy =
            (sample(u, x, y, z + 1) -
              sample(u, x, y, z - 1) -
              sample(w, x + 1, y, z) +
              sample(w, x - 1, y, z)) /
            2;
          const wz =
            (sample(v, x + 1, y, z) -
              sample(v, x - 1, y, z) -
              sample(u, x, y + 1, z) +
              sample(u, x, y - 1, z)) /
            2;
          result[(z * m.ny + y) * m.nx + x] = Math.hypot(wx, wy, wz);
        }
    return result;
  }

  function sample(a, m, x, y, z) {
    x = ((x % m.nx) + m.nx) % m.nx;
    y = ((y % m.ny) + m.ny) % m.ny;
    z = ((z % m.nz) + m.nz) % m.nz;
    const ix = Math.floor(x),
      iy = Math.floor(y),
      iz = Math.floor(z),
      fx = x - ix,
      fy = y - iy,
      fz = z - iz;
    let value = 0;
    for (let dz = 0; dz < 2; dz++)
      for (let dy = 0; dy < 2; dy++)
        for (let dx = 0; dx < 2; dx++)
          value +=
            a[
              (((iz + dz) % m.nz) * m.ny + ((iy + dy) % m.ny)) * m.nx +
                ((ix + dx) % m.nx)
            ] *
            (dx ? fx : 1 - fx) *
            (dy ? fy : 1 - fy) *
            (dz ? fz : 1 - fz);
    return value;
  }

  function trails(dataset, frame, field, values) {
    let stored = cache.get(frame);
    if (!stored) {
      stored = new Map();
      cache.set(frame, stored);
    }
    if (stored.has(field)) return stored.get(field);
    const m = dataset.metadata,
      u = scalar(dataset, frame, "velocity_x"),
      v = scalar(dataset, frame, "velocity_y"),
      w = scalar(dataset, frame, "velocity_z"),
      lines = [];
    for (let seed = 0; seed < 54; seed++) {
      let x = ((seed * 0.61803398875) % 1) * m.nx,
        y = ((seed * 0.41421356237 + 0.2) % 1) * m.ny,
        z = ((seed * 0.73205080757 + 0.4) % 1) * m.nz,
        points = [];
      for (let i = 0; i < 100; i++) {
        points.push({
          x: x / m.nx - 0.5,
          y: y / m.ny - 0.5,
          z: z / m.nz - 0.5,
          value: sample(values, m, x, y, z),
        });
        let ux = sample(u, m, x, y, z),
          vy = sample(v, m, x, y, z),
          wz = sample(w, m, x, y, z),
          speed = Math.hypot(ux, vy, wz);
        if (speed < 1e-9) break;
        const nx = x + (0.55 * ux) / speed,
          ny = y + (0.55 * vy) / speed,
          nz = z + (0.55 * wz) / speed;
        if (
          nx < 0 ||
          nx >= m.nx ||
          ny < 0 ||
          ny >= m.ny ||
          nz < 0 ||
          nz >= m.nz
        ) {
          if (points.length > 1) lines.push(points);
          points = [];
        }
        x = (nx + m.nx) % m.nx;
        y = (ny + m.ny) % m.ny;
        z = (nz + m.nz) % m.nz;
      }
      if (points.length > 1) lines.push(points);
    }
    stored.set(field, lines);
    return lines;
  }

  function render(
    canvas,
    dataset,
    frame,
    field,
    values,
    min,
    max,
    palette,
    color,
    mode,
    zSlice,
    vectors,
    streamlines,
  ) {
    const size = Math.min(canvas.clientWidth, 650),
      height = Math.min(size, canvas.clientHeight || size),
      ratio = devicePixelRatio || 1;
    canvas.width = Math.round(size * ratio);
    canvas.height = Math.round(height * ratio);
    const ctx = canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    ctx.fillStyle = "#080d14";
    ctx.fillRect(0, 0, size, height);
    const project = (p) => {
      const cy = Math.cos(camera.yaw),
        sy = Math.sin(camera.yaw),
        cp = Math.cos(camera.pitch),
        sp = Math.sin(camera.pitch),
        x = p.x * cy + p.z * sy,
        z = -p.x * sy + p.z * cy,
        y = p.y * cp - z * sp,
        depth = p.y * sp + z * cp;
      const factor =
        (Math.min(size, height) * 2.05 * camera.zoom) / (2.5 + depth);
      return {
        x: size / 2 + camera.panX * size + x * factor,
        y: height / 2 + camera.panY * height - y * factor,
        z: depth,
        factor,
      };
    };
    const tint = (value) => color((value - min) / (max - min || 1), palette),
      corners = [];
    for (let z = 0; z < 2; z++)
      for (let y = 0; y < 2; y++)
        for (let x = 0; x < 2; x++)
          corners.push({ x: x - 0.5, y: y - 0.5, z: z - 0.5 });
    ctx.lineWidth = 1;
    ctx.strokeStyle = "#3d566b";
    for (let i = 0; i < 8; i++)
      for (const bit of [1, 2, 4])
        if ((i & bit) === 0) {
          const a = project(corners[i]),
            b = project(corners[i | bit]);
          ctx.beginPath();
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
          ctx.stroke();
        }
    const m = dataset.metadata;
    if (mode === "slice") {
      const z = Math.min(m.nz - 1, Number(zSlice));

      const faces = [];

      for (let y = 0; y < m.ny; y++) {
        for (let x = 0; x < m.nx; x++) {
          const points = [
            {
              x: x / m.nx - 0.5,
              y: y / m.ny - 0.5,
              z: (z + 0.5) / m.nz - 0.5,
            },
            {
              x: (x + 1) / m.nx - 0.5,
              y: y / m.ny - 0.5,
              z: (z + 0.5) / m.nz - 0.5,
            },
            {
              x: (x + 1) / m.nx - 0.5,
              y: (y + 1) / m.ny - 0.5,
              z: (z + 0.5) / m.nz - 0.5,
            },
            {
              x: x / m.nx - 0.5,
              y: (y + 1) / m.ny - 0.5,
              z: (z + 0.5) / m.nz - 0.5,
            },
          ].map(project);

          faces.push({
            points,
            value: values[(z * m.ny + y) * m.nx + x],
            depth: points.reduce((sum, p) => sum + p.z, 0) / 4,
          });
        }
      }

      faces.sort((a, b) => b.depth - a.depth);

      for (const face of faces) {
        ctx.fillStyle = `rgb(${tint(face.value).join(",")})`;

        ctx.beginPath();

        face.points.forEach((p, i) =>
          i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y),
        );

        ctx.closePath();
        ctx.fill();
      }
    } else if (mode === "wavefront") {
      wavefronts(ctx, project, m, values, min, max, palette, color);
    } else if (mode === "lines" && streamlines && dataset.velocity) {
      const lines = trails(dataset, frame, field, values).map((line) => ({
        points: line.map((p) => ({
          ...project(p),
          value: p.value,
        })),
        depth: line.reduce((sum, p) => sum + project(p).z, 0) / line.length,
      }));

      lines.sort((a, b) => b.depth - a.depth);

      ctx.lineWidth = 1.7;
      ctx.lineCap = "round";

      for (const line of lines) {
        for (let i = 1; i < line.points.length; i++) {
          const a = line.points[i - 1];
          const b = line.points[i];

          ctx.strokeStyle = `rgba(${tint(b.value).join(",")},.83)`;

          ctx.beginPath();
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
          ctx.stroke();
        }

        if (vectors) {
          const p = line.points.at(-1);

          ctx.fillStyle = `rgb(${tint(p.value).join(",")})`;

          ctx.beginPath();
          ctx.arc(p.x, p.y, 2.5, 0, 2 * Math.PI);
          ctx.fill();
        }
      }
    } else {
      // Existing cloud renderer.

      const stride = Math.max(
        1,
        Math.ceil(Math.cbrt((m.nx * m.ny * m.nz) / 1600)),
      );

      const points = [];

      for (let z = 0; z < m.nz; z += stride) {
        for (let y = 0; y < m.ny; y += stride) {
          for (let x = 0; x < m.nx; x += stride) {
            const value = values[(z * m.ny + y) * m.nx + x];

            const p = project({
              x: (x + 0.5) / m.nx - 0.5,
              y: (y + 0.5) / m.ny - 0.5,
              z: (z + 0.5) / m.nz - 0.5,
            });

            points.push({
              ...p,
              value,
            });
          }
        }
      }

      points.sort((a, b) => b.z - a.z);

      for (const p of points) {
        ctx.fillStyle = `rgba(${tint(p.value).join(",")},.65)`;

        ctx.beginPath();

        ctx.arc(
          p.x,
          p.y,
          Math.max(1, (p.factor / m.nx) * 0.24),
          0,
          2 * Math.PI,
        );

        ctx.fill();
      }
    }
    ctx.font = "12px Consolas,monospace";
    for (const [label, p, c] of [
      ["x", { x: 0.67, y: -0.5, z: -0.5 }, "#ffb883"],
      ["y", { x: -0.5, y: 0.67, z: -0.5 }, "#8ce4c0"],
      ["z", { x: -0.5, y: -0.5, z: 0.67 }, "#98bdff"],
    ]) {
      const q = project(p);
      ctx.fillStyle = c;
      ctx.fillText(label, q.x, q.y);
    }
    ctx.font = "10px Consolas,monospace";
    ctx.fillStyle = "#9aaec2";
    ctx.fillText(
      "ARRASTE: GIRAR  ·  SHIFT+ARRASTE: MOVER  ·  SCROLL: ZOOM",
      12,
      height - 8,
    );
  }

  function bind(canvas, callback, isEnabled) {
    redraw = callback;
    enabled = isEnabled;
    canvas.addEventListener("pointerdown", (e) => {
      if (!enabled()) return;
      drag = { x: e.clientX, y: e.clientY, pan: e.shiftKey || e.button === 2 };
      canvas.setPointerCapture(e.pointerId);
      e.preventDefault();
    });
    canvas.addEventListener("pointermove", (e) => {
      if (!drag) return;
      const dx = e.clientX - drag.x,
        dy = e.clientY - drag.y;
      drag.x = e.clientX;
      drag.y = e.clientY;
      if (drag.pan) {
        camera.panX += dx / canvas.clientWidth;
        camera.panY += dy / canvas.clientHeight;
      } else {
        camera.yaw += dx * 0.009;
        camera.pitch = Math.max(-1.5, Math.min(1.5, camera.pitch + dy * 0.009));
      }
      redraw();
    });
    canvas.addEventListener("pointerup", () => (drag = null));
    canvas.addEventListener("pointercancel", () => (drag = null));
    canvas.addEventListener(
      "wheel",
      (e) => {
        if (!enabled()) return;
        e.preventDefault();
        camera.zoom = Math.max(
          0.4,
          Math.min(4, camera.zoom * Math.exp(-e.deltaY * 0.0015)),
        );
        redraw();
      },
      { passive: false },
    );
    canvas.addEventListener("contextmenu", (e) => {
      if (enabled()) e.preventDefault();
    });
  }

  function reset() {
    Object.assign(camera, {
      yaw: 0.7,
      pitch: -0.45,
      zoom: 1,
      panX: 0,
      panY: 0,
    });
    redraw();
  }

  function wavefronts(ctx, project, m, values, min, max, palette, color) {
    const range = max - min || 1;

    /*
     * Render only significant regions of the scalar field.
     *
     * For an electromagnetic pulse, energy_density and
     * electric_magnitude naturally form expanding shells.
     *
     * Sampling more points than cloud mode gives the shell
     * enough continuity without trying to draw every voxel.
     */
    const targetSamples = 14000;

    const stride = Math.max(
      1,
      Math.ceil(Math.cbrt((m.nx * m.ny * m.nz) / targetSamples)),
    );

    const points = [];

    /*
     * Ignore weak field values. This prevents the entire
     * simulation cube from appearing as a translucent fog.
     *
     * Values are normalized using the currently selected
     * color scale.
     */
    const threshold = 0.1;

    for (let z = 0; z < m.nz; z += stride) {
      for (let y = 0; y < m.ny; y += stride) {
        for (let x = 0; x < m.nx; x += stride) {
          const index = (z * m.ny + y) * m.nx + x;

          const value = values[index];

          let intensity = (value - min) / range;

          intensity = Math.max(0, Math.min(1, intensity));

          if (intensity < threshold) continue;

          const p = project({
            x: (x + 0.5) / m.nx - 0.5,
            y: (y + 0.5) / m.ny - 0.5,
            z: (z + 0.5) / m.nz - 0.5,
          });

          points.push({
            ...p,
            value,
            intensity,
          });
        }
      }
    }

    /*
     * Painter's algorithm:
     * draw farther points first.
     */
    points.sort((a, b) => b.z - a.z);

    ctx.save();

    /*
     * Makes overlapping wavefront samples glow instead of
     * simply covering one another.
     */
    ctx.globalCompositeOperation = "lighter";

    for (const p of points) {
      const rgb = color((p.value - min) / range, palette);

      /*
       * Weak parts of the wave are transparent.
       * Strong regions become more visible.
       */
      const alpha = 0.05 + 0.5 * Math.pow(p.intensity, 1.7);

      /*
       * Perspective-aware point radius.
       */
      const radius = Math.max(
        0.7,
        (p.factor / Math.max(m.nx, m.ny, m.nz)) * (0.3 + 0.65 * p.intensity),
      );

      /*
       * Soft glow around the actual voxel sample.
       */
      ctx.fillStyle = `rgba(${rgb.join(",")},${alpha * 0.2})`;

      ctx.beginPath();

      ctx.arc(p.x, p.y, radius * 2.7, 0, 2 * Math.PI);

      ctx.fill();

      /*
       * Bright core.
       */
      ctx.fillStyle = `rgba(${rgb.join(",")},${alpha})`;

      ctx.beginPath();

      ctx.arc(p.x, p.y, radius, 0, 2 * Math.PI);

      ctx.fill();
    }

    ctx.restore();
  }

  return { scalar, render, bind, reset };
})();
