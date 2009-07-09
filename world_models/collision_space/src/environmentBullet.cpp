/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/** \author Ioan Sucan */

#include "collision_space/environmentBullet.h"

void collision_space::EnvironmentModelBullet::addRobotModel(const boost::shared_ptr<planning_models::KinematicModel> &model, const std::vector<std::string> &links, double scale, double padding)
{
    collision_space::EnvironmentModel::addRobotModel(model, links, scale, padding);
    m_modelGeom.scale = scale;
    m_modelGeom.padding = padding;

    for (unsigned int i = 0 ; i < links.size() ; ++i)
    {
	/* skip this link if we have no geometry or if the link
	   name is not specified as enabled for collision
	   checking */
	planning_models::KinematicModel::Link *link = m_robotModel->getLink(links[i]);
	if (!link || !link->shape)
	    continue;
	
	kGeom *kg = new kGeom();
	kg->link = link;
	kg->enabled = true;
	kg->index = i;
	btCollisionObject *obj = createCollisionBody(link->shape, scale, padding);
	assert(obj);
	m_world->addCollisionObject(obj);
	obj->setUserPointer(reinterpret_cast<void*>(kg));
	kg->geom.push_back(obj);
	for (unsigned int k = 0 ; k < kg->link->attachedBodies.size() ; ++k)
	{
	    btCollisionObject *obja = createCollisionBody(kg->link->attachedBodies[k]->shape, scale, padding);
	    assert(obja);	
	    m_world->addCollisionObject(obja);
	    obja->setUserPointer(reinterpret_cast<void*>(kg));
	    kg->geom.push_back(obja);
	}
	m_modelGeom.linkGeom.push_back(kg);
    }
}

btCollisionObject* collision_space::EnvironmentModelBullet::createCollisionBody(shapes::Shape *shape, double scale, double padding)
{
    btCollisionShape *btshape = NULL;
    
    switch (shape->type)
    {
    case shapes::SPHERE:
	{
	    btshape = dynamic_cast<btCollisionShape*>(new btSphereShape(static_cast<shapes::Sphere*>(shape)->radius * scale + padding));
	}
	break;
    case shapes::BOX:
	{
	    const double *size = static_cast<shapes::Box*>(shape)->size;
	    btshape = dynamic_cast<btCollisionShape*>(new btBoxShape(btVector3((size[0] * scale + padding)/2.0, (size[1] * scale + padding)/2.0, (size[2] * scale + padding)/2.0)));
	}	
	break;
    case shapes::CYLINDER:
	{
	    double r2 = static_cast<shapes::Cylinder*>(shape)->radius * scale + padding / 2.0;
	    btshape = dynamic_cast<btCollisionShape*>(new btCylinderShapeZ(btVector3(r2, r2, (static_cast<shapes::Cylinder*>(shape)->length * scale + padding)/2.0)));
	}
	break;
    case shapes::MESH:
	{
	    shapes::Mesh *mesh = static_cast<shapes::Mesh*>(shape);
	    btTriangleMesh *btmesh = new btTriangleMesh();
	    
	    for (unsigned int i = 0 ; i < mesh->triangleCount ; ++i)
	    {
		unsigned int i3 = i * 3;
		unsigned int v1 = 3 * mesh->triangles[i3];
		unsigned int v2 = 3 * mesh->triangles[i3 + 1];
		unsigned int v3 = 3 * mesh->triangles[i3 + 2];
		
		btmesh->addTriangle(btVector3(mesh->vertices[v1], mesh->vertices[v1 + 1], mesh->vertices[v1 + 2]),
				    btVector3(mesh->vertices[v2], mesh->vertices[v2 + 1], mesh->vertices[v2 + 2]),
				    btVector3(mesh->vertices[v3], mesh->vertices[v3 + 1], mesh->vertices[v3 + 2]),
				    true);
	    }
	    
	    btGImpactMeshShape *gshape = new btGImpactMeshShape(btmesh);
	    gshape->setLocalScaling(btVector3(scale, scale, scale));
	    gshape->setMargin(padding);
	    gshape->updateBound();
	    btshape = dynamic_cast<btCollisionShape*>(gshape);
	}
	
    default:
	break;
    }
    
    if (btshape)
    {
	btCollisionObject *object = new btCollisionObject();
	object->setCollisionShape(btshape);
	return object;
    }
    else
	return NULL;
}

void collision_space::EnvironmentModelBullet::updateAttachedBodies(void)
{
    const unsigned int n = m_modelGeom.linkGeom.size();    
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	kGeom *kg = m_modelGeom.linkGeom[i];
	
	/* clear previosly attached bodies */
	for (unsigned int k = 1 ; k < kg->geom.size() ; ++k)
	{
	    m_world->removeCollisionObject(kg->geom[k]);
	    delete kg->geom[k]->getCollisionShape();
	    delete kg->geom[k];
	}
	
	kg->geom.resize(1);
	
	/* create new set of attached bodies */	
	const unsigned int nab = kg->link->attachedBodies.size();
	for (unsigned int k = 0 ; k < nab ; ++k)
	{
	    btCollisionObject *obja = createCollisionBody(kg->link->attachedBodies[k]->shape, m_modelGeom.scale, m_modelGeom.padding);
	    assert(obja);
	    m_world->addCollisionObject(obja);
	    obja->setUserPointer(reinterpret_cast<void*>(kg));
	    kg->geom.push_back(obja);
	}
    }
}

