// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

// Construction code adapted from Bullet3/examples/

#include "BulletArticulatedObject.h"
#include "BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletPhysicsManager.h"
#include "BulletURDFImporter.h"
#include "esp/scene/SceneNode.h"

namespace Mn = Magnum;
namespace Cr = Corrade;

namespace esp {
namespace physics {

// set node state from btTransform
// TODO: this should probably be moved
static void setRotationScalingFromBulletTransform(const btTransform& trans,
                                                  scene::SceneNode* node) {
  Mn::Matrix4 converted{trans};
  node->setRotation(Mn::Quaternion::fromMatrix(converted.rotation()));
  node->setTranslation(converted.translation());
}

///////////////////////////////////
// Class functions
///////////////////////////////////

BulletArticulatedObject::~BulletArticulatedObject() {
  // Corrade::Utility::Debug() << "deconstructing ~BulletArticulatedObject";
  if (objectMotionType_ == MotionType::DYNAMIC) {
    // KINEMATIC and STATIC objects have already been removed from the world.
    bWorld_->removeMultiBody(btMultiBody_.get());
  }
  // remove link collision objects from world
  for (int colIx = 0; colIx < btMultiBody_->getNumLinks(); ++colIx) {
    auto* linkCollider = btMultiBody_->getLinkCollider(colIx);
    bWorld_->removeCollisionObject(linkCollider);
    collisionObjToObjIds_->erase(linkCollider);
    delete linkCollider;
  }

  // remove fixed base rigid body
  if (bFixedObjectRigidBody_) {
    bWorld_->removeRigidBody(bFixedObjectRigidBody_.get());
    collisionObjToObjIds_->erase(bFixedObjectRigidBody_.get());
    bFixedObjectRigidBody_ = nullptr;
    bFixedObjectShape_ = nullptr;
  }

  // remove base collider
  auto* baseCollider = btMultiBody_->getBaseCollider();
  bWorld_->btCollisionWorld::removeCollisionObject(baseCollider);
  collisionObjToObjIds_->erase(baseCollider);
  delete baseCollider;

  for (auto& jlIter : jointLimitConstraints) {
    bWorld_->removeMultiBodyConstraint(jlIter.second.con);
    delete jlIter.second.con;
  }
}

void BulletArticulatedObject::initializeFromURDF(
    URDFImporter& urdfImporter,
    const Mn::Matrix4& worldTransform,
    gfx::DrawableGroup* drawables,
    scene::SceneNode* physicsNode,
    bool fixedBase) {
  Mn::Matrix4 rootTransformInWorldSpace{worldTransform};

  BulletURDFImporter& u2b = *(static_cast<BulletURDFImporter*>(&urdfImporter));
  u2b.setFixedBase(fixedBase);

  auto urdfModel = u2b.getModel();

  int flags = 0;

  URDF2BulletCached cache;
  u2b.InitURDF2BulletCache(cache, flags);

  int urdfLinkIndex = u2b.getRootLinkIndex();
  // int rootIndex = u2b.getRootLinkIndex();

  // NOTE: recursive path only
  u2b.ConvertURDF2BulletInternal(cache, urdfLinkIndex,
                                 rootTransformInWorldSpace, bWorld_.get(),
                                 flags, linkCompoundShapes_, linkChildShapes_);

  if (cache.m_bulletMultiBody) {
    btMultiBody* mb = cache.m_bulletMultiBody;
    jointLimitConstraints = cache.m_jointLimitConstraints;

    mb->setHasSelfCollision((flags & CUF_USE_SELF_COLLISION) !=
                            0);  // NOTE: default no

    mb->finalizeMultiDof();

    btTransform localInertialFrameRoot =
        cache.m_urdfLinkLocalInertialFrames[urdfLinkIndex];

    {
      mb->setBaseWorldTransform(btTransform(rootTransformInWorldSpace) *
                                localInertialFrameRoot);
    }
    {
      btAlignedObjectArray<btQuaternion> scratch_q;
      btAlignedObjectArray<btVector3> scratch_m;
      mb->forwardKinematics(scratch_q, scratch_m);
      mb->updateCollisionObjectWorldTransforms(scratch_q, scratch_m);
    }

    btMultiBody_.reset(
        mb);  // take ownership of the object in the URDFImporter cache
    bWorld_->addMultiBody(btMultiBody_.get());
    btMultiBody_->setCanSleep(true);

    bFixedObjectShape_ = std::make_unique<btCompoundShape>();

    // By convention, when fixed links in the URDF are assigned Noncollidable,
    // we then insert corresponding fixed rigid bodies with group Static.
    // Collisions with a fixed rigid body are cheaper than collisions with a
    // fixed link, due to problems with multibody sleeping behavior.
    {
      auto* col = mb->getBaseCollider();
      if (col->getBroadphaseHandle()->m_collisionFilterGroup ==
          int(CollisionGroup::Noncollidable)) {
        // The collider child is an aligned compound or single shape
        btTransform identity;
        identity.setIdentity();
        bFixedObjectShape_->addChildShape(identity, col->getCollisionShape());
      }

      for (int m = 0; m < mb->getNumLinks(); m++) {
        btMultiBodyLinkCollider* col = mb->getLink(m).m_collider;
        if (col) {
          if (col->getBroadphaseHandle()->m_collisionFilterGroup ==
              int(CollisionGroup::Noncollidable)) {
            bFixedObjectShape_->addChildShape(col->getWorldTransform(),
                                              col->getCollisionShape());
          }
        }
      }

      if (bFixedObjectShape_->getNumChildShapes() != 0) {
        btRigidBody::btRigidBodyConstructionInfo info =
            btRigidBody::btRigidBodyConstructionInfo(0.f, nullptr,
                                                     bFixedObjectShape_.get());
        bFixedObjectRigidBody_ = std::make_unique<btRigidBody>(info);
        bWorld_->addRigidBody(
            bFixedObjectRigidBody_.get(), int(CollisionGroup::Static),
            uint32_t(
                CollisionGroupHelper::getMaskForGroup(CollisionGroup::Static)));
        collisionObjToObjIds_->emplace(bFixedObjectRigidBody_.get(), objectId_);
      } else {
        bFixedObjectShape_ = nullptr;
      }
    }

    // Attach SceneNode visual components
    int urdfLinkIx = 0;
    for (auto& link : urdfModel->m_links) {
      int bulletLinkIx = cache.m_urdfLinkIndices2BulletLinkIndices[urdfLinkIx];

      ArticulatedLink* linkObject = nullptr;
      if (bulletLinkIx >= 0) {
        links_[bulletLinkIx] = std::make_unique<BulletArticulatedLink>(
            &physicsNode->createChild(), resMgr_, bWorld_, bulletLinkIx,
            collisionObjToObjIds_);
        linkObject = links_[bulletLinkIx].get();
      } else {
        if (!baseLink_) {
          baseLink_ = std::make_unique<BulletArticulatedLink>(
              &node().createChild(), resMgr_, bWorld_, bulletLinkIx,
              collisionObjToObjIds_);
        }
        linkObject = baseLink_.get();
      }

      linkObject->node().setType(esp::scene::SceneNodeType::OBJECT);
      // attach visual geometry for the link if specified
      if (link.second->m_visualArray.size() > 0) {
        ESP_CHECK(attachGeometry(linkObject, link.second, drawables),
                  "BulletArticulatedObject::initializeFromURDF(): Failed to "
                  "instance render asset (attachGeometry) for link "
                      << urdfLinkIndex << ".");
      }

      urdfLinkIx++;
    }

    // top level only valid in initial state, but computes valid sub-part AABBs.
    node().computeCumulativeBB();
  }

  // in case the base transform is not zero by default
  syncPose();
}

void BulletArticulatedObject::updateNodes(bool force) {
  isDeferringUpdate_ = false;
  if (force || btMultiBody_->getBaseCollider()->isActive()) {
    setRotationScalingFromBulletTransform(btMultiBody_->getBaseWorldTransform(),
                                          &node());
  }
  // update link transforms
  for (auto& link : links_) {
    if (force || btMultiBody_->getLinkCollider(link.first)->isActive())
      setRotationScalingFromBulletTransform(
          btMultiBody_->getLink(link.first).m_cachedWorldTransform,
          &link.second->node());
  }
}

////////////////////////////
// BulletArticulatedLink
////////////////////////////

bool BulletArticulatedObject::attachGeometry(
    ArticulatedLink* linkObject,
    const std::shared_ptr<io::URDF::Link>& link,
    gfx::DrawableGroup* drawables) {
  bool geomSuccess = false;

  for (auto& visual : link->m_visualArray) {
    bool visualSetupSuccess = true;
    // create a new child for each visual component
    scene::SceneNode& visualGeomComponent = linkObject->node().createChild();
    // cache the visual node
    linkObject->visualNodes_.push_back(&visualGeomComponent);
    visualGeomComponent.setType(esp::scene::SceneNodeType::OBJECT);
    visualGeomComponent.setTransformation(
        link->m_inertia.m_linkLocalFrame.invertedRigid() *
        visual.m_linkLocalFrame);

    // prep the AssetInfo, overwrite the filepath later
    assets::AssetInfo visualMeshInfo{assets::AssetType::UNKNOWN};
    visualMeshInfo.requiresLighting = true;

    // create a modified asset if necessary for material override
    std::shared_ptr<io::URDF::Material> material =
        visual.m_geometry.m_localMaterial;
    if (material) {
      visualMeshInfo.overridePhongMaterial = assets::PhongMaterialColor();
      visualMeshInfo.overridePhongMaterial->ambientColor =
          material->m_matColor.m_rgbaColor;
      visualMeshInfo.overridePhongMaterial->diffuseColor =
          material->m_matColor.m_rgbaColor;
      visualMeshInfo.overridePhongMaterial->specularColor =
          Mn::Color4(material->m_matColor.m_specularColor);
    }

    switch (visual.m_geometry.m_type) {
      case io::URDF::GEOM_CAPSULE:
        visualMeshInfo.type = esp::assets::AssetType::PRIMITIVE;
        // should be registered and cached already
        visualMeshInfo.filepath = visual.m_geometry.m_meshFileName;
        // scale by radius as suggested by magnum docs
        visualGeomComponent.scale(
            Mn::Vector3(visual.m_geometry.m_capsuleRadius));
        // Magnum capsule is Y up, URDF is Z up
        visualGeomComponent.setTransformation(
            visualGeomComponent.transformation() *
            Mn::Matrix4::rotationX(Mn::Rad(M_PI_2)));
        break;
      case io::URDF::GEOM_CYLINDER:
        visualMeshInfo.type = esp::assets::AssetType::PRIMITIVE;
        // the default created primitive handle for the cylinder with radius 1
        // and length 2
        visualMeshInfo.filepath =
            "cylinderSolid_rings_1_segments_12_halfLen_1_useTexCoords_false_"
            "useTangents_false_capEnds_true";
        visualGeomComponent.scale(
            Mn::Vector3(visual.m_geometry.m_capsuleRadius,
                        visual.m_geometry.m_capsuleHeight / 2.0,
                        visual.m_geometry.m_capsuleRadius));
        // Magnum cylinder is Y up, URDF is Z up
        visualGeomComponent.setTransformation(
            visualGeomComponent.transformation() *
            Mn::Matrix4::rotationX(Mn::Rad(M_PI_2)));
        break;
      case io::URDF::GEOM_BOX:
        visualMeshInfo.type = esp::assets::AssetType::PRIMITIVE;
        visualMeshInfo.filepath = "cubeSolid";
        visualGeomComponent.scale(visual.m_geometry.m_boxSize * 0.5);
        break;
      case io::URDF::GEOM_SPHERE: {
        visualMeshInfo.type = esp::assets::AssetType::PRIMITIVE;
        // default sphere prim is already constructed w/ radius 1
        visualMeshInfo.filepath = "icosphereSolid_subdivs_1";
        visualGeomComponent.scale(
            Mn::Vector3(visual.m_geometry.m_sphereRadius));
      } break;
      case io::URDF::GEOM_MESH: {
        visualGeomComponent.scale(visual.m_geometry.m_meshScale);
        visualMeshInfo.filepath = visual.m_geometry.m_meshFileName;
      } break;
      case io::URDF::GEOM_PLANE:
        Corrade::Utility::Debug()
            << "Trying to add visual plane, not implemented";
        // TODO:
        visualSetupSuccess = false;
        break;
      default:
        Corrade::Utility::Debug() << "BulletArticulatedObject::attachGeometry "
                                     ": Unsupported visual type.";
        visualSetupSuccess = false;
        break;
    }

    // add the visual shape to the SceneGraph
    if (visualSetupSuccess) {
      assets::RenderAssetInstanceCreationInfo::Flags flags;
      flags |= assets::RenderAssetInstanceCreationInfo::Flag::IsRGBD;
      flags |= assets::RenderAssetInstanceCreationInfo::Flag::IsSemantic;
      assets::RenderAssetInstanceCreationInfo creation(
          visualMeshInfo.filepath, Mn::Vector3{1}, flags, DEFAULT_LIGHTING_KEY);

      geomSuccess = const_cast<esp::assets::ResourceManager&>(resMgr_)
                        .loadAndCreateRenderAssetInstance(
                            visualMeshInfo, creation, &visualGeomComponent,
                            drawables, &linkObject->visualNodes_) != nullptr;

      // cache the visual component for later query
      if (geomSuccess) {
        linkObject->visualAttachments_.emplace_back(
            &visualGeomComponent, visual.m_geometry.m_meshFileName);
      }
    }
  }

  return geomSuccess;
}

void BulletArticulatedObject::resetStateFromSceneInstanceAttr(
    CORRADE_UNUSED bool defaultCOMCorrection) {
  auto sceneInstanceAttr = getSceneInstanceAttributes();
  if (!sceneInstanceAttr) {
    // if no scene instance attributes specified, no initial state is set
    return;
  }

  // now move objects
  // set object's location and rotation based on translation and rotation
  // params specified in instance attributes
  auto translate = sceneInstanceAttr->getTranslation();

  // construct initial transformation state.
  Mn::Matrix4 state =
      Mn::Matrix4::from(sceneInstanceAttr->getRotation().toMatrix(), translate);
  setTransformation(state);
  // set object's motion type if different than set value
  const physics::MotionType attrObjMotionType =
      static_cast<physics::MotionType>(sceneInstanceAttr->getMotionType());
  if (attrObjMotionType != physics::MotionType::UNDEFINED) {
    setMotionType(attrObjMotionType);
  }
  // set initial joint positions
  // get array of existing joint dofs
  std::vector<float> aoJointPose = getJointPositions();
  // get instance-specified initial joint positions
  std::map<std::string, float>& initJointPos =
      sceneInstanceAttr->getInitJointPose();
  // map instance vals into
  size_t idx = 0;
  for (const auto& elem : initJointPos) {
    if (idx >= aoJointPose.size()) {
      LOG(WARNING)
          << "BulletArticulatedObject::resetStateFromSceneInstanceAttr : "
          << "Attempting to specify more initial joint poses than "
             "exist in articulated object "
          << sceneInstanceAttr->getHandle() << ", so skipping";
      break;
    }
    aoJointPose[idx++] = elem.second;
  }
  setJointPositions(aoJointPose);

  // set initial joint velocities
  // get array of existing joint vel dofs
  std::vector<float> aoJointVels = getJointVelocities();
  // get instance-specified initial joint velocities
  std::map<std::string, float>& initJointVel =
      sceneInstanceAttr->getInitJointVelocities();
  idx = 0;
  for (const auto& elem : initJointVel) {
    if (idx >= aoJointVels.size()) {
      LOG(WARNING)
          << "BulletArticulatedObject::resetStateFromSceneInstanceAttr : "
          << "Attempting to specify more initial joint velocities than "
             "exist in articulated object "
          << sceneInstanceAttr->getHandle() << ", so skipping";
      break;
    }
    aoJointVels[idx++] = elem.second;
  }

  setJointVelocities(aoJointVels);

}  // BulletArticulatedObject::resetStateFromSceneInstanceAttr

void BulletArticulatedObject::setRootState(const Mn::Matrix4& state) {
  btTransform tr{state};
  btMultiBody_->setBaseWorldTransform(tr);
  if (bFixedObjectRigidBody_) {
    bFixedObjectRigidBody_->setWorldTransform(tr);
  }
  // update the simulation state
  updateKinematicState();
}

Mn::Vector3 BulletArticulatedObject::getRootLinearVelocity() const {
  return Mn::Vector3(btMultiBody_->getBaseVel());
}

void BulletArticulatedObject::setRootLinearVelocity(const Mn::Vector3& linVel) {
  btMultiBody_->setBaseVel(btVector3(linVel));
}

Mn::Vector3 BulletArticulatedObject::getRootAngularVelocity() const {
  return Mn::Vector3(btMultiBody_->getBaseOmega());
}
void BulletArticulatedObject::setRootAngularVelocity(
    const Mn::Vector3& angVel) {
  btMultiBody_->setBaseOmega(btVector3(angVel));
}

void BulletArticulatedObject::setJointForces(const std::vector<float>& forces) {
  if (forces.size() != size_t(btMultiBody_->getNumDofs())) {
    Corrade::Utility::Debug()
        << "setJointForces - Force vector size mis-match (input: "
        << forces.size() << ", expected: " << btMultiBody_->getNumDofs()
        << "), aborting.";
  }

  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    btMultibodyLink& link = btMultiBody_->getLink(i);
    for (int dof = 0; dof < link.m_dofCount; ++dof) {
      link.m_jointTorque[dof] = forces[dofCount];
      dofCount++;
    }
  }
}

void BulletArticulatedObject::addJointForces(const std::vector<float>& forces) {
  if (forces.size() != size_t(btMultiBody_->getNumDofs())) {
    Corrade::Utility::Debug()
        << "addJointForces - Force vector size mis-match (input: "
        << forces.size() << ", expected: " << btMultiBody_->getNumDofs()
        << "), aborting.";
  }

  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    btMultibodyLink& link = btMultiBody_->getLink(i);
    for (int dof = 0; dof < link.m_dofCount; ++dof) {
      link.m_jointTorque[dof] += forces[dofCount];
      dofCount++;
    }
  }
}

