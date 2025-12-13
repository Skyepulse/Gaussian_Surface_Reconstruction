import os
import logging
from argparse import ArgumentParser
import shutil

parser = ArgumentParser("Colmap converter txt to bin")
parser.add_argument("--input_path", "-i", required=True, type=str)
parser.add_argument("--output_path", "-o", required=False, type=str)

args = parser.parse_args()
input_path = args.input_path
output_path = args.output_path if args.output_path is not None else input_path

if not os.path.exists(input_path):
    print(f"Input path {input_path} does not exist. Exiting.")
    exit(1)

if not os.path.exists(output_path):
    os.makedirs(output_path)

# Run colmap model_converter
colmap_command = "colmap model_converter"
model_converter_cmd = (
    f'{colmap_command} '
    f'--input_path {input_path} '
    f'--output_path {output_path} '
    f'--output_type BIN'
)
exit_code = os.system(model_converter_cmd)
if exit_code != 0:
    print(f"Colmap model_converter failed with code {exit_code}. Exiting.")
    exit(exit_code)

print("Conversion from TXT to BIN completed successfully.")

