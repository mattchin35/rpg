import collections
import socket
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Tuple, Union

import numpy as np
import pandas as pd
from icecream import ic


# Parameters: mouse, session type
def make_session_info() -> Dict[str, Any]:
    # Information for this session (the user should edit this each session)
    session_info = collections.OrderedDict()
    session_info["ephys_rig"] = True  # determines reward pumps and ssh IPs

    # Parameters - box and rig
    session_info["box_name"] = socket.gethostname()
    gratings_dir = "/home/pi/gratings"  # './dummy_vis'

    if session_info["task_config"] in ["latent_inference_with_stimuli", "flush"]:
        session_info["visual_stimulus"] = True
    else:
        session_info["visual_stimulus"] = False

    if session_info["visual_stimulus"]:
        session_info["gray_level"] = (
            40  # the pixel value from 0-255 for the screen between stimuli
        )
        times = [0.5, 1]  # , 2]
        session_info["vis_gratings"] = [
            "vertical_grating_{}s.dat".format(t) for t in times
        ] + ["horizontal_grating_{}s.dat".format(t) for t in times]
        session_info["vis_gratings"] = [
            gratings_dir + "/" + g for g in session_info["vis_gratings"]
        ]
        session_info["vis_raws"] = []
        session_info["counterbalance_type"] = "rightA"  # 'leftA', 'rightA'
        session_info["grating_duration"] = 1
        session_info["inter_grating_interval"] = 2
        session_info["stimulus_duration"] = 10
        session_info["p_stimulus"] = 0.5
        session_info["num_sounds"] = 1

    session_info = session_defaults(session_info)
    session_info = sanity_checks(session_info)
    return session_info


def session_defaults(session_info: dict) -> dict:
    if session_info["task_config"] == "flush":
        ic("Defaulting intertrial interval to 4 seconds")
        session_info["intertrial_interval"] = 4  # in seconds
        session_info["use_dark_period"] = True

    elif session_info["task_config"] == "alternating_latent":
        ic("Defaulting intertrial interval to 2 seconds")
        session_info["intertrial_interval"] = 2  # in seconds

    if session_info["debug"]:
        pass

    return session_info


def sanity_checks(session_info: dict) -> dict:
    assert session_info["task_config"] in [
        "alternating_latent",
        "latent_inference",
        "flush",
        "latent_inference_with_stimuli",
    ], "Invalid task config, check your spelling!!"

    if session_info["visual_stimulus"]:
        assert session_info["vis_gratings"], "No visual stimuli specified"
        assert session_info["counterbalance_type"], "No counterbalance type specified"
        assert session_info["task_config"] in [
            "latent_inference_with_stimuli",
            "flush",
        ], "Invalid task config for stimulus task"
        assert (
            session_info["grating_duration"] + session_info["inter_grating_interval"]
            <= session_info["intertrial_interval"]
        ), "Intertrial interval too short for visual stimuli"
        assert session_info["grating_duration"] + session_info[
            "inter_grating_interval"
        ] < np.amin(session_info["dark_period_times"]), (
            "Intertrial interval too short for dark period"
        )
        assert session_info["num_sounds"] in [1, 2], "Invalid number of sounds"
        assert session_info["use_dark_period"], (
            "Invalid visual stimulus setting - must use dark periods for visual stimulus task!!"
        )

    return session_info


def get_solenoid_coefficients():
    df_calibration = pd.read_csv(
        "~/experiment_info/calibration_info/calibration_hardcode.csv"
    )
    # df_calibration = pd.read_csv(r"C:\Users\mattc\Documents\RPi_clone\calibration_hardcode.csv")
    pump_coefficient = {}
    for ix in df_calibration.index:
        pump_coefficient[str(df_calibration.loc[ix, "pump_number"])] = [
            df_calibration.loc[ix, "slope"],
            df_calibration.loc[ix, "intercept"],
        ]

    return pump_coefficient


def main():
    session_info = make_session_info()
    ic(session_info["calibration_coefficient"])


if __name__ == "__main__":
    main()
