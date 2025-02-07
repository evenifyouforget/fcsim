"""
Generate header include guards if not present
"""

import argparse
import logging
import pathlib

def check_guard(path: pathlib.Path) -> bool:
    try:
        with open(path, "r") as file:
            for line in file:
                if line.strip().startswith("#ifndef"):
                    # probably a guard line
                    return True
                if any(sub in line for sub in (';', '{', '(')):
                    # probably a code line
                    return False
    except OSError:
        raise ValueError(f"Could not read from file {path}")
    raise ValueError("File does not appear to be a C header file")

def add_guard(path: pathlib.Path) -> None:
    guard_name = path.stem.replace('.','_').replace('-','_').upper() + '_H'
    with open(path, "r") as file:
        raw = file.read()
    raw = f"""#ifndef {guard_name}
#define {guard_name}

""" + raw + """

#endif"""
    with open(path, "w") as file:
        file.write(raw)

def main() -> None:
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(
                    prog='add_guard.py',
                    description='Check if the specified files have an include guard, add it if not present. Might sometimes get it wrong for difficult cases')
    parser.add_argument('-d', '--dry', help="Don't actually modify files")
    parser.add_argument('filename', nargs='*', help="Files to check and modify")
    args = parser.parse_args()
    flag_dry = args.dry

    files = args.filename
    files = [pathlib.Path(f) for f in files]

    logging.info(f'Checking {len(files)} files')
    files = [f for f in files if not check_guard(f)]

    for file in files:
        logging.info(f'Writing {file}')
        if not flag_dry:
            add_guard(file)

if __name__ == "__main__":
    main()