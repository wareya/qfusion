#include "snd_env_sampler.h"
#include "snd_local.h"

static vec3_t oldListenerOrigin;
static vec3_t oldListenerVelocity;
static bool wasListenerInLiquid;

#define MAX_DIRECT_OBSTRUCTION_SAMPLES ( 8 )
#define MAX_REVERB_PRIMARY_RAY_SAMPLES ( 48 )

static vec3_t randomDirectObstructionOffsets[(1 << 8)];
static vec3_t randomReverbPrimaryRayDirs[(1 << 16)];

#ifndef M_2_PI
#define M_2_PI ( 2.0 * ( M_PI ) )
#endif

static inline void MakeRandomDirection( vec3_t dir ) {
	float theta = ( float )( M_2_PI * 0.999999 * random() );
	float phi = (float)( M_PI * random() );
	float sinTheta = sinf( theta );
	float cosTheta = cosf( theta );
	float sinPhi = sinf( phi );
	float cosPhi = cosf( phi );

	dir[0] = sinTheta * cosPhi;
	dir[1] = sinTheta * sinPhi;
	dir[2] = cosTheta;
}

#ifndef ARRAYSIZE
#define ARRAYSIZE( x ) ( sizeof( x ) / sizeof( x[0] ) )
#endif

static void ENV_InitRandomTables() {
	unsigned i, j;

	for( i = 0; i < ARRAYSIZE( randomDirectObstructionOffsets ); i++ ) {
		for( j = 0; j < 3; ++j ) {
			randomDirectObstructionOffsets[i][j] = -20.0f + 40.0f * random();
		}
	}

	for( i = 0; i < ARRAYSIZE( randomReverbPrimaryRayDirs ); i++ ) {
		MakeRandomDirection( randomReverbPrimaryRayDirs[i] );
	}
}

void ENV_Init() {
	if( !s_environment_effects->integer ) {
		return;
	}

	VectorClear( oldListenerOrigin );
	VectorClear( oldListenerVelocity );
	wasListenerInLiquid = false;

	ENV_InitRandomTables();
}

void ENV_Shutdown() {
}

void ENV_RegisterSource( src_t *src ) {
	if( src->priority != SRCPRI_LOCAL && s_environment_effects->integer != 0 ) {
		src->envUpdateState.envProps.useEfx = true;
	} else {
		src->envUpdateState.envProps.useEfx = false;
	}
	// Invalidate last update when reusing the source
	// (otherwise it might be misused for props interpolation)
	src->envUpdateState.lastEnvUpdateAt = 0;
	// Force an immediate update
	src->envUpdateState.nextEnvUpdateAt = 0;
	// Reset sampling patterns by setting an illegal quality value
	src->envUpdateState.directObstructionSamplingProps.quality = -1.0f;
	src->envUpdateState.reverbPrimaryRaysSamplingProps.quality = -1.0f;
}

void ENV_UnregisterSource( src_t *src ) {
	src->envUpdateState.envProps.useEfx = false;
}

static src_t *sourceUpdatesHeap[MAX_SRC];
static unsigned sourceUpdatesHeapSize;

static inline void ENV_PrepareUpdatesPriorityQueue() {
	sourceUpdatesHeapSize = 0;
}

static void ENV_ProcessUpdatesPriorityQueue();
static void ENV_AddSourceToUpdatesPriorityQueue( src_t *src, float urgencyScale );

static void ENV_UpdateSourceEnvironment( src_t *src, int64_t millisNow );
static void ENV_ComputeDirectObstruction( src_t *src );
static void ENV_ComputeReverberation( src_t *src );

static inline void ENV_CollectForcedEnvironmentUpdates() {
	src_t *src, *end;

	for( src = srclist, end = srclist + src_count; src != end; ++src ) {
		if( !src->isActive ) {
			continue;
		}

		if( src->priority != SRCPRI_LOCAL ) {
			ENV_AddSourceToUpdatesPriorityQueue( src, 1.0f );
			continue;
		}

		if( !src->envUpdateState.nextEnvUpdateAt ) {
			ENV_AddSourceToUpdatesPriorityQueue( src, 1.0f );
			continue;
		}
	}
}

