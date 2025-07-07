#include "PhysicsWorld.h"

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

            const uint32_t cMaxBodies = 1024;
            const uint32_t cNumBodyMutexes = 0;
            const uint32_t cMaxBodyPairs = 1024;
            const uint32_t cMaxContactConstraints = 1024;

			this->temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
			this->job_system = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

            this->physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, this->broad_phase_layer_interface, this->object_vs_broadphase_layer_filter, this->object_vs_object_layer_filter);
            
	        this->physics_system.SetBodyActivationListener(&body_activation_listener);
	        this->physics_system.SetContactListener(&contact_listener);
            this->body_interface = &physics_system.GetBodyInterface();

			JPH::Body* floor = this->body_interface->CreateBody(JPH::BodyCreationSettings(new JPH::BoxShape(JPH::RVec3(100.0f, 1.0f, 100.0f)), JPH::RVec3(0, -5.0, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::MOVING));
            this->body_interface->AddBody(floor->GetID(), JPH::EActivation::DontActivate);
			this->floor_id = floor->GetID();

			JPH::Body* box = this->body_interface->CreateBody(JPH::BodyCreationSettings(new JPH::BoxShape(JPH::RVec3(1.0f, 1.0f, 1.0f)), JPH::RVec3(0.0f, 10.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING));
			this->body_interface->AddBody(box->GetID(), JPH::EActivation::Activate);
			this->box_id = box->GetID();

			this->body_interface->SetLinearVelocity(this->box_id, JPH::Vec3(0.0f, -2.0f, 0.0f));
            this->physics_system.OptimizeBroadPhase();
            
			/*JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0, 2.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
	        JPH::BodyID sphere_id = body_interface->CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);
            this->body_interface->SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));
            this->physics_system.OptimizeBroadPhase();

            JPH::uint step = 0;
	        while (body_interface->IsActive(sphere_id))
	        {
	        	// Next step
	        	++step;
            
	        	// Output current position and velocity of the sphere
	        	JPH::RVec3 position = body_interface->GetCenterOfMassPosition(sphere_id);
	        	JPH::Vec3 velocity = body_interface->GetLinearVelocity(sphere_id);
	        	std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;
            
	        	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	        	const int cCollisionSteps = 1;
            
	        	// Step the world
	        	physics_system.Update(this->tick_rate, cCollisionSteps, this->temp_allocator, this->job_system);
	        }


            // Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	        body_interface->RemoveBody(sphere_id);

	        // Destroy the sphere. After this the sphere ID is no longer valid.
	        body_interface->DestroyBody(sphere_id);

	        // Remove and destroy the floor
	        body_interface->RemoveBody(floor->GetID());
	        body_interface->DestroyBody(floor->GetID());*/
        }

		void PhysicsWorld::update(double delta)
		{
			if(this->time > this->tick_rate)
			{
	        	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	        	const int COLLISION_STEPS = 1;
	        	JPH::RVec3 position = body_interface->GetCenterOfMassPosition(this->box_id);
	        	JPH::Vec3 velocity = body_interface->GetLinearVelocity(this->box_id);

				JPH::Mat44 transform = body_interface->GetWorldTransform(this->box_id);
				//std::cout << transform << std::endl;

				//std::cout << "Step " << this->time << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;
        
	        	// Step the world
	        	physics_system.Update(this->tick_rate, COLLISION_STEPS, this->temp_allocator, this->job_system);
				this->time = 0.0;
			}
			this->time += delta;
		}

		JPH::Mat44 PhysicsWorld::get_transform_test()
		{
			return this->body_interface->GetWorldTransform(this->box_id);
		}

        void PhysicsWorld::deinit()
        {
            delete this->temp_allocator;
			delete this->job_system;
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }


    }
}