std::vector<float> BulletArticulatedObject::getJointForces() {
  std::vector<float> forces(btMultiBody_->getNumDofs());
  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    btScalar* dofForces = btMultiBody_->getJointTorqueMultiDof(i);
    for (int dof = 0; dof < btMultiBody_->getLink(i).m_dofCount; ++dof) {
      forces[dofCount] = dofForces[dof];
      dofCount++;
    }
  }
  return forces;
}

void BulletArticulatedObject::setJointVelocities(
    const std::vector<float>& vels) {
  if (vels.size() != size_t(btMultiBody_->getNumDofs())) {
    Corrade::Utility::Debug()
        << "setJointVelocities - Velocity vector size mis-match (input: "
        << vels.size() << ", expected: " << btMultiBody_->getNumDofs()
        << "), aborting.";
  }

  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    if (btMultiBody_->getLink(i).m_dofCount > 0) {
      // this const_cast is only needed for Bullet 2.87. It is harmless in any
      // case.
      btMultiBody_->setJointVelMultiDof(i, const_cast<float*>(&vels[dofCount]));
      dofCount += btMultiBody_->getLink(i).m_dofCount;
    }
  }
}

std::vector<float> BulletArticulatedObject::getJointVelocities() {
  std::vector<float> vels(btMultiBody_->getNumDofs());
  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    btScalar* dofVels = btMultiBody_->getJointVelMultiDof(i);
    for (int dof = 0; dof < btMultiBody_->getLink(i).m_dofCount; ++dof) {
      vels[dofCount] = dofVels[dof];
      dofCount++;
    }
  }
  return vels;
}