static void ENV_CollectRegularEnvironmentUpdates() {
	src_t *src, *end;
	envUpdateState_t *updateState;
	int64_t millisNow;
	int contents;
	bool wasInLiquid, isInLiquid;

	millisNow = trap_Milliseconds();

	for( src = srclist, end = srclist + src_count; src != end; ++src ) {
		if( !src->isActive ) {
			continue;
		}

		updateState = &src->envUpdateState;
		if( src->priority == SRCPRI_LOCAL ) {
			// If this source has never been updated, add it to the queue, otherwise skip further updates.
			if( !updateState->nextEnvUpdateAt ) {
				ENV_AddSourceToUpdatesPriorityQueue( src, 5.0f );
			}
			continue;
		}

		contents = trap_PointContents( src->origin );
		isInLiquid = ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) != 0;
		wasInLiquid = updateState->wasInLiquid;
		updateState->wasInLiquid = isInLiquid;
		if( isInLiquid ^ wasInLiquid ) {
			ENV_AddSourceToUpdatesPriorityQueue( src, 2.0f );
			continue;
		}

		if( updateState->nextEnvUpdateAt <= millisNow ) {
			// If the playback has been just added
			if( !updateState->nextEnvUpdateAt ) {
				ENV_AddSourceToUpdatesPriorityQueue( src, 5.0f );
			} else {
				ENV_AddSourceToUpdatesPriorityQueue( src, 1.0f );
			}
			continue;
		}

		// If the sound is not fixed
		if( updateState->entNum >= 0 ) {
			// If the sound origin has been significantly modified
			if( DistanceSquared( src->origin, updateState->lastUpdateOrigin ) > 128 * 128 ) {
				ENV_AddSourceToUpdatesPriorityQueue( src, 1.5f );
				continue;
			}

			// If the entity velocity has been significantly modified
			if( DistanceSquared( src->velocity, updateState->lastUpdateVelocity ) > 200 * 200 ) {
				ENV_AddSourceToUpdatesPriorityQueue( src, 1.5f );
				continue;
			}
		}
	}
}

static void PushUpdatesHeap( src_t *src ) {
	src_t **heap = sourceUpdatesHeap;
	unsigned childIndex = sourceUpdatesHeapSize;

	sourceUpdatesHeap[sourceUpdatesHeapSize++] = src;

	while( childIndex > 0 ) {
		unsigned parentIndex = ( childIndex - 1 ) / 2;
		envUpdateState_t *childState = &heap[childIndex]->envUpdateState;
		envUpdateState_t *parentState = &heap[childIndex]->envUpdateState;
		if( childState->priorityInQueue > parentState->priorityInQueue ) {
			src_t *tmp = heap[childIndex];
			heap[childIndex] = heap[parentIndex];
			heap[parentIndex] = tmp;
		} else {
			break;
		}
		childIndex = parentIndex;
	}
}

static src_t *PopUpdatesHeap() {
	src_t *result;
	src_t **heap;
	unsigned holeIndex;

	heap = sourceUpdatesHeap;
	assert( sourceUpdatesHeapSize );
	result = heap[0];
	sourceUpdatesHeapSize--;
	heap[0] = heap[sourceUpdatesHeapSize];

	holeIndex = 0;
	// While a left child exists
	while( 2 * holeIndex + 1 < sourceUpdatesHeapSize ) {
		// Select the left child by default
		unsigned childIndex = 2 * holeIndex + 1;
		envUpdateState_t *holeState = &heap[holeIndex]->envUpdateState;
		envUpdateState_t *childState = &heap[childIndex]->envUpdateState;
		// If a right child exists
		if( childIndex < sourceUpdatesHeapSize - 1 ) {
			envUpdateState_t *rightChildState = &heap[childIndex + 1]->envUpdateState;
			// If right child is greater than left one
			if( rightChildState->priorityInQueue > childState->priorityInQueue ) {
				childIndex = childIndex + 1;
				childState = rightChildState;
			}
		}

		// Bubble down lesser hole value
		if( holeState->priorityInQueue < childState->priorityInQueue ) {
			src_t *tmp = heap[childIndex];
			heap[childIndex] = heap[holeIndex];
			heap[holeIndex] = tmp;
			holeIndex = childIndex;
		} else {
			break;
		}
	}

	return result;
}

