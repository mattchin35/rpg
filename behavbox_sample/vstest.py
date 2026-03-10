import time
from pathlib import Path

from session_info import DEFAULT_SCREEN_RESOLUTION, GRATINGS_DIR

import rpg

TEST_SEQUENCE = [
    ("horizontal_grating_1s.dat", "Expect horizontal grating"),
    ("vertical_grating_0.5s.dat", "Expect vertical grating"),
    ("horizontal_grating_0.5s.dat", "Expect horizontal grating"),
]


def require_file(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(
            f"Missing test asset {path}. Run behavbox_sample/make_gratings.py first."
        )


def main() -> None:
    print(f"Using grating directory: {GRATINGS_DIR}")
    print(f"Using screen resolution: {DEFAULT_SCREEN_RESOLUTION}")

    myscreen = rpg.Screen(resolution=DEFAULT_SCREEN_RESOLUTION, background=40)
    try:
        myscreen.display_greyscale(40)
        time.sleep(1)

        for filename, message in TEST_SEQUENCE:
            path = GRATINGS_DIR / filename
            require_file(path)
            print(message)
            grating = myscreen.load_grating(str(path))
            myscreen.display_grating(grating)
            myscreen.display_greyscale(40)
            time.sleep(1)

        myscreen.display_greyscale(0)
        time.sleep(1)
    finally:
        myscreen.close()


if __name__ == "__main__":
    main()
