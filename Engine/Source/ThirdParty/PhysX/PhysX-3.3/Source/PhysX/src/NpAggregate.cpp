/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  


#include "NpAggregate.h"

///////////////////////////////////////////////////////////////////////////////

#include "PxActor.h"
#include "NpRigidStatic.h"
#include "NpRigidDynamic.h"
#include "NpParticleSystem.h"
#include "NpParticleFluid.h"
#include "NpArticulationLink.h"

using namespace physx;

PX_FORCE_INLINE void setAggregate(NpAggregate* aggregate, PxActor& actor)
{
	NpActor& np = NpActor::getFromPxActor(actor);
	np.setAggregate(aggregate, actor);
}

///////////////////////////////////////////////////////////////////////////////


NpAggregate::NpAggregate(PxU32 maxActors, bool selfCollisions)
: PxAggregate(PxConcreteType::eAGGREGATE, PxBaseFlag::eOWNS_MEMORY | PxBaseFlag::eIS_RELEASABLE)
, mAggregate(this, maxActors, selfCollisions)
, mNbActors(0)
{
	mActors = (PxActor**)PX_ALLOC(sizeof(PxActor*)*maxActors, PX_DEBUG_EXP("PxActor*"));
}

NpAggregate::~NpAggregate()
{
	NpFactory::getInstance().onAggregateRelease(this);
	if(getBaseFlags()&PxBaseFlag::eOWNS_MEMORY)
		PX_FREE(mActors);
}

void NpAggregate::removeAndReinsert(PxActor& actor, bool reinsert)
{
	NpActor& np = NpActor::getFromPxActor(actor);
	Scb::Actor& scb = NpActor::getScbFromPxActor(actor);

	np.setAggregate(NULL, actor);
	
	mAggregate.removeActor(scb, reinsert);
}

void NpAggregate::release()
{
	NP_WRITE_CHECK(getOwnerScene());

	NpPhysics::getInstance().notifyDeletionListenersUserRelease(this, NULL);

	/*
	"An aggregate should be empty when it gets released. If it isn't, the behavior should be: remove the actors from
	the aggregate, then remove the aggregate from the scene (if any) then delete it. I guess that implies the actors
	get individually reinserted into the broad phase if the aggregate is in a scene."
	*/
	for(PxU32 i=0;i<mNbActors;i++)
	{
		if (mActors[i]->getType() == PxActorType::eARTICULATION_LINK)
			static_cast<NpArticulationLink*>(mActors[i])->getRoot().setAggregate(NULL);

		removeAndReinsert(*mActors[i], true);
	}

	NpScene* s = getAPIScene();
	if(s)
	{
		s->getScene().removeAggregate(getScbAggregate());
		s->removeFromAggregateList(*this);

		GRB_EVENT(s, GrbInteropEvent3, GrbInteropEvent3::PxAggregateRelease, this);
	}

	mAggregate.destroy();
}

void NpAggregate::addActorInternal(PxActor& actor, NpScene& s)
{
	if (actor.getType() != PxActorType::eARTICULATION_LINK)
	{
		Scb::Actor& scb = NpActor::getScbFromPxActor(actor);

		mAggregate.addActor(scb);

		s.addActorInternal(actor);
	}
	else if (!actor.getScene())  // This check makes sure that a link of an articulation gets only added once.
	{
		NpArticulationLink& al = static_cast<NpArticulationLink&>(actor);
		NpArticulation& npArt = al.getRoot();
		NpArticulationLink* const* links = npArt.getLinks();
		for(PxU32 i=0; i < npArt.getNbLinks(); i++)
		{
			mAggregate.addActor(links[i]->getScbActorFast());
		}

		s.addArticulationInternal(npArt);
	}
}