static void ENV_AddSourceToUpdatesPriorityQueue( src_t *src, float urgencyScale ) {
	float attenuationScale;

	assert( urgencyScale >= 0.0f );

	attenuationScale = src->attenuation / 20.0f;
	clamp_high( attenuationScale, 1.0f );
	attenuationScale = sqrtf( attenuationScale );
	assert( attenuationScale >= 0.0f && attenuationScale <= 1.0f );

	src->envUpdateState.priorityInQueue = urgencyScale;
	src->envUpdateState.priorityInQueue *= 1.0f - 0.7f * attenuationScale;

	PushUpdatesHeap( src );
}

static void ENV_ProcessUpdatesPriorityQueue() {
	int64_t millis = trap_Milliseconds();
	src_t *src;
	unsigned numUpdates = 0;

	// Always do at least a single update
	for( ;; ) {
		if( !sourceUpdatesHeapSize ) {
			break;
		}

		src = PopUpdatesHeap();
		ENV_UpdateSourceEnvironment( src, millis );
		numUpdates++;
		// Stop doing updates if there were enough updates this frame and left sources have low priority
		if( numUpdates >= 3 && src->envUpdateState.priorityInQueue < 1.0f ) {
			break;
		}
	}
}

void ENV_UpdateRelativeSoundsSpatialization( const vec3_t origin, const vec3_t velocity ) {
	src_t *src, *end;

	for( src = srclist, end = srclist + src_count; src != end; ++src ) {
		if( !src->isActive ) {
			continue;
		}
		if( src->attenuation ) {
			continue;
		}
		VectorCopy( origin, src->origin );
		VectorCopy( velocity, src->velocity );
	}
}

void ENV_UpdateListener( const vec3_t origin, const vec3_t velocity ) {
	vec3_t testedOrigin;
	bool needsForcedUpdate = false;
	bool isListenerInLiquid;

	if( !s_environment_effects->integer ) {
		return;
	}

	ENV_UpdateRelativeSoundsSpatialization( origin, velocity );

	// Check whether we have teleported or entered/left a liquid.
	// Run a forced major update in this case.

	if( DistanceSquared( origin, oldListenerOrigin ) > 100.0f * 100.0f ) {
		needsForcedUpdate = true;
	} else if( DistanceSquared( velocity, oldListenerVelocity ) > 200.0f * 200.0f ) {
		needsForcedUpdate = true;
	}

	// Check the "head" contents. We assume the regular player viewheight.
	VectorCopy( origin, testedOrigin );
	testedOrigin[2] += 18;
	int contents = trap_PointContents( testedOrigin );

	isListenerInLiquid = ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) != 0;
	if( wasListenerInLiquid != isListenerInLiquid ) {
		needsForcedUpdate = true;
	}

	VectorCopy( origin, oldListenerOrigin );
	VectorCopy( velocity, oldListenerVelocity );
	wasListenerInLiquid = isListenerInLiquid;

	// Sanitize the possibly modified cvar before the environment update
	if( s_environment_sampling_quality->value < 0.0f || s_environment_sampling_quality->value > 1.0f ) {
		trap_Cvar_ForceSet( s_environment_sampling_quality->name, "0.5" );
	}

	ENV_PrepareUpdatesPriorityQueue();

	if( needsForcedUpdate ) {
		ENV_CollectForcedEnvironmentUpdates();
	} else {
		ENV_CollectRegularEnvironmentUpdates();
	}

	ENV_ProcessUpdatesPriorityQueue();
}

