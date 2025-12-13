import matplotlib.pyplot as plt
import numpy as np
import os
from argparse import ArgumentParser

parser = ArgumentParser("Output metrics visualization")
parser.add_argument("--input_path", "-i", required=True, type=str)
parser.add_argument("--output_path", "-o", required=False, type=str)

args = parser.parse_args()
input_path = args.input_path
output_path = args.output_path if args.output_path is not None else input_path
if not os.path.exists(output_path):
    os.makedirs(output_path)

# Check directories
directories = ['milo', 'sugar', 'triangle', 'triangle_2']
for directory in directories:
    dir_path = os.path.join(input_path, directory)
    if not os.path.exists(dir_path):
        print(f"Directory {dir_path} does not exist. Exiting.")
        exit(1)

# Metrics headers
global_metrics_headers = [
    'number of vertices:',
    'number of faces:',
    'number of unique edges:',
    'euler characteristic:',
    'number of boundary loops:',
    'number of boundary edges:',
    'average loop size:',
    'maximum loop size:',
    'minimum loop size:',
    'edge boundary ratio:',
    'number of connected components:',
    'largest component size:',
    'smallest component size:',
    'average component size:',
    'minimum face area:',
    'maximum face area:',
    'average face area:',
    'number of degenerate faces:',
    'area degeneracy ratio:',
    '5th percentile min face angle:',
    'max face angle:',
    'connected components distribution:',
    'boundary loops distribution:'
]

num_meshes_processed = 0

def read_metrics_file(file_path, index):
    returned_metrics = {}
    with open(file_path, 'r') as f:
        f.readline()
        line_num = 0
        for line in f:

            if 'LOOP DETAILS' in line:
                num_loops = returned_metrics.get('number of boundary loops:', 0)
                loop_sizes = []
                for _ in range(num_loops):
                    loop_line = f.readline()
                    parts = loop_line.strip().split()
                    if len(parts) == 4 and parts[0] == 'Loop' and parts[2] == 'size:':
                        loop_sizes.append(int(parts[3]))
                returned_metrics['connected components distribution:'] = loop_sizes
                continue

            if 'COMPONENT SIZES' in line:
                num_components = returned_metrics.get('number of connected components:', 0)
                component_sizes = []
                for _ in range(num_components):
                    comp_line = f.readline()
                    parts = comp_line.strip().split()
                    if len(parts) == 4 and parts[0] == 'Component' and parts[2] == 'size:':
                        component_sizes.append(int(parts[3]))
                returned_metrics['boundary loops distribution:'] = component_sizes
                continue

            if line_num >= len(global_metrics_headers):
                break

            parts = line.strip().split()
            if len(parts) >= 2:
                key = global_metrics_headers[line_num]
                line_num += 1
                value = parts[-1]
                returned_metrics[key] = int(value) if value.isdigit() else float(value)

    return returned_metrics

# Load metrics
all_metrics = []
mesh_names = []

for directory in directories:
    dir_path = os.path.join(input_path, directory)
    subdirs = [d for d in os.listdir(dir_path) if os.path.isdir(os.path.join(dir_path, d))]
    for subdir in subdirs:
        subdir_path = os.path.join(dir_path, subdir)
        txt_files = [f for f in os.listdir(subdir_path) if f.endswith('.txt')]
        if len(txt_files) != 1:
            print(f"Expected exactly one .txt file in {subdir_path}")
            exit(1)

        metrics = read_metrics_file(os.path.join(subdir_path, txt_files[0]), num_meshes_processed)

        all_metrics.append(metrics)
        mesh_names.append(subdir[:-8] if subdir.endswith("_metrics") else subdir)
        num_meshes_processed += 1

# =========================
# Plotting utilities
# =========================

def safe_get(metrics, key, default=0):
    return metrics[key] if key in metrics else default

# =========================
# Per-mesh plots
# =========================

per_mesh_dir = os.path.join(output_path, "per_mesh")
os.makedirs(per_mesh_dir, exist_ok=True)

