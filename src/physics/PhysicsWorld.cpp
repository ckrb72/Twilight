#include "PhysicsWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <iostream>
#include <cstdarg>


static void TraceImpl(const char *inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	std::cout << buffer << std::endl;
}

namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr uint32_t NUM_LAYERS(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl()
	{
		// Create a mapping table from object to broad phase layer
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint32_t GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char *			GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		default:													JPH_ASSERT(false); return "INVALID";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

// An example contact listener
class MyContactListener : public JPH::ContactListener
{
public:
	// See: ContactListener
	virtual JPH::ValidateResult	OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override
	{
		std::cout << "Contact validate callback" << std::endl;

		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void			OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
	{
		std::cout << "A contact was added" << std::endl;
	}

	virtual void			OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
	{
		std::cout << "A contact was persisted" << std::endl;
	}

	virtual void			OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
	{
		std::cout << "A contact was removed" << std::endl;
	}
};


// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
	virtual void		OnBodyActivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData) override
	{
		std::cout << "A body got activated" << std::endl;
	}

	virtual void		OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData) override
	{
		std::cout << "A body went to sleep" << std::endl;
	}
};



namespace Twilight
{
    namespace Physics
    {
        PhysicsWorld::PhysicsWorld()
        {

        }

        PhysicsWorld::~PhysicsWorld()
        {

        }

        void PhysicsWorld::init()
        {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();

            JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
            JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
            const uint32_t cMaxBodies = 1024;
            const uint32_t cNumBodyMutexes = 0;
            const uint32_t cMaxBodyPairs = 1024;
            const uint32_t cMaxContactConstraints = 1024;
            BPLayerInterfaceImpl broad_phase_layer_interface;
            ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
            ObjectLayerPairFilterImpl object_vs_object_layer_filter;

            JPH::PhysicsSystem physics_system;
            physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);
            MyBodyActivationListener body_activation_listener;
	        physics_system.SetBodyActivationListener(&body_activation_listener);
            MyContactListener contact_listener;
	        physics_system.SetContactListener(&contact_listener);
            JPH::BodyInterface &body_interface = physics_system.GetBodyInterface();

            JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
	        floor_shape_settings.SetEmbedded();

            JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	        JPH::ShapeRefC floor_shape = floor_shape_result.Get();
            JPH::BodyCreationSettings floor_settings(floor_shape, JPH::RVec3(0.0, -1.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);
            JPH::Body *floor = body_interface.CreateBody(floor_settings);
            body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);
            JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0, 2.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
	        JPH::BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);
            body_interface.SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));
            const float cDeltaTime = 1.0f / 60.0f;
            physics_system.OptimizeBroadPhase();

            JPH::uint step = 0;
	        while (body_interface.IsActive(sphere_id))
	        {
	        	// Next step
	        	++step;
            
	        	// Output current position and velocity of the sphere
	        	JPH::RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
	        	JPH::Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);
	        	std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;
            
	        	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	        	const int cCollisionSteps = 1;
            
	        	// Step the world
	        	physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	        }

            std::cout <<"Got Here" << std::endl;


            // Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	        body_interface.RemoveBody(sphere_id);

	        // Destroy the sphere. After this the sphere ID is no longer valid.
	        body_interface.DestroyBody(sphere_id);

	        // Remove and destroy the floor
	        body_interface.RemoveBody(floor->GetID());
	        body_interface.DestroyBody(floor->GetID());

	        // Unregisters all types with the factory and cleans up the default material
	        JPH::UnregisterTypes();

	        // Destroy the factory
	        delete JPH::Factory::sInstance;
	        JPH::Factory::sInstance = nullptr;
        }

        void PhysicsWorld::deinit()
        {
            
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }


    }
}