static void ENV_InterpolateEnvironmentProps( src_t *src, int64_t millisNow ) {
	envProps_t *newProps = &src->envUpdateState.envProps;
	envProps_t *oldProps = &src->envUpdateState.oldEnvProps;

	// Do not interpolate completely different effects
	if( ( newProps->useEfx ^ oldProps->useEfx ) || ( newProps->useFlanger ^ oldProps->useFlanger ) ) {
		// Just save the old props
		*oldProps = *newProps;
		return;
	}

	if( !newProps->useEfx ) {
		return;
	}

	if( src->envUpdateState.lastEnvUpdateAt + 150 >= millisNow ) {
		float frac = ( millisNow - src->envUpdateState.lastEnvUpdateAt ) / 150.0f;
		assert( frac >= 0.0f );
		clamp_high( frac, 1.0f );
		// Elder the old props are, greater is the weight for the new props
		float newWeight = 0.5f + 0.3f * frac;
		float oldWeight = 0.5f - 0.3f * frac;
		assert( fabsf( newWeight + oldWeight - 1.0f ) < 0.001f );

		newProps->directObstruction = newWeight * newProps->directObstruction + oldWeight * oldProps->directObstruction;
		oldProps->directObstruction = newProps->directObstruction;
		// Sanitize result to avoid FP computation errors leading to result being out of range
		clamp( newProps->directObstruction, 0, 1 );

		if( !newProps->useFlanger ) {
			const uint8_t *newBasePtr = ( const uint8_t * )&newProps->reverbProps;
			const uint8_t *oldBasePtr = ( const uint8_t * )&oldProps->reverbProps;
			int i;

			for( i = 0; i < NUM_REVERB_PARAMS; i++ ) {
				const reverbParamDef_t *paramDef = &reverbParamsDefs[i];
				float *newValue = ( float * )( newBasePtr + paramDef->propsMemberOffset );
				float *oldValue = ( float * )( oldBasePtr + paramDef->propsMemberOffset );
				*newValue = newWeight * ( *newValue ) + oldWeight * ( *oldValue );
				*oldValue = *newValue;
				clamp( *newValue, paramDef->minValue, paramDef->maxValue );
			}
		}
	}

	src->envUpdateState.lastEnvUpdateAt = millisNow;
}

static void ENV_UpdateSourceEnvironment( src_t *src, int64_t millisNow ) {
	envUpdateState_t *updateState = &src->envUpdateState;
	envProps_t *envProps = &src->envUpdateState.envProps;

	if( src->priority == SRCPRI_LOCAL ) {
		src->envUpdateState.envProps.useEfx = false;
		// Check whether the source has never been updated for this local sound.
		assert( !src->envUpdateState.nextEnvUpdateAt );
		// Prevent later updates
		src->envUpdateState.nextEnvUpdateAt = INT64_MAX;
		// Apply the update immediately and return
		S_UpdateEFX( src );
		return;
	}

	// Randomize the update period a bit.
	// Otherwise there will be another updates spike
	// an update period ahead after a forced/initial update.
	updateState->nextEnvUpdateAt = (int64_t)( millisNow + 108 + 32 * random() );

	VectorCopy( src->origin, updateState->lastUpdateOrigin );
	VectorCopy( src->velocity, updateState->lastUpdateVelocity );

	if( updateState->wasInLiquid || wasListenerInLiquid ) {
		envProps->useFlanger = true;

		if( updateState->wasInLiquid && wasListenerInLiquid ) {
			ENV_ComputeDirectObstruction( src );
		} else {
			// Just use a very high obstruction
			envProps->directObstruction = 0.9f;
		}
	} else {
		envProps->useFlanger = false;
		ENV_ComputeDirectObstruction( src );
		ENV_ComputeReverberation( src );
	}

	ENV_InterpolateEnvironmentProps( src, millisNow );

	S_UpdateEFX( src );
}

static void ENV_SetupSamplingProps( samplingProps_t *props, unsigned minSamples, unsigned maxSamples ) {
	unsigned numSamples;
	float quality = s_environment_sampling_quality->value;

	// If the quality is valid and has not been modified since the pattern has been set
	if( props->quality == quality ) {
		return;
	}

	assert( quality >= 0.0f && quality <= 1.0f );
	assert( minSamples < maxSamples );

	numSamples = (unsigned)( minSamples + ( maxSamples - minSamples ) * quality );
	assert( numSamples && numSamples <= maxSamples );

	props->quality = quality;
	props->numSamples = numSamples;
	props->valueIndex = (uint16_t)( random() * UINT16_MAX );
}

