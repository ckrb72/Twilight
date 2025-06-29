#pragma once

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
#include <vector>

namespace Twilight
{
    namespace Physics
    {
        namespace Layers
        {
        	static constexpr JPH::ObjectLayer NON_MOVING = 0;
        	static constexpr JPH::ObjectLayer MOVING = 1;
        	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
        };

        namespace BroadPhaseLayers
        {
        	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        	static constexpr JPH::BroadPhaseLayer MOVING(1);
        	static constexpr uint32_t NUM_LAYERS(2);
        };

        class PhysicsWorld
        {
            private:
                // TODO: Implement all of these in different files so compiling doesn't become a nightmare

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

                BPLayerInterfaceImpl broad_phase_layer_interface;
                ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
                ObjectLayerPairFilterImpl object_vs_object_layer_filter;
                JPH::PhysicsSystem physics_system;
                JPH::TempAllocatorImpl* temp_allocator;
                JPH::JobSystemThreadPool* job_system;
                MyContactListener contact_listener;
                MyBodyActivationListener body_activation_listener;
                JPH::BodyInterface* body_interface;

                const float tick_rate = 1.0f / 60.0f;
                double time = 0.0;

                std::vector<JPH::BodyID> bodies;

            public:
                PhysicsWorld();
                ~PhysicsWorld();

                void init();
                void update(double delta);
                void deinit();
        };
    }
}