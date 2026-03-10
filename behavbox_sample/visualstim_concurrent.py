import logging
import queue
import time
from multiprocessing import Process, Queue
from typing import Dict, Tuple

import _sample_path
from visualstim import VisualStim

import rpg


class VisualStimMultiprocess(VisualStim):
    def __init__(self, session_info):
        super().__init__(session_info)
        self.gratings_on = False
        self.presenter_commands = Queue()
        self.stimulus_commands = Queue()
        self.t_start = time.perf_counter()

    def stimulus_A_on(self) -> None:
        self._switch_or_start(
            f"vertical_grating_{self.session_info['grating_duration']}s.dat"
        )

    def stimulus_B_on(self) -> None:
        self._switch_or_start(
            f"horizontal_grating_{self.session_info['grating_duration']}s.dat"
        )

    def _switch_or_start(self, grating_name: str) -> None:
        if self.active_process is not None and self.active_process.is_alive():
            self.stimulus_commands.put(grating_name)
            print(f"queued switch to {grating_name}")
        else:
            self.loop_grating(grating_name, self.session_info["stimulus_duration"])
        self.gratings_on = True

    def display_default_greyscale(self):
        if self.active_process is not None and self.active_process.is_alive():
            self.stimulus_commands.put("default_greyscale")
        else:
            super().display_default_greyscale()
        self.gratings_on = False
        print("main process gratings off")

    def display_dark_greyscale(self):
        if self.active_process is not None and self.active_process.is_alive():
            self.stimulus_commands.put("dark_greyscale")
        else:
            super().display_dark_greyscale()
        self.gratings_on = False
        print("main process gratings off")

    def loop_grating(self, grating_name: str, stimulus_duration: float):
        logging.info(
            ";%s;[configuration];starting_process;%s;", time.time(), grating_name
        )
        if self.active_process is not None and self.active_process.is_alive():
            raise ValueError("A process is already running")

        self.close()
        self.active_process = Process(
            target=self._loop_grating_worker,
            args=(
                self.session_info,
                dict(self.grating_paths),
                grating_name,
                stimulus_duration,
                self.stimulus_commands,
                self.presenter_commands,
            ),
        )
        self.gratings_on = True
        self.t_start = time.perf_counter()
        self.active_process.start()

    @staticmethod
    def _loop_grating_worker(
        session_info,
        grating_paths: Dict[str, str],
        grating_name: str,
        stimulus_duration: float,
        in_queue: Queue,
        out_queue: Queue,
    ):
        logging.info(";%s;[stimulus];%s_loop_start;", time.time(), grating_name)
        myscreen = rpg.Screen(
            resolution=session_info["screen_resolution"],
            background=session_info["gray_level"],
        )
        try:
            gratings = {
                name: myscreen.load_grating(path)
                for name, path in grating_paths.items()
            }
            gratings_on = True
            t_start = time.perf_counter()
            while gratings_on and time.perf_counter() - t_start < stimulus_duration:
                out_queue.put(f"show:{grating_name}")
                myscreen.display_grating(gratings[grating_name])
                myscreen.display_greyscale(session_info["gray_level"])

                if time.perf_counter() - t_start >= stimulus_duration:
                    break

                try:
                    while True:
                        command = in_queue.get(block=False)
                        if command in grating_paths:
                            grating_name = command
                            t_start = time.perf_counter()
                        elif command == "default_greyscale":
                            myscreen.display_greyscale(session_info["gray_level"])
                            gratings_on = False
                            break
                        elif command == "dark_greyscale":
                            myscreen.display_greyscale(0)
                            gratings_on = False
                            break
                        elif command == "gratings_off":
                            gratings_on = False
                            break
                        else:
                            raise ValueError(f"Unknown command: {command}")
                except queue.Empty:
                    pass

                if gratings_on:
                    remaining = stimulus_duration - (time.perf_counter() - t_start)
                    if remaining > 0:
                        time.sleep(
                            min(session_info["inter_grating_interval"], remaining)
                        )

            out_queue.put("stimulus_process_done")
        finally:
            myscreen.close()

    def end_gratings_process(self):
        if self.active_process is not None and self.active_process.is_alive():
            self.stimulus_commands.put("gratings_off")
            self.active_process.join()
            print(f"full process time {time.perf_counter() - self.t_start:.3f}s")
        self.active_process = None
        self.gratings_on = False
        self.empty_stimulus_queue()


def drain_presenter_queue(presenter_commands: Queue) -> Tuple[str, ...]:
    messages = []
    try:
        while True:
            messages.append(presenter_commands.get(block=False))
    except queue.Empty:
        pass
    return tuple(messages)