bool NpAggregate::addActor(PxActor& actor)
{
	NP_WRITE_CHECK(getOwnerScene());

	if(mNbActors==mAggregate.getMaxActorCount())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add actor to aggregate, max number of actors reached");
		return false;
	}

	if(actor.getAggregate())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add actor to aggregate, actor already belongs to an aggregate");
		return false;
	}

	if(actor.getScene())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add actor to aggregate, actor already belongs to a scene");
		return false;
	}

	if(actor.getType() == PxActorType::eARTICULATION_LINK)
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add articulation link to aggregate, only whole articulations can be added");
		return false;
	}

	setAggregate(this, actor);

	mActors[mNbActors++] = &actor;

	// PT: when an object is added to a aggregate at runtime, i.e. when the aggregate has already been added to the scene,
	// we need to immediately add the newcomer to the scene as well.
	NpScene* s = getAPIScene();
	if(s)
	{
		addActorInternal(actor, *s);

		GRB_EVENT(s, GrbInteropEvent3, GrbInteropEvent3::PxAggregateAddActor, this, &actor);
	}

	return true;
}

bool NpAggregate::removeActorAndReinsert(PxActor& actor, bool reinsert)
{
	for(PxU32 i=0;i<mNbActors;i++)
	{
		if(mActors[i]==&actor)
		{
			mActors[i] = mActors[--mNbActors];
			removeAndReinsert(actor, reinsert);
			return true;
		}
	}
	Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't remove actor, actor doesn't belong to aggregate");
	return false;
}

bool NpAggregate::removeActor(PxActor& actor)
{
	NP_WRITE_CHECK(getOwnerScene());

	if(actor.getType() == PxActorType::eARTICULATION_LINK)
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't remove articulation link, only whole articulations can be removed");
		return false;
	}

	// PT: there are really 2 cases here:
	// a) the user just wants to remove the actor from the aggregate, but the actor is still alive so if the aggregate has been added to a scene,
	//    we must reinsert the removed actor to that same scene
	// b) this is called by the framework when releasing an actor, in which case we don't want to reinsert it anywhere.
	//
	// We assume that when called by the user, we always want to reinsert. The framework however will call the internal function
	// without reinsertion.
	return removeActorAndReinsert(actor, true);
}

bool NpAggregate::addArticulation(PxArticulation& art)
{
	NP_WRITE_CHECK(getOwnerScene());

	if((mNbActors+art.getNbLinks()) > mAggregate.getMaxActorCount())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add articulation links, max number of actors reached");
		return false;
	}

	if(art.getAggregate())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add articulation to aggregate, articulation already belongs to an aggregate");
		return false;
	}

	if(art.getScene())
	{
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't add articulation to aggregate, articulation already belongs to a scene");
		return false;
	}

	NpArticulation& npArt = static_cast<NpArticulation&>(art);
	npArt.setAggregate(this);
	NpArticulationLink* const* links = npArt.getLinks();
	for(PxU32 i=0; i < npArt.getNbLinks(); i++)
	{
		NpArticulationLink& l = *links[i];

		setAggregate(this, l);

		mActors[mNbActors++] = &l;

		mAggregate.addActor(l.getScbActorFast());
	}

	// PT: when an object is added to a aggregate at runtime, i.e. when the aggregate has already been added to the scene,
	// we need to immediately add the newcomer to the scene as well.
	NpScene* s = getAPIScene();
	if(s)
	{
		s->addArticulationInternal(art);	
	}

	return true;
}

bool NpAggregate::removeArticulationAndReinsert(PxArticulation& art, bool reinsert)
{
	NpArticulation* npArt = static_cast<NpArticulation*>(&art);

	bool found = false;
	PxU32 idx = 0;
	while(idx < mNbActors)
	{
		if ((mActors[idx]->getType() == PxActorType::eARTICULATION_LINK) && (&static_cast<NpArticulationLink*>(mActors[idx])->getRoot() == npArt))
		{
			PxActor* a = mActors[idx];
			mActors[idx] = mActors[--mNbActors];
			removeAndReinsert(*a, reinsert);
			found = true;
		}
		else
			idx++;
	}

	npArt->setAggregate(NULL);

	if (!found)
		Ps::getFoundation().error(PxErrorCode::eDEBUG_WARNING, __FILE__, __LINE__, "PxAggregate: can't remove articulation, articulation doesn't belong to aggregate");
	return found;
}

