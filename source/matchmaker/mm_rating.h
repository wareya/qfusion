/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef __MM_RATING_H__
#define __MM_RATING_H__

#ifdef __cplusplus
extern "C" {
#endif

//=============================================
//	rating
//=============================================

// rating info structure and utilities freely usable by all modules

#define MM_RATING_DEFAULT       0.0
#define MM_DEVIATION_DEFAULT    1.0
#define MM_DEVIATION_MIN        0.0     // FIXME
#define MM_DEVIATION_MAX        1.0
#define MM_DEFAULT_T            4.0
#define MM_PROBABILITY_DEFAULT  0.5

// We were thinking about using strings without much care about actual uuid representation,
// but string manipulation turned to be painful in the current codebase state,
// so its better to introduce this value-type.
typedef struct mm_uuid_s {
	uint64_t hiPart;
	uint64_t loPart;
} mm_uuid_t;

// Let pass non-modified parameters by value to reduce visual clutter
inline bool Uuid_Compare( mm_uuid_t u1, mm_uuid_t u2 ) {
	return u1.hiPart == u2.hiPart && u1.loPart == u2.loPart;
}

inline mm_uuid_t Uuid_ZeroUuid() {
	mm_uuid_t result = { 0, 0 };
	return result;
}

inline mm_uuid_t Uuid_AllBitsSetUuid() {
	mm_uuid_t result = { (uint64_t)-1, (uint64_t)-1 };
	return result;
}

#define UUID_DATA_LENGTH ( 36 )
#define UUID_BUFFER_SIZE ( UUID_DATA_LENGTH + 1 )

typedef struct clientRating_s {
	mm_uuid_t uuid;
	char gametype[32];
	float rating;
	float deviation;
	struct  clientRating_s *next;
} clientRating_t;

bool Uuid_FromString( const char *buffer, mm_uuid_t *uuid );

const char *Uuid_ToString( char *buffer, mm_uuid_t uuid );

inline bool Uuid_IsValidAuthSessionId( mm_uuid_t uuid ) {
	if( uuid.hiPart == 0 && uuid.loPart == 0 ) {
		return false;
	}
	if( uuid.hiPart == (uint64_t)-1 && uuid.loPart == (uint64_t)-1) {
		return false;
	}
	return true;
}

inline bool Uuid_IsZeroUuid( mm_uuid_t uuid ) {
	return uuid.hiPart == 0 && uuid.loPart == 0;
}

inline bool Uuid_IsAllBitsSetUuid( mm_uuid_t uuid ) {
	return uuid.hiPart == (uint64_t)-1 && uuid.loPart == (uint64_t)-1;
}

// returns the given rating or NULL
clientRating_t *Rating_Find( clientRating_t *ratings, const char *gametype );
clientRating_t *Rating_FindId( clientRating_t *ratings, const mm_uuid_t *uuid );
// detaches given rating from the list, returns the element and sets the ratings argument
// to point to the new root. Returns NULL if gametype wasn't found
clientRating_t *Rating_Detach( clientRating_t **list, const char *gametype );
clientRating_t *Rating_DetachId( clientRating_t **list, const mm_uuid_t *uuid );

// returns a value between 0-1 for single clientRating against list of other clientRatings
// if single is on the list, it is ignored for the calculation
float Rating_GetProbability( clientRating_t *single, clientRating_t *list );
// head-on probability
float Rating_GetProbabilitySingle( clientRating_t *single, clientRating_t *other );

// TODO: Teams probability
// TODO: balanced team making
// TODO: find best opponent
// TODO: find best pairs

// create an average clientRating out of list of clientRatings
void Rating_AverageRating( clientRating_t *out, clientRating_t *list );

#ifdef __cplusplus
};
#endif

#endif
