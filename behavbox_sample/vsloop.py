import time

from session_info import make_session_info
from visualstim_concurrent import VisualStimMultiprocess, drain_presenter_queue


def alternate_process(t_stimulus: int):
    start = time.perf_counter()
    while time.perf_counter() - start < t_stimulus:
        print(f"{time.perf_counter() - start:.2f}s elapsed in parent process")
        time.sleep(0.5)


def run_multiprocessing():
    session_info = make_session_info()
    t_stimulus = session_info["stimulus_duration"]
    visualstim = VisualStimMultiprocess(session_info)
    grating_name = f"vertical_grating_{session_info['grating_duration']}s.dat"

    tstart = time.perf_counter()
    visualstim.loop_grating(grating_name, t_stimulus)
    alternate_process(t_stimulus)
    visualstim.end_gratings_process()

    print(f"Total time elapsed: {time.perf_counter() - tstart:.3f}s")
    print(drain_presenter_queue(visualstim.presenter_commands))


def main():
    run_multiprocessing()


if __name__ == "__main__":
    main()