for i, metrics in enumerate(all_metrics):
    fig, axes = plt.subplots(2, 1, figsize=(8, 8))
    fig.suptitle(mesh_names[i])

    # Boundary loop sizes (histogram)
    loop_sizes = safe_get(metrics, 'connected components distribution:', [])
    if loop_sizes:
        axes[0].hist(
            loop_sizes,
            bins=20,
            edgecolor='black',
            linewidth=0.5
        )
        axes[0].set_title("Boundary loop size distribution")
        axes[0].set_xlabel("Loop size")
        axes[0].set_ylabel("Count")
    else:
        axes[0].text(0.5, 0.5, "No boundary loops", ha='center')

    # Connected component sizes (ECDF)
    comp_sizes = safe_get(metrics, 'boundary loops distribution:', [])
    if comp_sizes:
        sizes = np.sort(np.array(comp_sizes))
        y = np.arange(1, len(sizes) + 1) / len(sizes)

        axes[1].step(sizes, y, where="post")
        axes[1].set_xscale("log")
        axes[1].set_title("Connected component size ECDF")
        axes[1].set_xlabel("Component size (faces)")
        axes[1].set_ylabel("Fraction ≤ x")
    else:
        axes[1].text(0.5, 0.5, "No components", ha='center')

    plt.tight_layout()
    plt.savefig(os.path.join(per_mesh_dir, f"{mesh_names[i]}_diagnostics.png"))
    plt.close(fig)

# =========================
# Global comparison plots
# =========================

global_dir = os.path.join(output_path, "global")
os.makedirs(global_dir, exist_ok=True)

fig = plt.figure(figsize=(18, 10))
gs = fig.add_gridspec(2, 2)

# Panel A — Size / complexity
ax_a = fig.add_subplot(gs[0, 0])
size_metrics = [
    'number of vertices:',
    'number of faces:',
    'number of unique edges:',
    'largest component size:'
]

x = np.arange(len(size_metrics))
width = 0.15

for i, metrics in enumerate(all_metrics):
    ax_a.bar(
        x + i * width,
        [safe_get(metrics, k) for k in size_metrics],
        width,
        label=mesh_names[i]
    )

ax_a.set_yscale("log")
ax_a.set_xticks(x + width * (len(all_metrics) - 1) / 2)
ax_a.set_xticklabels([k.replace(':', '') for k in size_metrics], rotation=20)
ax_a.set_title("Size / complexity (log)")
ax_a.legend()

# Panel B — Topology
ax_b = fig.add_subplot(gs[0, 1])
topo_metrics = [
    'euler characteristic:',
    'number of connected components:',
    'number of boundary loops:'
]

x = np.arange(len(topo_metrics))
for i, metrics in enumerate(all_metrics):
    ax_b.plot(
        x,
        [safe_get(metrics, k) for k in topo_metrics],
        marker='o',
        label=mesh_names[i]
    )

ax_b.set_xticks(x)
ax_b.set_xticklabels([k.replace(':', '') for k in topo_metrics], rotation=20)
ax_b.set_title("Topology")
ax_b.legend()

# Panel C — Ratios
ax_c = fig.add_subplot(gs[1, 0])
ratio_metrics = [
    'edge boundary ratio:',
    'area degeneracy ratio:'
]

x = np.arange(len(ratio_metrics))
for i, metrics in enumerate(all_metrics):
    ax_c.plot(
        x,
        [safe_get(metrics, k) for k in ratio_metrics],
        marker='o',
        label=mesh_names[i]
    )

ax_c.set_xticks(x)
ax_c.set_xticklabels([k.replace(':', '') for k in ratio_metrics], rotation=20)
ax_c.set_title("Ratio-based metrics")
ax_c.legend()

# Panel D — Degeneracy + summaries
ax_d = fig.add_subplot(gs[1, 1])
summary_metrics = [
    'number of degenerate faces:',
    'average component size:',
    'average loop size:',
    'maximum loop size:'
]

x = np.arange(len(summary_metrics))
for i, metrics in enumerate(all_metrics):
    ax_d.plot(
        x,
        [safe_get(metrics, k) for k in summary_metrics],
        marker='s',
        label=mesh_names[i]
    )

ax_d.set_xticks(x)
ax_d.set_xticklabels([k.replace(':', '') for k in summary_metrics], rotation=20)
ax_d.set_title("Degeneracy and distribution summaries")
ax_d.legend()

plt.tight_layout()
plt.savefig(os.path.join(global_dir, "mesh_comparison.png"))
plt.close(fig)