void BulletArticulatedObject::setJointPositions(
    const std::vector<float>& positions) {
  if (positions.size() != size_t(btMultiBody_->getNumPosVars())) {
    Corrade::Utility::Debug()
        << "setJointPositions - Position vector size mis-match (input: "
        << positions.size() << ", expected: " << btMultiBody_->getNumPosVars()
        << "), aborting.";
  }

  int posCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    auto& link = btMultiBody_->getLink(i);
    if (link.m_posVarCount > 0) {
      btMultiBody_->setJointPosMultiDof(
          i, const_cast<float*>(&positions[posCount]));
      posCount += link.m_posVarCount;
    }
  }

  // update the simulation state
  updateKinematicState();
}

std::vector<float> BulletArticulatedObject::getJointPositions() {
  std::vector<float> positions(btMultiBody_->getNumPosVars());
  int posCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    btScalar* linkPos = btMultiBody_->getJointPosMultiDof(i);
    for (int pos = 0; pos < btMultiBody_->getLink(i).m_posVarCount; ++pos) {
      positions[posCount] = linkPos[pos];
      posCount++;
    }
  }
  return positions;
}

std::vector<float> BulletArticulatedObject::getJointPositionLimits(
    bool upperLimits) {
  std::vector<float> posLimits(btMultiBody_->getNumPosVars());
  int posCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    if (jointLimitConstraints.count(i) > 0) {
      // a joint limit constraint exists for this link's parent joint
      auto& jlc = jointLimitConstraints.at(i);
      posLimits[posCount] = upperLimits ? jlc.upperLimit : jlc.lowerLimit;
      posCount++;
    } else {
      // iterate through joint dofs. Note: multi-dof joints cannot be limited,
      // so ok to skip.
      for (int posVar = 0; posVar < btMultiBody_->getLink(i).m_posVarCount;
           ++posVar) {
        posLimits[posCount] = upperLimits ? INFINITY : -INFINITY;
        posCount++;
      }
    }
  }
  CHECK(posCount == btMultiBody_->getNumPosVars());
  return posLimits;
}

