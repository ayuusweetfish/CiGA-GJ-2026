#!/usr/bin/env python3
"""
Open Images chain montage.
Given a chain file (list.txt) that describes a sequence of bounding boxes
across images, download the images and create a montage with boxes, labels,
and connecting lines between shared objects.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from io import BytesIO

import boto3
import botocore
from PIL import Image
import tqdm

BUCKET_NAME = "open-images-dataset"
SPLITS = ["train", "validation", "test"]  # order to try


# ----------------------------------------------------------------------
#  Parse list.txt
# ----------------------------------------------------------------------
def parse_chain_file(path):
    """Return a list of dicts with keys:
        image_id, bbox_left, bbox_right, label_right, label_left
    bbox is (xmin, xmax, ymin, ymax) or None.
    """
    items = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(" -> ")
            if len(parts) == 3:
                bbox1_str, img_id, rest = parts
                if " | " in rest:
                    bbox2_str, label = rest.split(" | ")
                else:
                    bbox2_str, label = rest, None
            elif len(parts) == 2:
                bbox1_str, img_id = parts
                bbox2_str, label = None, None
            else:
                raise ValueError(f"Cannot parse line: {line}")

            def parse_bbox(s):
                if s is None:
                    return None
                nums = [float(x) for x in s.split()]
                if len(nums) != 4:
                    raise ValueError(f"Wrong bbox format: {s}")
                # treat all-zeros as None
                if all(v == 0.0 for v in nums):
                    return None
                return tuple(nums)

            bbox_left = parse_bbox(bbox1_str)
            bbox_right = parse_bbox(bbox2_str)

            items.append(
                {
                    "image_id": img_id,
                    "bbox_left": bbox_left,
                    "bbox_right": bbox_right,
                    "label_right": label,
                    "label_left": None,  # filled later
                }
            )

    # Inherit labels along the chain
    for i in range(1, len(items)):
        prev_label = items[i - 1]["label_right"]
        if items[i]["bbox_left"] is not None:
            items[i]["label_left"] = prev_label
    return items


# ----------------------------------------------------------------------
#  Download images (with fallback over splits)
# ----------------------------------------------------------------------
def download_image(image_id, download_dir, bucket, splits):
    """Try to download an image from each split.  Return the split used, or raise."""
    for split in splits:
        key = f"{split}/{image_id}.jpg"
        dest = os.path.join(download_dir, f"{image_id}.jpg")
        if os.path.exists(dest):
            return split  # already there
        try:
            bucket.download_file(key, dest)
            return split
        except botocore.exceptions.ClientError as e:
            # If not 404, maybe other error – we'll abort on anything else
            if e.response["Error"]["Code"] != "404":
                raise
    raise RuntimeError(
        f"Could not download {image_id} from any split {splits}"
    )


def download_all_images(items, download_dir, splits, num_workers):
    bucket = boto3.resource(
        "s3",
        config=botocore.config.Config(signature_version=botocore.UNSIGNED),
    ).Bucket(BUCKET_NAME)

    os.makedirs(download_dir, exist_ok=True)

    # Collect unique image IDs to download
    unique_ids = list({it["image_id"] for it in items})
    print(f"Downloading {len(unique_ids)} images…")
    from concurrent.futures import ThreadPoolExecutor, as_completed

    with ThreadPoolExecutor(max_workers=num_workers) as executor:
        futures = {
            executor.submit(
                download_image, img_id, download_dir, bucket, splits
            ): img_id
            for img_id in unique_ids
        }
        for future in tqdm.tqdm(
            as_completed(futures), total=len(futures), desc="Downloading"
        ):
            try:
                future.result()
            except Exception as e:
                img_id = futures[future]
                sys.exit(f"Failed to download {img_id}: {e}")


# ----------------------------------------------------------------------
#  Montage creation
# ----------------------------------------------------------------------
def get_image_size(path):
    """Return (width, height) using PIL."""
    with Image.open(path) as im:
        return im.size


def create_montage(items, download_dir, output_path, height, gap):
    # Resize images to uniform height
    print("Resizing images…")
    resized_dir = tempfile.mkdtemp(prefix="chain_resized_")
    resized_files = []
    widths = []
    for idx, it in enumerate(items):
        img_id = it["image_id"]
        src = os.path.join(download_dir, f"{img_id}.jpg")
        if not os.path.exists(src):
            raise FileNotFoundError(f"Image not found: {src}")
        dst = os.path.join(resized_dir, f"{idx:04d}.jpg")
        subprocess.run(
            ["magick", src, "-resize", f"x{height}", dst], check=True
        )
        w, h = get_image_size(dst)
        if h != height:
            raise RuntimeError(f"Resize failed for {src} (expected h={height})")
        resized_files.append(dst)
        widths.append(w)

    # Montage: tile all images in one row with gap
    print("Creating montage strip…")
    montage_args = [
        "montage",
        "-mode",
        "concatenate",
        "-tile",
        "x1",
        "-geometry",
        f"+{gap}+0",
    ]
    montage_args += resized_files
    montage_args.append("montage_temp.jpg")
    subprocess.run(montage_args, check=True)

    # Calculate x-offsets of each image in the montage
    offsets = []
    cum = 0
    for w in widths:
        offsets.append(cum)
        cum += w + gap

    # Alternating colours for each pair of connected boxes
    color_cycle = ["blue", "red"]
    shape_cmds = []   # list of (color, draw_string)
    text_cmds = []    # list of (color, draw_string)

    # Connection lines (pair idx = i-1)
    for i in range(1, len(items)):
        prev = items[i - 1]
        curr = items[i]
        color = color_cycle[(i - 1) % len(color_cycle)]
        if prev["bbox_right"] and curr["bbox_left"]:
            x1_prev = offsets[i - 1] + prev["bbox_right"][1] * widths[i - 1]
            y_centre_prev = (
                (prev["bbox_right"][2] + prev["bbox_right"][3]) / 2 * height
            )
            x2_curr = offsets[i] + curr["bbox_left"][0] * widths[i]
            y_centre_curr = (
                (curr["bbox_left"][2] + curr["bbox_left"][3]) / 2 * height
            )
            shape_cmds.append((
                color,
                f"line {x1_prev:.1f},{y_centre_prev:.1f} "
                f"{x2_curr:.1f},{y_centre_curr:.1f}"
            ))

    # Bounding boxes and text — colour by which pair they belong to
    for i, it in enumerate(items):
        ox = offsets[i]
        w = widths[i]
        # left box belongs to pair (i-1) if i > 0
        if i > 0 and it["bbox_left"]:
            xmin, xmax, ymin, ymax = it["bbox_left"]
            x1 = ox + xmin * w
            x2 = ox + xmax * w
            y1 = ymin * height
            y2 = ymax * height
            color = color_cycle[(i - 1) % len(color_cycle)]
            shape_cmds.append((color, f"rectangle {x1:.1f},{y1:.1f} {x2:.1f},{y2:.1f}"))
            if it["label_left"]:
                text_y = max(14, y1 - 4)  # clamp inside image
                text_cmds.append((
                    color,
                    f"text {x1:.1f},{text_y:.1f} '{it['label_left']}'"
                ))
        # right box belongs to pair i if not the last image
        if i < len(items) - 1 and it["bbox_right"]:
            xmin, xmax, ymin, ymax = it["bbox_right"]
            x1 = ox + xmin * w
            x2 = ox + xmax * w
            y1 = ymin * height
            y2 = ymax * height
            color = color_cycle[i % len(color_cycle)]
            shape_cmds.append((color, f"rectangle {x1:.1f},{y1:.1f} {x2:.1f},{y2:.1f}"))
            if it["label_right"]:
                text_y = max(14, y1 - 4)  # clamp inside image
                text_cmds.append((
                    color,
                    f"text {x1:.1f},{text_y:.1f} '{it['label_right']}'"
                ))

    # Draw shapes (lines + rectangles) with per-shape colours
    print("Adding boxes, labels and lines…")
    cmd_shape = ["magick", "montage_temp.jpg"]
    current_color = None
    for color, draw_str in shape_cmds:
        if color != current_color:
            cmd_shape += ["-fill", "none", "-stroke", color, "-strokewidth", "2"]
            current_color = color
        cmd_shape += ["-draw", draw_str]
    shape_out = "montage_shapes.jpg"
    cmd_shape.append(shape_out)
    subprocess.run(cmd_shape, check=True)

    # Draw text with per-pair colours
    cmd_text = [
        "magick",
        shape_out,
        "-stroke", "none",
        "-pointsize", "14",
    ]
    current_color = None
    for color, draw_str in text_cmds:
        if color != current_color:
            cmd_text += ["-fill", color]
            current_color = color
        cmd_text += ["-draw", draw_str]
    cmd_text.append(output_path)
    subprocess.run(cmd_text, check=True)

    # Cleanup
    os.remove("montage_temp.jpg")
    os.remove(shape_out)
    import shutil
    shutil.rmtree(resized_dir)

    print(f"Montage saved to {output_path}")


# ----------------------------------------------------------------------
#  Main
# ----------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("chain_file", help="Path to list.txt")
    parser.add_argument(
        "--download-dir",
        default="downloaded_images",
        help="Directory to store downloaded images (default: downloaded_images)",
    )
    parser.add_argument(
        "--output",
        default="chain_montage.jpg",
        help="Output image file (default: chain_montage.jpg)",
    )
    parser.add_argument(
        "--height",
        type=int,
        default=300,
        help="Height of each image in the montage (default: 300)",
    )
    parser.add_argument(
        "--gap",
        type=int,
        default=20,
        help="Horizontal gap between images (default: 20)",
    )
    parser.add_argument(
        "--splits",
        nargs="+",
        default=SPLITS,
        help=f"Open Images splits to try, in order (default: {' '.join(SPLITS)})",
    )
    parser.add_argument(
        "--num-workers",
        type=int,
        default=5,
        help="Parallel download threads (default: 5)",
    )
    args = parser.parse_args()

    # 1. Parse
    items = parse_chain_file(args.chain_file)
    print(f"Parsed {len(items)} images from chain.")

    # 2. Download
    download_all_images(
        items, args.download_dir, args.splits, args.num_workers
    )

    # 3. Montage
    create_montage(
        items, args.download_dir, args.output, args.height, args.gap
    )


if __name__ == "__main__":
    main()
