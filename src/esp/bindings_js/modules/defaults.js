// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

export const defaultAgentConfig = {
  height: 1.5,
  radius: 0.1,
  mass: 32.0,
  linearAcceleration: 20.0,
  angularAcceleration: 4 * Math.PI,
  linearFriction: 0.5,
  angularFriction: 1.0,
  coefficientOfRestitution: 0.0
};

export const defaultStartState = {
  position: [-4.94049, -2.63092, -7.57733],
  rotation: [0, 0.980792, 0, 0.195056]
};

export const defaultGoal = {
  position: [2.2896811962127686, 0.11950381100177765, 16.97636604309082]
};

export const defaultEpisode = {
  startState: defaultStartState,
  goal: defaultGoal
};

export const defaultResolution = { height: 480, width: 640 };

export const defaultScene =
  window.location.href.indexOf("localhost") === -1
    ? "https://habitat-resources.s3.amazonaws.com/data/scene_datasets/habitat-test-scenes/skokloster-castle.glb"
    : "skokloster-castle.glb";

export const infoSemanticFileName = "info_semantic.json";

export const dataHome = "data/";
export const taskHome = "data/tasks/";
export const sceneHome = "data/scenes/";
export const flythroughHome = "data/replays/";
export const primitiveObjectHandles = [
  "cylinderSolid_rings_1_segments_12_halfLen_1_useTexCoords_false_useTangents_false_capEnds_true"
];

export const fileBasedObjects = {
  objects: [
    {
      object: "apple",
      objectIcon: "/data/test_assets/objects/apple.png",
      objectHandle: "/data/objects/apple.phys_properties.json",
      physicsProperties: "test_assets/objects/apple.phys_properties.json",
      renderMesh: "test_assets/objects/apple.glb"
    },
    {
      object: "banana",
      objectIcon: "/data/test_assets/objects/banana.png",
      objectHandle: "/data/objects/banana.phys_properties.json",
      physicsProperties: "test_assets/objects/banana.phys_properties.json",
      renderMesh: "test_assets/objects/banana.glb"
    },
    {
      object: "cracker box",
      objectIcon: "/data/test_assets/objects/cracker_box.png",
      objectHandle: "/data/objects/cracker_box.phys_properties.json",
      physicsProperties: "test_assets/objects/cracker_box.phys_properties.json",
      renderMesh: "test_assets/objects/cracker_box.glb"
    },
    {
      object: "colored wood blocks",
      objectIcon: "/data/test_assets/objects/colored_wood_blocks.png",
      objectHandle: "/data/objects/colored_wood_blocks.phys_properties.json",
      physicsProperties:
        "test_assets/objects/colored_wood_blocks.phys_properties.json",
      renderMesh: "test_assets/objects/colored_wood_blocks.glb"
    },
    {
      object: "gelatin box",
      objectIcon: "/data/test_assets/objects/gelatin_box.png",
      objectHandle: "/data/objects/gelatin_box.phys_properties.json",
      physicsProperties: "test_assets/objects/gelatin_box.phys_properties.json",
      renderMesh: "test_assets/objects/gelatin_box.glb"
    },
    {
      object: "hammer",
      objectIcon: "/data/test_assets/objects/hammer.png",
      objectHandle: "/data/objects/hammer.phys_properties.json",
      physicsProperties: "test_assets/objects/hammer.phys_properties.json",
      renderMesh: "test_assets/objects/hammer.glb"
    },
    {
      object: "master chef can",
      objectIcon: "/data/test_assets/objects/master_chef_can.png",
      objectHandle: "/data/objects/master_chef_can.phys_properties.json",
      physicsProperties:
        "test_assets/objects/master_chef_can.phys_properties.json",
      renderMesh: "test_assets/objects/master_chef_can.glb"
    },
    {
      object: "soccer ball",
      objectIcon: "/data/test_assets/objects/mini_soccer_ball.png",
      objectHandle: "/data/objects/mini_soccer_ball.phys_properties.json",
      physicsProperties:
        "test_assets/objects/mini_soccer_ball.phys_properties.json",
      renderMesh: "test_assets/objects/mini_soccer_ball.glb"
    },
    {
      object: "mustard bottle",
      objectIcon: "/data/test_assets/objects/mustard_bottle.png",
      objectHandle: "/data/objects/mustard_bottle.phys_properties.json",
      physicsProperties:
        "test_assets/objects/mustard_bottle.phys_properties.json",
      renderMesh: "test_assets/objects/mustard_bottle.glb"
    },
    {
      object: "orange",
      objectIcon: "/data/test_assets/objects/orange.png",
      objectHandle: "/data/objects/orange.phys_properties.json",
      physicsProperties: "test_assets/objects/orange.phys_properties.json",
      renderMesh: "test_assets/objects/orange.glb"
    },
    {
      object: "red bowl",
      objectIcon: "/data/test_assets/objects/bowl.png",
      objectHandle: "/data/objects/bowl.phys_properties.json",
      physicsProperties: "test_assets/objects/bowl.phys_properties.json",
      renderMesh: "test_assets/objects/bowl.glb"
    },
    {
      object: "red mug",
      objectIcon: "/data/test_assets/objects/mug.png",
      objectHandle: "/data/objects/mug.phys_properties.json",
      physicsProperties: "test_assets/objects/mug.phys_properties.json",
      renderMesh: "test_assets/objects/mug.glb"
    },
    {
      object: "red plate",
      objectIcon: "/data/test_assets/objects/plate.png",
      objectHandle: "/data/objects/plate.phys_properties.json",
      physicsProperties: "test_assets/objects/plate.phys_properties.json",
      renderMesh: "test_assets/objects/plate.glb"
    },
    {
      object: "red sphere",
      objectIcon: "/data/test_assets/objects/sphere.png",
      objectHandle: "/data/objects/sphere.phys_properties.json",
      physicsProperties: "test_assets/objects/sphere.phys_properties.json",
      renderMesh: "test_assets/objects/sphere.glb"
    },
    {
      object: "tomato soup can",
      objectIcon: "/data/test_assets/objects/tomato_soup_can.png",
      objectHandle: "/data/objects/tomato_soup_can.phys_properties.json",
      physicsProperties:
        "test_assets/objects/tomato_soup_can.phys_properties.json",
      renderMesh: "test_assets/objects/tomato_soup_can.glb"
    },
    {
      object: "toy airplane",
      objectIcon: "/data/test_assets/objects/toy_airplane.png",
      objectHandle: "/data/objects/toy_airplane.phys_properties.json",
      physicsProperties:
        "test_assets/objects/toy_airplane.phys_properties.json",
      renderMesh: "test_assets/objects/toy_airplane.glb"
    },
    {
      object: "wood block",
      objectIcon: "/data/test_assets/objects/wood_block.png",
      objectHandle: "/data/objects/wood_block.phys_properties.json",
      physicsProperties: "test_assets/objects/wood_block.phys_properties.json",
      renderMesh: "test_assets/objects/wood_block.glb"
    }
  ]
};

