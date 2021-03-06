// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.h"
#include "SubMovieSceneTrack.generated.h"

/**
 * A track that holds a sub-sequence within a larger sequence
 */
UCLASS( MinimalAPI )
class USubMovieSceneTrack : public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FName GetTrackName() const override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual void AddSection( UMovieSceneSection* Section ) override;
	virtual void RemoveSection( UMovieSceneSection* Section ) override;
	virtual void RemoveAllAnimationData() override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool HasSection( UMovieSceneSection* Section ) const override;

	/**
	 * Adds a movie scene section
	 *
	 * @param SubMovieScene	The movie scene to add
	 * @param Time	The time to add the section at
	 */
	virtual void AddMovieSceneSection( class UMovieSceneSequence* SubMovieSceneSequence, float Time );
protected:
	/** All movie scene sections.  */
	UPROPERTY()
	TArray<class UMovieSceneSection*> SubMovieSceneSections;
};
