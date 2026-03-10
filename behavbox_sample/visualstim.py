import logging
import time
from collections import OrderedDict
from multiprocessing import Process, Queue
from pathlib import Path
from typing import Dict, Optional

import _sample_path
from base_classes import VisualStimBase

import rpg


class VisualStim(VisualStimBase):
    def __init__(self, session_info):
        self.session_info = session_info
        self.gratings: Dict[str, object] = OrderedDict()
        self.grating_paths: Dict[str, str] = OrderedDict()
        self.myscreen: Optional[rpg.Screen] = None
        self.active_process = None
        self.presenter_commands = Queue()
        self.stimulus_commands = Queue()
        self.load_session_gratings()
        logging.info(";%s;[initialization];visualstim_created;", time.time())

    def _ensure_screen(self) -> rpg.Screen:
        if self.myscreen is None:
            self.myscreen = rpg.Screen(
                resolution=self.session_info["screen_resolution"],
                background=self.session_info["gray_level"],
            )
        return self.myscreen

    def _ensure_loaded_grating(self, grating_name: str):
        if grating_name not in self.gratings:
            screen = self._ensure_screen()
            self.gratings[grating_name] = screen.load_grating(
                self.grating_paths[grating_name]
            )
        return self.gratings[grating_name]

    def display_default_greyscale(self):
        self._ensure_screen().display_greyscale(self.session_info["gray_level"])

    def display_dark_greyscale(self):
        self._ensure_screen().display_greyscale(0)

    def load_grating_file(self, grating_file: str):
        path = Path(grating_file).expanduser().resolve()
        logging.info(";%s;[initialization];loading_grating;%s;", time.time(), path)
        self.grating_paths[path.name] = str(path)
        print(f"registered {path.name}")

    def load_grating_dir(self, grating_directory):
        directory = Path(grating_directory).expanduser().resolve()
        for path in sorted(directory.iterdir()):
            if path.is_file():
                self.load_grating_file(str(path))

    def load_session_gratings(self):
        for filepath in self.session_info["vis_gratings"]:
            self.load_grating_file(filepath)

    def list_gratings(self):
        print(list(self.grating_paths))

    def clear_gratings(self):
        self.gratings = OrderedDict()
        print("cleared loaded gratings")

    def show_grating(self, grating_name):
        logging.info(";%s;[stimulus];%s_on;", time.time(), grating_name)
        self._ensure_screen().display_grating(self._ensure_loaded_grating(grating_name))
        self.display_default_greyscale()
        logging.info(";%s;[stimulus];%s_off;", time.time(), grating_name)

    def show_grating_async(self, grating_name):
        if self.active_process is not None and self.active_process.is_alive():
            raise ValueError("A process is already running")
        self.close()
        self.active_process = Process(
            target=self._show_grating_worker,
            args=(self.session_info, dict(self.grating_paths), grating_name),
        )
        self.active_process.start()

    @staticmethod
    def _show_grating_worker(session_info, grating_paths, grating_name):
        myscreen = rpg.Screen(
            resolution=session_info["screen_resolution"],
            background=session_info["gray_level"],
        )
        try:
            grating = myscreen.load_grating(grating_paths[grating_name])
            myscreen.display_grating(grating)
            myscreen.display_greyscale(session_info["gray_level"])
        finally:
            myscreen.close()

    def close(self):
        if self.myscreen is not None:
            self.myscreen.close()
            self.myscreen = None

    def __del__(self):
        self.close()

    def loop_grating(self, grating_name: str, duration: float):
        start = time.perf_counter()
        while time.perf_counter() - start < duration:
            self.show_grating(grating_name)
            remaining = duration - (time.perf_counter() - start)
            if remaining <= 0:
                break
            time.sleep(min(self.session_info["inter_grating_interval"], remaining))