static void ENV_ComputeDirectObstruction( src_t *src ) {
	trace_t trace;
	envUpdateState_t *updateState;
	envProps_t *envProps;
	float *originOffset;
	vec3_t testedListenerOrigin;
	vec3_t testedSourceOrigin;
	float squareDistance;
	unsigned numTestedRays, numPassedRays;
	unsigned i, valueIndex;

	updateState = &src->envUpdateState;
	envProps = &src->envUpdateState.envProps;

	VectorCopy( oldListenerOrigin, testedListenerOrigin );
	// TODO: We assume standard view height
	testedListenerOrigin[2] += 18.0f;

	squareDistance = DistanceSquared( testedListenerOrigin, src->origin );
	// Shortcut for sounds relative to the player
	if( squareDistance < 32.0f * 32.0f ) {
		envProps->directObstruction = 0.0f;
		return;
	}

	if( !trap_InPVS( testedListenerOrigin, src->origin ) ) {
		envProps->directObstruction = 1.0f;
		return;
	}

	trap_Trace( &trace, testedListenerOrigin, src->origin, vec3_origin, vec3_origin, MASK_SOLID );
	if( trace.fraction == 1.0f && !trace.startsolid ) {
		// Consider zero obstruction in this case
		envProps->directObstruction = 0.0f;
		return;
	}

	ENV_SetupSamplingProps( &updateState->directObstructionSamplingProps, 3, MAX_DIRECT_OBSTRUCTION_SAMPLES );

	numPassedRays = 0;
	numTestedRays = updateState->directObstructionSamplingProps.numSamples;
	valueIndex = updateState->directObstructionSamplingProps.valueIndex;
	for( i = 0; i < numTestedRays; i++ ) {
		valueIndex = ( valueIndex + 1 ) % ARRAYSIZE( randomDirectObstructionOffsets );
		originOffset = randomDirectObstructionOffsets[ valueIndex ];

		VectorAdd( src->origin, originOffset, testedSourceOrigin );
		trap_Trace( &trace, testedListenerOrigin, testedSourceOrigin, vec3_origin, vec3_origin, MASK_SOLID );
		if( trace.fraction == 1.0f && !trace.startsolid ) {
			numPassedRays++;
		}
	}

	envProps->directObstruction = 1.0f - 0.9f * ( numPassedRays / (float)numTestedRays );
}

#define REVERB_SAMPLING_ENVIRONMENT_DISTANCE_LIMIT ( 4096.0f )

