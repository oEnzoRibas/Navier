"""Optional static viewer for the 2D example checkpoints; not a Python binding.
Usage: python examples/plot_2d.py heat2d.data fluid2d.data --output simulations.png
Requires numpy and matplotlib, outside the C++ build dependencies.
"""
import argparse
import json
from pathlib import Path
import struct

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_checkpoints(path):
    metadata = json.loads(Path(str(path) + ".json").read_text(encoding="utf-8"))
    frames = {}
    with Path(path).open("rb") as stream:
        if stream.read(8) != b"NAVIER01":
            raise ValueError("Invalid Navier file signature")
        file_size = Path(path).stat().st_size
        while stream.tell() < file_size:
            header = stream.read(24)
            if len(header) != 24:
                raise ValueError("Truncated record header")
            step, time, count = struct.unpack("<QdQ", header)
            if count > (file_size - stream.tell()) // 8:
                raise ValueError("Truncated record payload")
            expected = metadata["nx"] * metadata["ny"] * len(metadata["fields"])
            if count != expected:
                raise ValueError("Field layout does not match metadata")
            values = np.frombuffer(stream.read(count * 8), dtype="<f8").copy()
            if not np.all(np.isfinite(values)):
                raise ValueError("Nonfinite simulation values")
            frames[step] = (time, values.reshape(len(metadata["fields"]), metadata["ny"], metadata["nx"]))
    if not frames:
        raise ValueError("No checkpoints")
    return metadata, frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("heat", type=Path)
    parser.add_argument("fluid", type=Path)
    parser.add_argument("--output", type=Path, default=Path("simulations.png"))
    args = parser.parse_args()
    heat_meta, heat = read_checkpoints(args.heat)
    fluid_meta, fluid = read_checkpoints(args.fluid)
    if heat_meta["fields"] != ["temperature"] or fluid_meta["fields"] != ["density", "velocity_x", "velocity_y"]:
        raise ValueError("Expected heat and fluid producers in that order")
    fig, axes = plt.subplots(2, 2, figsize=(12, 10), constrained_layout=True)
    fig.suptitle("Navier | duas simulações 2D\nEstados recuperados dos arquivos de checkpoints", fontsize=17)
    heat_steps = [min(heat), max(heat)]
    heat_max = float(heat[heat_steps[0]][1][0].max())
    for axis, step in zip(axes[0], heat_steps):
        time, fields = heat[step]
        image = axis.imshow(fields[0], origin="lower", extent=(0, 1, 0, 1), cmap="inferno", vmin=0, vmax=heat_max)
        axis.set(title=f"Calor | passo {step} | t={time:.4f}", xlabel="x", ylabel="y")
        fig.colorbar(image, ax=axis, label="Temperatura (normalizada)", shrink=0.8)
    fluid_steps = [min(fluid), max(fluid)]
    initial = fluid[fluid_steps[0]][1]
    speed_max = float(np.hypot(initial[1], initial[2]).max())
    n = fluid_meta["nx"]
    xx, yy = np.meshgrid(np.arange(n), np.arange(fluid_meta["ny"]))
    stride = max(1, n // 12)
    selection = (slice(None, None, stride), slice(None, None, stride))
    for axis, step in zip(axes[1], fluid_steps):
        time, fields = fluid[step]
        speed = np.hypot(fields[1], fields[2])
        image = axis.imshow(speed, origin="lower", cmap="viridis", vmin=0, vmax=speed_max)
        axis.quiver(xx[selection], yy[selection], fields[1][selection], fields[2][selection],
                    color="white", scale=speed_max / (n / 20), scale_units="xy", angles="xy", width=0.005)
        axis.set(title=f"Fluido | passo {step} | t={time:.0f} (unid. lattice)", xlabel="x (células)", ylabel="y (células)")
        fig.colorbar(image, ax=axis, label="Módulo da velocidade (unid. lattice)", shrink=0.8)
    fig.savefig(args.output, dpi=160)
    plt.close(fig)
    heat_initial = heat[heat_steps[0]][1][0]
    heat_final = heat[heat_steps[1]][1][0]
    dt = heat[heat_steps[1]][0] - heat[heat_steps[0]][0]
    exact = heat_initial * np.exp(-2 * np.pi**2 * heat_meta["coefficient"] * dt)
    fluid_final = fluid[fluid_steps[1]][1]
    energy = lambda f: float((0.5 * f[0] * (f[1]**2 + f[2]**2)).sum())
    print(json.dumps({
        "heat_checkpoints": len(heat), "fluid_checkpoints": len(fluid),
        "heat_initial_peak": float(heat_initial.max()), "heat_final_peak": float(heat_final.max()),
        "heat_max_abs_error_vs_continuum": float(np.abs(heat_final - exact).max()),
        "fluid_initial_mass": float(initial[0].sum()), "fluid_final_mass": float(fluid_final[0].sum()),
        "fluid_initial_energy": energy(initial), "fluid_final_energy": energy(fluid_final),
        "figure": str(args.output),
    }, indent=2))


if __name__ == "__main__":
    main()
