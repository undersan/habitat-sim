# ---
# jupyter:
#   accelerator: GPU
#   colab:
#     name: Rigid Object Tutorial
#     provenance: []
#   jupytext:
#     cell_metadata_filter: -all
#     formats: nb_python//py:percent,colabs//ipynb
#     notebook_metadata_filter: all
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.6.0
#   kernelspec:
#     display_name: Python 3
#     name: python3
# ---

# %%
# !curl -L https://raw.githubusercontent.com/facebookresearch/habitat-sim/master/examples/colab_utils/colab_install.sh | NIGHTLY=true bash -s

# %%
# %cd /content/habitat-sim
## [setup]
import os
import random
import sys

import git
import numpy as np
import magnum as mn

import habitat_sim
from habitat_sim.utils import viz_utils as vut

if "google.colab" in sys.modules:
    os.environ["IMAGEIO_FFMPEG_EXE"] = "/usr/bin/ffmpeg"

repo = git.Repo(".", search_parent_directories=True)
dir_path = repo.working_tree_dir
# %cd $dir_path
data_path = os.path.join(dir_path, "data")
output_path = os.path.join(dir_path, "examples/tutorials/replay_tutorial_output/")


def remove_all_objects(sim):
    for id_ in sim.get_existing_object_ids():
        sim.remove_object(id_)


def place_agent(sim):
    # place our agent in the scene
    agent_state = habitat_sim.AgentState()
    agent_state.position = [-0.15, -0.7, 1.0]
    agent_state.rotation = np.quaternion(-0.83147, 0, 0.55557, 0)
    agent = sim.initialize_agent(0, agent_state)
    return agent.scene_node.transformation_matrix()

def render_replay_add_agent_user_transform(sim):
    agent = sim.get_agent(0)
    sim.render_replay.add_user_transform_to_keyframe(
        "agent",
        agent.body.object.translation,
        agent.body.object.rotation
    )

def render_replay_set_agent_from_user_transform(sim):
    agent = sim.get_agent(0)
    (agent_translation, agent_rotation) = sim.render_replay.player_get_user_transform("agent")
    agent.body.object.translation = agent_translation
    agent.body.object.rotation = agent_rotation

def make_backend_configuration_for_render_replay_playback(need_separate_semantic_scene_graph=False):

    backend_cfg = habitat_sim.SimulatorConfiguration()
    backend_cfg.scene_id = "NONE"  # see Asset.h EMPTY_SCENE
    backend_cfg.force_separate_semantic_scene_graph = need_separate_semantic_scene_graph

    return backend_cfg


def make_configuration():
    # simulator configuration
    backend_cfg = habitat_sim.SimulatorConfiguration()
    backend_cfg.scene_id = os.path.join(
        data_path, 
        # "scene_datasets/habitat-test-scenes/apartment_1.glb"
        "/home/eundersander/projects/matterport/v1/tasks/mp3d_habitat/mp3d/D7G3Y4RVNrH/D7G3Y4RVNrH.glb"
    )
    assert os.path.exists(backend_cfg.scene_id)
    backend_cfg.enable_physics = True
    backend_cfg.enable_render_replay_save = True

    # sensor configurations
    # Note: all sensors must have the same resolution
    # setup 2 rgb sensors for 1st and 3rd person views
    camera_resolution = [544, 720]
    sensors = {
        "rgba_camera_1stperson": {
            "sensor_type": habitat_sim.SensorType.COLOR,
            "resolution": camera_resolution,
            "position": [0.0, 0.6, 0.0],
            "orientation": [0.0, 0.0, 0.0],
        },
        "semantic_camera": {
            "sensor_type": habitat_sim.SensorType.SEMANTIC,
            "resolution": camera_resolution,
            "position": [0.0, 0.6, 0.0],
            "orientation": [0.0, 0.0, 0.0],
        },
    }

    sensor_specs = []
    for sensor_uuid, sensor_params in sensors.items():
        sensor_spec = habitat_sim.SensorSpec()
        sensor_spec.uuid = sensor_uuid
        sensor_spec.sensor_type = sensor_params["sensor_type"]
        sensor_spec.resolution = sensor_params["resolution"]
        sensor_spec.position = sensor_params["position"]
        sensor_spec.orientation = sensor_params["orientation"]
        sensor_specs.append(sensor_spec)

    # agent configuration
    agent_cfg = habitat_sim.agent.AgentConfiguration()
    agent_cfg.sensor_specifications = sensor_specs

    return habitat_sim.Configuration(backend_cfg, [agent_cfg])


def simulate_with_moving_agent(
    sim, duration=1.0, agent_vel=np.array([0, 0, 0]), get_frames=True
):

    # simulate dt seconds at 60Hz to the nearest fixed timestep
    agent = sim.get_agent(0)
    time_step = 1.0 / 60.0
    print("Simulating " + str(duration) + " world seconds.")
    observations = []
    start_time = sim.get_world_time()
    while sim.get_world_time() < start_time + duration:
        agent.body.object.translation += agent_vel * time_step
        render_replay_add_agent_user_transform(sim)
        sim.step_physics(time_step)
        if get_frames:
            observations.append(sim.get_sensor_observations())

    return observations