void BulletArticulatedObject::addArticulatedLinkForce(int linkId,
                                                      Mn::Vector3 force) {
  CHECK(getNumLinks() > linkId);
  btMultiBody_->addLinkForce(linkId, btVector3{force});
}

float BulletArticulatedObject::getArticulatedLinkFriction(int linkId) {
  CHECK(getNumLinks() > linkId);
  return btMultiBody_->getLinkCollider(linkId)->getFriction();
}

void BulletArticulatedObject::setArticulatedLinkFriction(int linkId,
                                                         float friction) {
  CHECK(getNumLinks() > linkId);
  btMultiBody_->getLinkCollider(linkId)->setFriction(friction);
}

JointType BulletArticulatedObject::getLinkJointType(int linkId) const {
  CHECK(getNumLinks() > linkId && linkId >= 0);
  return JointType(int(btMultiBody_->getLink(linkId).m_jointType));
}

int BulletArticulatedObject::getLinkDoFOffset(int linkId) const {
  CHECK(getNumLinks() > linkId && linkId >= 0);
  return btMultiBody_->getLink(linkId).m_dofOffset;
}

int BulletArticulatedObject::getLinkNumDoFs(int linkId) const {
  CHECK(getNumLinks() > linkId && linkId >= 0);
  return btMultiBody_->getLink(linkId).m_dofCount;
}

