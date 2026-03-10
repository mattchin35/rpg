import os
from collections import OrderedDict
from pathlib import Path
from typing import Any, Dict

BASE_DIR = Path(__file__).resolve().parent
GRATINGS_DIR = BASE_DIR / "generated_gratings"
TEST_DURATIONS = [0.5, 1, 2]


def _get_env_int(name: str, default: int) -> int:
    raw = os.environ.get(name)
    if raw is None:
        return default
    return int(raw)


DEFAULT_SCREEN_RESOLUTION = (
    _get_env_int("RPG_TEST_WIDTH", 1280),
    _get_env_int("RPG_TEST_HEIGHT", 720),
)


def make_session_info() -> Dict[str, Any]:
    session_info = OrderedDict()
    session_info["gray_level"] = 40
    session_info["screen_resolution"] = DEFAULT_SCREEN_RESOLUTION
    session_info["grating_duration"] = 1
    session_info["inter_grating_interval"] = 1
    session_info["stimulus_duration"] = 10
    session_info["vis_gratings"] = [
        str(GRATINGS_DIR / f"vertical_grating_{duration}s.dat")
        for duration in TEST_DURATIONS
    ] + [
        str(GRATINGS_DIR / f"horizontal_grating_{duration}s.dat")
        for duration in TEST_DURATIONS
    ]
    return session_info


def main() -> None:
    print(make_session_info())


if __name__ == "__main__":
    main()
