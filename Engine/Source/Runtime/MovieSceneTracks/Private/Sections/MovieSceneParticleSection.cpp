// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParticleSection.h"


UMovieSceneParticleSection::UMovieSceneParticleSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	ParticleKeys.SetDefaultValue((int32)EParticleKey::Inactive);
	ParticleKeys.SetUseDefaultValueBeforeFirstKey(true);
	SetIsInfinite(true);
}

void UMovieSceneParticleSection::AddKey( float Time, EParticleKey::Type KeyType )
{
	ParticleKeys.AddKey( Time, (int32)KeyType );
}

FIntegralCurve& UMovieSceneParticleSection::GetParticleCurve()
{
	return ParticleKeys;
}

void UMovieSceneParticleSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	ParticleKeys.ShiftCurve( DeltaPosition, KeyHandles );
}

void UMovieSceneParticleSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection( DilationFactor, Origin, KeyHandles );

	ParticleKeys.ScaleCurve( Origin, DilationFactor, KeyHandles );
}

void UMovieSceneParticleSection::GetKeyHandles( TSet<FKeyHandle>& KeyHandles ) const
{
	for ( auto It( ParticleKeys.GetKeyHandleIterator() ); It; ++It )
	{
		float Time = ParticleKeys.GetKeyTime( It.Key() );
		if ( IsTimeWithinSection( Time ) )
		{
			KeyHandles.Add( It.Key() );
		}
	}
}