int BulletArticulatedObject::getLinkJointPosOffset(int linkId) const {
  CHECK(getNumLinks() > linkId && linkId >= 0);
  return btMultiBody_->getLink(linkId).m_cfgOffset;
}

int BulletArticulatedObject::getLinkNumJointPos(int linkId) const {
  CHECK(getNumLinks() > linkId && linkId >= 0);
  return btMultiBody_->getLink(linkId).m_posVarCount;
}

void BulletArticulatedObject::reset() {
  // reset positions and velocities to zero
  // clears forces/torques
  // Note: does not update root state
  std::vector<float> zeros(btMultiBody_->getNumPosVars(), 0.0f);
  // handle spherical quaternions
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    auto& link = btMultiBody_->getLink(i);
    if (link.m_jointType ==
        btMultibodyLink::eFeatherstoneJointType::eSpherical) {
      // need a valid identity quaternion [0,0,0,1]
      zeros[link.m_cfgOffset + 3] = 1;
    }
  }

  // also updates kinematic state
  setJointPositions(zeros);

  btMultiBody_->clearConstraintForces();
  btMultiBody_->clearVelocities();
  btMultiBody_->clearForcesAndTorques();
}

void BulletArticulatedObject::setActive(bool active) {
  if (!active) {
    btMultiBody_->goToSleep();
  } else {
    btMultiBody_->wakeUp();
  }
}

