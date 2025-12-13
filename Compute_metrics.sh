#!/usr/bin/env bash

# Compute_metrics.sh generates all metrics for a Mesh folder
#
# Usage: ./compute_metrics.sh [Mesh_Folder] [Output_Folder]

set -e

# -----------------------------
# Fetch input arguments
# -----------------------------
Mesh_Folder="$1"
Output_Folder="$2"

# -----------------------------
# Check executable existence
# -----------------------------
EXEC="./off_polyscope/build/off_viewer"

if [ ! -x "$EXEC" ]; then
    echo "Error: Executable off_viewer not found in ./off_polyscope/build/off_viewer"
    exit 1
fi

# -----------------------------
# Check if input arguments are provided
# -----------------------------
if [ -z "$Mesh_Folder" ]; then
    echo "Error: Mesh_Folder argument is missing."
    echo "Usage: ./compute_metrics.sh [Mesh_Folder] [Output_Folder]"
    exit 1
fi

if [ -z "$Output_Folder" ]; then
    echo "Error: Output_Folder argument is missing."
    echo "Usage: ./compute_metrics.sh [Mesh_Folder] [Output_Folder]"
    exit 1
fi

# -----------------------------
# Create output folder if needed
# -----------------------------
mkdir -p "$Output_Folder"

# -----------------------------
# Check if Mesh_Folder exists
# -----------------------------
if [ ! -d "$Mesh_Folder" ]; then
    echo "Error: Mesh_Folder \"$Mesh_Folder\" does not exist."
    exit 1
fi

# -----------------------------
# Required subfolders
# -----------------------------
for sub in milo sugar triangle triangle_2; do
    if [ ! -d "$Mesh_Folder/$sub" ]; then
        echo "Error: Subfolder \"$sub\" not found in \"$Mesh_Folder\"."
        exit 1
    fi
done

# -----------------------------
# Process mesh files
# -----------------------------
for sub in milo sugar triangle triangle_2; do
    mkdir -p "$Output_Folder/$sub"

    for mesh_file in \
        "$Mesh_Folder/$sub"/*.off \
        "$Mesh_Folder/$sub"/*.ply \
        "$Mesh_Folder/$sub"/*.obj; do

        # Skip if glob didn't match anything
        [ -e "$mesh_file" ] || continue

        file_name="$(basename "$mesh_file")"
        name_no_ext="${file_name%.*}"

        # Skip point_cloud.ply
        if [ "$name_no_ext" = "point_cloud" ]; then
            echo "Skipping \"$mesh_file\" as it is named point_cloud.ply"
            continue
        fi

        output_metrics_folder="$Output_Folder/$sub/${name_no_ext}_metrics"

        echo "Processing \"$mesh_file\"..."
        "$EXEC" "$mesh_file" "$output_metrics_folder"
    done
done

echo "All metrics computed and saved in \"$Output_Folder\"."