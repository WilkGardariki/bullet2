/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#include "ParallelPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#include "ParallelIslandDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "BulletDynamics/ConstraintSolver/btTypedConstraint.h"
#include "BulletCollision/CollisionDispatch/btSimulationIslandManager.h"
#include "SimulationIsland.h"


ParallelPhysicsEnvironment::ParallelPhysicsEnvironment(ParallelIslandDispatcher* dispatcher, btOverlappingPairCache* pairCache):
CcdPhysicsEnvironment(dispatcher,pairCache)
{
	
}

ParallelPhysicsEnvironment::~ParallelPhysicsEnvironment()
{

}



/// Perform an integration step of duration 'timeStep'.
bool	ParallelPhysicsEnvironment::proceedDeltaTimeOneStep(float timeStep)
{
	// Make sure the broadphase / overlapping AABB paircache is up-to-date
	btOverlappingPairCache*	scene = m_collisionWorld->getPairCache();
	scene->refreshOverlappingPairs();

	// Find the connected sets that can be simulated in parallel
	// Using union find

#ifdef USE_QUICKPROF
	btProfiler::beginBlock("IslandUnionFind");
#endif //USE_QUICKPROF

	getSimulationIslandManager()->updateActivationState(getCollisionWorld(),getCollisionWorld()->getDispatcher());

	{
		int i;
		int numConstraints = m_constraints.size();
		for (i=0;i< numConstraints ; i++ )
		{
			btTypedConstraint* constraint = m_constraints[i];

			const btRigidBody* colObj0 = &constraint->getRigidBodyA();
			const btRigidBody* colObj1 = &constraint->getRigidBodyB();

			if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
				((colObj1) && ((colObj1)->mergesSimulationIslands())))
			{
				if (colObj0->IsActive() || colObj1->IsActive())
				{

					getSimulationIslandManager()->getUnionFind().unite((colObj0)->m_islandTag1,
						(colObj1)->m_islandTag1);
				}
			}
		}
	}

	//Store the island id in each body
	getSimulationIslandManager()->storeIslandActivationState(getCollisionWorld());

#ifdef USE_QUICKPROF
	btProfiler::endBlock("IslandUnionFind");
#endif //USE_QUICKPROF

	

	///build simulation islands
	
#ifdef USE_QUICKPROF
	btProfiler::beginBlock("BuildIslands");
#endif //USE_QUICKPROF

	std::vector<SimulationIsland> simulationIslands;
	simulationIslands.resize(GetNumControllers());

	int k;
	for (k=0;k<GetNumControllers();k++)
	{
			CcdPhysicsController* ctrl = m_controllers[k];
			int tag = ctrl->getRigidBody()->m_islandTag1;
			if (tag>=0)
			{
				simulationIslands[tag].m_controllers.push_back(ctrl);
			}
	}

	btDispatcher* dispatcher = getCollisionWorld()->getDispatcher();

	
	//this is a brute force approach, will rethink later about more subtle ways
	int i;

			assert(0);
/*
	for (i=0;i<	scene->GetNumOverlappingPairs();i++)
	{


		btBroadphasePair* pair = &scene->GetOverlappingPair(i);

		btCollisionObject*	col0 = static_cast<btCollisionObject*>(pair->m_pProxy0->m_clientObject);
		btCollisionObject*	col1 = static_cast<btCollisionObject*>(pair->m_pProxy1->m_clientObject);
		
		if (col0->m_islandTag1 > col1->m_islandTag1)
		{
			simulationIslands[col0->m_islandTag1].m_overlappingPairIndices.push_back(i);
		} else
		{
			simulationIslands[col1->m_islandTag1].m_overlappingPairIndices.push_back(i);
		}
		
	}
	*/
	
	//store constraint indices for each island
	for (unsigned int ui=0;ui<m_constraints.size();ui++)
	{
		btTypedConstraint& constraint = *m_constraints[ui];
		if (constraint.getRigidBodyA().m_islandTag1 > constraint.getRigidBodyB().m_islandTag1)
		{
			simulationIslands[constraint.getRigidBodyA().m_islandTag1].m_constraintIndices.push_back(ui);
		} else
		{
			simulationIslands[constraint.getRigidBodyB().m_islandTag1].m_constraintIndices.push_back(ui);
		}

	}

	//add all overlapping pairs for each island

	for (i=0;i<dispatcher->getNumManifolds();i++)
	{
		 btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
		 
		 //filtering for response

		 btCollisionObject* colObj0 = static_cast<btCollisionObject*>(manifold->getBody0());
		 btCollisionObject* colObj1 = static_cast<btCollisionObject*>(manifold->getBody1());
		 {
			 int islandTag = colObj0->m_islandTag1;
			 if (colObj1->m_islandTag1 > islandTag)
				 islandTag = colObj1->m_islandTag1;

				if (dispatcher->needsResponse(*colObj0,*colObj1))
					simulationIslands[islandTag].m_manifolds.push_back(manifold);
			
		 }
	}
		
	#ifdef USE_QUICKPROF
		btProfiler::endBlock("BuildIslands");
	#endif //USE_QUICKPROF


#ifdef USE_QUICKPROF
	btProfiler::beginBlock("SimulateIsland");
#endif //USE_QUICKPROF
	
	btTypedConstraint** constraintBase = 0;
	if (m_constraints.size())
		constraintBase = &m_constraints[0];



	assert(0);
	/*
	//Each simulation island can be processed in parallel (will be put on a job queue)
	for (k=0;k<simulationIslands.size();k++)
	{
		if (simulationIslands[k].m_controllers.size())
		{
			assert(0);//seems to be wrong, passing ALL overlapping pairs
			simulationIslands[k].Simulate(m_debugDrawer,m_numIterations, constraintBase ,&scene->GetOverlappingPair(0),dispatcher,getBroadphase(),m_solver,timeStep);
		}
	}
	*/


#ifdef USE_QUICKPROF
	btProfiler::endBlock("SimulateIsland");
#endif //USE_QUICKPROF

	return true;

}
