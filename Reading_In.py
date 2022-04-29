import re
import sys
import os.path
import numpy as np
import matplotlib.pyplot as plt

home = os.path.expanduser('~')


class FileFormatError(Exception):
    """Raised when one or more lines of the csv file do not match the expected format."""


def main():
    argc = len(sys.argv)
    if argc < 2:
        raise ValueError("Too few arguments provided.")
    elif argc > 3:
        raise ValueError("Too many arguments provided.")
    remove = False
    if argc == 3:
        if sys.argv[2] != "remove":
            raise ValueError("If a 3rd command-line argument is provided, it can only be \"remove\" (for removing the "
                             ".csv file after it has been plotted and the figure saved).")
        remove = True
    file_raw = sys.argv[1]
    if not os.path.exists(file_raw) or os.path.isdir(file_raw) or not file_raw.endswith(".csv"):
        raise FileNotFoundError("You have not provided a valid path to a .csv file.")
    file = os.path.abspath(file_raw)
    name = os.path.split(file)[1].strip(".csv")
    rgx = r"^\d+(\.\d+)?,\s*\d+(\.\d+)?\s*\r?\n?$"
    with open(file=file) as f:
        for count, line in enumerate(f, start=1):
            matched = re.fullmatch(pattern=rgx, string=line)
            if not matched:
                raise FileFormatError(f"Line number {count} does not match the expected format.")
    arr = np.loadtxt(file, delimiter=",")
    new = np.transpose(arr)
    fig = plt.figure(num="Data", figsize=(8, 4.5), dpi=170)
    axes = plt.axes()
    axes.plot(new[0], new[1])
    axes.set_title(f"Data for {name}")
    axes.set_xlabel("Time (s)")
    axes.set_ylabel("Voltage (V)")
    plt.show()
    exp_v = f"{home}/Exp_V_Figures"
    if not os.path.exists(exp_v):
        os.mkdir(exp_v)
    fig.savefig(f"{exp_v}/{name}.jpeg", dpi=200)
    if remove:
        os.remove(path=file)
    print(f"\nFigure saved to: {exp_v}/{name}.jpeg\n")


if __name__ == "__main__":
    main()