bool BulletArticulatedObject::isActive() const {
  return btMultiBody_->isAwake();
}

bool BulletArticulatedObject::getCanSleep() {
  return btMultiBody_->getCanSleep();
}

void BulletArticulatedObject::setMotionType(MotionType mt) {
  if (mt == objectMotionType_) {
    return;
  }
  if (mt == MotionType::UNDEFINED) {
    return;
  }

  // only need to change the state if the previous state was different (i.e.,
  // DYNAMIC -> other)
  if (mt == MotionType::DYNAMIC) {
    bWorld_->addMultiBody(btMultiBody_.get());
  } else if (objectMotionType_ == MotionType::DYNAMIC) {
    // TODO: STATIC and KINEMATIC are equivalent for simplicity. Could manually
    // limit STATIC...
    bWorld_->removeMultiBody(btMultiBody_.get());
  }
  objectMotionType_ = mt;
}

void BulletArticulatedObject::clampJointLimits() {
  auto pose = getJointPositions();
  bool poseModified = false;

  // some small contrived error term for overflow
  float corrective_eps = 0.000001;

  int dofCount = 0;
  for (int i = 0; i < btMultiBody_->getNumLinks(); ++i) {
    if (jointLimitConstraints.count(i) > 0) {
      // a joint limit constraint exists for this link
      auto& jlc = jointLimitConstraints.at(i);

      // position clamping:
      if (pose[dofCount] < jlc.lowerLimit - corrective_eps) {
        poseModified = true;
        pose[dofCount] = jlc.lowerLimit;
      } else if (pose[dofCount] > jlc.upperLimit + corrective_eps) {
        poseModified = true;
        pose[dofCount] = jlc.upperLimit;
      }
    }

    // continue incrementing the dof counter
    for (int dof = 0; dof < btMultiBody_->getLink(i).m_dofCount; ++dof) {
      dofCount++;
    }
  }

  if (poseModified) {
    setJointPositions(pose);
  }
}