void collision_space::EnvironmentModelBullet::updateRobotModel(void)
{ 
    const unsigned int n = m_modelGeom.linkGeom.size();
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	m_modelGeom.linkGeom[i]->geom[0]->setWorldTransform(m_modelGeom.linkGeom[i]->link->globalTrans);
	const unsigned int nab = m_modelGeom.linkGeom[i]->link->attachedBodies.size();
	for (unsigned int k = 0 ; k < nab ; ++k)
	    m_modelGeom.linkGeom[i]->geom[k + 1]->setWorldTransform(m_modelGeom.linkGeom[i]->link->attachedBodies[k]->globalTrans);
    }
}

bool collision_space::EnvironmentModelBullet::isCollision(void)
{   
    m_world->getPairCache()->setOverlapFilterCallback(&m_genericCollisionFilterCallback);
    m_world->performDiscreteCollisionDetection();
    
    unsigned int numManifolds = m_world->getDispatcher()->getNumManifolds();
    for(unsigned int i = 0; i < numManifolds; ++i)
    {
	btPersistentManifold* contactManifold = m_world->getDispatcher()->getManifoldByIndexInternal(i);
	
	if (contactManifold->getNumContacts() > 0)
	    return true;
    }
    
    return false;
}

bool collision_space::EnvironmentModelBullet::getCollisionContacts(std::vector<Contact> &contacts, unsigned int max_count)
{        
    m_world->getPairCache()->setOverlapFilterCallback(&m_genericCollisionFilterCallback);
    m_world->performDiscreteCollisionDetection();
    bool collision = false;
    contacts.clear();
    
    unsigned int numManifolds = m_world->getDispatcher()->getNumManifolds();
    for(unsigned int i = 0; i < numManifolds; ++i)
    {
	btPersistentManifold* contactManifold = m_world->getDispatcher()->getManifoldByIndexInternal(i);
	int nc = contactManifold->getNumContacts();
	if (nc > 0)
	{
	    collision = true;
	    for (int j = 0 ; j < nc && contacts.size() < max_count ; ++j)
	    {
		btManifoldPoint& pt = contactManifold->getContactPoint(j);
		collision_space::EnvironmentModelBullet::Contact add;
		add.pos = pt.getPositionWorldOnB();
		add.normal = pt.m_normalWorldOnB;
		add.depth = -pt.getDistance();
		add.link1 = NULL;
		add.link2 = NULL;
		btCollisionObject* b0 = reinterpret_cast<btCollisionObject*>(contactManifold->getBody0());
		btCollisionObject* b1 = reinterpret_cast<btCollisionObject*>(contactManifold->getBody1());
		if (b1 && b0)
		{
		    kGeom* kg0 = reinterpret_cast<kGeom*>(b0->getUserPointer());
		    kGeom* kg1 = reinterpret_cast<kGeom*>(b1->getUserPointer());
		    if (kg0)
			add.link1 = kg0->link;
		    if (kg1)
			add.link2 = kg1->link;
		}
		contacts.push_back(add);
	    }
	    if (contacts.size() >= max_count)
		break;
	}
    }
    
    return collision;
}

bool collision_space::EnvironmentModelBullet::isSelfCollision(void)
{
    m_world->getPairCache()->setOverlapFilterCallback(&m_selfCollisionFilterCallback);
    m_world->performDiscreteCollisionDetection();
    
    unsigned int numManifolds = m_world->getDispatcher()->getNumManifolds();
    for(unsigned int i = 0; i < numManifolds; ++i)
    {
	btPersistentManifold* contactManifold = m_world->getDispatcher()->getManifoldByIndexInternal(i);
	
	if (contactManifold->getNumContacts() > 0)
	    return true;
    }
    
    return false;
}

void collision_space::EnvironmentModelBullet::addPointCloud(unsigned int n, const double *points)
{
    btTransform t;
    t.setIdentity();
    
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	unsigned int i4 = i * 4;
	btCollisionObject *object = new btCollisionObject();
	btCollisionShape *shape = new btSphereShape(points[i4 + 3]);
	object->setCollisionShape(shape);
	t.setOrigin(btVector3(points[i4], points[i4 + 1], points[i4 + 2]));
	object->setWorldTransform(t);
	m_world->addCollisionObject(object);	
	m_dynamicObstacles.push_back(object);
    }
}

void collision_space::EnvironmentModelBullet::addStaticPlane(double a, double b, double c, double d)
{
    btCollisionObject *object = new btCollisionObject();
    btCollisionShape *shape = new btStaticPlaneShape(btVector3(a, b, c), d);
    object->setCollisionShape(shape);
    m_world->addCollisionObject(object);
}

void collision_space::EnvironmentModelBullet::clearObstacles(void)
{
    for (unsigned int i = 0 ; i < m_dynamicObstacles.size() ; ++i)
    {
	btCollisionObject* obj = m_dynamicObstacles[i];
	m_world->removeCollisionObject(obj);
	delete obj->getCollisionShape();
	delete obj;
    }
    m_dynamicObstacles.clear();
}

int collision_space::EnvironmentModelBullet::setCollisionCheck(const std::string &link, bool state)
{ 
    int result = -1;
    for (unsigned int j = 0 ; j < m_modelGeom.linkGeom.size() ; ++j)
    {
	if (m_modelGeom.linkGeom[j]->link->name == link)
	{
	    result = m_modelGeom.linkGeom[j]->enabled ? 1 : 0;
	    m_modelGeom.linkGeom[j]->enabled = state;
	    break;
	}
    }

    return result;    
}