export const flythroughReplayTask = {
  name: "replay_task_1.json",
  config: "tasks/replay_task_1.json"
};

export const flythroughReplayFile = {
  name: "replay_task_1.csv",
  location: "replays/replay_task_1.csv"
};

export const taskFiles = {
  tasks: [
    {
      name: "task_1.json",
      config: "tasks/task_1.json",
      scene: "house_with_empty_room.glb",
      flythroughTask: {
        name: "replay_task_1.json",
        config: "tasks/replay_task_1.json"
      },
      flythroughReplayFile: {
        name: "replay_task_1.csv",
        location: "replays/replay_task_1.csv"
      },
      trainingTask: {
        name: "training_task_1.json",
        config: "tasks/training_task_1.json"
      }
    },
    {
      name: "task_2.json",
      config: "tasks/task_2.json",
      scene: "empty_house.glb",
      flythroughTask: {
        name: "replay_task_2.json",
        config: "tasks/replay_task_2.json"
      },
      flythroughReplayFile: {
        name: "replay_task_2.csv",
        location: "replays/replay_task_2.csv"
      },
      trainingTask: {
        name: "training_task_2.json",
        config: "tasks/training_task_2.json"
      }
    },
    {
      name: "task_3.json",
      config: "tasks/task_3.json",
      scene: "house_with_empty_garage.glb",
      flythroughTask: {
        name: "replay_task_3.json",
        config: "tasks/replay_task_3.json"
      },
      flythroughReplayFile: {
        name: "replay_task_3.csv",
        location: "replays/replay_task_3.csv"
      },
      trainingTask: {
        name: "training_task_3.json",
        config: "tasks/training_task_3.json"
      }
    },
    {
      name: "task_4.json",
      config: "tasks/task_4.json",
      scene: "house.glb",
      flythroughTask: {
        name: "replay_task_4.json",
        config: "tasks/replay_task_4.json"
      },
      flythroughReplayFile: {
        name: "replay_task_4.csv",
        location: "replays/replay_task_4.csv"
      },
      trainingTask: {
        name: "training_task_4.json",
        config: "tasks/training_task_4.json"
      }
    }
  ]
};

export const trainingTask = {
  name: "training_task_1.json",
  config: "tasks/training_task_1.json"
};

export const inventorySlots = 1;
