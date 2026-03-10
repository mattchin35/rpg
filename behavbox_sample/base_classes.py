import queue
from abc import ABC, abstractmethod
from multiprocessing import Process, Queue
from threading import Thread
from typing import List, Union


class VisualStimBase(ABC):
    gratings_on = False
    presenter_commands: Union[List[str], Queue]
    active_process: Union[Thread, Process, None]

    @abstractmethod
    def show_grating(self, grating_name: str): ...

    @abstractmethod
    def loop_grating(self, grating_name: str, duration: float): ...

    @abstractmethod
    def display_default_greyscale(self): ...

    def end_gratings_process(self):
        if self.active_process is not None:
            self.active_process.join()
        self.gratings_on = False

    def empty_stimulus_queue(self):
        commands = []
        while True:
            try:
                commands.append(self.stimulus_commands.get(block=False))
            except queue.Empty:
                break
        return commands

    def empty_presenter_queue(self):
        commands = []
        while True:
            try:
                commands.append(self.presenter_commands.get(block=False))
            except queue.Empty:
                break
        return commands