# [/setup]
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--no-show-video", dest="show_video", action="store_false")
    parser.add_argument("--no-make-video", dest="make_video", action="store_false")
    parser.set_defaults(show_video=True, make_video=True)
    args, _ = parser.parse_known_args()
    show_video = args.show_video
    make_video = args.make_video
    if make_video and not os.path.exists(output_path):
        os.mkdir(output_path)

    cfg = make_configuration()
    sim = None
    random.seed(0)
    replay_filepaths = []
    num_episodes = 1

    # %%
    # @title Run three episodes. Save videos and replays.

    # for episode_index in range(num_episodes):
    if False:  # temp disable recording

        if not sim:
            sim = habitat_sim.Simulator(cfg)
        else:
            sim.reconfigure(cfg)
        place_agent(sim)
        observations = []

        # simulate with empty scene
        observations += simulate_with_moving_agent(
            sim,
            duration=1.0,
            agent_vel=np.array([0.5, 0.0, 0.0]),
            get_frames=make_video,
        )

        # get the physics object attributes manager
        obj_templates_mgr = sim.get_object_template_manager()

        obj_templates_mgr.load_configs(str(os.path.join(data_path, "objects")))
        chefcan_template_handle = obj_templates_mgr.get_template_handles(
            "data/objects/chefcan"
        )[0]

        y_dist = 0.1

        # drop some dynamic objects
        id_1 = sim.add_object_by_handle(chefcan_template_handle)
        sim.set_translation(
            np.array([2.4, -0.64 + random.uniform(-y_dist, y_dist), 0]), id_1
        )
        id_2 = sim.add_object_by_handle(chefcan_template_handle)
        sim.set_translation(
            np.array([2.4, -0.64 + random.uniform(-y_dist, y_dist), 0.28]), id_2
        )
        id_3 = sim.add_object_by_handle(chefcan_template_handle)
        sim.set_translation(
            np.array([2.4, -0.64 + random.uniform(-y_dist, y_dist), -0.28]), id_3
        )

        # simulate
        observations += simulate_with_moving_agent(
            sim, duration=2.0, agent_vel=np.array([0.0, -0.4, 0.0]), get_frames=True
        )

        # remove some objects
        sim.remove_object(id_1)
        sim.remove_object(id_2)

        observations += simulate_with_moving_agent(
            sim, duration=2.0, agent_vel=np.array([0.4, 0.0, 0.0]), get_frames=True
        )

        episodeName = "episode{}".format(episode_index)

        if make_video:
            vut.make_video(
                observations,
                "rgba_camera_1stperson",
                "color",
                output_path + episodeName,
                open_vid=show_video,
            )

        # save a replay at the end of an episode
        replay_filepath = output_path + episodeName + ".json"
        sim.render_replay.write_saved_keyframes_to_file(replay_filepath)
        replay_filepaths.append(replay_filepath)

        remove_all_objects(sim)

    # %%
    # @title Play replays.

    # use same agents/sensors from earlier, with different backend config
    playback_cfg = habitat_sim.Configuration(
        make_backend_configuration_for_render_replay_playback(need_separate_semantic_scene_graph=True),
        cfg.agents)

    if not sim:
        sim = habitat_sim.Simulator(playback_cfg)
    else:
        sim.reconfigure(playback_cfg)

    place_agent(sim)
    agent = sim.get_agent(0)

    for episode_index in range(num_episodes):

        episodeName = "episode{}".format(episode_index)
        replay_filepath = output_path + episodeName + ".json"

        sim.render_replay.player_load_from_file(replay_filepath)

        observations = []

        for frame in range(sim.render_replay.player_get_num_keyframes()):
            sim.render_replay.player_set_keyframe_index(frame)
            render_replay_set_agent_from_user_transform(sim)
            observations.append(sim.get_sensor_observations())

        # # play in reverse
        # for frame in range(sim.render_replay.player_get_num_keyframes() - 2, -1, -1):
        #     sim.render_replay.player_set_keyframe_index(frame)
        #     render_replay_set_agent_from_user_transform(sim)
        #     observations.append(sim.get_sensor_observations())

        # # play forward from different camera view
        # agent.body.object.translation = [-1.6, -1.1, 0.2]

        # for frame in range(sim.render_replay.player_get_num_keyframes()):
        #     sim.render_replay.player_set_keyframe_index(frame)
        #     # render_replay_set_agent_from_user_transform(sim)
        #     observations.append(sim.get_sensor_observations())

        if make_video:
            vut.make_video(
                observations,
                "rgba_camera_1stperson",
                "color",
                output_path + episodeName + "_playback",
                open_vid=show_video,
            )