void BulletArticulatedObject::updateKinematicState() {
  btMultiBody_->forwardKinematics(scratch_q_, scratch_m_);
  btMultiBody_->updateCollisionObjectWorldTransforms(scratch_q_, scratch_m_);
  // Need to update the aabbs manually also for broadphase collision detection
  for (int linkIx = 0; linkIx < btMultiBody_->getNumLinks(); ++linkIx) {
    bWorld_->updateSingleAabb(btMultiBody_->getLinkCollider(linkIx));
  }
  bWorld_->updateSingleAabb(btMultiBody_->getBaseCollider());
  if (bFixedObjectRigidBody_) {
    bWorld_->updateSingleAabb(bFixedObjectRigidBody_.get());
  }
  // update visual shapes
  if (!isDeferringUpdate_) {
    updateNodes(true);
  }
}

/**
 * @brief Specific callback function for ArticulatedObject::contactTest to
 * screen self-collisions.
 */
struct AOSimulationContactResultCallback
    : public SimulationContactResultCallback {
  btMultiBody* mb_ = nullptr;
  btRigidBody* fixedBaseColObj_ = nullptr;

  /**
   * @brief Constructor taking the AO's btMultiBody as input to screen
   * self-collisions.
   */
  AOSimulationContactResultCallback(btMultiBody* mb,
                                    btRigidBody* fixedBaseColObj)
      : mb_(mb), fixedBaseColObj_(fixedBaseColObj) {
    bCollision = false;
  }

  bool needsCollision(btBroadphaseProxy* proxy0) const override {
    // base method checks for group|mask filter
    bool collides = SimulationContactResultCallback::needsCollision(proxy0);
    // check for self-collision
    if (!mb_->hasSelfCollision()) {
      // This should always be a valid conversion to btCollisionObject
      auto* co = static_cast<btCollisionObject*>(proxy0->m_clientObject);
      auto* mblc = dynamic_cast<btMultiBodyLinkCollider*>(co);
      if (mblc) {
        if (mblc->m_multiBody == mb_) {
          // screen self-collisions
          collides = false;
        }
      } else if (co == fixedBaseColObj_) {
        // screen self-collisions w/ fixed base rigid
        collides = false;
      }
    }
    return collides;
  }
};