bool NpAggregate::removeArticulation(PxArticulation& art)
{
	NP_WRITE_CHECK(getOwnerScene());

	// see comments in removeActor()
	return removeArticulationAndReinsert(art, true);
}

PxU32 NpAggregate::getNbActors() const
{
	NP_READ_CHECK(getOwnerScene());
	return mNbActors;
}

PxU32 NpAggregate::getMaxNbActors() const
{
	NP_READ_CHECK(getOwnerScene());
	return mAggregate.getMaxActorCount();
}

PxU32 NpAggregate::getActors(PxActor** userBuffer, PxU32 bufferSize, PxU32 startIndex) const
{
	NP_READ_CHECK(getOwnerScene());

	const PxU32 size = getCurrentSizeFast();
	PxActor*const* PX_RESTRICT actors = mActors + startIndex;

	const PxU32 remainder = (PxU32)PxMax<PxI32>(PxI32(size - startIndex), 0);
	const PxU32 writeCount = PxMin(remainder, bufferSize);
	PxMemCopy(userBuffer, actors, writeCount*sizeof(PxActor*));

	return writeCount;
}

PxScene* NpAggregate::getScene()
{
	return getAPIScene();
}

NpScene* NpAggregate::getAPIScene() const
{
	return mAggregate.getScbSceneForAPI() ? static_cast<NpScene*>(mAggregate.getScbSceneForAPI()->getPxScene()) : NULL;
}

NpScene* NpAggregate::getOwnerScene() const
{
	return mAggregate.getScbScene() ? static_cast<NpScene*>(mAggregate.getScbScene()->getPxScene()) : NULL;
}

bool NpAggregate::getSelfCollision() const
{
	NP_READ_CHECK(getOwnerScene());
	return mAggregate.getSelfCollide();
}

// PX_SERIALIZATION
void NpAggregate::exportExtraData(PxSerializationContext& stream)
{
	if(mActors)
	{
		stream.alignData(PX_SERIAL_ALIGN);
		stream.writeData(mActors, mNbActors * sizeof(PxActor*));
	}
}

void NpAggregate::importExtraData(PxDeserializationContext& context)
{
	if(mActors)
		mActors = context.readExtraData<PxActor*, PX_SERIAL_ALIGN>(mNbActors);
}

void NpAggregate::resolveReferences(PxDeserializationContext& context)
{
	// Resolve actor pointers if needed
	for(PxU32 i=0; i < mNbActors; i++)
	{
		context.translatePxBase(mActors[i]);
		{
			//update aggregate if mActors is in external reference
			NpActor& np = NpActor::getFromPxActor(*mActors[i]);
			if(np.getAggregate() == NULL)
			{
				np.setAggregate(this, *mActors[i]);
			}
			if(mActors[i]->getType() == PxActorType::eARTICULATION_LINK)
			{
				NpArticulation& articulation = static_cast<NpArticulationLink*>(mActors[i])->getRoot();
				if(!articulation.getAggregate())
					articulation.setAggregate(this);
			}
		}
	}	
}

NpAggregate* NpAggregate::createObject(PxU8*& address, PxDeserializationContext& context)
{
	NpAggregate* obj = new (address) NpAggregate(PxBaseFlag::eIS_RELEASABLE);
	address += sizeof(NpAggregate);	
	obj->importExtraData(context);
	obj->resolveReferences(context);
	return obj;
}

void NpAggregate::requires(PxProcessPxBaseCallback& c)
{
	for(PxU32 i=0; i < mNbActors; i++)
	{
		PxArticulationLink *link = mActors[i]->isArticulationLink();
		if( link )
		{
			PxArticulation& arti = link->getArticulation();
			c.process(arti);			
		}
		else
		{			
			c.process(*mActors[i]);
		}
	}
}
// ~PX_SERIALIZATION