static void ENV_ComputeReverberation( src_t *src ) {
	trace_t trace;
	float primaryHitDistances[MAX_REVERB_PRIMARY_RAY_SAMPLES];
	vec3_t reflectionPoints[MAX_REVERB_PRIMARY_RAY_SAMPLES];
	vec3_t testedRayPoint;
	vec3_t testedListenerOrigin;
	envUpdateState_t *updateState;
	reverbProps_t *reverbProps;
	float distanceLimit, distance, squareDistance, averageDistance;
	float primaryEmissionRadius;
	float averageHitFactor;
	unsigned numPrimaryRays, numRaysHitSky, numReflectionPoints;
	unsigned numPassedSecondaryRays;
	unsigned i, valueIndex;

	updateState = &src->envUpdateState;
	reverbProps = &updateState->envProps.reverbProps;

	VectorCopy( oldListenerOrigin, testedListenerOrigin );
	testedListenerOrigin[2] += 18.0f;

	distanceLimit = REVERB_SAMPLING_ENVIRONMENT_DISTANCE_LIMIT;
	distanceLimit -= ( 0.75f * REVERB_SAMPLING_ENVIRONMENT_DISTANCE_LIMIT ) * ( src->attenuation / 15.0f );
	assert( distanceLimit <= REVERB_SAMPLING_ENVIRONMENT_DISTANCE_LIMIT );
	clamp_low( distanceLimit, 1024.0f );

	squareDistance = DistanceSquared( testedListenerOrigin, src->origin );
	if( squareDistance < 32.0f * 32.0f ) {
		primaryEmissionRadius = distanceLimit;
	} else {
		distance = sqrtf( squareDistance );
		primaryEmissionRadius = max( distance, distanceLimit );
	}

	ENV_SetupSamplingProps( &updateState->reverbPrimaryRaysSamplingProps, 16, MAX_REVERB_PRIMARY_RAY_SAMPLES );

	averageDistance = 0.0f;
	numRaysHitSky = 0;
	numReflectionPoints = 0;
	numPrimaryRays = updateState->reverbPrimaryRaysSamplingProps.numSamples;
	valueIndex = updateState->reverbPrimaryRaysSamplingProps.valueIndex;
	for( i = 0; i < numPrimaryRays; ++i ) {
		float *sampleDir, *reflectionPoint;
		valueIndex = ( valueIndex + 1 ) % ARRAYSIZE( randomReverbPrimaryRayDirs );
		sampleDir = randomReverbPrimaryRayDirs[valueIndex];

		VectorScale( sampleDir, primaryEmissionRadius, testedRayPoint );
		VectorAdd( testedRayPoint, src->origin, testedRayPoint );
		trap_Trace( &trace, src->origin, testedRayPoint, vec3_origin, vec3_origin, MASK_SOLID );

		if( trace.fraction == 1.0f || trace.startsolid ) {
			continue;
		}

		// Skip surfaces non-reflective for sounds
		if( trace.surfFlags & ( SURF_SKY|SURF_NOIMPACT|SURF_NOMARKS|SURF_FLESH|SURF_DUST|SURF_NOSTEPS ) ) {
			// Go even further for sky. Simulate an "absorption" of sound by the void.
			if( trace.surfFlags & SURF_SKY ) {
				numRaysHitSky++;
			}
			continue;
		}

		if( DistanceSquared( src->origin, trace.endpos ) < 2 * 2 ) {
			continue;
		}

		// Do not use the trace.endpos exactly as a source of a reflected wave.
		// (a following trace call would probably fail for this start origin).
		// Add -sampleDir offset to the trace.endpos
		reflectionPoint = reflectionPoints[numReflectionPoints];
		VectorCopy( sampleDir, reflectionPoint );
		VectorNegate( reflectionPoint, reflectionPoint );
		VectorAdd( trace.endpos, reflectionPoint, reflectionPoint );

		squareDistance = DistanceSquared( src->origin, trace.endpos );
		if( squareDistance < distanceLimit * distanceLimit ) {
			primaryHitDistances[numReflectionPoints] = sqrtf( squareDistance );
		} else {
			primaryHitDistances[numReflectionPoints] = distanceLimit;
		}
		assert( primaryHitDistances[numReflectionPoints] >= 0.0f );
		assert( primaryHitDistances[numReflectionPoints] <= distanceLimit );
		averageDistance += primaryHitDistances[numReflectionPoints];

		numReflectionPoints++;
	}

	averageHitFactor = ( numReflectionPoints / (float)numPrimaryRays );
	assert( averageHitFactor >= 0.0f && averageHitFactor <= 1.0f );

	// Obviously gain should be higher for enclosed environments.
	// Do not lower it way too hard here as it is affected by "room size factor" too
	reverbProps->gain = 0.20f + 0.05f * averageHitFactor;

	// Simulate sound absorption by the void by lowering this value compared to its default one
	reverbProps->lateReverbGain = 1.26f - 0.26f * sqrtf( numRaysHitSky / (float)numPrimaryRays );

	if( numReflectionPoints ) {
		float averageDistanceFactor, smallRoomFactor, roomSizeFactor, hitDistanceStdDev;
		unsigned numPassedSigmaTestPoints;

		averageDistance /= (float)numReflectionPoints;
		averageDistanceFactor = averageDistance / REVERB_SAMPLING_ENVIRONMENT_DISTANCE_LIMIT;
		assert( averageDistanceFactor >= 0.0f && averageDistanceFactor <= 1.0f );

		// This value is just an optimized intermediate result, does not have much sense being taken alone.
		smallRoomFactor = sqrtf( 1.0f - averageDistanceFactor );

		// Lower the density is the more metallic and distinct the reverberation is.
		// Let enclosed and small environments have lower density.
		reverbProps->density = 1.0f - 0.7f * ( averageHitFactor * smallRoomFactor );
		// Boost the reflections gain for small rooms.
		reverbProps->reflectionsGain = 0.05f + 0.3f * smallRoomFactor;
		// Obviously the reflections delay should be higher for large rooms.
		reverbProps->reflectionsDelay = 0.005f + 0.2f * averageDistanceFactor;
		// Lower late reverb gain for large rooms/environments
		reverbProps->lateReverbGain *= 1.0f - 0.2f * averageDistanceFactor;

		// Compute the standard deviation of hit distances to cut off high outliers
		hitDistanceStdDev = 0.0f;
		for( i = 0; i < numReflectionPoints; i++ ) {
			float delta = primaryHitDistances[i] - averageDistance;
			hitDistanceStdDev += delta * delta;
		}
		hitDistanceStdDev /= numReflectionPoints;
		hitDistanceStdDev = sqrtf( hitDistanceStdDev );

		numPassedSigmaTestPoints = 0;
		roomSizeFactor = 0.0f;
		// Try count only points that have a hit distance > sigma
		for( i = 0; i < numReflectionPoints; i++ ) {
			if( primaryHitDistances[i] < averageDistance + hitDistanceStdDev ) {
				continue;
			}
			roomSizeFactor += sqrtf( primaryHitDistances[i] / distanceLimit );
			numPassedSigmaTestPoints++;
		}

		if( numPassedSigmaTestPoints > 0 ) {
			// Interpolate to prevent a jitter of roomSizeFactor
			float frac = numPassedSigmaTestPoints / (float)numReflectionPoints;
			assert( frac >= 0.0f && frac <= 1.0f );
			frac = sqrtf( frac );
			frac = sqrtf( frac );
			roomSizeFactor /= numPassedSigmaTestPoints;
			assert( roomSizeFactor >= 0.0f && roomSizeFactor <= 1.0f );
			roomSizeFactor = frac * roomSizeFactor + ( 1.0f - frac ) * sqrtf( averageDistanceFactor );
		} else {
			roomSizeFactor = sqrtf( averageDistanceFactor );
		}

		assert( roomSizeFactor >= 0.0f && roomSizeFactor <= 1.0f );
		// Lowering the diffusion adds more "coloration" to the reverb.
		// Lower the diffusion for larger enclosed environments.
		reverbProps->diffusion = 1.0f - roomSizeFactor * averageHitFactor;
		reverbProps->lateReverbDelay = 0.001f + 0.05f * roomSizeFactor;
		reverbProps->decayTime = 0.45f + 1.55f * roomSizeFactor;
		// Lower gain for huge environments. Otherwise it sounds way too artificial.
		reverbProps->gain *= 1.0f - 0.2f * roomSizeFactor;
	} else {
		// The gain is very low for zero reflections point so it should not be weird if an environment differs.
		reverbProps->gain = 0.0f;
		reverbProps->density = 1.0f;
		reverbProps->diffusion = 0.0f;
		reverbProps->reflectionsGain = 0.01f;
		reverbProps->reflectionsDelay = 0.0f;
		reverbProps->lateReverbDelay = 0.1f;
		reverbProps->decayTime = 0.5f;
	}

	// Compute and set reverb obstruction

	numPassedSecondaryRays = 0;
	for( i = 0; i < numReflectionPoints; i++ ) {
		trap_Trace( &trace, reflectionPoints[i], testedListenerOrigin, vec3_origin, vec3_origin, MASK_SOLID );
		if( trace.fraction == 1.0f && !trace.startsolid ) {
			numPassedSecondaryRays++;
		}
	}

	reverbProps->gainHf = 0.1f;
	if( numReflectionPoints ) {
		reverbProps->gainHf += 0.8f * ( numPassedSecondaryRays / (float) numReflectionPoints );
	}
}