bool BulletArticulatedObject::contactTest() {
  AOSimulationContactResultCallback src(btMultiBody_.get(),
                                        bFixedObjectRigidBody_.get());

  auto* baseCollider = btMultiBody_->getBaseCollider();
  // Do a contact test for each piece of the AO and return at soonest contact.
  // Should be cheaper to hit multiple local aabbs than to check the full scene.
  if (bFixedObjectRigidBody_) {
    src.m_collisionFilterGroup =
        bFixedObjectRigidBody_->getBroadphaseHandle()->m_collisionFilterGroup;
    src.m_collisionFilterMask =
        bFixedObjectRigidBody_->getBroadphaseHandle()->m_collisionFilterMask;

    bWorld_->getCollisionWorld()->contactTest(bFixedObjectRigidBody_.get(),
                                              src);
    if (src.bCollision) {
      return src.bCollision;
    }
  } else if (baseCollider) {
    src.m_collisionFilterGroup =
        baseCollider->getBroadphaseHandle()->m_collisionFilterGroup;
    src.m_collisionFilterMask =
        baseCollider->getBroadphaseHandle()->m_collisionFilterMask;
    bWorld_->getCollisionWorld()->contactTest(baseCollider, src);
    if (src.bCollision) {
      return src.bCollision;
    }
  }
  for (int colIx = 0; colIx < btMultiBody_->getNumLinks(); ++colIx) {
    auto* linkCollider = btMultiBody_->getLinkCollider(colIx);
    src.m_collisionFilterGroup =
        linkCollider->getBroadphaseHandle()->m_collisionFilterGroup;
    src.m_collisionFilterMask =
        linkCollider->getBroadphaseHandle()->m_collisionFilterMask;
    bWorld_->getCollisionWorld()->contactTest(linkCollider, src);
    if (src.bCollision) {
      return src.bCollision;
    }
  }
  return false;
}  // contactTest

}  // namespace physics
}  // namespace esp
