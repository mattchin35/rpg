import _sample_path
from session_info import DEFAULT_SCREEN_RESOLUTION, GRATINGS_DIR, TEST_DURATIONS

import rpg

REPO_ROOT = _sample_path.REPO_ROOT


def build_test_gratings() -> None:
    GRATINGS_DIR.mkdir(parents=True, exist_ok=True)

    for duration in TEST_DURATIONS:
        vertical_options = {
            "duration": duration,
            "angle": 90,
            "spac_freq": 0.2,
            "temp_freq": 1,
            "resolution": DEFAULT_SCREEN_RESOLUTION,
        }
        rpg.build_grating(
            str(GRATINGS_DIR / f"vertical_grating_{duration}s.dat"),
            vertical_options,
        )

        horizontal_options = {
            "duration": duration,
            "angle": 0,
            "spac_freq": 0.2,
            "temp_freq": 1,
            "resolution": DEFAULT_SCREEN_RESOLUTION,
        }
        rpg.build_grating(
            str(GRATINGS_DIR / f"horizontal_grating_{duration}s.dat"),
            horizontal_options,
        )


def main() -> None:
    print(f"Building test gratings in {GRATINGS_DIR}")
    print(f"Using resolution {DEFAULT_SCREEN_RESOLUTION}")
    build_test_gratings()
    print("Done")


if __name__ == "__main__":
    main()
